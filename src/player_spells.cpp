// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "player_spells.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "actor_player.hpp"
#include "array2.hpp"
#include "browser.hpp"
#include "colors.hpp"
#include "common_text.hpp"
#include "debug.hpp"
#include "direction.hpp"
#include "draw_box.hpp"
#include "io.hpp"
#include "map.hpp"
#include "msg_log.hpp"
#include "panel.hpp"
#include "player_bon.hpp"
#include "pos.hpp"
#include "property_data.hpp"
#include "property_handler.hpp"
#include "query.hpp"
#include "random.hpp"
#include "rect.hpp"
#include "saving.hpp"
#include "terrain.hpp"
#include "terrain_data.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static std::vector<Spell*> s_learned_spells;

static SpellSkill s_spell_skills[(size_t)SpellId::END];

static void try_cast(Spell* const spell)
{
        const auto& props = map::g_player->m_properties;

        bool allow_cast =
                props.allow_cast_intr_spell_absolute(
                        Verbose::yes);

        if (allow_cast)
        {
                allow_cast =
                        allow_cast &&
                        props.allow_speak(Verbose::yes);
        }

        if (!allow_cast)
        {
                return;
        }

        msg_log::clear();

        const auto skill = map::g_player->spell_skill(spell->id());

        const auto spi_cost_range =
                spell->spi_cost_range(
                        skill,
                        map::g_player);

        if (spi_cost_range.max >= map::g_player->m_sp)
        {
                const std::string msg =
                        "Low spirit, try casting spell anyway? " +
                        common_text::g_yes_or_no_hint;

                msg_log::add(
                        msg,
                        colors::light_white(),
                        MsgInterruptPlayer::no,
                        MorePromptOnMsg::no,
                        CopyToMsgHistory::no);

                if (query::yes_or_no() == BinaryAnswer::no)
                {
                        msg_log::clear();

                        return;
                }

                msg_log::clear();
        }

        msg_log::add("I cast " + spell->name() + "!");

        if (map::g_player->is_alive())
        {
                spell->cast(map::g_player, skill, SpellSrc::learned);
        }
}

