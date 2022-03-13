// =============================================================================
// Copyright 2011-2021 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "bash.hpp"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "actor.hpp"
#include "actor_data.hpp"
#include "actor_hit.hpp"
#include "actor_player.hpp"
#include "array2.hpp"
#include "audio.hpp"
#include "colors.hpp"
#include "common_text.hpp"
#include "debug.hpp"
#include "direction.hpp"
#include "game_time.hpp"
#include "global.hpp"
#include "inventory.hpp"
#include "io.hpp"
#include "item.hpp"
#include "item_data.hpp"
#include "item_factory.hpp"
#include "map.hpp"
#include "map_parsing.hpp"
#include "msg_log.hpp"
#include "player_bon.hpp"
#include "pos.hpp"
#include "property_data.hpp"
#include "property_handler.hpp"
#include "query.hpp"
#include "random.hpp"
#include "terrain.hpp"
#include "terrain_data.hpp"
#include "terrain_door.hpp"
#include "text_format.hpp"
#include "wpn_dmg.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static bool is_allowed_use_wpn_on_terrain(
        const item::Item& wpn,
        const terrain::Terrain& terrain)
{
        bool allow_wpn_att_terrain = wpn.data().melee.attack_terrain;

        if (!allow_wpn_att_terrain)
        {
                return false;
        }

        const auto wpn_dmg_type = wpn.data().melee.dmg_type;

        switch (terrain.id())
        {
        case terrain::Id::door:
        {
                if (terrain.is_hidden())
                {
                        // Emulate walls
                        allow_wpn_att_terrain = true;
                }
                else
                {
                        // Revealed door
                        const auto& door =
                                static_cast<const terrain::Door&>(terrain);

                        const auto door_type = door.type();

                        if (door_type == terrain::DoorType::gate)
                        {
                                // Only allow blunt weapons for gates
                                // (feels weird to attack a barred gate
                                // with an axe...)
                                allow_wpn_att_terrain =
                                        (wpn_dmg_type == DmgType::blunt);
                        }
                        else
                        {
                                // Not gate (i.e. wooden, metal)
                                allow_wpn_att_terrain = true;
                        }
                }
        }
        break;

        case terrain::Id::wall:
        {
                allow_wpn_att_terrain = true;
        }
        break;

        default:
        {
                allow_wpn_att_terrain = false;
        }
        break;
        }  // Terrain id switch

        return allow_wpn_att_terrain;
}

static std::unique_ptr<const item::Item> make_tmp_kick_wpn()
{
        auto kick_wpn =
                std::unique_ptr<item::Item>(
                        item::make(item::Id::player_kick));

        return kick_wpn;
}

static const item::Item* get_wielded_wpn_or_unarmed()
{
        const auto* wpn = map::g_player->m_inv.item_in_slot(SlotId::wpn);

        if (!wpn)
        {
                wpn = &map::g_player->unarmed_wpn();
        }

        ASSERT(wpn);

        return wpn;
}

static void try_kick_living_monster(actor::Actor& mon)
{
        const bool melee_allowed =
                map::g_player->m_properties.allow_attack_melee(
                        Verbose::yes);

        if (!melee_allowed)
        {
                return;
        }

        map::g_player->kick_mon(mon);

        bash::try_sprain_player();

        // Attacking ends cloaking
        map::g_player->m_properties.end_prop(PropId::cloaked);
}

static void bash_corpse_with_wpn(
        actor::Actor& mon,
        const P& att_pos,
        const item::Item& wpn,
        const item::Item& kick_wpn)
{
        const bool is_seeing_cell = map::g_seen.at(att_pos);

        std::string corpse_name =
                is_seeing_cell
                ? mon.m_data->corpse_name_the
                : "a corpse";

        corpse_name = text_format::first_to_lower(corpse_name);

        // Decide if we should kick or use wielded weapon
        const auto& wpn_used_att_corpse =
                wpn.data().melee.attack_corpse
                ? wpn
                : kick_wpn;

        const auto melee_att_msg =
                wpn_used_att_corpse.data().melee.attack_msgs.player;

        const std::string msg =
                "I " +
                melee_att_msg + " " +
                corpse_name + ".";

        msg_log::add(msg);

        const auto dmg_range = wpn_used_att_corpse.melee_dmg(map::g_player);

        const int dmg = dmg_range.total_range().roll();

        actor::hit(mon, dmg, wpn_used_att_corpse.data().melee.dmg_type);

        if (&wpn_used_att_corpse == &kick_wpn)
        {
                bash::try_sprain_player();
        }

        if (mon.m_state == ActorState::destroyed)
        {
                std::vector<actor::Actor*> corpses_here;

                for (auto* const actor : game_time::g_actors)
                {
                        if ((actor->m_pos == att_pos) &&
                            actor->is_corpse())
                        {
                                corpses_here.push_back(actor);
                        }
                }

                if (!corpses_here.empty())
                {
                        msg_log::more_prompt();
                }

                for (auto* const other_corpse : corpses_here)
                {
                        const std::string name =
                                text_format::first_to_upper(
                                        other_corpse
                                                ->m_data
                                                ->corpse_name_a);

                        msg_log::add(name + ".");
                }
        }

        // Attacking ends cloaking
        map::g_player->m_properties.end_prop(PropId::cloaked);

        game_time::tick();
}

