// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "look.hpp"

#include <climits>
#include <string>

#include "actor_mon.hpp"
#include "actor_player.hpp"
#include "actor_see.hpp"
#include "attack_data.hpp"
#include "game_time.hpp"
#include "inventory.hpp"
#include "io.hpp"
#include "item.hpp"
#include "map.hpp"
#include "marker.hpp"
#include "msg_log.hpp"
#include "property.hpp"
#include "property_data.hpp"
#include "property_handler.hpp"
#include "query.hpp"
#include "terrain.hpp"
#include "terrain_mob.hpp"
#include "text_format.hpp"

// -----------------------------------------------------------------------------
// private
// -----------------------------------------------------------------------------
static std::string get_mon_memory_turns_descr(
        const actor::ActorData& actor_data,
        const actor::Actor& actor)
{
        const int nr_turns_aware = actor_data.nr_turns_aware;

        if (nr_turns_aware <= 0)
        {
                return "";
        }

        const std::string name_a = text_format::first_to_upper(actor.name_a());

        if (nr_turns_aware < 50)
        {
                const std::string nr_turns_aware_str =
                        std::to_string(nr_turns_aware);

                return name_a +
                        " will remember hostile creatures for at least " +
                        nr_turns_aware_str +
                        " turns.";
        }
        else
        {
                // Very high number of turns awareness
                return name_a +
                        " remembers hostile creatures for a very long time.";
        }
}

static std::string get_mon_dlvl_descr(
        const actor::ActorData& actor_data,
        const actor::Actor& actor)
{
        const int dlvl = actor_data.spawn_min_dlvl;

        if ((dlvl <= 1) || (dlvl >= g_dlvl_last))
        {
                return "";
        }

        const std::string dlvl_str = std::to_string(dlvl);

        if (actor_data.is_unique)
        {
                return (
                        actor.name_the() +
                        " usually dwells beneath level " +
                        dlvl_str +
                        ".");
        }
        else
        {
                // Not unique
                return (
                        "They usually dwell beneath level " +
                        dlvl_str +
                        ".");
        }
}

