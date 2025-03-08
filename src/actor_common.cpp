// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "actor.hpp"

#include <algorithm>
#include <climits>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ability_values.hpp"
#include "actor_data.hpp"
#include "actor_items.hpp"
#include "actor_player_state.hpp"
#include "actor_see.hpp"
#include "array2.hpp"
#include "colors.hpp"
#include "debug.hpp"
#include "direction.hpp"
#include "fov.hpp"
#include "game_time.hpp"
#include "gfx.hpp"
#include "global.hpp"
#include "inventory.hpp"
#include "item.hpp"
#include "item_data.hpp"
#include "item_explosive.hpp"
#include "item_misc.hpp"
#include "map.hpp"
#include "map_parsing.hpp"
#include "msg_log.hpp"
#include "player_bon.hpp"
#include "player_spells.hpp"
#include "pos.hpp"
#include "property.hpp"
#include "property_data.hpp"
#include "property_handler.hpp"
#include "random.hpp"
#include "rect.hpp"
#include "spells.hpp"
#include "text_format.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static Color color_player()
{
        if (!actor::is_alive(*map::g_player)) {
                return colors::red();
        }

        if (actor::player_state::g_active_explosive) {
                return colors::yellow();
        }

        const std::optional<Color> color_override =
                map::g_player->m_properties.override_actor_color();

        if (color_override) {
                return color_override.value();
        }

        const auto* const lantern_item =
                map::g_player->m_inv.item_in_backpack(item::Id::lantern);

        if (lantern_item) {
                const auto* const lantern = static_cast<const item::Lantern*>(lantern_item);

                if (lantern->is_activated()) {
                        return actor::player_state::g_lantern_color;
                }
        }

        if (map::g_player->shock_tot() >= 75) {
                return colors::magenta();
        }

        if (map::g_player->m_properties.has(prop::Id::invis) ||
            map::g_player->m_properties.has(prop::Id::cloaked)) {
                return colors::gray();
        }

        if (map::g_dark.at(map::g_player->m_pos)) {
                Color tmp_color = map::g_player->m_data->color;

                tmp_color = tmp_color.shaded(40);

                tmp_color.set_rgb(
                        tmp_color.r(),
                        tmp_color.g(),
                        std::min(255, tmp_color.b() + 20));

                return tmp_color;
        }

        return map::g_player->m_data->color;
}

static Color color_monster(const actor::Actor& actor)
{
        if (!actor::is_alive(actor)) {
                return actor.m_data->color;
        }

        // Use the wall color if this is either:
        // * A Lurking Ooze, and the player isn't hallucinating it as something else, or
        // * The player is hallucinating another monster as a Lurking Ooze.
        // HACK: Handle via actor data instead:
        const std::string lurking_ooze_id = "MON_OOZE_LURKING";
        if (
                (!actor.m_mimic_data && (actor::id(actor) == lurking_ooze_id)) ||
                (actor.m_mimic_data && (actor.m_mimic_data->id == lurking_ooze_id))) {
                return map::g_wall_color;
        }

        const std::optional<Color> color_override = actor.m_properties.override_actor_color();

        if (color_override) {
                return color_override.value();
        }

        const actor::ActorData* const data =
                actor.m_mimic_data
                ? actor.m_mimic_data
                : actor.m_data;

        return data->color;
}

static LightSize calc_light_size_player_specific()
{
        auto light_size = LightSize::none;

        const item::Explosive* active_explosive = actor::player_state::g_active_explosive.get();

        if (active_explosive) {
                const item::Id id = active_explosive->data().id;

                if (id == item::Id::flare) {
                        light_size = LightSize::fov;
                }
        }

        for (auto* const item : map::g_player->m_inv.m_backpack) {
                const auto item_light_size = item->light_size();

                if ((int)light_size < (int)item_light_size) {
                        light_size = std::max(light_size, item_light_size);
                }
        }

        return light_size;
}

static LightSize calc_light_size(const actor::Actor& actor)
{
        bool do_radiant_self = false;
        bool do_radiant_adj = false;
        bool do_radiant_fov = false;

        if (actor::is_alive(actor)) {
                do_radiant_self = actor.m_properties.has(prop::Id::radiant_self);
                do_radiant_adj = actor.m_properties.has(prop::Id::radiant_adjacent);
                do_radiant_fov = actor.m_properties.has(prop::Id::radiant_fov);
        }

        const bool is_burning = actor.m_properties.has(prop::Id::burning);

        auto light_size = LightSize::none;

        if (do_radiant_fov) {
                light_size = LightSize::fov;
        }
        else if (do_radiant_adj) {
                light_size = LightSize::small;
        }
        else if (is_burning || do_radiant_self) {
                light_size = LightSize::single;
        }
        else {
                light_size = LightSize::none;
        }

        if (actor::is_player(&actor)) {
                light_size = std::max(light_size, calc_light_size_player_specific());
        }

        return light_size;
}

