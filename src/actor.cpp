// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "actor.hpp"

#include <algorithm>
#include <climits>

#include "ability_values.hpp"
#include "actor_data.hpp"
#include "actor_items.hpp"
#include "actor_mon.hpp"
#include "actor_player.hpp"
#include "actor_see.hpp"
#include "array2.hpp"
#include "audio_data.hpp"
#include "fov.hpp"
#include "game.hpp"
#include "game_time.hpp"
#include "global.hpp"
#include "inventory.hpp"
#include "item.hpp"
#include "map.hpp"
#include "map_parsing.hpp"
#include "msg_log.hpp"
#include "player_bon.hpp"
#include "property.hpp"
#include "property_data.hpp"
#include "property_handler.hpp"
#include "random.hpp"
#include "rect.hpp"
#include "sound.hpp"
#include "text_format.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// actor
// -----------------------------------------------------------------------------
namespace actor
{
int max_hp(const Actor& actor)
{
        int result = actor.m_base_max_hp;

        result = actor.m_properties.affect_max_hp(result);

        return std::max(1, result);
}

int max_sp(const Actor& actor)
{
        int result = actor.m_base_max_sp;

        result = actor.m_properties.affect_max_spi(result);

        return std::max(1, result);
}

bool is_player(const Actor* const actor)
{
        if (!actor)
        {
                return false;
        }

        return (actor->m_data->id == Id::player);
}

void init_actor(Actor& actor, const P& pos_, ActorData& data)
{
        actor.m_pos = pos_;
        actor.m_data = &data;

        if (is_player(&actor))
        {
                actor.m_base_max_hp = data.hp;
        }
        else
        {
                const int hp_max_variation_pct = 25;
                const int hp_variation = (data.hp * hp_max_variation_pct) / 100;
                Range hp_range(data.hp - hp_variation, data.hp + hp_variation);
                hp_range.min = std::max(1, hp_range.min);
                actor.m_base_max_hp = hp_range.roll();
        }

        actor.m_hp = actor.m_base_max_hp;
        actor.m_sp = actor.m_base_max_sp = data.spi;
        actor.m_ai_state.spawn_pos = actor.m_pos;

        actor.m_properties.apply_natural_props_from_actor_data();

        if (!actor::is_player(&actor))
        {
                actor_items::make_for_actor(actor);
        }
}

void print_aware_invis_mon_msg(const Mon& mon)
{
        std::string mon_ref;

        if (mon.m_data->is_ghost)
        {
                mon_ref = "some foul entity";
        }
        else if (mon.m_data->is_humanoid)
        {
                mon_ref = "someone";
        }
        else
        {
                mon_ref = "a creature";
        }

        msg_log::add(
                "There is " + mon_ref + " here!",
                colors::msg_note(),
                MsgInterruptPlayer::no,
                MorePromptOnMsg::yes);
}

void make_player_aware_mon(Actor& actor)
{
        if (is_player(&actor))
        {
                ASSERT(false);

                return;
        }

        // NOTE: This will also "discover" the monster (give XP), if seen.
        static_cast<actor::Mon&>(actor).make_player_aware_of_me();
}

void make_player_aware_seen_monsters()
{
        const auto player_seen_actors = actor::seen_actors(*map::g_player);

        for (auto* actor : player_seen_actors)
        {
                make_player_aware_mon(*actor);
        }
}

// -----------------------------------------------------------------------------
// Actor
// -----------------------------------------------------------------------------
Actor::~Actor()
{
        // Free all items owning actors
        for (auto* item : m_inv.m_backpack)
        {
                item->clear_actor_carrying();
        }

        for (auto& slot : m_inv.m_slots)
        {
                if (slot.item)
                {
                        slot.item->clear_actor_carrying();
                }
        }
}

int Actor::ability(const AbilityId id, const bool is_affected_by_props) const
{
        return m_data->ability_values.val(id, is_affected_by_props, *this);
}

gfx::TileId Actor::tile() const
{
        if (is_corpse())
        {
                return gfx::TileId::corpse2;
        }

        if (m_mimic_data)
        {
                return m_mimic_data->tile;
        }

        const auto tile_override = m_properties.override_actor_tile();

        if (tile_override)
        {
                return tile_override.value();
        }

        // HACK: Overriding tile for (firearm) Cultists
        if (id() == Id::cultist)
        {
                const auto* const wpn = m_inv.item_in_slot(SlotId::wpn);

                ASSERT(wpn);

                if (!wpn)
                {
                        return gfx::TileId::cultist_pistol;
                }

                switch (wpn->id())
                {
                case item::Id::pistol:
                case item::Id::revolver:
                        return gfx::TileId::cultist_pistol;

                case item::Id::pump_shotgun:
                        return gfx::TileId::cultist_pump_shotgun;

                case item::Id::sawed_off:
                        return gfx::TileId::cultist_sawed_off_shotgun;

                case item::Id::machine_gun:
                        return gfx::TileId::cultist_tommygun;

                case item::Id::rifle:
                        return gfx::TileId::cultist_rifle;

                default:
                        ASSERT(false);
                        return gfx::TileId::cultist_pistol;
                }
        }

        return m_data->tile;
}

char Actor::character() const
{
        if (is_corpse())
        {
                return '&';
        }

        if (m_mimic_data)
        {
                return m_mimic_data->character;
        }

        const auto c_override = m_properties.override_actor_character();

        if (c_override)
        {
                return c_override.value();
        }

        return m_data->character;
}

std::string Actor::name_the() const
{
        if (m_mimic_data)
        {
                return m_mimic_data->name_the;
        }

        const auto name_override = m_properties.override_actor_name_the();

        if (name_override)
        {
                return name_override.value();
        }

        return m_data->name_the;
}

std::string Actor::name_a() const
{
        if (m_mimic_data)
        {
                return m_mimic_data->name_a;
        }

        const auto name_override = m_properties.override_actor_name_a();

        if (name_override)
        {
                return name_override.value();
        }

        return m_data->name_a;
}

std::string Actor::descr() const
{
        if (m_mimic_data)
        {
                return m_mimic_data->descr;
        }

        const auto descr_override = m_properties.override_actor_descr();

        if (descr_override)
        {
                return descr_override.value();
        }

        return m_data->descr;
}

bool Actor::is_leader_of(const Actor* const actor) const
{
        if (!actor)
        {
                return false;
        }

        return actor->m_leader == this;
}

bool Actor::is_actor_my_leader(const Actor* const actor) const
{
        return m_leader == actor;
}

bool Actor::is_in_same_group_as(const Actor* actor) const
{
        if (!actor)
        {
                return false;
        }

        // Consider the actors to be in the same group if one of the actors is
        // the leader of the other, or they have the same leader.
        return (
                is_leader_of(actor) ||
                is_actor_my_leader(actor) ||
                actor->is_actor_my_leader(m_leader));
}

bool Actor::restore_hp(
        const int hp_restored,
        const bool is_allowed_above_max,
        const Verbose verbose)
{
        bool is_hp_gained = is_allowed_above_max;

        const int dif_from_max = actor::max_hp(*this) - hp_restored;

        // If HP is below limit, but restored HP will push it over the limit, HP
        // is set to max.
        if (!is_allowed_above_max &&
            (m_hp > dif_from_max) &&
            (m_hp < actor::max_hp(*this)))
        {
                m_hp = actor::max_hp(*this);

                is_hp_gained = true;
        }

        // If HP is below limit, and restored HP will NOT push it over the
        // limit, restored HP is added to current.
        if (is_allowed_above_max ||
            (m_hp <= dif_from_max))
        {
                m_hp += hp_restored;

                is_hp_gained = true;
        }

        if ((verbose == Verbose::yes) && is_hp_gained)
        {
                if (is_player(this))
                {
                        msg_log::add("I feel healthier!", colors::msg_good());
                }
                else if (can_player_see_actor(*this))
                {
                        const std::string actor_name_the =
                                text_format::first_to_upper(
                                        m_data->name_the);

                        msg_log::add(actor_name_the + " looks healthier.");
                }
        }

        return is_hp_gained;
}

bool Actor::restore_sp(
        const int spi_restored,
        const bool is_allowed_above_max,
        const Verbose verbose)
{
        // Maximum allowed level to increase spirit to
        // * If we allow above max, we can raise spirit "infinitely"
        // * Otherwise we cap to max sp, or current sp, whichever is higher
        const int limit =
                is_allowed_above_max
                ? INT_MAX
                : std::max(m_sp, actor::max_sp(*this));

        const int sp_before = m_sp;

        m_sp = std::min(m_sp + spi_restored, limit);

        const bool is_spi_gained = m_sp > sp_before;

        if (verbose == Verbose::yes &&
            is_spi_gained)
        {
                if (is_player(this))
                {
                        msg_log::add(
                                "I feel more spirited!",
                                colors::msg_good());
                }
                else
                {
                        if (can_player_see_actor(*this))
                        {
                                const std::string actor_name_the =
                                        text_format::first_to_upper(
                                                m_data->name_the);

                                msg_log::add(
                                        actor_name_the +
                                        " looks more spirited.");
                        }
                }
        }

        return is_spi_gained;
}

void Actor::change_max_hp(const int change, const Verbose verbose)
{
        m_base_max_hp = std::max(1, m_base_max_hp + change);

        if (verbose == Verbose::no)
        {
                return;
        }

        if (is_player(this))
        {
                if (change > 0)
                {
                        msg_log::add(
                                "I feel more vigorous!",
                                colors::msg_good());
                }
                else if (change < 0)
                {
                        msg_log::add(
                                "I feel frailer!",
                                colors::msg_bad());
                }
        }
        else if (can_player_see_actor(*this))
        {
                const std::string actor_name_the =
                        text_format::first_to_upper(
                                name_the());

                if (change > 0)
                {
                        msg_log::add(actor_name_the + " looks more vigorous.");
                }
                else if (change < 0)
                {
                        msg_log::add(actor_name_the + " looks frailer.");
                }
        }
}

void Actor::change_max_sp(const int change, const Verbose verbose)
{
        m_base_max_sp = std::max(1, m_base_max_sp + change);

        if (verbose == Verbose::no)
        {
                return;
        }

        if (is_player(this))
        {
                if (change > 0)
                {
                        msg_log::add(
                                "My spirit is stronger!",
                                colors::msg_good());
                }
                else if (change < 0)
                {
                        msg_log::add(
                                "My spirit is weaker!",
                                colors::msg_bad());
                }
        }
        else if (can_player_see_actor(*this))
        {
                const std::string actor_name_the =
                        text_format::first_to_upper(
                                name_the());

                if (change > 0)
                {
                        msg_log::add(
                                actor_name_the +
                                " appears to grow in spirit.");
                }
                else if (change < 0)
                {
                        msg_log::add(
                                actor_name_the +
                                " appears to shrink in spirit.");
                }
        }
}

int Actor::armor_points() const
{
        int ap = 0;

        // Worn armor
        if (m_data->is_humanoid)
        {
                auto* armor =
                        static_cast<item::Armor*>(
                                m_inv.item_in_slot(SlotId::body));

                if (armor)
                {
                        ap += armor->armor_points();
                }
        }

        // "Natural armor"
        if (is_player(this))
        {
                if (player_bon::has_trait(Trait::thick_skinned))
                {
                        ++ap;
                }
        }

        return ap;
}

std::string Actor::death_msg() const
{
        // Do not print a standard split message if this monster will split on
        // death (it will print a split message instead)
        if (m_properties.has(PropId::splits_on_death))
        {
                const auto* const splits =
                        static_cast<const PropSplitsOnDeath*>(
                                m_properties.prop(PropId::splits_on_death));

                if (splits->prevent_std_death_msg())
                {
                        return "";
                }
        }

        const std::string actor_name_the =
                text_format::first_to_upper(
                        name_the());

        std::string msg_end;

        if (m_data->death_msg_override.empty())
        {
                msg_end = "dies.";
        }
        else
        {
                msg_end = m_data->death_msg_override;
        }

        return actor_name_the + " " + msg_end;
}

void Actor::add_light(Array2<bool>& light_map) const
{
        bool do_radiant_self = false;
        bool do_radiant_adj = false;
        bool do_radiant_fov = false;

        if (is_alive())
        {
                do_radiant_self = m_properties.has(PropId::radiant_self);
                do_radiant_adj = m_properties.has(PropId::radiant_adjacent);
                do_radiant_fov = m_properties.has(PropId::radiant_fov);
        }

        const bool is_burning = m_properties.has(PropId::burning);

        auto lgt_size = LgtSize::none;

        if (do_radiant_fov)
        {
                lgt_size = LgtSize::fov;
        }
        else if (do_radiant_adj)
        {
                lgt_size = LgtSize::small;
        }
        else if (is_burning || do_radiant_self)
        {
                lgt_size = LgtSize::single;
        }

        // TODO: This is exactly the same code as in actor_player.cpp
        switch (lgt_size)
        {
        case LgtSize::single:
        {
                light_map.at(m_pos) = true;
        }
        break;

        case LgtSize::small:
        {
                for (const auto d : dir_utils::g_dir_list_w_center)
                {
                        light_map.at(m_pos + d) = true;
                }
        }
        break;

        case LgtSize::fov:
        {
                const R fov_lmt = fov::fov_rect(m_pos, map::dims());

                Array2<bool> blocked(map::dims());

                map_parsers::BlocksLos()
                        .run(
                                blocked,
                                fov_lmt,
                                MapParseMode::overwrite);

                FovMap fov_map;
                fov_map.hard_blocked = &blocked;
                fov_map.light = &map::g_light;
                fov_map.dark = &map::g_dark;

                const auto actor_fov = fov::run(m_pos, fov_map);

                for (int x = fov_lmt.p0.x; x <= fov_lmt.p1.x; ++x)
                {
                        for (int y = fov_lmt.p0.y; y <= fov_lmt.p1.y; ++y)
                        {
                                if (!actor_fov.at(x, y).is_blocked_hard)
                                {
                                        light_map.at(x, y) = true;
                                }
                        }
                }
        }
        break;

        case LgtSize::none:
        {
        }
        break;
        }

        add_light_hook(light_map);
}

}  // namespace actor
