// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "view_actor_descr.hpp"

#include "SDL_keycode.h"

#include <algorithm>
#include <cstddef>
#include <iterator>

#include "ability_values.hpp"
#include "actor.hpp"
#include "actor_data.hpp"
#include "actor_sneak.hpp"
#include "attack_data.hpp"
#include "colors.hpp"
#include "debug.hpp"
#include "global.hpp"
#include "inventory.hpp"
#include "io.hpp"
#include "item.hpp"
#include "map.hpp"
#include "panel.hpp"
#include "property.hpp"
#include "property_data.hpp"
#include "property_handler.hpp"
#include "text.hpp"
#include "text_format.hpp"

// -----------------------------------------------------------------------------
// private
// -----------------------------------------------------------------------------
// NOTE: This is the order that the properties will show up in the description.

// NOTE: r_acid could possibly also go here, but it's not really relevant
// information for the player...

static const std::string s_cannot_be_harmed_by_start =
        "They cannot be harmed by";

static const std::pair<PropId, std::string> s_cannot_be_harmed_by_props[] = {
        {PropId::r_phys, "{COLOR_GRAY}physical damage{reset_color}"},
        {PropId::r_fire, "{COLOR_LIGHT_RED}fire{reset_color}"},
        {PropId::r_elec, "{COLOR_YELLOW}electricity{reset_color}"},
        {PropId::r_poison, "{COLOR_LIGHT_GREEN}poison{reset_color}"},
        {PropId::r_disease, "{COLOR_GREEN}disease{reset_color}"},
        {PropId::r_spell, "{COLOR_MAGENTA}magic{reset_color}"},
};

static const std::string s_unaffected_by_start =
        "They are unaffected by";

static const std::pair<PropId, std::string> s_unaffected_by_props[] = {
        {PropId::r_fear, "fear"},
        {PropId::r_conf, "confusion"},
};

static const std::string s_cannot_be_start =
        "They cannot be";

static const std::pair<PropId, std::string> s_cannot_be_props[] = {
        {PropId::r_slow, "slowed"},
        {PropId::r_para, "paralyzed"},
};

static const std::string s_cannot_start =
        "They cannot";

static const std::pair<PropId, std::string> s_cannot_props[] = {
        {PropId::r_sleep, "faint"},
};

static const std::string s_can_start =
        "They can";

static const std::pair<PropId, std::string> s_can_props[] = {
        {PropId::darkvision, "see in darkness"},
};

static const std::pair<PropId, std::string> s_custom_props[] = {
        {PropId::reduced_pierce_dmg,
         "Piercing attacks such as pistol shots or dagger strikes are "
         "very ineffective against them"},
        {PropId::radiant_self,
         "They emit light and can be seen in darkness"},
        {PropId::radiant_adjacent,
         "They emit light and can be seen in darkness"},
        {PropId::radiant_fov,
         "They emit light and can be seen in darkness"},
};

struct MonShockStrings
{
        std::string color_fmt_str {};
        std::string shock_str {};
        std::string punct_str {};
};

static std::string get_mon_memory_turns_descr(
        const actor::ActorData& actor_data,
        const actor::Actor& actor)
{
        const int nr_turns_aware = actor_data.nr_turns_aware;

        if (nr_turns_aware <= 0) {
                return "";
        }

        const std::string name_a = text_format::first_to_upper(actor.name_a());

        if (nr_turns_aware < 50) {
                const std::string nr_turns_aware_str =
                        std::to_string(nr_turns_aware);

                return name_a +
                        " will remember hostile creatures for at least "
                        "{COLOR_DARK_YELLOW}" +
                        nr_turns_aware_str +
                        "{_}turns{reset_color}.";
        }
        else {
                // Very high number of turns awareness
                return (
                        name_a +
                        " remembers hostile creatures for a "
                        "{COLOR_DARK_YELLOW}very long time{reset_color}.");
        }
}

static std::string mon_speed_type_to_str(const actor::Speed speed)
{
        switch (speed) {
        case actor::Speed::slow:
                return "slowly";

        case actor::Speed::normal:
                return "";

        case actor::Speed::fast:
                return "fast";

        case actor::Speed::very_fast:
                return "very swiftly";
        }

        ASSERT(false);

        return "";
}