static void apply_light(
        const LightSize light_size,
        const P& origin,
        Array2<bool>& light_map)
{
        switch (light_size) {
        case LightSize::none: {
        } break;

        case LightSize::single: {
                light_map.at(origin) = true;
        } break;

        case LightSize::small: {
                for (const auto d : dir_utils::g_dir_list_w_center) {
                        light_map.at(origin + d) = true;
                }
        } break;

        case LightSize::fov: {
                const R fov_lmt = fov::fov_rect(origin, map::dims());

                Array2<bool> blocked(map::dims());

                map_parsers::BlocksLos().run(blocked, fov_lmt, MapParseMode::overwrite);

                FovMap fov_map;
                fov_map.hard_blocked = &blocked;
                fov_map.light = &map::g_light;
                fov_map.dark = &map::g_dark;

                const auto actor_fov = fov::run(origin, fov_map);

                for (int x = fov_lmt.p0.x; x <= fov_lmt.p1.x; ++x) {
                        for (int y = fov_lmt.p0.y; y <= fov_lmt.p1.y; ++y) {
                                if (!actor_fov.at(x, y).is_blocked_hard) {
                                        light_map.at(x, y) = true;
                                }
                        }
                }
        } break;
        }
}

// -----------------------------------------------------------------------------
// actor
// -----------------------------------------------------------------------------
namespace actor
{
void init_actor(Actor& actor, const P& pos, ActorData& data)
{
        actor.m_pos = pos;
        actor.m_data = &data;

        if (is_player(&actor)) {
                player_state::init();
                actor.m_base_max_hp = data.hp;
        }
        else {
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

        if (!actor::is_player(&actor)) {
                actor_items::make_for_actor(actor);
        }
}

std::string id(const actor::Actor& actor)
{
        return actor.m_data->id;
}

Color color(const Actor& actor)
{
        if (is_player(&actor)) {
                return color_player();
        }
        else {
                return color_monster(actor);
        }
}

gfx::TileId tile(const Actor& actor)
{
        if (actor::is_corpse(actor)) {
                return gfx::TileId::corpse2;
        }

        if (actor.m_mimic_data) {
                return actor.m_mimic_data->tile;
        }

        const auto tile_override = actor.m_properties.override_actor_tile();

        if (tile_override) {
                return tile_override.value();
        }

        // HACK: Overriding tile for (firearm) Cultists
        if (actor::id(actor) == "MON_CULTIST") {
                const item::Item* const wpn = actor.m_inv.item_in_slot(SlotId::wpn);

                ASSERT(wpn);

                if (!wpn) {
                        return gfx::TileId::cultist_pistol;
                }

                switch (wpn->id()) {
                case item::Id::pistol:
                case item::Id::revolver:
                        return gfx::TileId::cultist_pistol;

                case item::Id::pump_shotgun:
                        return gfx::TileId::cultist_pump_shotgun;

                case item::Id::sawed_off:
                        return gfx::TileId::cultist_sawed_off_shotgun;

                case item::Id::tommy_gun:
                        return gfx::TileId::cultist_tommygun;

                case item::Id::rifle:
                        return gfx::TileId::cultist_rifle;

                default:
                        ASSERT(false);
                        return gfx::TileId::cultist_pistol;
                }
        }

        return actor.m_data->tile;
}

char character(const Actor& actor)
{
        if (actor::is_corpse(actor)) {
                return '&';
        }

        if (actor.m_mimic_data) {
                return actor.m_mimic_data->character;
        }

        const auto c_override = actor.m_properties.override_actor_character();

        if (c_override) {
                return c_override.value();
        }

        return actor.m_data->character;
}

std::string name_the(const Actor& actor)
{
        if (actor.m_mimic_data) {
                return actor.m_mimic_data->name_the;
        }

        const auto name_override = actor.m_properties.override_actor_name_the();

        if (name_override) {
                return name_override.value();
        }

        return actor.m_data->name_the;
}

std::string name_a(const Actor& actor)
{
        if (actor.m_mimic_data) {
                return actor.m_mimic_data->name_a;
        }

        const auto name_override = actor.m_properties.override_actor_name_a();

        if (name_override) {
                return name_override.value();
        }

        return actor.m_data->name_a;
}

std::string descr(const Actor& actor)
{
        if (actor.m_mimic_data) {
                return actor.m_mimic_data->descr;
        }

        const auto descr_override = actor.m_properties.override_actor_descr();

        if (descr_override) {
                return descr_override.value();
        }

        return actor.m_data->descr;
}

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
        return actor && (actor->m_data->id == "MON_PLAYER");
}

void add_light(const Actor& actor, Array2<bool>& light_map)
{
        const LightSize light_size = calc_light_size(actor);

        apply_light(light_size, actor.m_pos, light_map);
}

SpellSkill spell_skill(const actor::Actor& actor, SpellId id)
{
        if (is_player(&actor)) {
                return player_spells::spell_skill(id);
        }
        else {
                for (const auto& spell : actor.m_mon_spells) {
                        if (spell.spell->id() == id) {
                                return spell.skill;
                        }
                }

                ASSERT(false);

                return SpellSkill::basic;
        }
}

int ability(
        const Actor& actor,
        const AbilityId id,
        const AbilityAffectedByProperties affected_by_props)
{
        return actor.m_data->ability_values.val(id, affected_by_props, actor);
}

bool restore_hp(
        Actor& actor,
        const int hp_restored,
        const AllowRestoreAboveMax allow_above_max,
        const Verbose verbose)
{
        bool is_hp_gained = (allow_above_max == AllowRestoreAboveMax::yes);

        const int dif_from_max = max_hp(actor) - hp_restored;

        // If HP is below limit, but restored HP will push it over the limit, HP
        // is set to max.
        if (!(allow_above_max == AllowRestoreAboveMax::yes) &&
            (actor.m_hp > dif_from_max) &&
            (actor.m_hp < actor::max_hp(actor))) {
                actor.m_hp = actor::max_hp(actor);

                is_hp_gained = true;
        }

        // If HP is below limit, and restored HP will NOT push it over the
        // limit, restored HP is added to current.
        if ((allow_above_max == AllowRestoreAboveMax::yes) ||
            (actor.m_hp <= dif_from_max)) {
                actor.m_hp += hp_restored;

                is_hp_gained = true;
        }

        if ((verbose == Verbose::yes) && is_hp_gained) {
                if (is_player(&actor)) {
                        msg_log::add("I feel healthier!", colors::msg_good());
                }
                else if (can_player_see_actor(actor)) {
                        const std::string actor_name_the =
                                text_format::first_to_upper(
                                        actor.m_data->name_the);

                        msg_log::add(actor_name_the + " looks healthier.");
                }
        }

        return is_hp_gained;
}

bool restore_sp(
        Actor& actor,
        const int sp_restored,
        const AllowRestoreAboveMax allow_above_max,
        const Verbose verbose)
{
        // Maximum allowed level to increase spirit to
        // * If we allow above max, we can raise spirit "infinitely"
        // * Otherwise we cap to max sp, or current sp, whichever is higher
        const int limit =
                (allow_above_max == AllowRestoreAboveMax::yes)
                ? INT_MAX
                : std::max(actor.m_sp, actor::max_sp(actor));

        const int sp_before = actor.m_sp;

        actor.m_sp = std::min(actor.m_sp + sp_restored, limit);

        const bool is_sp_gained = actor.m_sp > sp_before;

        if ((verbose == Verbose::yes) && is_sp_gained) {
                if (is_player(&actor)) {
                        msg_log::add("I feel more spirited!", colors::msg_good());
                }
                else {
                        if (can_player_see_actor(actor)) {
                                const std::string actor_name_the =
                                        text_format::first_to_upper(
                                                actor.m_data->name_the);

                                msg_log::add(actor_name_the + " looks more spirited.");
                        }
                }
        }

        return is_sp_gained;
}

void change_max_hp(
        Actor& actor,
        const int change,
        const Verbose verbose)
{
        actor.m_base_max_hp = std::max(1, actor.m_base_max_hp + change);

        if (verbose == Verbose::no) {
                return;
        }

        if (is_player(&actor)) {
                if (change > 0) {
                        msg_log::add("I feel more vigorous!", colors::msg_good());
                }
                else if (change < 0) {
                        msg_log::add("I feel frailer!", colors::msg_bad());
                }
        }
        else if (can_player_see_actor(actor)) {
                const std::string actor_name_the =
                        text_format::first_to_upper(
                                name_the(actor));

                if (change > 0) {
                        msg_log::add(actor_name_the + " looks more vigorous.");
                }
                else if (change < 0) {
                        msg_log::add(actor_name_the + " looks frailer.");
                }
        }
}

void change_max_sp(
        Actor& actor,
        const int change,
        const Verbose verbose)
{
        actor.m_base_max_sp = std::max(1, actor.m_base_max_sp + change);

        if (verbose == Verbose::no) {
                return;
        }

        if (is_player(&actor)) {
                if (change > 0) {
                        msg_log::add("My spirit is stronger!", colors::msg_good());
                }
                else if (change < 0) {
                        msg_log::add("My spirit is weaker!", colors::msg_bad());
                }
        }
        else if (can_player_see_actor(actor)) {
                const std::string actor_name_the =
                        text_format::first_to_upper(
                                name_the(actor));

                if (change > 0) {
                        msg_log::add(actor_name_the + " appears to grow in spirit.");
                }
                else if (change < 0) {
                        msg_log::add(actor_name_the + " appears to shrink in spirit.");
                }
        }
}

bool is_alive(const actor::Actor& actor)
{
        return actor.m_state == ActorState::alive;
}

bool is_corpse(const actor::Actor& actor)
{
        return actor.m_state == ActorState::corpse;
}

std::string death_msg(const actor::Actor& actor)
{
        // Do not print a standard split message if this monster will split on
        // death (it will print a split message instead)
        if (actor.m_properties.has(prop::Id::splits_on_death)) {
                const auto* const splits =
                        static_cast<const prop::SplitsOnDeath*>(
                                actor.m_properties.prop(prop::Id::splits_on_death));

                if (splits->prevent_std_death_msg()) {
                        return "";
                }
        }

        const std::string actor_name_the =
                text_format::first_to_upper(
                        actor::name_the(actor));

        std::string msg_end;

        if (actor.m_data->death_msg_override.empty()) {
                msg_end = "dies.";
        }
        else {
                msg_end = actor.m_data->death_msg_override;
        }

        return actor_name_the + " " + msg_end;
}

int armor_points(const actor::Actor& actor)
{
        int armor_points = 0;

        if (actor.m_data->is_humanoid) {
                // Add armor from items.
                const std::vector<SlotId> slot_ids {
                        SlotId::body,
                        SlotId::head};

                for (SlotId slot_id : slot_ids) {
                        item::Item* item = actor.m_inv.item_in_slot(slot_id);

                        if (item) {
                                armor_points += item->armor_points();
                        }
                }
        }

        // Add armor from properties.
        armor_points += actor.m_properties.armor_points();

        if (is_player(&actor)) {
                // Add armor from player traits.
                if (player_bon::has_trait(Trait::thick_skinned)) {
                        ++armor_points;
                }

                if (player_bon::has_trait(Trait::callous)) {
                        ++armor_points;
                }
        }

        return armor_points;
}

bool is_in_same_group(const Actor* actor_1, const Actor* actor_2)
{
        if (!actor_1 || !actor_2) {
                return false;
        }

        if (actor_1 == actor_2) {
                return true;
        }

        // Consider the actors to be in the same group if one of the actors is
        // the leader of the other, or they have the same leader.
        return (
                actor_1->is_leader_of(actor_2) ||
                actor_2->is_leader_of(actor_1) ||
                actor_1->is_actor_my_leader(actor_2->m_leader));
}

std::vector<Actor*> other_actors_in_same_group(const actor::Actor* const actor)
{
        std::vector<Actor*> result;

        if (!actor) {
                return result;
        }

        std::copy_if(
                std::begin(game_time::g_actors),
                std::end(game_time::g_actors),
                std::back_inserter(result),
                [actor](const actor::Actor* const other_actor) {
                        return (
                                (is_in_same_group(actor, other_actor) &&
                                 (actor != other_actor)));
                });

        return result;
}

int nr_other_actors_in_same_group(const actor::Actor* const actor)
{
        const int nr_in_group =
                (int)std::count_if(
                        std::begin(game_time::g_actors),
                        std::end(game_time::g_actors),
                        [actor](const actor::Actor* const other_actor) {
                                return (
                                        (is_in_same_group(actor, other_actor) &&
                                         (actor != other_actor)));
                        });

        return nr_in_group;
}

}  // namespace actor
