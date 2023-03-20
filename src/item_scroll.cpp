// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "item_scroll.hpp"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <ostream>
#include <string>

#include "actor.hpp"
#include "actor_see.hpp"
#include "array2.hpp"
#include "debug.hpp"
#include "game.hpp"
#include "global.hpp"
#include "inventory.hpp"
#include "item_data.hpp"
#include "item_factory.hpp"
#include "map.hpp"
#include "msg_log.hpp"
#include "player_bon.hpp"
#include "player_spells.hpp"
#include "property_data.hpp"
#include "property_handler.hpp"
#include "random.hpp"
#include "saving.hpp"
#include "spells.hpp"
#include "text_format.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static std::vector<std::string> s_fake_names;

static SpellSkill player_skill_for_scroll(const SpellId spell_id)
{
        SpellSkill skill = actor::spell_skill(*map::g_player, spell_id);

        const bool has_necronomicon =
                map::g_player->m_inv.has_item_in_backpack(
                        item::Id::necronomicon);

        const auto skill_cap =
                has_necronomicon
                ? SpellSkill::transcendent
                : SpellSkill::master;

        if (skill < skill_cap) {
                skill = (SpellSkill)((int)skill + 1);
        }

        return skill;
}

// -----------------------------------------------------------------------------
// scroll
// -----------------------------------------------------------------------------
namespace scroll
{
void init()
{
        TRACE_FUNC_BEGIN;

        // Randomize scroll fake names
        s_fake_names.clear();
        s_fake_names.emplace_back("Cruensseasrjit");
        s_fake_names.emplace_back("Rudsceleratus");
        s_fake_names.emplace_back("Rudminuox");
        s_fake_names.emplace_back("Cruo stragara-na");
        s_fake_names.emplace_back("Praya navita");
        s_fake_names.emplace_back("Pretia Cruento");
        s_fake_names.emplace_back("Pestis Cruento");
        s_fake_names.emplace_back("Cruento Pestis");
        s_fake_names.emplace_back("Domus-bhaava");
        s_fake_names.emplace_back("Acerbus-shatruex");
        s_fake_names.emplace_back("Pretaanluxis");
        s_fake_names.emplace_back("Praansilenux");
        s_fake_names.emplace_back("Quodpipax");
        s_fake_names.emplace_back("Lokemundux");
        s_fake_names.emplace_back("Profanuxes");
        s_fake_names.emplace_back("Shaantitus");
        s_fake_names.emplace_back("Geropayati");
        s_fake_names.emplace_back("Vilomaxus");
        s_fake_names.emplace_back("Bhuudesco");
        s_fake_names.emplace_back("Durbentia");
        s_fake_names.emplace_back("Bhuuesco");
        s_fake_names.emplace_back("Maravita");
        s_fake_names.emplace_back("Infirmux");

        std::vector<std::string> cmb;
        cmb.clear();
        cmb.emplace_back("Cruo");
        cmb.emplace_back("Cruonit");
        cmb.emplace_back("Cruentu");
        cmb.emplace_back("Marana");
        cmb.emplace_back("Domus");
        cmb.emplace_back("Malax");
        cmb.emplace_back("Caecux");
        cmb.emplace_back("Eximha");
        cmb.emplace_back("Vorox");
        cmb.emplace_back("Bibox");
        cmb.emplace_back("Pallex");
        cmb.emplace_back("Profanx");
        cmb.emplace_back("Invisuu");
        cmb.emplace_back("Invisux");
        cmb.emplace_back("Odiosuu");
        cmb.emplace_back("Odiosux");
        cmb.emplace_back("Vigra");
        cmb.emplace_back("Crudux");
        cmb.emplace_back("Desco");
        cmb.emplace_back("Esco");
        cmb.emplace_back("Gero");
        cmb.emplace_back("Klaatu");
        cmb.emplace_back("Barada");
        cmb.emplace_back("Nikto");

        const size_t nr_cmb_parts = cmb.size();

        for (size_t i = 0; i < nr_cmb_parts; ++i) {
                for (size_t ii = 0; ii < nr_cmb_parts; ii++) {
                        if (i != ii) {
                                s_fake_names.push_back(cmb[i] + " " + cmb[ii]);
                        }
                }
        }

        std::vector<item::ItemData*> scroll_data;

        for (auto& d : item::g_data) {
                if (d.type == ItemType::scroll) {
                        scroll_data.push_back(&d);
                }
        }

        for (auto& d : scroll_data) {
                // False name
                const size_t idx = rnd::idx(s_fake_names);

                const auto& title = s_fake_names[idx];

                d->base_name_un_id.names[(size_t)ItemNameType::plain] =
                        "Manuscript titled " + title;

                d->base_name_un_id.names[(size_t)ItemNameType::plural] =
                        "Manuscripts titled " + title;

                d->base_name_un_id.names[(size_t)ItemNameType::a] =
                        "a Manuscript titled " + title;

                s_fake_names.erase(s_fake_names.begin() + (int)idx);

                // True name
                const std::unique_ptr<const Scroll> scroll(
                        static_cast<const Scroll*>(
                                item::make(d->id, 1)));

                const auto real_type_name = scroll->real_name();

                const auto real_name =
                        "Manuscript of " + real_type_name;

                const auto real_name_plural =
                        "Manuscripts of " + real_type_name;

                const auto real_name_a =
                        "a Manuscript of " + real_type_name;

                d->base_name.names[(size_t)ItemNameType::plain] =
                        real_name;

                d->base_name.names[(size_t)ItemNameType::plural] =
                        real_name_plural;

                d->base_name.names[(size_t)ItemNameType::a] =
                        real_name_a;
        }

        // Randomize scroll spawning chances - some scrolls have a "high"
        // chance of spawning, and some have a "low" chance. The effect of this
        // should be that there is less chance to find all spells.
        rnd::shuffle(scroll_data);

        const size_t nr_scrolls = scroll_data.size();
        const size_t nr_high_chance = nr_scrolls / 2;

        for (size_t i = 0; i < nr_scrolls; ++i) {
                scroll_data[i]->chance_to_incl_in_spawn_list =
                        (i < nr_high_chance)
                        ? g_high_spawn_chance
                        : g_low_spawn_chance;
        }

        TRACE_FUNC_END;
}

void save()
{
        for (size_t i = 0; i < (size_t)item::Id::END; ++i) {
                if (item::g_data[i].type != ItemType::scroll) {
                        continue;
                }

                auto& names = item::g_data[i].base_name_un_id.names;

                saving::put_str(names[(size_t)ItemNameType::plain]);
                saving::put_str(names[(size_t)ItemNameType::plural]);
                saving::put_str(names[(size_t)ItemNameType::a]);
        }
}

void load()
{
        for (size_t i = 0; i < (size_t)item::Id::END; ++i) {
                if (item::g_data[i].type != ItemType::scroll) {
                        continue;
                }

                auto& names = item::g_data[i].base_name_un_id.names;

                names[(size_t)ItemNameType::plain] = saving::get_str();
                names[(size_t)ItemNameType::plural] = saving::get_str();
                names[(size_t)ItemNameType::a] = saving::get_str();
        }
}

Scroll::Scroll(item::ItemData* const item_data) :
        Item(item_data),
        m_domain_feeling_dlvl_countdown(rnd::range(1, 3)),
        m_domain_feeling_turn_countdown(rnd::range(100, 200))
{
}

void Scroll::save_hook() const
{
        saving::put_int(m_domain_feeling_dlvl_countdown);
        saving::put_int(m_domain_feeling_turn_countdown);
}

void Scroll::load_hook()
{
        m_domain_feeling_dlvl_countdown = saving::get_int();
        m_domain_feeling_turn_countdown = saving::get_int();
}

std::string Scroll::real_name() const
{
        const std::unique_ptr<const Spell> spell(make_spell());

        return spell->name();
}

std::vector<std::string> Scroll::descr_hook() const
{
        const std::unique_ptr<const Spell> spell(make_spell());

        if (m_data->is_identified) {
                const auto skill = player_skill_for_scroll(spell->id());

                return spell->descr(skill, SpellSrc::manuscript);
        }
        else {
                // Not identified
                auto lines = m_data->base_descr;

                if (m_data->is_spell_domain_known) {
                        const std::string domain_str = spell->domain_descr();

                        if (!domain_str.empty()) {
                                lines.push_back(domain_str);
                        }
                }
                else {
                        lines.emplace_back(
                                "Perhaps keeping it for a while will reveal "
                                "something about it.");
                }

                return lines;
        }
}

void Scroll::on_player_reached_new_dlvl_hook()
{
        auto& d = data();

        if (d.is_spell_domain_known ||
            d.is_identified ||
            (m_domain_feeling_dlvl_countdown <= 0)) {
                return;
        }

        --m_domain_feeling_dlvl_countdown;
}

void Scroll::on_actor_turn_in_inv_hook(const InvType inv_type)
{
        (void)inv_type;

        if (!actor::is_player(m_actor_carrying)) {
                return;
        }

        auto& d = data();

        if (d.is_spell_domain_known ||
            d.is_identified ||
            (m_domain_feeling_dlvl_countdown > 0) ||
            !map::g_player->m_properties.allow_act()) {
                return;
        }

        ASSERT(m_domain_feeling_turn_countdown > 0);

        --m_domain_feeling_turn_countdown;

        if (m_domain_feeling_turn_countdown <= 0) {
                TRACE << "Scroll domain discovered" << std::endl;

                const std::string name_plural =
                        d.base_name_un_id.names[(size_t)ItemNameType::plural];

                const std::unique_ptr<const Spell> spell(make_spell());

                const auto domain = spell->domain();

                if (domain != SpellDomain::END) {
                        const std::string domain_str =
                                text_format::first_to_lower(
                                        spells::spell_domain_title(
                                                spell->domain()));

                        if (!domain_str.empty()) {
                                msg_log::add(
                                        std::string(
                                                "I feel like " +
                                                name_plural +
                                                " belong to the " +
                                                domain_str +
                                                " domain."),
                                        colors::text(),
                                        MsgInterruptPlayer::no,
                                        MorePromptOnMsg::yes);
                        }
                }

                d.is_spell_domain_known = true;
        }
}

ItemPrePickResult Scroll::pre_pickup_hook()
{
        if (!player_bon::is_bg(Bg::exorcist)) {
                return ItemPrePickResult::do_pickup;
        }

        // Is exorcist

        msg_log::add("I destroy the profane text!");

        game::incr_player_xp(5);

        map::g_player->restore_sp(999, false, Verbose::no);
        map::g_player->restore_sp(7, true);

        return ItemPrePickResult::destroy_item;
}

ConsumeItem Scroll::activate(actor::Actor* const actor)
{
        TRACE_FUNC_BEGIN;

        // Check properties which NEVER allows reading or speaking
        if (!actor->m_properties.allow_read_absolute(Verbose::yes) ||
            !actor->m_properties.allow_speak(Verbose::yes)) {
                return ConsumeItem::no;
        }

        const auto& player_pos = map::g_player->m_pos;

        if (map::g_dark.at(player_pos) &&
            !map::g_light.at(player_pos) &&
            !map::g_player->m_properties.has(prop::Id::darkvision)) {
                msg_log::add("It's too dark to read here.");

                TRACE_FUNC_END;

                return ConsumeItem::no;
        }

        // OK, we can try to cast

        const bool is_identified_before = m_data->is_identified;

        if (is_identified_before) {
                const std::string scroll_name =
                        name(
                                ItemNameType::a,
                                ItemNameInfo::none);

                msg_log::add("I read " + scroll_name + "...");
        }
        else {
                // Not already identified
                msg_log::add(
                        "I recite the forbidden incantations on the "
                        "manuscript...");
        }

        const std::string crumble_str = "The Manuscript crumbles to dust.";

        // Check properties which MAY allow reading, with a random chance
        if (!actor->m_properties.allow_read_chance(Verbose::yes)) {
                msg_log::add(crumble_str);

                TRACE_FUNC_END;

                return ConsumeItem::yes;
        }

        // OK, we are fully allowed to read the scroll - cast the spell

        const std::unique_ptr<const Spell> spell(make_spell());

        const SpellId id = spell->id();

        const auto skill = player_skill_for_scroll(id);

        const auto seen_foes = actor::seen_foes(*map::g_player);

        spell->cast(
                map::g_player,
                skill,
                SpellSrc::manuscript,
                seen_foes);

        msg_log::add(crumble_str);

        identify(Verbose::yes);

        // Learn and recall spell.
        if (spell->player_can_learn()) {
                player_spells::learn_spell(id, Verbose::yes);
                player_spells::recall_spell(id);
        }

        TRACE_FUNC_END;

        return ConsumeItem::yes;
}

Spell* Scroll::make_spell() const
{
        return spells::make(m_data->spell_cast_from_scroll);
}

void Scroll::identify(const Verbose verbose)
{
        if (m_data->is_identified) {
                return;
        }

        m_data->is_identified = true;

        if (verbose == Verbose::yes) {
                const std::string name_after =
                        name(
                                ItemNameType::a,
                                ItemNameInfo::none);

                msg_log::add("I have identified " + name_after + ".");

                game::add_history_event("Identified " + name_after);
        }
}

std::string Scroll::name_info_str() const
{
        if (m_data->is_spell_domain_known && !m_data->is_identified) {
                const std::unique_ptr<const Spell> spell(make_spell());

                const SpellDomain domain = spell->domain();

                if (domain != SpellDomain::END) {
                        const auto domain_title =
                                spells::spell_domain_title(domain);

                        return "(" + domain_title + ")";
                }
        }

        return "";
}

}  // namespace scroll