static std::string get_mon_speed_descr(
        const actor::ActorData& actor_data,
        const actor::Actor& actor)
{
        const std::string speed_type_str = mon_speed_type_to_str(actor_data.speed);

        if (speed_type_str.empty()) {
                return "";
        }

        if (actor_data.is_unique) {
                return (
                        actor.name_the() +
                        " appears to move{_}" +
                        speed_type_str +
                        ".");
        }
        else {
                return (
                        "They appear to move{_}" +
                        speed_type_str +
                        ".");
        }
}

static MonShockStrings mon_shock_lvl_to_strings(const MonShockLvl shock_lvl)
{
        MonShockStrings result;

        switch (shock_lvl) {
        case MonShockLvl::unsettling:
                result.color_fmt_str = "{COLOR_DARK_BROWN}";
                result.shock_str = "unsettling";
                result.punct_str = ".";
                break;

        case MonShockLvl::frightening:
                result.color_fmt_str = "{COLOR_GRAY}";
                result.shock_str = "frightening";
                result.punct_str = ".";
                break;

        case MonShockLvl::terrifying:
                result.color_fmt_str = "{COLOR_RED}";
                result.shock_str = "terrifying";
                result.punct_str = "!";
                break;

        case MonShockLvl::mind_shattering:
                result.color_fmt_str = "{COLOR_LIGHT_RED}";
                result.shock_str = "mind shattering";
                result.punct_str = "!";
                break;

        case MonShockLvl::none:
        case MonShockLvl::END:
                break;
        }

        return result;
}

static std::string get_mon_shock_descr(
        const actor::ActorData& actor_data,
        const actor::Actor& actor)
{
        const auto shock_strings =
                mon_shock_lvl_to_strings(
                        actor_data.mon_shock_lvl);

        if (shock_strings.shock_str.empty()) {
                return "";
        }

        const std::string prefix_str =
                actor_data.is_unique
                ? (actor.name_the() + " is ")
                : ("They are ");

        return (
                shock_strings.color_fmt_str +
                prefix_str +
                shock_strings.shock_str +
                " to behold" +
                shock_strings.punct_str +
                "{reset_color}");
}

static std::string get_mon_wielded_wpn_str(
        const actor::ActorData& actor_data,
        const actor::Actor& actor)
{
        const auto* const wpn = actor.m_inv.item_in_slot(SlotId::wpn);

        if (!wpn) {
                return "";
        }

        const std::string pronoun_str =
                actor_data.is_unique
                ? actor.name_the()
                : "It";

        const std::string wpn_name_a =
                wpn->name(
                        ItemNameType::a,
                        ItemNameInfo::none,
                        ItemNameAttackInfo::none);

        return pronoun_str + " is wielding " + wpn_name_a + ".";
}

static std::string get_melee_hit_chance_descr(actor::Actor& actor)
{
        const auto* wielded_item =
                map::g_player->m_inv.item_in_slot(SlotId::wpn);

        const auto* const player_wpn =
                wielded_item
                ? static_cast<const item::Wpn*>(wielded_item)
                : &map::g_player->unarmed_wpn();

        if (!player_wpn) {
                ASSERT(false);

                return "";
        }

        const MeleeAttData att_data(map::g_player, actor, *player_wpn);

        const int hit_chance =
                ability_roll::success_chance_pct_actual(
                        att_data.hit_chance_tot);

        std::string descr =
                "The chance to hit " +
                actor.name_the() +
                " in melee combat is currently{_}{COLOR_LIGHT_GREEN}" +
                std::to_string(hit_chance) +
                "%{reset_color}";

        if (att_data.is_backstab) {
                descr += " (because they are unaware)";
        }

        descr += ".";

        return descr;
}

static std::string get_sneak_chance_descr(actor::Actor& actor)
{
        actor::SneakParameters p;
        p.actor_searching = &actor;
        p.actor_sneaking = map::g_player;

        const int tot_value =
                ability_roll::success_chance_pct_actual(
                        actor::calc_total_sneak_ability(p));

        std::string descr =
                "The chance to remain undetected by " +
                actor.name_the() +
                " is currently{_}{COLOR_LIGHT_GREEN}" +
                std::to_string(tot_value) +
                "%{reset_color}.";

        return descr;
}

static bool has_natural_property(
        const actor::ActorData& actor_data,
        const PropId id)
{
        return actor_data.natural_props[(size_t)id];
}

