// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
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

#include "actor.hpp"
#include "actor_player_state.hpp"
#include "actor_see.hpp"
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
#include "text_format.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static std::vector<Spell*> s_learned_spells;

static SpellSkill s_spell_skills[(size_t)SpellId::END];

static bool s_is_forgotten[(size_t)SpellId::END];

static void try_cast(Spell* const spell)
{
        const prop::PropHandler& props = map::g_player->m_properties;

        if (!props.allow_cast_intr_spell_absolute(Verbose::yes)) {
                return;
        }

        // TODO: Not all spells "require making noise", it seems insconsistent
        // to outright prevent casting here.
        if (!props.allow_speak(Verbose::yes)) {
                return;
        }

        if (player_spells::is_spell_forgotten(spell->id())) {
                const std::string name =
                        text_format::first_to_upper(spell->name());

                msg_log::add(
                        name +
                        " has faded from my memory, I can no longer "
                        "cast it.");

                return;
        }

        msg_log::clear();

        const SpellSkill skill = actor::spell_skill(*map::g_player, spell->id());

        const Range cost_range = spell->cost_range(skill, map::g_player);

        int resource_avail = 0;

        switch (spell->cost_type()) {
        case SpellCostType::spirit: {
                resource_avail = map::g_player->m_sp;

                // If the player is an Exorcist, also take fervor into account
                // (Exorcists can cast with fervor if SP runs out).
                if (player_bon::is_bg(Bg::exorcist)) {
                        resource_avail += actor::player_state::g_exorcist_fervor;
                }
        } break;

        case SpellCostType::hit_points: {
                resource_avail = map::g_player->m_hp;
        }
        }

        if (cost_range.max >= resource_avail) {
                const std::string resource_name =
                        (spell->cost_type() == SpellCostType::spirit)
                        ? "spirit"
                        : "health";

                const std::string msg =
                        "Low " +
                        resource_name +
                        ", try casting spell anyway? " +
                        common_text::g_yes_or_no_hint;

                msg_log::add(
                        msg,
                        colors::light_white(),
                        MsgInterruptPlayer::no,
                        MorePromptOnMsg::no,
                        CopyToMsgHistory::no);

                if (query::yes_or_no() == BinaryAnswer::no) {
                        msg_log::clear();

                        return;
                }

                msg_log::clear();
        }

        msg_log::add("I cast " + spell->name() + "!");

        if (actor::is_alive(*map::g_player)) {
                const std::vector<actor::Actor*> seen_foes =
                        actor::seen_foes(*map::g_player);

                spell->cast(
                        map::g_player,
                        skill,
                        SpellSrc::learned,
                        seen_foes);
        }
}

