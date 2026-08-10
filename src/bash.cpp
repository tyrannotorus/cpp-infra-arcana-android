// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "bash.hpp"

#include <algorithm>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "actor.hpp"
#include "actor_data.hpp"
#include "actor_hit.hpp"
#include "array2.hpp"
#include "audio.hpp"
#include "audio_data.hpp"
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
#include "item_weapon.hpp"
#include "map.hpp"
#include "msg_log.hpp"
#include "player_bon.hpp"
#include "pos.hpp"
#include "property_data.hpp"
#include "property_handler.hpp"
#include "query.hpp"
#include "random.hpp"
#include "state.hpp"
#include "terrain.hpp"
#include "terrain_data.hpp"
#include "text_format.hpp"
#include "wpn_dmg.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static std::unique_ptr<const item::Item> make_tmp_kick_wpn()
{
        auto kick_wpn =
                std::unique_ptr<item::Item>(
                        item::make(item::Id::player_kick));

        return kick_wpn;
}

static const item::Item* get_wielded_wpn_or_unarmed()
{
        const item::Item* wpn = map::g_player->m_inv.item_in_slot(SlotId::wpn);

        if (!wpn) {
                wpn = &map::g_player->unarmed_wpn();
        }

        ASSERT(wpn);

        return wpn;
}

static void print_player_attack_seen_terrain_msg(
        const terrain::Terrain& terrain,
        const item::Item& wpn)
{
        const std::string terrain_name = terrain.name(Article::the);
        const std::string melee_att_msg = wpn.data().melee.attack_msgs.player;

        msg_log::add("I " + melee_att_msg + " " + terrain_name + "!");
}

static void print_player_attack_unseen_terrain_msg(const item::Item& wpn)
{
        const std::string melee_att_msg = wpn.data().melee.attack_msgs.player;

        msg_log::add("I " + melee_att_msg + " something!");
}

static void print_player_attack_terrain_msg(
        const terrain::Terrain& terrain,
        const P& pos,
        const item::Item& wpn)
{
        if (map::g_seen.at(pos)) {
                print_player_attack_seen_terrain_msg(terrain, wpn);
        }
        else {
                print_player_attack_unseen_terrain_msg(wpn);
        }
}

static void kick_living_monster(actor::Actor& mon)
{
        map::g_player->kick_mon(mon);

        bash::try_sprain_player();

        // Attacking ends cloaking and sanctuary.
        map::g_player->m_properties.end_prop(prop::Id::cloaked);
        map::g_player->m_properties.end_prop(prop::Id::sanctuary);
}

static void attack_corpse_with_wpn_or_kick(
        actor::Actor& mon,
        const item::Item& wpn,
        const item::Item& kick_wpn)
{
        const item::Item& wpn_used_att_corpse =
                wpn.data().melee.can_attack_corpse
                ? wpn
                : kick_wpn;

        bash::attack_corpse(mon, wpn_used_att_corpse);
}