static void add_or_list_to_sentence(
        std::string& base_str,
        const std::vector<std::string>& names)
{
        const auto nr_names = names.size();

        for (size_t i = 0; i < nr_names; ++i) {
                if ((nr_names > 2) && (i > 0)) {
                        base_str += ",";
                }

                base_str += " ";

                if ((nr_names >= 2) && (i == (nr_names - 1))) {
                        base_str += "or ";
                }

                base_str += names[i];
        }
}

static std::string get_mon_natural_properties_descr(
        const actor::ActorData& actor_data)
{
        std::string descr;

        std::vector<std::string> cannot_be_harmed_by_names;
        std::vector<std::string> unaffected_by_names;
        std::vector<std::string> cannot_be_names;
        std::vector<std::string> cannot_names;
        std::vector<std::string> can_names;
        std::vector<std::string> custom_entries;

        for (const auto& p : s_cannot_be_harmed_by_props) {
                if (has_natural_property(actor_data, p.first)) {
                        cannot_be_harmed_by_names.push_back(p.second);
                }
        }

        for (const auto& p : s_unaffected_by_props) {
                if (has_natural_property(actor_data, p.first)) {
                        unaffected_by_names.push_back(p.second);
                }
        }

        for (const auto& p : s_cannot_be_props) {
                if (has_natural_property(actor_data, p.first)) {
                        cannot_be_names.push_back(p.second);
                }
        }

        for (const auto& p : s_cannot_props) {
                if (has_natural_property(actor_data, p.first)) {
                        cannot_names.push_back(p.second);
                }
        }

        for (const auto& p : s_can_props) {
                if (has_natural_property(actor_data, p.first)) {
                        can_names.push_back(p.second);
                }
        }

        for (const auto& p : s_custom_props) {
                if (has_natural_property(actor_data, p.first)) {
                        custom_entries.push_back(p.second);
                }
        }

        if (!cannot_be_harmed_by_names.empty()) {
                descr += s_cannot_be_harmed_by_start;
                add_or_list_to_sentence(descr, cannot_be_harmed_by_names);
                descr += ".";
        }

        if (!unaffected_by_names.empty()) {
                if (!descr.empty()) {
                        descr += " ";
                }

                descr += s_unaffected_by_start;
                add_or_list_to_sentence(descr, unaffected_by_names);
                descr += ".";
        }

        if (!cannot_be_names.empty()) {
                if (!descr.empty()) {
                        descr += " ";
                }

                descr += s_cannot_be_start;
                add_or_list_to_sentence(descr, cannot_be_names);
                descr += ".";
        }

        if (!cannot_names.empty()) {
                if (!descr.empty()) {
                        descr += " ";
                }

                descr += s_cannot_start;
                add_or_list_to_sentence(descr, cannot_names);
                descr += ".";
        }

        if (!can_names.empty()) {
                if (!descr.empty()) {
                        descr += " ";
                }

                descr += s_can_start;
                add_or_list_to_sentence(descr, can_names);
                descr += ".";
        }

        for (const auto& entry : custom_entries) {
                if (!descr.empty()) {
                        descr += " ";
                }

                descr += entry + ".";
        }

        return descr;
}

static std::string auto_description_str(actor::Actor& actor)
{
        std::string str;

        const auto& actor_data =
                actor.m_mimic_data
                ? *actor.m_mimic_data
                : *actor.m_data;

        if (!actor.is_actor_my_leader(map::g_player)) {
                text_format::append_with_space(
                        str,
                        get_melee_hit_chance_descr(actor));
        }

        const auto& ai = actor_data.ai;
        const auto looks = ai[(size_t)actor::AiId::looks];

        if (!actor.is_aware_of_player() &&
            !actor.is_actor_my_leader(map::g_player) &&
            looks) {
                text_format::append_with_space(
                        str,
                        get_sneak_chance_descr(actor));
        }

        if (actor_data.allow_speed_descr) {
                text_format::append_with_space(
                        str,
                        get_mon_speed_descr(actor_data, actor));
        }

        if (!actor.is_actor_my_leader(map::g_player)) {
                text_format::append_with_space(
                        str,
                        get_mon_memory_turns_descr(actor_data, actor));
        }

        if (!looks) {
                text_format::append_with_space(
                        str,
                        "They cannot visually detect other creatures");

                const auto pursues =
                        ai[(size_t)actor::AiId::moves_to_target_when_los] ||
                        ai[(size_t)actor::AiId::paths_to_target_when_aware];

                if (pursues) {
                        str += " (but will pursue any threat once aware)";
                }

                str += ".";
        }

        if (actor_data.is_undead) {
                text_format::append_with_space(
                        str,
                        "{COLOR_MAGENTA}This creature is undead.{reset_color}");
        }

        if (!actor.is_actor_my_leader(map::g_player)) {
                text_format::append_with_space(
                        str,
                        get_mon_shock_descr(actor_data, actor));
        }

        if (actor_data.allow_wielded_wpn_descr) {
                text_format::append_with_space(
                        str,
                        get_mon_wielded_wpn_str(actor_data, actor));
        }

        const auto natural_properties_descr =
                get_mon_natural_properties_descr(actor_data);

        if (!natural_properties_descr.empty()) {
                if (!str.empty()) {
                        str += "\n\n";
                }

                str += natural_properties_descr;
        }

        return str;
}

