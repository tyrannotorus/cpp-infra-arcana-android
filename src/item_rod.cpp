// =============================================================================
// Copyright 2011-2024 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "item_rod.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>

#include "actor.hpp"
#include "actor_hit.hpp"
#include "actor_see.hpp"
#include "array2.hpp"
#include "audio_data.hpp"
#include "debug.hpp"
#include "direction.hpp"
#include "game.hpp"
#include "game_time.hpp"
#include "item_data.hpp"
#include "item_factory.hpp"
#include "knockback.hpp"
#include "map.hpp"
#include "misc.hpp"
#include "msg_log.hpp"
#include "player_bon.hpp"
#include "pos.hpp"
#include "property.hpp"
#include "property_data.hpp"
#include "property_factory.hpp"
#include "property_handler.hpp"
#include "random.hpp"
#include "rect.hpp"
#include "saving.hpp"
#include "sound.hpp"
#include "spells.hpp"
#include "terrain.hpp"
#include "text_format.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
std::vector<rod::RodLook> s_rod_looks;

// -----------------------------------------------------------------------------
// rod
// -----------------------------------------------------------------------------
namespace rod
{
void init()
{
        TRACE_FUNC_BEGIN;

        // Init possible rod colors and fake names
        s_rod_looks.clear();

        s_rod_looks.push_back(
                {"Iron", "an Iron", colors::gray()});

        s_rod_looks.push_back(
                {"Zinc", "a Zinc", colors::light_white()});

        s_rod_looks.push_back(
                {"Chromium", "a Chromium", colors::light_white()});

        s_rod_looks.push_back(
                {"Tin", "a Tin", colors::light_white()});

        s_rod_looks.push_back(
                {"Silver", "a Silver", colors::light_white()});

        s_rod_looks.push_back(
                {"Golden", "a Golden", colors::yellow()});

        s_rod_looks.push_back(
                {"Nickel", "a Nickel", colors::light_white()});

        s_rod_looks.push_back(
                {"Copper", "a Copper", colors::brown()});

        s_rod_looks.push_back(
                {"Lead", "a Lead", colors::gray()});

        s_rod_looks.push_back(
                {"Tungsten", "a Tungsten", colors::white()});

        s_rod_looks.push_back(
                {"Platinum", "a Platinum", colors::light_white()});

        s_rod_looks.push_back(
                {"Lithium", "a Lithium", colors::white()});

        s_rod_looks.push_back(
                {"Zirconium", "a Zirconium", colors::white()});

        s_rod_looks.push_back(
                {"Gallium", "a Gallium", colors::light_white()});

        s_rod_looks.push_back(
                {"Cobalt", "a Cobalt", colors::light_blue()});

        s_rod_looks.push_back(
                {"Titanium", "a Titanium", colors::light_white()});

        s_rod_looks.push_back(
                {"Magnesium", "a Magnesium", colors::white()});

        for (auto& d : item::g_data) {
                if (d.type == ItemType::rod) {
                        // Color and false name
                        const size_t idx =
                                rnd::range(0, (int)s_rod_looks.size() - 1);

                        RodLook& look = s_rod_looks[idx];

                        d.base_name_un_id.names[(size_t)ItemNameType::plain] =
                                look.name_plain + " Rod";

                        d.base_name_un_id.names[(size_t)ItemNameType::plural] =
                                look.name_plain + " Rods";

                        d.base_name_un_id.names[(size_t)ItemNameType::a] =
                                look.name_a + " Rod";

                        d.color = look.color;

                        s_rod_looks.erase(s_rod_looks.begin() + idx);

                        // True name
                        const auto* const rod =
                                static_cast<const Rod*>(
                                        item::make(d.id, 1));

                        const std::string real_type_name = rod->real_name();

                        delete rod;

                        const std::string real_name =
                                "Rod of " + real_type_name;

                        const std::string real_name_plural =
                                "Rods of " + real_type_name;

                        const std::string real_name_a =
                                "a Rod of " + real_type_name;

                        d.base_name.names[(size_t)ItemNameType::plain] =
                                real_name;

                        d.base_name.names[(size_t)ItemNameType::plural] =
                                real_name_plural;

                        d.base_name.names[(size_t)ItemNameType::a] =
                                real_name_a;
                }
        }

        TRACE_FUNC_END;
}

void save()
{
        for (int i = 0; i < (int)item::Id::END; ++i) {
                auto& d = item::g_data[i];

                if (d.type == ItemType::rod) {
                        saving::put_str(
                                d.base_name_un_id
                                        .names[(size_t)ItemNameType::plain]);

                        saving::put_str(
                                d.base_name_un_id
                                        .names[(size_t)ItemNameType::plural]);

                        saving::put_str(
                                d.base_name_un_id
                                        .names[(size_t)ItemNameType::a]);

                        saving::put_str(colors::color_to_name(d.color));
                }
        }
}

void load()
{
        for (int i = 0; i < (int)item::Id::END; ++i) {
                auto& d = item::g_data[i];

                if (d.type == ItemType::rod) {
                        d.base_name_un_id.names[(size_t)ItemNameType::plain] = saving::get_str();
                        d.base_name_un_id.names[(size_t)ItemNameType::plural] = saving::get_str();
                        d.base_name_un_id.names[(size_t)ItemNameType::a] = saving::get_str();
                        d.color = colors::name_to_color(saving::get_str());
                }
        }
}

void Rod::save_hook() const
{
        saving::put_int(m_nr_charge_turns_left);
}

void Rod::load_hook()
{
        m_nr_charge_turns_left = saving::get_int();
}

void Rod::set_max_charge_turns_left()
{
        m_nr_charge_turns_left = nr_turns_to_recharge();

        if (player_bon::has_trait(Trait::elec_incl)) {
                m_nr_charge_turns_left /= 2;
        }
}

ConsumeItem Rod::activate(actor::Actor* const actor)
{
        (void)actor;

        // Prevent using it if still charging, and identified (player character
        // knows that it's useless)
        if ((m_nr_charge_turns_left > 0) && m_data->is_identified) {
                const std::string rod_name =
                        name(
                                ItemNameType::plain,
                                ItemNameInfo::none);

                msg_log::add("The " + rod_name + " is still charging.");

                return ConsumeItem::no;
        }

        m_data->is_tried = true;

        // TODO: Sfx

        const std::string rod_name_a =
                name(
                        ItemNameType::a,
                        ItemNameInfo::none);

        msg_log::add("I activate " + rod_name_a + "...");

        if (m_nr_charge_turns_left == 0) {
                run_effect();

                set_max_charge_turns_left();
        }

        if (m_data->is_identified) {
                map::g_player->incr_shock(8.0, ShockSrc::use_strange_item);
        }
        else {
                // Not identified
                msg_log::add("Nothing happens.");
        }

        if (actor::is_alive(*map::g_player)) {
                game_time::tick();
        }

        return ConsumeItem::no;
}

void Rod::on_std_turn_in_inv_hook(const InvType inv_type)
{
        (void)inv_type;

        // Already fully charged?
        if (m_nr_charge_turns_left == 0) {
                return;
        }

        // All charges not finished, continue countdown

        ASSERT(m_nr_charge_turns_left > 0);

        --m_nr_charge_turns_left;

        if ((m_nr_charge_turns_left == 0) &&
            m_data->is_identified) {
                const std::string my_name =
                        name(
                                ItemNameType::plain,
                                ItemNameInfo::none);

                msg_log::add("The " + my_name + " has finished charging.");
        }
}

std::vector<std::string> Rod::descr_hook() const
{
        if (m_data->is_identified) {
                return {descr_identified()};
        }
        else {
                // Not identified
                return m_data->base_descr;
        }
}

void Rod::identify(const Verbose verbose)
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

std::string Rod::name_info_str(const ItemNameIdentified id_type) const
{
        if (m_data->is_identified || (id_type == ItemNameIdentified::force_identified)) {
                if (m_nr_charge_turns_left > 0) {
                        const std::string turns_left_str = std::to_string(m_nr_charge_turns_left);

                        return "(" + turns_left_str + " turns)";
                }
                else {
                        return "";
                }
        }
        else {
                // Not identified
                return m_data->is_tried ? "(Tried)" : "";
        }
}

void Curing::run_effect()
{
        auto& player = *map::g_player;

        std::vector<prop::Id> props_can_heal = {
                prop::Id::blind,
                prop::Id::deaf,
                prop::Id::poisoned,
                prop::Id::infected,
                prop::Id::diseased,
                prop::Id::weakened,
                prop::Id::hp_sap};

        bool is_something_healed = false;

        for (prop::Id prop_id : props_can_heal) {
                if (player.m_properties.end_prop(prop_id)) {
                        is_something_healed = true;
                }
        }

        if (actor::restore_hp(player, 3, actor::AllowRestoreAboveMax::no)) {
                is_something_healed = true;
        }

        if (!is_something_healed) {
                msg_log::add("I feel fine.");
        }

        identify(Verbose::yes);
}

void Opening::run_effect()
{
        bool is_any_opened = false;

        for (const auto& p : map::rect().positions()) {
                if (!map::g_seen.at(p)) {
                        continue;
                }

                const auto did_open =
                        spells::run_opening_spell_effect_at(
                                p,
                                SpellSkill::master);

                if (did_open == terrain::DidOpen::yes) {
                        is_any_opened = true;
                }
        }

        if (is_any_opened) {
                identify(Verbose::yes);
        }
}

void Bless::run_effect()
{
        const int nr_turns = rnd::range(8, 12);

        prop::Prop* prop = prop::make(prop::Id::blessed);

        prop->set_duration(nr_turns);

        map::g_player->m_properties.apply(prop);

        identify(Verbose::yes);
}

void CloudMinds::run_effect()
{
        msg_log::add("I vanish from the minds of my enemies.");

        for (auto* actor : game_time::g_actors) {
                if (actor::is_player(actor)) {
                        continue;
                }

                actor->m_mon_aware_state.aware_counter = 0;
                actor->m_mon_aware_state.wary_counter = 0;
        }

        identify(Verbose::yes);
}

void Shockwave::run_effect()
{
        msg_log::add("It triggers a shock wave around me.");

        const P& player_pos = map::g_player->m_pos;

        for (const P& d : dir_utils::g_dir_list) {
                const P p = player_pos + d;

                if (!map::is_pos_inside_outer_walls(p)) {
                        continue;
                }

                auto* const terrain = map::g_terrain.at(p);

                terrain->hit(DmgType::explosion, nullptr);
        }

        for (actor::Actor* actor : game_time::g_actors) {
                if (actor::is_player(actor) || !actor::is_alive(*actor)) {
                        continue;
                }

                const P& other_pos = actor->m_pos;

                const bool is_adj = is_pos_adj(player_pos, other_pos, false);

                if (!is_adj) {
                        continue;
                }

                if (actor::can_player_see_actor(*actor)) {
                        std::string msg =
                                text_format::first_to_upper(actor::name_the(*actor)) +
                                " is hit!";

                        msg = text_format::first_to_upper(msg);

                        msg_log::add(msg);
                }

                actor::hit(
                        *actor,
                        rnd::range(1, 6),
                        DmgType::explosion,
                        map::g_player);

                // Surived the damage? Knock the monster back
                if (actor::is_alive(*actor)) {
                        knockback::run(
                                *actor,
                                player_pos,
                                knockback::KnockbackSource::other,
                                Verbose::yes,
                                1);  // 1 extra turn paralyzed
                }
        }

        Snd snd("",
                audio::SfxId::END,
                IgnoreMsgIfOriginSeen::yes,
                player_pos,
                map::g_player,
                SndVol::high,
                AlertsMon::yes);

        snd.run();

        identify(Verbose::yes);
}

}  // namespace rod
