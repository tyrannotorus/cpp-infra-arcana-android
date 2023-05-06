// =============================================================================
// Copyright 2011-2023 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "item_device.hpp"

#include <algorithm>
#include <memory>
#include <vector>

#include "actor.hpp"
#include "actor_data.hpp"
#include "actor_factory.hpp"
#include "actor_hit.hpp"
#include "actor_see.hpp"
#include "array2.hpp"
#include "audio.hpp"
#include "audio_data.hpp"
#include "debug.hpp"
#include "direction.hpp"
#include "draw_blast.hpp"
#include "game.hpp"
#include "game_time.hpp"
#include "inventory.hpp"
#include "item_data.hpp"
#include "map.hpp"
#include "map_parsing.hpp"
#include "msg_log.hpp"
#include "player_bon.hpp"
#include "pos.hpp"
#include "property_data.hpp"
#include "property_factory.hpp"
#include "property_handler.hpp"
#include "random.hpp"
#include "saving.hpp"
#include "spells.hpp"
#include "teleport.hpp"
#include "terrain.hpp"
#include "terrain_data.hpp"
#include "terrain_factory.hpp"
#include "terrain_mob.hpp"
#include "text_format.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static Array2<std::vector<actor::Actor*>> get_actor_array()
{
        Array2<std::vector<actor::Actor*>> a(map::dims());

        for (auto* actor : game_time::g_actors) {
                const auto& p = actor->m_pos;

                a.at(p).push_back(actor);
        }

        return a;
}

// -----------------------------------------------------------------------------
// device
// -----------------------------------------------------------------------------
namespace device
{
// -----------------------------------------------------------------------------
// Device
// -----------------------------------------------------------------------------
Device::Device(item::ItemData* const item_data) :
        Item(item_data),
        m_condition(rnd::coin_toss() ? Condition::fine : Condition::shoddy) {}

void Device::identify(const Verbose verbose)
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

                msg_log::add("All its properties are now known to me.");

                game::add_history_event("Comprehended " + name_after);

                game::incr_player_xp(15);
        }
}

void Device::save_hook() const
{
        saving::put_int((int)m_condition);
}

void Device::load_hook()
{
        m_condition = (Condition)saving::get_int();
}

std::vector<std::string> Device::descr_hook() const
{
        if (m_data->is_identified) {
                const std::string descr = descr_identified();

                std::vector<std::string> out = {descr};

                std::string cond_str = "It seems ";

                switch (m_condition) {
                case Condition::fine:
                        cond_str += "to be in fine condition.";
                        break;

                case Condition::shoddy:
                        cond_str += "to be in shoddy condition.";
                        break;

                case Condition::breaking:
                        cond_str += "almost broken.";
                        break;
                }

                out.push_back(cond_str);

                return out;
        }
        else {
                // Not identified
                return m_data->base_descr;
        }
}

ConsumeItem Device::activate(actor::Actor* const actor)
{
        ASSERT(actor);

        if (!m_data->is_identified) {
                msg_log::add(
                        "This device is completely alien to me, I could never "
                        "understand it through normal means.");

                return ConsumeItem::no;
        }

        bool should_warn = false;
        bool should_hurt_user = false;
        bool should_fail = false;
        bool should_degrade = false;

        int max = 8;

        if (actor::is_player(actor) &&
            player_bon::has_trait(Trait::elec_incl)) {
                max += 2;
        }

        const int rnd = rnd::range(1, max);

        switch (m_condition) {
        case Condition::breaking: {
                should_warn = (rnd == 7) || (rnd == 8);
                should_hurt_user = (rnd == 5) || (rnd == 6);
                should_fail = (rnd == 3) || (rnd == 4);
                should_degrade = (rnd <= 2);
        } break;

        case Condition::shoddy: {
                should_warn = (rnd == 5) || (rnd == 6);
                should_hurt_user = (rnd == 4);
                should_fail = (rnd == 3);
                should_degrade = (rnd <= 2);
        } break;

        case Condition::fine: {
                should_warn = (rnd == 5) || (rnd == 6);
                should_degrade = (rnd <= 4);
        } break;
        }

        if (!map::g_player->is_alive()) {
                return ConsumeItem::no;
        }

        if (!should_fail) {
                if (should_degrade) {
                        audio::play(audio::SfxId::strange_device_damaged);
                }
                else {
                        audio::play(audio::SfxId::strange_device_activate);
                }
        }

        const std::string item_name =
                name(
                        ItemNameType::plain,
                        ItemNameInfo::none);

        const std::string item_name_a =
                name(
                        ItemNameType::a,
                        ItemNameInfo::none);

        msg_log::add("I activate " + item_name_a + "...");

        ConsumeItem consumed = ConsumeItem::no;

        if (should_hurt_user) {
                msg_log::add(
                        "It hits me with a jolt of electricity!",
                        colors::msg_bad());

                actor::hit(
                        *actor,
                        rnd::range(1, 3),
                        DmgType::electric,
                        nullptr);
        }

        if (should_fail) {
                msg_log::add("It suddenly stops.");
        }
        else {
                consumed = run_effect();
        }

        if (consumed == ConsumeItem::no) {
                if (should_degrade) {
                        if (m_condition == Condition::breaking) {
                                msg_log::add("The " + item_name + " breaks!");

                                consumed = ConsumeItem::yes;
                        }
                        else {
                                msg_log::add(
                                        "The " +
                                        item_name +
                                        " makes a terrible grinding noise. "
                                        "I seem to have damaged it.");

                                m_condition = (Condition)((int)m_condition - 1);
                        }
                }

                if (should_warn) {
                        msg_log::add("The " + item_name + " hums ominously.");
                }
        }

        map::g_player->incr_shock(12.0, ShockSrc::use_strange_item);

        game_time::tick();

        return consumed;
}