static void bash_something_at_pos(const P& pos)
{
        // Try kicking/bashing something (monster, corpse, terrain) in the given
        // position according to priority.

        terrain::Terrain* const terrain = map::g_terrain.at(pos);
        const bool is_pos_seen = map::g_seen.at(pos);
        const std::unique_ptr<const item::Item> kick_wpn = make_tmp_kick_wpn();
        const item::Item* wpn = get_wielded_wpn_or_unarmed();
        actor::Actor* actor = map::living_actor_at(pos);
        const bool is_player_pos = (pos == map::g_player->m_pos);

        // --- Kick living actor that the player is AWARE of? ---
        if (!is_player_pos && actor && actor::is_player_aware_of_me(*actor)) {
                const bool is_melee_allowed =
                        map::g_player->m_properties.allow_attack_melee(
                                Verbose::yes);

                if (is_melee_allowed) {
                        kick_living_monster(*actor);
                }

                return;
        }

        // --- Bash seen terrain? ---
        if (is_pos_seen && !is_player_pos) {
                // Use weapon on terrain?
                if (bash::is_allowed_use_wpn_on_terrain(*wpn, *terrain)) {
                        bash::attack_terrain(pos, *wpn);

                        return;
                }

                // Kick terrain?
                if (bash::is_allowed_use_wpn_on_terrain(*kick_wpn, *terrain)) {
                        bash::attack_terrain(pos, *kick_wpn);

                        return;
                }
        }

        // --- Bash seen corpse? ---
        if (is_pos_seen) {
                actor::Actor* const corpse = bash::get_corpse_to_bash_at(pos);

                if (corpse) {
                        attack_corpse_with_wpn_or_kick(*corpse, *wpn, *kick_wpn);

                        return;
                }
        }

        // --- Kick living actor that the player is UNAWARE of? ---

        // NOTE: This is only allowed on some terrain, and only if the creature
        // is at least humanoid size - the player cannot melee attack a monster
        // on the floor that they are unaware of (it is assumed that the player
        // kicks into the air, not down at the floor - except possibly when
        // bashing a corpse, but supporting bashing a corpse and accidentally
        // hitting a creature on the floor doesn't really seem necessary).
        if (!is_player_pos &&
            actor &&
            (actor->m_data->actor_size >= actor::Size::humanoid) &&
            bash::is_open_terrain(*terrain)) {
                // Only try an actual attack if there are no status effects
                // preventing melee (terrified).
                const bool melee_allowed =
                        map::g_player->m_properties.allow_attack_melee(
                                Verbose::no);

                if (melee_allowed) {
                        kick_living_monster(*actor);

                        return;
                }
        }

        // --- Kick unseen terrain? ---
        if (!is_player_pos && !is_pos_seen) {
                // NOTE: Kick is always used on unseen terrain when using the
                // bash command.
                if (bash::is_allowed_use_wpn_on_terrain(*kick_wpn, *terrain)) {
                        bash::attack_terrain(pos, *kick_wpn);

                        return;
                }
                else if (!bash::is_open_terrain(*terrain)) {
                        bash::do_fake_attack_on_unseen_terrain(pos, *kick_wpn);

                        return;
                }
        }

        if (is_player_pos) {
                msg_log::add("I cannot find anything there to bash.");

                return;
        }

        if (bash::is_open_terrain(*terrain)) {
                // Attack "the air" (regardless of whether the position is seen
                // or not).
                bash::attack_air();

                return;
        }

        if (is_pos_seen) {
                // This is a SEEN position, and it is not a terrain that the
                // player can attempt to attack unknown monsters on (and it was
                // not possible to attack the terrain itself).
                const std::string terrain_name = terrain->name(Article::the);

                msg_log::add("Kicking " + terrain_name + " would be useless.");

                return;
        }

        {
                // This is an UNSEEN position, and it is not a terrain that the
                // player can attempt to attack unknown monsters on (i.e. it is
                // not "floor" etc).
                bash::do_fake_attack_on_unseen_terrain(pos, *wpn);
        }
}

// -----------------------------------------------------------------------------
// bash
// -----------------------------------------------------------------------------
namespace bash
{
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

        const Dir input_dir = query::dir(AllowCenter::yes);

        msg_log::clear();

        if (input_dir == Dir::END) {
                // Invalid direction
                states::draw();
                io::update_screen();

                return;
        }

        // The chosen direction is valid

        const P pos = map::g_player->m_pos + dir_utils::offset(input_dir);

        bash_something_at_pos(pos);
}

void run_at(const P& pos)
{
        bash_something_at_pos(pos);
}

bool is_kickable_terrain(const terrain::Terrain& terrain)
{
        const std::unique_ptr<const item::Item> kick_wpn = make_tmp_kick_wpn();

        return is_allowed_use_wpn_on_terrain(*kick_wpn, terrain);
}

void try_sprain_player()
{
        const bool is_frenzied = map::g_player->m_properties.has(prop::Id::frenzied);

        const bool is_player_ghoul = player_bon::bg() == Bg::ghoul;

        if (is_player_ghoul || is_frenzied) {
                return;
        }

        int sprain_one_in_n = 0;

        if (player_bon::has_trait(Trait::rugged)) {
                sprain_one_in_n = 12;
        }
        else if (player_bon::has_trait(Trait::tough)) {
                sprain_one_in_n = 8;
        }
        else {
                sprain_one_in_n = 4;
        }

        if (rnd::one_in(sprain_one_in_n)) {
                msg_log::add("I sprain myself.", colors::msg_bad());

                const int dmg = rnd::range(1, 2);

                actor::hit(*map::g_player, dmg, DmgType::pure, nullptr);
        }
}

void attack_terrain(const P& att_pos, const item::Item& wpn)
{
        io::flash_at(att_pos, colors::gray());

        terrain::Terrain* const terrain = map::g_terrain.at(att_pos);

        print_player_attack_terrain_msg(*terrain, att_pos, wpn);

        if (wpn.data().melee.dmg_type == DmgType::kicking) {
                bash::try_sprain_player();
        }

        if (!actor::is_alive(*map::g_player)) {
                return;
        }

        const WpnDmg wpn_dmg = wpn.melee_dmg(map::g_player);

        const int dmg = wpn_dmg.roll_scaled(100);

        terrain->hit(
                wpn.data().melee.dmg_type,
                map::g_player,
                map::g_player->m_pos,
                dmg);

        // Attacking ends cloaking and sanctuary.
        map::g_player->m_properties.end_prop(prop::Id::cloaked);
        map::g_player->m_properties.end_prop(prop::Id::sanctuary);

        game_time::tick();
}