// -----------------------------------------------------------------------------
// player_spells
// -----------------------------------------------------------------------------
namespace player_spells
{
void init()
{
        cleanup();
}

void cleanup()
{
        for (Spell* spell : s_learned_spells)
        {
                delete spell;
        }

        s_learned_spells.clear();

        for (size_t i = 0; i < (size_t)SpellId::END; ++i)
        {
                s_spell_skills[i] = (SpellSkill)0;
        }
}

void save()
{
        saving::put_int((int)s_learned_spells.size());

        for (Spell* s : s_learned_spells)
        {
                saving::put_int((int)s->id());
        }

        for (size_t i = 0; i < (size_t)SpellId::END; ++i)
        {
                saving::put_int((int)s_spell_skills[i]);
        }
}

void load()
{
        const int nr_spells = saving::get_int();

        for (int i = 0; i < nr_spells; ++i)
        {
                const auto id = (SpellId)saving::get_int();

                s_learned_spells.push_back(spells::make(id));
        }

        for (size_t i = 0; i < (size_t)SpellId::END; ++i)
        {
                s_spell_skills[i] = (SpellSkill)saving::get_int();
        }
}

bool is_spell_learned(const SpellId id)
{
        return (
                std::any_of(
                        std::cbegin(s_learned_spells),
                        std::cend(s_learned_spells),
                        [id](const auto* const s) {
                                return s->id() == id;
                        }));
}

void learn_spell(const SpellId id, const Verbose verbose)
{
        ASSERT(id != SpellId::END);

        if (is_spell_learned(id))
        {
                // Spell already known
                return;
        }

        auto* const spell = spells::make(id);

        const bool player_can_learn = spell->player_can_learn();

        ASSERT(player_can_learn);

        // Robustness for release mode
        if (!player_can_learn)
        {
                return;
        }

        if (verbose == Verbose::yes)
        {
                msg_log::add(
                        "I can now cast " +
                        spell->name() +
                        " from memory.");
        }

        s_learned_spells.push_back(spell);
}

void unlearn_spell(const SpellId id, const Verbose verbose)
{
        ASSERT(id != SpellId::END);

        if (!is_spell_learned(id))
        {
                // Spell was already unknown
                return;
        }

        auto spell_iterator = std::begin(s_learned_spells);

        for (; spell_iterator != std::end(s_learned_spells);
             ++spell_iterator)
        {
                if ((*spell_iterator)->id() == id)
                {
                        break;
                }
        }

        if (spell_iterator == std::end(s_learned_spells))
        {
                return;
        }

        const auto* const spell = *spell_iterator;

        ASSERT(spell->player_can_learn());

        if (verbose == Verbose::yes)
        {
                const auto name = spell->name();

                msg_log::add(
                        "I no longer recall how to cast " +
                        name +
                        "!");
        }

        delete spell;

        s_learned_spells.erase(spell_iterator);
}

void incr_spell_skill(const SpellId id, const Verbose verbose)
{
        ASSERT(id != SpellId::END);

        TRACE
                << "Increasing spell skill for spell id: "
                << (int)id
                << std::endl;

        auto& skill = s_spell_skills[(size_t)id];

        TRACE << "skill before: " << (int)skill << std::endl;

        if (skill != SpellSkill::master)
        {
                skill = (SpellSkill)((int)skill + 1);
        }

        if (is_spell_learned(id) && (verbose == Verbose::yes))
        {
                const std::unique_ptr<const Spell> spell(spells::make(id));

                const auto name = spell->name();

                msg_log::add(
                        "I am more skilled at casting " +
                        name +
                        "!");
        }

        TRACE << "skill after: " << (int)skill << std::endl;
}

SpellSkill spell_skill(const SpellId id)
{
        ASSERT(id != SpellId::END);

        if (id == SpellId::END)
        {
                return SpellSkill::basic;
        }

        auto skill = s_spell_skills[(size_t)id];

        // Altar skill bonus - max level is master.
        if ((skill < SpellSkill::master) &&
            is_getting_altar_bonus())
        {
                skill = (SpellSkill)((int)skill + 1);
        }

        // Erudition skill bonus - max level is master.
        if ((skill < SpellSkill::master) &&
            map::g_player->m_properties.has(PropId::erudition))
        {
                skill = (SpellSkill)((int)skill + 1);
        }

        // Necronomicon skill bonus - transcendent skill allowed.
        const bool has_necronomicon =
                map::g_player->m_inv.has_item_in_backpack(
                        item::Id::necronomicon);

        if ((skill != SpellSkill::transcendent) && has_necronomicon)
        {
                skill = (SpellSkill)((int)skill + 1);
        }

        return skill;
}

void set_spell_skill(const SpellId id, const SpellSkill val)
{
        ASSERT(id != SpellId::END);

        if (id == SpellId::END)
        {
                return;
        }

        s_spell_skills[(size_t)id] = val;
}

bool is_getting_altar_bonus()
{
        if (player_bon::is_bg(Bg::exorcist))
        {
                return false;
        }

        for (const auto& d : dir_utils::g_dir_list)
        {
                const auto p = map::g_player->m_pos + d;

                if (map::g_terrain.at(p)->id() == terrain::Id::altar)
                {
                        return true;
                }
        }

        return false;
}

}  // namespace player_spells

// -----------------------------------------------------------------------------
// BrowseSpell
// -----------------------------------------------------------------------------
void BrowseSpell::on_start()
{
        if (s_learned_spells.empty())
        {
                // Exit screen
                states::pop();

                msg_log::add("I do not know any spells.");
                return;
        }

        m_browser.reset((int)s_learned_spells.size());

        m_browser.disable_selection_audio();
}