std::string Device::name_info_str() const
{
        if (m_data->is_identified) {
                switch (m_condition) {
                case Condition::breaking:
                        return "(breaking)";

                case Condition::shoddy:
                        return "(shoddy)";

                case Condition::fine:
                        return "(fine)";
                }
        }

        return "";
}

// -----------------------------------------------------------------------------
// Blaster
// -----------------------------------------------------------------------------
ConsumeItem Blaster::run_effect()
{
        const auto tgt_bucket = actor::seen_foes(*map::g_player);

        if (tgt_bucket.empty()) {
                msg_log::add("It seems to peruse area.");

                return ConsumeItem::no;
        }

        // Targets are available
        const std::unique_ptr<Spell> spell(spells::make(SpellId::darkbolt));

        const auto seen_foes = actor::seen_foes(*map::g_player);

        spell->cast(
                map::g_player,
                SpellSkill::expert,
                SpellSrc::item,
                seen_foes);

        return ConsumeItem::no;
}

std::string Blaster::descr_identified() const
{
        return (
                "When activated, this device blasts one visible hostile "
                "creature with infernal power.");
}

// -----------------------------------------------------------------------------
// Rejuvenator
// -----------------------------------------------------------------------------
ConsumeItem Rejuvenator::run_effect()
{
        msg_log::add("It repairs my body.");

        std::vector<prop::Id> props_can_heal = {
                prop::Id::blind,
                prop::Id::deaf,
                prop::Id::poisoned,
                prop::Id::infected,
                prop::Id::diseased,
                prop::Id::weakened,
                prop::Id::hp_sap,
                prop::Id::wound};

        for (prop::Id prop_id : props_can_heal) {
                map::g_player->m_properties.end_prop(prop_id);
        }

        map::g_player->restore_hp(999);

        map::g_player->incr_shock(50.0, ShockSrc::use_strange_item);

        return ConsumeItem::no;
}

std::string Rejuvenator::descr_identified() const
{
        return (
                "When activated, this device heals all wounds and physical "
                "maladies. The procedure is very painful and invasive "
                "however, and causes great shock to the user.");
}

// -----------------------------------------------------------------------------
// Translocator
// -----------------------------------------------------------------------------
ConsumeItem Translocator::run_effect()
{
        const auto seen_foes = actor::seen_foes(*map::g_player);

        if (seen_foes.empty()) {
                msg_log::add("It seems to peruse area.");
        }
        else {
                // Seen targets are available
                for (auto* actor : seen_foes) {
                        msg_log::add(
                                text_format::first_to_upper(actor->name_the()) +
                                " is teleported.");

                        draw_blast_at_cells(
                                std::vector<P> {actor->m_pos},
                                colors::yellow());

                        teleport(*actor);
                }
        }

        return ConsumeItem::no;
}

std::string Translocator::descr_identified() const
{
        return (
                "When activated, this device teleports all visible enemies "
                "to different locations.");
}

// -----------------------------------------------------------------------------
// Sentry drone
// -----------------------------------------------------------------------------
ConsumeItem SentryDrone::run_effect()
{
        msg_log::add("The Sentry Drone awakens!");

        actor::spawn(
                map::g_player->m_pos,
                {"MON_SENTRY_DRONE"},
                map::rect())
                .make_aware_of_player()
                .set_leader(map::g_player);

        return ConsumeItem::yes;
}

// -----------------------------------------------------------------------------
// Deafening
// -----------------------------------------------------------------------------
ConsumeItem Deafening::run_effect()
{
        msg_log::add("The device emits a piercing resonance.");

        for (auto* const actor : game_time::g_actors) {
                if (actor::is_player(actor)) {
                        continue;
                }

                actor->m_properties.apply(prop::make(prop::Id::deaf));
        }

        return ConsumeItem::no;
}

std::string Deafening::descr_identified() const
{
        return (
                "When activated, this device causes temporary deafness in "
                "all creatures in a large area (on the whole map), except "
                "for the user.");
}

// -----------------------------------------------------------------------------
// Force Field
// -----------------------------------------------------------------------------
ConsumeItem ForceField::run_effect()
{
        msg_log::add("The air thickens around me.");

        Range duration_range(85, 100);

        const int duration = duration_range.roll();

        const auto actors = get_actor_array();

        const auto blocked_parser =
                map_parsers::BlocksWalking(ParseActors::yes);

        const std::vector<terrain::Id> specific_allowed_terrains = {
                terrain::Id::chasm};

        const auto specific_allowed_terrains_parser =
                map_parsers::IsAnyOfTerrains(specific_allowed_terrains);

        for (const auto& d : dir_utils::g_dir_list) {
                const auto p = map::g_player->m_pos + d;

                if (blocked_parser.run(p) &&
                    !specific_allowed_terrains_parser.run(p)) {
                        continue;
                }

                auto actors_here = actors.at(p);

                // Destroy corpses in cells with force fields
                for (auto* const actor : actors_here) {
                        if (actor->is_corpse()) {
                                actor->m_state = ActorState::destroyed;

                                terrain::make_blood(p);
                                terrain::make_gore(p);
                        }
                }

                auto* const force_field =
                        static_cast<terrain::ForceField*>(
                                terrain::make(
                                        terrain::Id::force_field,
                                        p));

                force_field->set_nr_turns(duration);

                game_time::add_mob(force_field);
        }

        return ConsumeItem::no;
}

std::string ForceField::descr_identified() const
{
        return (
                "When activated, this device constructs a temporary opaque "
                "barrier around the user, blocking all physical matter. "
                "The barrier can only be created in empty spaces "
                "(i.e. not in spaces occupied by creatures, walls, etc).");
}

}  // namespace device