void attack_corpse(actor::Actor& mon, const item::Item& wpn)
{
        io::flash_at(mon.m_pos, colors::light_red());

        std::string corpse_name =
                map::g_seen.at(mon.m_pos)
                ? mon.m_data->corpse_name_the
                : "a corpse";

        corpse_name = text_format::first_to_lower(corpse_name);

        const std::string melee_att_msg = wpn.data().melee.attack_msgs.player;

        const std::string msg =
                "I " +
                melee_att_msg + " " +
                corpse_name + ".";

        msg_log::add(msg);

        const WpnDmg wpn_dmg = wpn.melee_dmg(map::g_player);

        const int dmg = wpn_dmg.roll_scaled(100);

        actor::hit(mon, dmg, wpn.data().melee.dmg_type, map::g_player);

        const DmgType dmg_type = wpn.data().melee.dmg_type;

        if (dmg_type == DmgType::kicking) {
                bash::try_sprain_player();
        }

        if (mon.m_state == ActorState::destroyed) {
                std::vector<actor::Actor*> corpses_here;

                for (actor::Actor* const actor : game_time::g_actors) {
                        if ((actor->m_pos == mon.m_pos) && actor::is_corpse(*actor)) {
                                corpses_here.push_back(actor);
                        }
                }

                if (!corpses_here.empty()) {
                        msg_log::more_prompt();
                }

                for (actor::Actor* const other_corpse : corpses_here) {
                        const std::string name =
                                text_format::first_to_upper(
                                        other_corpse
                                                ->m_data
                                                ->corpse_name_a);

                        msg_log::add(name + ".");
                }
        }

        // Attacking ends cloaking and sanctuary.
        map::g_player->m_properties.end_prop(prop::Id::cloaked);
        map::g_player->m_properties.end_prop(prop::Id::sanctuary);

        game_time::tick();
}

actor::Actor* get_corpse_to_bash_at(const P& pos)
{
        actor::Actor* corpse = nullptr;

        // Check all corpses here, stop at any corpse which is prioritized for
        // bashing (Zombies).
        for (actor::Actor* const actor : game_time::g_actors) {
                if ((actor->m_pos == pos) &&
                    (actor->m_state == ActorState::corpse)) {
                        corpse = actor;

                        if (actor->m_data->prio_corpse_bash) {
                                break;
                        }
                }
        }

        return corpse;
}

void do_fake_attack_on_unseen_terrain(const P& pos, const item::Item& wpn)
{
        io::flash_at(pos, colors::gray());

        print_player_attack_unseen_terrain_msg(wpn);

        // Attacking ends cloaking and sanctuary.
        map::g_player->m_properties.end_prop(prop::Id::cloaked);
        map::g_player->m_properties.end_prop(prop::Id::sanctuary);

        game_time::tick();
}

void attack_air()
{
        msg_log::add("*Whoosh!*");

        audio::play(audio::SfxId::miss_medium);

        // Attacking ends cloaking and sanctuary.
        map::g_player->m_properties.end_prop(prop::Id::cloaked);
        map::g_player->m_properties.end_prop(prop::Id::sanctuary);

        game_time::tick();
}

bool is_allowed_use_wpn_on_terrain(
        const item::Item& wpn,
        const terrain::Terrain& terrain)
{
        const DmgType wpn_dmg_type = wpn.data().melee.dmg_type;

        return terrain.allow_player_melee_attack(wpn_dmg_type, wpn);
}

bool is_open_terrain(
        const terrain::Terrain& terrain)
{
        // Open terrain is terrain that is either:
        // * "Floor like" terrain (floor, grass),
        // * Terrain that is possible to walk through,
        // * ...or terrain that is explicitly listed here as an exception.

        // Open terrain is possible to attempt to attack an unknown creature
        // inside by kicking or striking this position with a melee weapon (on
        // non-open terrain this would instead yield a message such as "kicking
        // the wall would be useless").

        if (terrain.m_data->is_floor_like) {
                return true;
        }

        if (terrain.is_walkable()) {
                return true;
        }

        const std::vector<terrain::Id> exceptions = {
                terrain::Id::chasm,
                terrain::Id::stairs,
                terrain::Id::chest,
        };

        if (std::find(std::begin(exceptions), std::end(exceptions), terrain.id()) !=
            std::end(exceptions)) {
                return true;
        }

        return false;
}

}  // namespace bash