static bool should_show_property(const actor::Actor& actor, const Prop& property)
{
        // Show all temporary negative properties (burning, fear etc), and
        // Frenzy (special case, it is typically very important to know, and
        // would be reasonable for the player character to notice this).
        //
        // We do not want to show temporary positive properties, since those are
        // not marked on the creature symbol on the map (perhaps they should,
        // but currently they are not), and also because it should not be known
        // whether a monster has Spell Shield active (and perhaps other
        // properties as well).
        //
        return (
                actor.m_properties.is_temporary_negative_prop(property) ||
                property.id() == PropId::frenzied);
}

static std::vector<PropListEntry> temporary_properties_to_show(const actor::Actor& actor)
{
        std::vector<PropListEntry> prop_list = actor.m_properties.property_names_and_descr();

        for (auto it = std::begin(prop_list); it != std::end(prop_list);) {
                const auto* const prop = it->prop;

                if (should_show_property(actor, *prop)) {
                        ++it;
                }
                else {
                        it = prop_list.erase(it);
                }
        }

        return prop_list;
}

static std::string temporary_properties_str(actor::Actor& actor)
{
        std::string str;

        // Properties
        std::vector<PropListEntry> prop_list = temporary_properties_to_show(actor);

        for (const auto& entry : prop_list) {
                if (!str.empty()) {
                        str += "\n";
                }

                // HACK: This assumes the good/bad/text colors, which is defined
                // in a data file. However the text formatter does not recognize
                // those IDs.
                if (entry.title.color == colors::msg_good()) {
                        str += "{COLOR_LIGHT_GREEN}";
                }
                else if (entry.title.color == colors::msg_bad()) {
                        str += "{COLOR_LIGHT_RED}";
                }
                else {
                        str += "{COLOR_WHITE}";
                }

                str += entry.title.str;
                str += "{color_reset}: ";
                str += entry.descr;
        }

        return str;
}

// -----------------------------------------------------------------------------
// View actor description
// -----------------------------------------------------------------------------
StateId ViewActorDescr::id() const
{
        return StateId::view_actor;
}

void ViewActorDescr::draw()
{
        io::cover_panel(Panel::screen);

        draw_interface();

        int y = 0;

        // Fixed decription
        {
                Text text;

                text.set_w(panels::w(Panel::info_screen_content));
                text.set_str(m_actor.descr());
                text.set_color(colors::text());

                text.draw(Panel::info_screen_content, {0, y});

                y += text.nr_lines();
        }

        // Auto description
        {
                const std::string auto_descr_str =
                        auto_description_str(m_actor);

                if (!auto_descr_str.empty()) {
                        ++y;

                        Text text;

                        text.set_w(panels::w(Panel::info_screen_content));
                        text.set_str(auto_description_str(m_actor));
                        text.set_color(colors::text());

                        text.draw(Panel::info_screen_content, {0, y});

                        y += text.nr_lines();
                }
        }

        // Current properties (not all are shown)
        {
                ++y;

                Text text;

                text.set_w(panels::w(Panel::info_screen_content));
                text.set_str(temporary_properties_str(m_actor));
                text.set_color(colors::text());

                text.draw(Panel::info_screen_content, {0, y});
        }
}

void ViewActorDescr::update()
{
        const auto input = io::read_input();

        switch (input.key) {
        case SDLK_SPACE:
        case SDLK_ESCAPE: {
                // Exit screen
                states::pop();
        } break;

        default:
        {
        } break;
        }
}

std::string ViewActorDescr::title() const
{
        return text_format::first_to_upper(m_actor.name_a());
}
