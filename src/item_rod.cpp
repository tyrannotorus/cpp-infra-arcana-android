// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
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
#include "explosion.hpp"
#include "game.hpp"
#include "game_time.hpp"
#include "item_data.hpp"
#include "item_factory.hpp"
#include "knockback.hpp"
#include "map.hpp"
#include "map_parsing.hpp"
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
#include "teleport.hpp"
#include "terrain.hpp"
#include "terrain_door.hpp"
#include "terrain_factory.hpp"
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

        s_rod_looks.push_back({"Iron", "an Iron", colors::gray()});
        s_rod_looks.push_back({"Zinc", "a Zinc", colors::light_white()});
        s_rod_looks.push_back({"Chromium", "a Chromium", colors::light_white()});
        s_rod_looks.push_back({"Tin", "a Tin", colors::light_white()});
        s_rod_looks.push_back({"Silver", "a Silver", colors::light_white()});
        s_rod_looks.push_back({"Golden", "a Golden", colors::yellow()});
        s_rod_looks.push_back({"Nickel", "a Nickel", colors::light_white()});
        s_rod_looks.push_back({"Copper", "a Copper", colors::brown()});
        s_rod_looks.push_back({"Lead", "a Lead", colors::gray()});
        s_rod_looks.push_back({"Tungsten", "a Tungsten", colors::white()});
        s_rod_looks.push_back({"Platinum", "a Platinum", colors::light_white()});
        s_rod_looks.push_back({"Lithium", "a Lithium", colors::white()});
        s_rod_looks.push_back({"Zirconium", "a Zirconium", colors::white()});
        s_rod_looks.push_back({"Gallium", "a Gallium", colors::light_white()});
        s_rod_looks.push_back({"Cobalt", "a Cobalt", colors::light_blue()});
        s_rod_looks.push_back({"Titanium", "a Titanium", colors::light_white()});
        s_rod_looks.push_back({"Magnesium", "a Magnesium", colors::white()});

        for (item::ItemData& d : item::g_data) {
                if (d.type != ItemType::rod) {
                        continue;
                }

                // Color and false name
                const size_t idx = rnd::range(0, (int)s_rod_looks.size() - 1);

                RodLook& look = s_rod_looks[idx];

                d.base_name_un_id.names[(size_t)ItemNameType::plain] = look.name_plain + " Rod";
                d.base_name_un_id.names[(size_t)ItemNameType::plural] = look.name_plain + " Rods";
                d.base_name_un_id.names[(size_t)ItemNameType::a] = look.name_a + " Rod";

                d.color = look.color;

                s_rod_looks.erase(std::begin(s_rod_looks) + (int)idx);

                // True name
                std::unique_ptr<const Rod> tmp_rod(
                        static_cast<const Rod*>(
                                item::make(d.id, 1)));

                const std::string real_type_name = tmp_rod->real_name();

                const std::string real_name = "Rod of " + real_type_name;
                const std::string real_name_plural = "Rods of " + real_type_name;
                const std::string real_name_a = "a Rod of " + real_type_name;

                d.base_name.names[(size_t)ItemNameType::plain] = real_name;
                d.base_name.names[(size_t)ItemNameType::plural] = real_name_plural;
                d.base_name.names[(size_t)ItemNameType::a] = real_name_a;
        }

        TRACE_FUNC_END;
}

void save()
{
        for (int i = 0; i < (int)item::Id::END; ++i) {
                item::ItemData& d = item::g_data[i];

                if (d.type != ItemType::rod) {
                        continue;
                }

                saving::put_str(d.base_name_un_id.names[(size_t)ItemNameType::plain]);
                saving::put_str(d.base_name_un_id.names[(size_t)ItemNameType::plural]);
                saving::put_str(d.base_name_un_id.names[(size_t)ItemNameType::a]);
                saving::put_str(colors::color_to_name(d.color));
        }
}