// A spell's row in the list. Laid out like an inventory row (see
// InvState::draw_backpack_item): indented as if a "(x)" selection key
// were drawn at the start, which it is not - there is no keyboard to
// press it on, entries are engaged by tapping.
static void draw_spell_menu_line(
        const Spell* const spell,
        const int y,
        const bool is_marked)
{
        // Matches InvState's s_key_indent_w
        constexpr int key_indent_w = 4;

        const std::string name = text_format::first_to_upper(spell->name());

        constexpr int cost_label_x = 23 + key_indent_w;
        constexpr int skill_label_x = cost_label_x + 10;

        int x = key_indent_w;

        Color color;

        if (s_is_forgotten[(size_t)spell->id()]) {
                color =
                        is_marked
                        ? colors::light_magenta()
                        : colors::magenta().shaded(20);
        }
        else {
                color =
                        is_marked
                        ? colors::menu_highlight()
                        : colors::menu_dark();
        }

        io::draw_text(
                name,
                Panel::inventory_menu,
                {x, y},
                color);

        std::string fill_str;

        // NOTE: Signed - a name reaching the cost column gives no dots at
        // all. Unsigned, as this was, it wrapped to an enormous count and
        // hung the game on a long enough spell name.
        const int fill_size = cost_label_x - x - (int)name.size();

        for (int ii = 0; ii < fill_size; ++ii) {
                fill_str.push_back('.');
        }

        const SpellSkill skill = player_spells::spell_skill(spell->id());

        const Range cost = spell->cost_range(skill, map::g_player);

        if (cost.min > 0) {
                const Color fill_color = colors::gray().shaded(70);

                io::draw_text(
                        fill_str,
                        Panel::inventory_menu,
                        {x + (int)name.size(), y},
                        fill_color);

                x = cost_label_x;

                const std::string cost_label =
                        (spell->cost_type() == SpellCostType::spirit)
                        ? "SP: "
                        : "HP: ";

                io::draw_text(
                        cost_label,
                        Panel::inventory_menu,
                        {x, y},
                        colors::dark_gray());

                x += (int)cost_label.size();

                const Color cost_color =
                        (spell->cost_type() == SpellCostType::spirit)
                        ? colors::light_blue()
                        : colors::light_red();

                io::draw_text(
                        cost.str(),
                        Panel::inventory_menu,
                        {x, y},
                        cost_color);
        }

        if (spell->can_be_improved_with_skill()) {
                x = skill_label_x;

                std::string str = "Skill: ";

                io::draw_text(
                        str,
                        Panel::inventory_menu,
                        {x, y},
                        colors::dark_gray());

                x += (int)str.size();

                switch (skill) {
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
                        Panel::inventory_menu,
                        {x, y},
                        colors::white());
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
        for (Spell* spell : s_learned_spells) {
                delete spell;
        }

        s_learned_spells.clear();

        for (size_t i = 0; i < (size_t)SpellId::END; ++i) {
                s_spell_skills[i] = (SpellSkill)0;

                s_is_forgotten[i] = false;
        }
}

void save()
{
        saving::put_int((int)s_learned_spells.size());

        for (Spell* s : s_learned_spells) {
                saving::put_int((int)s->id());
        }

        for (size_t i = 0; i < (size_t)SpellId::END; ++i) {
                saving::put_int((int)s_spell_skills[i]);

                saving::put_bool(s_is_forgotten[i]);
        }
}

void load()
{
        const int nr_spells = saving::get_int();

        for (int i = 0; i < nr_spells; ++i) {
                const auto id = (SpellId)saving::get_int();

                s_learned_spells.push_back(spells::make(id));
        }

        for (size_t i = 0; i < (size_t)SpellId::END; ++i) {
                s_spell_skills[i] = (SpellSkill)saving::get_int();

                s_is_forgotten[i] = saving::get_bool();
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

        if (is_spell_learned(id)) {
                // Spell already known
                return;
        }

        Spell* const spell = spells::make(id);

        const bool player_can_learn = spell->player_can_learn();

        ASSERT(player_can_learn);

        // Robustness for release mode
        if (!player_can_learn) {
                return;
        }

        if (verbose == Verbose::yes) {
                msg_log::add(
                        "I can now cast " +
                        spell->name() +
                        " from memory.");
        }

        s_learned_spells.push_back(spell);
}

void remove_learned_spell(const SpellId id)
{
        ASSERT(id != SpellId::END);

        if (!is_spell_learned(id)) {
                return;
        }

        auto spell_iterator = std::begin(s_learned_spells);

        for (; spell_iterator != std::end(s_learned_spells);
             ++spell_iterator) {
                if ((*spell_iterator)->id() == id) {
                        break;
                }
        }

        if (spell_iterator == std::end(s_learned_spells)) {
                return;
        }

        const Spell* const spell = *spell_iterator;

        ASSERT(spell->player_can_learn());

        delete spell;

        s_learned_spells.erase(spell_iterator);
}

bool is_spell_forgotten(SpellId id)
{
        ASSERT(id != SpellId::END);

        return s_is_forgotten[(size_t)id];
}

void forget_spell(const SpellId id)
{
        if (is_spell_forgotten(id)) {
                return;
        }

        std::unique_ptr<const Spell> spell(spells::make(id));

        const std::string name = spell->name();

        msg_log::add("I no longer recall how to cast " + name + "!");

        s_is_forgotten[(size_t)id] = true;
}

bool recall_spell(const SpellId id)
{
        TRACE_FUNC_BEGIN;

        TRACE << "Trying to recall spell id '" << (int)id << "'" << std::endl;

        if (!is_spell_forgotten(id)) {
                TRACE << "Spell not forgotten" << std::endl;

                TRACE_FUNC_END;

                return false;
        }

        TRACE << "Spell is forgotten, recalling" << std::endl;

        std::unique_ptr<const Spell> spell(spells::make(id));

        const std::string name = spell->name();

        msg_log::add("I remember how to cast " + name + " again!");

        s_is_forgotten[(size_t)id] = false;

        TRACE_FUNC_END;

        return true;
}

bool recall_all_spells()
{
        bool is_any_recalled = false;

        for (size_t i = 0; i < (size_t)SpellId::END; ++i) {
                const bool is_recalled = recall_spell((SpellId)i);

                is_any_recalled = is_recalled || is_any_recalled;
        }

        return is_any_recalled;
}

void incr_spell_skill(const SpellId id, const Verbose verbose)
{
        ASSERT(id != SpellId::END);

        TRACE
                << "Increasing spell skill for spell id: "
                << (int)id
                << std::endl;

        SpellSkill& skill = s_spell_skills[(size_t)id];

        TRACE << "skill before: " << (int)skill << std::endl;

        if (skill != SpellSkill::master) {
                skill = (SpellSkill)((int)skill + 1);
        }

        if (is_spell_learned(id) && (verbose == Verbose::yes)) {
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

        if (id == SpellId::END) {
                return SpellSkill::basic;
        }

        const prop::PropHandler& properties = map::g_player->m_properties;

        SpellSkill skill = s_spell_skills[(size_t)id];

        // Altar skill bonus - max level is master.
        if ((skill < SpellSkill::master) && is_getting_altar_bonus()) {
                skill = (SpellSkill)((int)skill + 1);
        }

        // Erudition skill bonus - max level is master.
        if ((skill < SpellSkill::master) && properties.has(prop::Id::erudition)) {
                skill = (SpellSkill)((int)skill + 1);
        }

        // Focused + Sage skill bonus - max level is master.
        if ((skill < SpellSkill::master) &&
            properties.has(prop::Id::meditative_focused) &&
            player_bon::has_trait(Trait::sage)) {
                skill = (SpellSkill)((int)skill + 1);
        }

        // Necronomicon skill bonus - transcendent skill allowed.
        const bool has_necronomicon =
                map::g_player->m_inv.has_item_in_backpack(
                        item::Id::necronomicon);

        if ((skill != SpellSkill::transcendent) && has_necronomicon) {
                skill = (SpellSkill)((int)skill + 1);
        }

        return skill;
}

void set_spell_skill(const SpellId id, const SpellSkill val)
{
        ASSERT(id != SpellId::END);

        if (id == SpellId::END) {
                return;
        }

        s_spell_skills[(size_t)id] = val;
}

bool is_getting_altar_bonus()
{
        if (player_bon::is_bg(Bg::exorcist)) {
                return false;
        }

        for (const P& d : dir_utils::g_dir_list) {
                const P p = map::g_player->m_pos + d;

                if (map::g_terrain.at(p)->id() == terrain::Id::altar) {
                        return true;
                }
        }

        return false;
}

}  // namespace player_spells

// -----------------------------------------------------------------------------
// BrowseSpell
// -----------------------------------------------------------------------------
StateId BrowseSpell::id() const
{
        return StateId::browse_spells;
}

void BrowseSpell::on_start()
{
        if (s_learned_spells.empty()) {
                // Exit screen
                states::pop();

                msg_log::add("I do not know any spells.");
                return;
        }

        m_browser.reset(
                (int)s_learned_spells.size(),
                panels::h(Panel::inventory_menu));

        m_browser.set_selection_audio_enabled(false);
}

Spell* BrowseSpell::marked_spell() const
{
        const int y = m_browser.y();

        if ((y < 0) || (y >= (int)s_learned_spells.size())) {
                return nullptr;
        }

        return s_learned_spells[y];
}

std::vector<ActionPin> BrowseSpell::marked_entry_actions() const
{
        if (!m_allow_cast || !marked_spell()) {
                // Opened only to be read (from the character screen) -
                // there is nothing to do with a spell here
                return {};
        }

        return {{(int)SpellActionId::cast, "cast", {}}};
}

void BrowseSpell::run_action(const int action_id)
{
        ASSERT((SpellActionId)action_id == SpellActionId::cast);

        (void)action_id;

        Spell* const spell = marked_spell();

        if (!spell) {
                return;
        }

        // NOTE: The screen closes BEFORE the spell runs - casting spends
        // the turn, and some spells go on to aim out on the map (see
        // CtrlObj, CtrlTele), which must happen over the game view rather
        // than over this list.
        states::pop();

        // NOTE: This object is now deleted!

        try_cast(spell);
}

void BrowseSpell::draw_spell_descr()
{
        const Spell* const spell = marked_spell();

        if (!spell) {
                return;
        }

        if (spell != m_viewed_spell) {
                // Another spell is being looked at - its description
                // starts at the top
                m_descr.reset_scroll();

                m_viewed_spell = spell;
        }

        const SpellSkill skill = player_spells::spell_skill(spell->id());

        m_descr.draw(
                spell->descr(skill, SpellSrc::learned),
                colors::light_white());
}

void BrowseSpell::draw()
{
        draw_box(panels::screen_box_area());

        io::draw_text_center(
                " Cast spell ",
                Panel::screen,
                {panels::center_x(Panel::screen), panels::screen_box_area().p0.y},
                colors::title());

        io::draw_text_center(
                " " + common_text::g_menu_select_hint + " ",
                Panel::screen,
                {panels::center_x(Panel::screen), panels::screen_box_area().p1.y},
                colors::title());

        // The pins are laid out first - the description needs to know how
        // many of its bottom rows they take (see ActionListState)
        prepare_action_pins();

        const Range idx_range_shown = m_browser.range_shown();

        int y = 0;

        for (int i = idx_range_shown.min; i <= idx_range_shown.max; ++i) {
                draw_spell_menu_line(
                        s_learned_spells[i],
                        y,
                        m_browser.is_at_idx(i));

                ++y;
        }

        draw_spell_descr();

        // Where the list continues, its edge fades out
        draw_list_fades();

        // Last - the pins sit on top of the faded out end of the text
        draw_action_pins();
}

void BrowseSpell::update()
{
        const io::InputData input = io::read_input();

        if (handle_pending_action()) {
                // NOTE: This object may now be deleted!
                return;
        }

        const MenuAction action =
                m_browser.read(input, MenuInputMode::scrolling_and_letters);

        switch (action) {
        case MenuAction::selected: {
                // The same thing the [ cast ] pin does. NOTE: On a screen
                // WITH pins a tap only marks the row (see
                // ActionListState::try_tap), so this is the keyboard path.
                if (m_allow_cast) {
                        run_action((int)SpellActionId::cast);
                }

                // NOTE: This object may now be deleted!
                return;
        } break;

        case MenuAction::esc:
        case MenuAction::space: {
                // Exit screen
                states::pop();
                return;
        } break;

        default:
                break;
        }
}