void BrowseSpell::draw()
{
        draw_box(panels::area(Panel::screen));

        const int nr_spells = (int)s_learned_spells.size();

        io::draw_text_center(
                " Use which power? ",
                Panel::screen,
                P(panels::center_x(Panel::screen), 0),
                colors::title());

        P p(0, 0);

        for (int i = 0; i < nr_spells; ++i)
        {
                std::string key_str = "(?)";

                key_str[1] = m_browser.menu_keys()[i];

                const bool is_marked = m_browser.is_at_idx(i);

                auto* const spell = s_learned_spells[i];
                const auto name = spell->name();

                constexpr int spi_label_x = 23;
                constexpr int skill_label_x = spi_label_x + 10;

                p.x = 0;

                auto color =
                        is_marked
                        ? colors::menu_key_highlight()
                        : colors::menu_key_dark();

                io::draw_text(
                        key_str,
                        Panel::item_menu,
                        p,
                        color);

                p.x = (int)key_str.size() + 1;

                color =
                        is_marked
                        ? colors::menu_highlight()
                        : colors::menu_dark();

                io::draw_text(
                        name,
                        Panel::item_menu,
                        p,
                        color);

                std::string fill_str;

                const size_t fill_size = spi_label_x - p.x - name.size();

                for (size_t ii = 0; ii < fill_size; ++ii)
                {
                        fill_str.push_back('.');
                }

                const auto id = spell->id();

                const auto skill = player_spells::spell_skill(id);

                const auto spi_cost =
                        spell->spi_cost_range(
                                skill,
                                map::g_player);

                if (spi_cost.min > 0)
                {
                        const Color fill_color = colors::gray().fraction(3.0);

                        io::draw_text(
                                fill_str,
                                Panel::item_menu,
                                P(p.x + (int)name.size(), p.y),
                                fill_color);

                        p.x = spi_label_x;

                        std::string str = "SP: ";

                        io::draw_text(
                                str,
                                Panel::item_menu,
                                p,
                                colors::dark_gray());

                        p.x += (int)str.size();

                        io::draw_text(
                                spi_cost.str(),
                                Panel::item_menu,
                                p,
                                colors::white());
                }

                if (spell->can_be_improved_with_skill())
                {
                        p.x = skill_label_x;

                        std::string str = "Skill: ";

                        io::draw_text(
                                str,
                                Panel::item_menu,
                                p,
                                colors::dark_gray());

                        p.x += (int)str.size();

                        switch (skill)
                        {
                        case SpellSkill::basic:
                                str = "I";
                                break;

                        case SpellSkill::expert:
                                str = "II";
                                break;

                        case SpellSkill::master:
                                str = "III";
                                break;

                        case SpellSkill::transcendent:
                                str = "IV";
                                break;
                        }

                        io::draw_text(
                                str,
                                Panel::item_menu,
                                p,
                                colors::white());
                }

                if (is_marked)
                {
                        const auto descr =
                                spell->descr(skill, SpellSrc::learned);

                        std::vector<ColoredString> lines;

                        lines.reserve(descr.size());
                        for (const auto& line : descr)
                        {
                                lines.emplace_back(
                                        line,
                                        colors::light_white());
                        }

                        if (!lines.empty())
                        {
                                io::draw_descr_box(lines);
                        }
                }

                ++p.y;
        }
}

void BrowseSpell::update()
{
        auto input = io::get();

        const MenuAction action =
                m_browser.read(
                        input,
                        MenuInputMode::scrolling_and_letters);

        switch (action)
        {
        case MenuAction::selected:
        {
                auto* const spell = s_learned_spells[m_browser.y()];

                // Exit screen
                states::pop();

                try_cast(spell);

                return;
        }
        break;

        case MenuAction::esc:
        case MenuAction::space:
        {
                // Exit screen
                states::pop();
                return;
        }
        break;

        default:
                break;
        }
}