static std::string mon_speed_type_to_str(const actor::ActorData& actor_data)
{
        switch (actor_data.speed)
        {
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
        const std::string speed_type_str = mon_speed_type_to_str(actor_data);

        if (speed_type_str.empty())
        {
                return "";
        }

        if (actor_data.is_unique)
        {
                return (
                        actor.name_the() +
                        " appears to move " +
                        speed_type_str +
                        ".");
        }
        else
        {
                // Not unique
                return (
                        "They appear to move " +
                        speed_type_str +
                        ".");
        }
}

static void mon_shock_lvl_to_str(
        const actor::ActorData& actor_data,
        std::string& shock_str_out,
        std::string& punct_str_out)
{
        shock_str_out = "";
        punct_str_out = "";

        switch (actor_data.mon_shock_lvl)
        {
        case ShockLvl::unsettling:
                shock_str_out = "unsettling";
                punct_str_out = ".";
                break;

        case ShockLvl::frightening:
                shock_str_out = "frightening";
                punct_str_out = ".";
                break;

        case ShockLvl::terrifying:
                shock_str_out = "terrifying";
                punct_str_out = "!";
                break;

        case ShockLvl::mind_shattering:
                shock_str_out = "mind shattering";
                punct_str_out = "!";
                break;

        case ShockLvl::none:
        case ShockLvl::END:
                break;
        }
}

static std::string get_mon_shock_descr(
        const actor::ActorData& actor_data,
        const actor::Actor& actor)
{
        std::string shock_str;

        std::string shock_punct_str;

        mon_shock_lvl_to_str(actor_data, shock_str, shock_punct_str);

        if (shock_str.empty())
        {
                return "";
        }

        if (actor_data.is_unique)
        {
                return (
                        actor.name_the() +
                        " is " +
                        shock_str +
                        " to behold" +
                        shock_punct_str);
        }
        else
        {
                // Not unique
                return (
                        "They are " +
                        shock_str +
                        " to behold" +
                        shock_punct_str);
        }
}

static std::string get_melee_hit_chance_descr(actor::Actor& actor)
{
        const auto* wielded_item =
                map::g_player->m_inv.item_in_slot(SlotId::wpn);

        const auto* const player_wpn =
                wielded_item
                ? static_cast<const item::Wpn*>(wielded_item)
                : &map::g_player->unarmed_wpn();

        if (!player_wpn)
        {
                ASSERT(false);

                return "";
        }

        const MeleeAttData att_data(map::g_player, actor, *player_wpn);

        const int hit_chance =
                ability_roll::hit_chance_pct_actual(
                        att_data.hit_chance_tot);

        std::string descr =
                "The chance to hit " +
                actor.name_the() +
                " in melee combat is currently " +
                std::to_string(hit_chance) +
                "%";

        if (att_data.is_backstab)
        {
                descr += " (because they are unaware)";
        }

        descr += ".";

        return descr;
}

// -----------------------------------------------------------------------------
// View actor description
// -----------------------------------------------------------------------------
StateId ViewActorDescr::id() const
{
        return StateId::view_actor;
}

void ViewActorDescr::on_start()
{
        const auto* const actor_data =
                m_actor.m_mimic_data
                ? m_actor.m_mimic_data
                : m_actor.m_data;

        // Fixed decription
        const auto fixed_descr = m_actor.descr();

        {
                const auto fixed_lines =
                        text_format::split(
                                fixed_descr,
                                panels::w(Panel::info_screen_content));

                for (const auto& line : fixed_lines)
                {
                        m_lines.emplace_back(
                                line,
                                colors::text());
                }
        }

        // Auto description
        {
                const auto auto_descr =
                        actor_data->allow_generated_descr
                        ? auto_description_str()
                        : "";

                if (!auto_descr.empty())
                {
                        m_lines.resize(m_lines.size() + 1);

                        const auto auto_descr_lines =
                                text_format::split(
                                        auto_descr,
                                        panels::w(Panel::info_screen_content));

                        for (const auto& line : auto_descr_lines)
                        {
                                m_lines.emplace_back(
                                        line,
                                        colors::text());
                        }
                }
        }

        // Properties
        auto prop_list =
                m_actor.m_properties
                        .property_names_temporary_negative();

        // Remove all non-negative properties (we should not show temporary
        // spell resistance for example), and all natural properties (properties
        // which all monsters of this type starts with)
        for (auto it = std::begin(prop_list); it != std::end(prop_list);)
        {
                const auto* const prop = it->prop;

                // NOTE: Using the real actor's data here is intentional
                const bool is_natural_prop =
                        m_actor.m_data->natural_props[(size_t)prop->id()];

                if (is_natural_prop ||
                    (prop->duration_mode() == PropDurationMode::indefinite) ||
                    (prop->alignment() != PropAlignment::bad))
                {
                        it = prop_list.erase(it);
                }
                else
                {
                        // Not a natural property
                        ++it;
                }
        }

        const std::string offset = "   ";

        if (!prop_list.empty())
        {
                m_lines.resize(m_lines.size() + 1);

                m_lines.emplace_back("Current properties", colors::text());

                const int max_w_descr =
                        (panels::x1(Panel::info_screen_content) * 3) / 4;

                for (const auto& e : prop_list)
                {
                        const auto& title = e.title;

                        m_lines.emplace_back(offset + title.str, e.title.color);

                        const auto descr_formatted =
                                text_format::split(
                                        e.descr,
                                        max_w_descr);

                        for (const auto& descr_line : descr_formatted)
                        {
                                m_lines.emplace_back(
                                        offset + descr_line,
                                        colors::gray());
                        }

                        // Add an empty line between each property, and also
                        // after the last one
                        m_lines.emplace_back("", colors::text());
                }
        }
}

void ViewActorDescr::draw()
{
        io::cover_panel(Panel::screen);

        draw_interface();

        const auto nr_lines = m_lines.size();

        const auto panel_h = panels::h(Panel::info_screen_content);

        size_t btm_nr =
                std::min(
                        m_top_idx + panel_h - 1,
                        (int)nr_lines - 1);

        int y = 0;

        for (size_t idx = m_top_idx; idx <= btm_nr; ++idx)
        {
                const auto& line = m_lines[idx];

                io::draw_text(
                        line.str,
                        Panel::info_screen_content,
                        {0, y},
                        line.color);

                ++y;
        }
}

void ViewActorDescr::update()
{
        const int line_jump = 3;
        const int nr_lines = m_lines.size();

        const auto input = io::get();

        switch (input.key)
        {
        case SDLK_KP_2:
        case SDLK_DOWN:
        {
                m_top_idx += line_jump;

                const int panel_h = panels::h(Panel::info_screen_content);

                if (nr_lines <= panel_h)
                {
                        m_top_idx = 0;
                }
                else
                {
                        m_top_idx = std::min(
                                nr_lines - panel_h,
                                m_top_idx);
                }
        }
        break;

        case SDLK_KP_8:
        case SDLK_UP:
        {
                m_top_idx = std::max(0, m_top_idx - line_jump);
        }
        break;

        case SDLK_SPACE:
        case SDLK_ESCAPE:
        {
                // Exit screen
                states::pop();
        }
        break;

        default:
        {
        }
        break;
        }
}

std::string ViewActorDescr::auto_description_str() const
{
        std::string str;

        const auto& actor_data =
                m_actor.m_mimic_data
                ? *m_actor.m_mimic_data
                : *m_actor.m_data;

        text_format::append_with_space(
                str,
                get_melee_hit_chance_descr(m_actor));

        text_format::append_with_space(
                str,
                get_mon_dlvl_descr(actor_data, m_actor));

        text_format::append_with_space(
                str,
                get_mon_speed_descr(actor_data, m_actor));

        text_format::append_with_space(
                str,
                get_mon_memory_turns_descr(actor_data, m_actor));

        if (actor_data.is_undead)
        {
                text_format::append_with_space(
                        str,
                        "This creature is undead.");
        }

        text_format::append_with_space(
                str,
                get_mon_shock_descr(actor_data, m_actor));

        return str;
}

std::string ViewActorDescr::title() const
{
        return text_format::first_to_upper(m_actor.name_a());
}

// -----------------------------------------------------------------------------
// Look
// -----------------------------------------------------------------------------
namespace look
{
void print_location_info_msgs(const P& pos)
{
        bool is_cell_seen = false;

        if (map::is_pos_inside_map(pos))
        {
                is_cell_seen = map::g_seen.at(pos);
        }

        if (is_cell_seen)
        {
                // Describe terrain
                const auto* const terrain = map::g_terrain.at(pos);

                std::string str = terrain->name(Article::a);
                str = text_format::first_to_upper(terrain->name(Article::a));

                msg_log::add(
                        str + ".",
                        colors::text(),
                        MsgInterruptPlayer::no,
                        MorePromptOnMsg::no,
                        CopyToMsgHistory::no);

                // Describe mobile terrains
                for (auto* mob : game_time::g_mobs)
                {
                        if (mob->pos() == pos)
                        {
                                str = mob->name(Article::a);

                                str = text_format::first_to_upper(str);

                                msg_log::add(
                                        str + ".",
                                        colors::text(),
                                        MsgInterruptPlayer::no,
                                        MorePromptOnMsg::no,
                                        CopyToMsgHistory::no);
                        }
                }

                // Describe darkness
                if (map::g_dark.at(pos) && !map::g_light.at(pos))
                {
                        msg_log::add(
                                "It is very dark here.",
                                colors::text(),
                                MsgInterruptPlayer::no,
                                MorePromptOnMsg::no,
                                CopyToMsgHistory::no);
                }

                // Describe item
                const auto* item = map::g_items.at(pos);

                if (item)
                {
                        str =
                                item->name(
                                        ItemRefType::plural,
                                        ItemRefInf::yes,
                                        ItemRefAttInf::wpn_main_att_mode);

                        str = text_format::first_to_upper(str);

                        msg_log::add(
                                str + ".",
                                colors::text(),
                                MsgInterruptPlayer::no,
                                MorePromptOnMsg::no,
                                CopyToMsgHistory::no);
                }

                // Describe dead actors
                for (auto* actor : game_time::g_actors)
                {
                        if (actor->is_corpse() && actor->m_pos == pos)
                        {
                                ASSERT(!actor->m_data->corpse_name_a.empty());

                                str = text_format::first_to_upper(
                                        actor->m_data->corpse_name_a);

                                msg_log::add(
                                        str + ".",
                                        colors::text(),
                                        MsgInterruptPlayer::no,
                                        MorePromptOnMsg::no,
                                        CopyToMsgHistory::no);
                        }
                }
        }

        print_living_actor_info_msg(pos);

        if (!is_cell_seen)
        {
                msg_log::add(
                        "I have no vision here.",
                        colors::text(),
                        MsgInterruptPlayer::no,
                        MorePromptOnMsg::no,
                        CopyToMsgHistory::no);
        }
}

void print_living_actor_info_msg(const P& pos)
{
        auto* actor = map::first_actor_at_pos(pos);

        if (!actor ||
            actor->is_player() ||
            !actor->is_alive())
        {
                return;
        }

        if (actor::can_player_see_actor(*actor))
        {
                const std::string str =
                        text_format::first_to_upper(
                                actor->name_a());

                msg_log::add(
                        str + ".",
                        colors::text(),
                        MsgInterruptPlayer::no,
                        MorePromptOnMsg::no,
                        CopyToMsgHistory::no);
        }
        else
        {
                // Cannot see actor
                if (actor->is_player_aware_of_me())
                {
                        msg_log::add(
                                "There is a creature here.",
                                colors::text(),
                                MsgInterruptPlayer::no,
                                MorePromptOnMsg::no,
                                CopyToMsgHistory::no);
                }
        }
}

}  // namespace look