static bool allow_bash_terrain(const terrain::Terrain* const terrain)
{
        return (
                !terrain->is_walkable() &&
                (terrain->id() != terrain::Id::stairs));
}

static void bash_terrain_with_wpn(
        const P& att_pos,
        const item::Item& wpn,
        const item::Item& kick_wpn)
{
        auto* const terrain = map::g_terrain.at(att_pos);

        const auto& wpn_used_att_terrain =
                is_allowed_use_wpn_on_terrain(wpn, *terrain)
                ? &wpn
                : &kick_wpn;

        const auto dmg_range = wpn_used_att_terrain->melee_dmg(map::g_player);
        const int dmg = dmg_range.total_range().roll();
        const auto dmg_type = wpn_used_att_terrain->data().melee.dmg_type;

        switch (dmg_type)
        {
        case DmgType::kicking:
        {
                const std::string terrain_name =
                        map::g_seen.at(att_pos)
                        ? terrain->name(Article::the)
                        : "something";

                msg_log::add(
                        "I kick " +
                        terrain_name +
                        "!");

                bash::try_sprain_player();
        }
        break;

        case DmgType::blunt:
        case DmgType::slashing:
        {
                msg_log::add("*WHAM!*");
        }
        break;

        default:
        {
        }
        break;
        }

        terrain->hit(
                wpn_used_att_terrain->data().melee.dmg_type,
                map::g_player,
                map::g_player->m_pos,
                dmg);

        // Attacking ends cloaking
        map::g_player->m_properties.end_prop(PropId::cloaked);

        game_time::tick();
}

static void bash_pos(const P& pos)
{
        const auto kick_wpn = make_tmp_kick_wpn();
        const auto* wpn = get_wielded_wpn_or_unarmed();

        // Bash terrain?
        auto* const terrain = map::g_terrain.at(pos);

        if ((pos != map::g_player->m_pos) &&
            allow_bash_terrain(terrain))
        {
                bash_terrain_with_wpn(pos, *wpn, *kick_wpn);

                return;
        }

        // Kick living actor?
        if (pos != map::g_player->m_pos)
        {
                auto* living_actor =
                        map::first_actor_at_pos(
                                pos,
                                ActorState::alive);

                if (living_actor)
                {
                        try_kick_living_monster(*living_actor);

                        return;
                }
        }

        // Destroy corpse?
        actor::Actor* corpse = nullptr;

        // Check all corpses here, stop at any corpse which is prioritized for
        // bashing (Zombies)
        for (auto* const actor : game_time::g_actors)
        {
                if ((actor->m_pos == pos) &&
                    (actor->m_state == ActorState::corpse))
                {
                        corpse = actor;

                        if (actor->m_data->prio_corpse_bash)
                        {
                                break;
                        }
                }
        }

        if (corpse)
        {
                bash_corpse_with_wpn(*corpse, pos, *wpn, *kick_wpn);

                return;
        }

        // Nothing to kick here, attack the air.
        msg_log::add("*Whoosh!*");

        audio::play(audio::SfxId::miss_medium);
}

// -----------------------------------------------------------------------------
// bash
// -----------------------------------------------------------------------------
namespace bash
{
void try_sprain_player()
{
        const bool is_frenzied =
                map::g_player->m_properties.has(PropId::frenzied);

        const bool is_player_ghoul = player_bon::bg() == Bg::ghoul;

        if (is_player_ghoul || is_frenzied)
        {
                return;
        }

        int sprain_one_in_n = 0;

        if (player_bon::has_trait(Trait::rugged))
        {
                sprain_one_in_n = 12;
        }
        else if (player_bon::has_trait(Trait::tough))
        {
                sprain_one_in_n = 8;
        }
        else
        {
                sprain_one_in_n = 4;
        }

        if (rnd::one_in(sprain_one_in_n))
        {
                msg_log::add("I sprain myself.", colors::msg_bad());

                const int dmg = rnd::range(1, 2);

                actor::hit(*map::g_player, dmg, DmgType::pure);
        }
}

void run()
{
        msg_log::clear();

        const std::string query_msg =
                common_text::g_direction_query +
                " " +
                common_text::g_cancel_hint;

        // Choose direction
        msg_log::add(
                query_msg,
                colors::light_white(),
                MsgInterruptPlayer::no,
                MorePromptOnMsg::no,
                CopyToMsgHistory::no);

        const auto input_dir = query::dir(AllowCenter::yes);

        msg_log::clear();

        if (input_dir == Dir::END)
        {
                // Invalid direction
                io::update_screen();

                return;
        }

        // The chosen direction is valid

        P pos(map::g_player->m_pos + dir_utils::offset(input_dir));

        bash_pos(pos);
}

void bash_terrain_at_pos(const P& pos)
{
        if (pos == map::g_player->m_pos)
        {
                return;
        }

        const auto kick_wpn = make_tmp_kick_wpn();
        const auto* wpn = get_wielded_wpn_or_unarmed();

        bash_terrain_with_wpn(pos, *wpn, *kick_wpn);
}

}  // namespace bash