void load()
{
        for (int i = 0; i < (int)item::Id::END; ++i) {
                item::ItemData& d = item::g_data[i];

                if (d.type != ItemType::rod) {
                        continue;
                }

                d.base_name_un_id.names[(size_t)ItemNameType::plain] = saving::get_str();
                d.base_name_un_id.names[(size_t)ItemNameType::plural] = saving::get_str();
                d.base_name_un_id.names[(size_t)ItemNameType::a] = saving::get_str();
                d.color = colors::name_to_color(saving::get_str());
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

Color Rod::interface_color() const
{
        return colors::violet();
}

int Rod::nr_turns_to_recharge() const
{
        return 250;
}

ConsumeItem Rod::activate(actor::Actor* const actor)
{
        (void)actor;

        // Prevent using it if still charging, and identified (player character knows that it's
        // useless)
        if ((m_nr_charge_turns_left > 0) && m_data->is_identified) {
                const std::string rod_name = name(ItemNameType::plain, ItemNameInfo::none);

                msg_log::add("The " + rod_name + " is still charging.");

                return ConsumeItem::no;
        }

        m_data->is_tried = true;

        // TODO: Sfx

        const std::string rod_name_a = name(ItemNameType::a, ItemNameInfo::none);

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

        if ((m_nr_charge_turns_left == 0) && m_data->is_identified) {
                const std::string my_name = name(ItemNameType::plain, ItemNameInfo::none);

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
                const std::string name_after = name(ItemNameType::a, ItemNameInfo::none);

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

std::string Opening::real_name() const
{
        return "Opening";
}

std::string Opening::descr_identified() const
{
        return (
                "When activated, this device opens all locks, lids and "
                "doors in the surrounding area (except heavy doors "
                "operated externally by a switch).");
}

void Opening::run_effect()
{
        bool is_any_opened = false;

        for (const P& p : map::rect().positions()) {
                if (!map::g_seen.at(p)) {
                        continue;
                }

                const terrain::DidOpen did_open =
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

std::string CloudMinds::real_name() const
{
        return "Cloud Minds";
}

std::string CloudMinds::descr_identified() const
{
        return (
                "When activated, this device clouds the memories of "
                "all creatures in the area, causing them to forget "
                "the presence of the user.");
}

int CloudMinds::nr_turns_to_recharge() const
{
        return 90;
}

void CloudMinds::run_effect()
{
        msg_log::add("I vanish from the minds of my enemies.");

        for (actor::Actor* actor : game_time::g_actors) {
                if (actor::is_player(actor)) {
                        continue;
                }

                actor->m_mon_aware_state.aware_counter = 0;
                actor->m_mon_aware_state.wary_counter = 0;
        }

        identify(Verbose::yes);
}

std::string Shockwave::real_name() const
{
        return "Shockwave";
}

std::string Shockwave::descr_identified() const
{
        return (
                "When activated, this device generates a shock wave "
                "which violently pushes away any adjacent creatures "
                "and destroys structures.");
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

                terrain::Terrain* const terrain = map::g_terrain.at(p);

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

                // Surived the damage? Knock the monster back.
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

std::string Deafening::real_name() const
{
        return "Deafening";
}

std::string Deafening::descr_identified() const
{
        return (
                "When activated, this device causes temporary deafness in "
                "all creatures in a large area, except for the user.");
}

void Deafening::run_effect()
{
        bool is_any_seen = false;

        const int max_dist = rnd::range(g_fov_radi_int * 2, g_fov_radi_int * 3);

        for (actor::Actor* const actor : game_time::g_actors) {
                if (actor::is_player(actor) ||
                    !actor::is_alive(*actor) ||
                    (king_dist(map::g_player->m_pos, actor->m_pos) > max_dist)) {
                        continue;
                }

                actor->m_properties.apply(prop::make(prop::Id::deaf));

                if (actor::can_player_see_actor(*actor)) {
                        is_any_seen = true;
                }
        }

        if (is_any_seen) {
                identify(Verbose::yes);
        }
}

std::string DoorCreation::real_name() const
{
        return "Gateways";
}

std::string DoorCreation::descr_identified() const
{
        return (
                "When activated, this device materializes an entryway somewhere "
                "in an adjacent surface.");
}

void DoorCreation::run_effect()
{
        std::vector<P> pos_bucket;

        map_parsers::IsAnyOfTerrains parser(
                {
                        terrain::Id::wall,
                        terrain::Id::rubble_high,
                });

        for (const P& offset : dir_utils::g_dir_list) {
                const P pos(map::g_player->m_pos + offset);

                if (map::is_pos_inside_outer_walls(pos) && parser.run(pos)) {
                        pos_bucket.push_back(pos);
                }
        }

        if (pos_bucket.empty()) {
                return;
        }

        const P pos = rnd::element(pos_bucket);

        const bool is_seen = map::g_seen.at(pos);

        if (is_seen) {
                msg_log::add("A door appears!");
        }

        auto* const door =
                static_cast<terrain::Door*>(
                        terrain::make(terrain::Id::door, pos));

        door->init_type_and_state(terrain::DoorType::wood, terrain::DoorSpawnState::closed);

        map::update_terrain(door);

        if (is_seen) {
                identify(Verbose::yes);
        }
}

std::string Unbinding::real_name() const
{
        return "Unbinding";
}

std::string Unbinding::descr_identified() const
{
        return (
                "When activated, this device breaks the user free from any bonds "
                "(entangled, stuck, nailed), and ends slowing.");
}

void Unbinding::run_effect()
{
        std::vector<prop::Id> props_ended = {
                prop::Id::slowed,
                prop::Id::entangled,
                prop::Id::stuck,
                prop::Id::nailed,
        };

        const bool is_identified =
                std::any_of(
                        std::cbegin(props_ended),
                        std::cend(props_ended),
                        [](const prop::Id id) {
                                return map::g_player->m_properties.has(id);
                        });

        std::for_each(
                std::cbegin(props_ended),
                std::cend(props_ended),
                [](const prop::Id id) {
                        map::g_player->m_properties.end_prop(id);
                });

        if (is_identified) {
                identify(Verbose::yes);
        }
}

std::string Mist::real_name() const
{
        return "Mist";
}

std::string Mist::descr_identified() const
{
        return (
                "When activated, this device alters the atmosphere in order to "
                "cover the user in a dense mist. "
                "The mist is also strangely soothing (-10% temporary shock).");
}

void Mist::run_effect()
{
        msg_log::add("A shroud of vapor envelops me.");

        explosion::run_mist_explosion_at(map::g_player->m_pos);

        identify(Verbose::yes);
}

std::string MiGoHypno::real_name() const
{
        return "Sleep";
}

std::string MiGoHypno::descr_identified() const
{
        return (
                "When activated, this device selects a single nearby seen creature, "
                "and attempts to hypnotize it, putting them to sleep if susceptible. "
                "It is unknown how the target selection works.");
}

void MiGoHypno::run_effect()
{
        const int max_dist = g_fov_radi_int;

        std::vector<actor::Actor*> target_bucket;

        std::vector<actor::Actor*> seen_foes = actor::seen_foes(*map::g_player);

        for (actor::Actor* const actor : seen_foes) {
                if (actor::is_player(actor) ||
                    !actor::is_alive(*actor) ||
                    (king_dist(map::g_player->m_pos, actor->m_pos) > max_dist)) {
                        continue;
                }

                target_bucket.push_back(actor);
        }

        if (target_bucket.empty()) {
                return;
        }

        actor::Actor* const target = rnd::element(target_bucket);

        spells::run_mi_go_hypno_effect(*target);

        if (actor::can_player_see_actor(*target)) {
                identify(Verbose::yes);
        }
}

std::string Displacement::real_name() const
{
        return "Displacement";
}

std::string Displacement::descr_identified() const
{
        return "When activated, this device moves the user a short distance.";
}

void Displacement::run_effect()
{
        const int max_dist = g_fov_radi_int - 1;

        teleport(*map::g_player, ShouldCtrlTele::never, max_dist);

        identify(Verbose::yes);
}

}  // namespace rod
