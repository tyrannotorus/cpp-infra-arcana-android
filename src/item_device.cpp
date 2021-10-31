// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
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
#include "actor_player.hpp"
#include "actor_see.hpp"
#include "array2.hpp"
#include "audio.hpp"
#include "audio_data.hpp"
#include "debug.hpp"
#include "direction.hpp"
#include "game.hpp"
#include "game_time.hpp"
#include "inventory.hpp"
#include "io.hpp"
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
#include "terrain_data.hpp"
#include "terrain_mob.hpp"
#include "text_format.hpp"

// -----------------------------------------------------------------------------
// device
// -----------------------------------------------------------------------------
namespace device
{
// -----------------------------------------------------------------------------
// Device
// -----------------------------------------------------------------------------
Device::Device(item::ItemData* const item_data) :
        Item(item_data) {}

void Device::identify(const Verbose verbose)
{
        if (m_data->is_identified)
        {
                return;
        }

        m_data->is_identified = true;

        if (verbose == Verbose::yes)
        {
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

// -----------------------------------------------------------------------------
// Strange device
// -----------------------------------------------------------------------------
StrangeDevice::StrangeDevice(item::ItemData* const item_data) :
        Device(item_data),
        condition(rnd::coin_toss() ? Condition::fine : Condition::shoddy) {}

void StrangeDevice::save_hook() const
{
        saving::put_int((int)condition);
}

void StrangeDevice::load_hook()
{
        condition = Condition(saving::get_int());
}

std::vector<std::string> StrangeDevice::descr_hook() const
{
        if (m_data->is_identified)
        {
                const std::string descr = descr_identified();

                std::vector<std::string> out = {descr};

                std::string cond_str = "It seems ";

                switch (condition)
                {
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
        else
        {
                // Not identified
                return m_data->base_descr;
        }
}

ConsumeItem StrangeDevice::activate(actor::Actor* const actor)
{
        ASSERT(actor);

        if (!m_data->is_identified)
        {
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

        if (actor->is_player() && player_bon::has_trait(Trait::elec_incl))
        {
                max += 2;
        }

        const int rnd = rnd::range(1, max);

        switch (condition)
        {
        case Condition::breaking:
        {
                should_warn = (rnd == 7) || (rnd == 8);
                should_hurt_user = (rnd == 5) || (rnd == 6);
                should_fail = (rnd == 3) || (rnd == 4);
                should_degrade = (rnd <= 2);
        }
        break;

        case Condition::shoddy:
        {
                should_warn = (rnd == 5) || (rnd == 6);
                should_hurt_user = (rnd == 4);
                should_fail = (rnd == 3);
                should_degrade = (rnd <= 2);
        }
        break;

        case Condition::fine:
        {
                should_warn = (rnd == 5) || (rnd == 6);
                should_degrade = (rnd <= 4);
        }
        break;
        }

        if (!map::g_player->is_alive())
        {
                return ConsumeItem::no;
        }

        if (!should_fail)
        {
                if (should_degrade)
                {
                        audio::play(audio::SfxId::strange_device_damaged);
                }
                else
                {
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

        if (should_hurt_user)
        {
                msg_log::add(
                        "It hits me with a jolt of electricity!",
                        colors::msg_bad());

                actor::hit(*actor, rnd::range(1, 3), DmgType::electric);
        }

        if (should_fail)
        {
                msg_log::add("It suddenly stops.");
        }
        else
        {
                consumed = run_effect();
        }

        if (consumed == ConsumeItem::no)
        {
                if (should_degrade)
                {
                        if (condition == Condition::breaking)
                        {
                                msg_log::add("The " + item_name + " breaks!");

                                consumed = ConsumeItem::yes;
                        }
                        else
                        {
                                msg_log::add(
                                        "The " +
                                        item_name +
                                        " makes a terrible grinding noise. "
                                        "I seem to have damaged it.");

                                condition = (Condition)((int)condition - 1);
                        }
                }

                if (should_warn)
                {
                        msg_log::add("The " + item_name + " hums ominously.");
                }
        }

        map::g_player->incr_shock(12.0, ShockSrc::use_strange_item);

        game_time::tick();

        return consumed;
}

std::string StrangeDevice::name_info_str() const
{
        if (m_data->is_identified)
        {
                switch (condition)
                {
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

        if (tgt_bucket.empty())
        {
                msg_log::add("It seems to peruse area.");
        }
        else
        {
                // Targets are available
                const std::unique_ptr<Spell> spell(
                        spells::make(SpellId::aza_gaze));

                spell->cast(
                        map::g_player,
                        SpellSkill::basic,
                        SpellSrc::item);
        }

        return ConsumeItem::no;
}

// -----------------------------------------------------------------------------
// Rejuvenator
// -----------------------------------------------------------------------------
ConsumeItem Rejuvenator::run_effect()
{
        msg_log::add("It repairs my body.");

        std::vector<PropId> props_can_heal = {
                PropId::blind,
                PropId::deaf,
                PropId::poisoned,
                PropId::infected,
                PropId::diseased,
                PropId::weakened,
                PropId::hp_sap,
                PropId::wound};

        for (PropId prop_id : props_can_heal)
        {
                map::g_player->m_properties.end_prop(prop_id);
        }

        map::g_player->restore_hp(999);

        map::g_player->incr_shock(50.0, ShockSrc::use_strange_item);

        return ConsumeItem::no;
}

// -----------------------------------------------------------------------------
// Translocator
// -----------------------------------------------------------------------------
ConsumeItem Translocator::run_effect()
{
        const auto seen_foes = actor::seen_foes(*map::g_player);

        if (seen_foes.empty())
        {
                msg_log::add("It seems to peruse area.");
        }
        else
        {
                // Seen targets are available
                for (auto* actor : seen_foes)
                {
                        msg_log::add(
                                text_format::first_to_upper(actor->name_the()) +
                                " is teleported.");

                        io::draw_blast_at_cells(
                                std::vector<P> {actor->m_pos},
                                colors::yellow());

                        teleport(*actor);
                }
        }

        return ConsumeItem::no;
}

// -----------------------------------------------------------------------------
// Sentry drone
// -----------------------------------------------------------------------------
ConsumeItem SentryDrone::run_effect()
{
        msg_log::add("The Sentry Drone awakens!");

        actor::spawn(
                map::g_player->m_pos,
                {actor::Id::sentry_drone},
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

        for (auto* const actor : game_time::g_actors)
        {
                if (actor->is_player())
                {
                        continue;
                }

                actor->m_properties.apply(property_factory::make(PropId::deaf));
        }

        return ConsumeItem::no;
}

// -----------------------------------------------------------------------------
// Force Field
// -----------------------------------------------------------------------------
ConsumeItem ForceField::run_effect()
{
        msg_log::add("The air thickens around me.");

        Range duration_range(85, 100);

        const int duration = duration_range.roll();

        const auto actors = map::get_actor_array();

        const auto blocked_parser =
                map_parsers::BlocksWalking(ParseActors::yes);

        const std::vector<terrain::Id> specific_allowed_terrains = {
                terrain::Id::chasm};

        const auto specific_allowed_terrains_parser =
                map_parsers::IsAnyOfTerrains(specific_allowed_terrains);

        for (const auto& d : dir_utils::g_dir_list)
        {
                const auto p = map::g_player->m_pos + d;

                if (blocked_parser.run(p) &&
                    !specific_allowed_terrains_parser.run(p))
                {
                        continue;
                }

                auto actors_here = actors.at(p);

                // Destroy corpses in cells with force fields
                for (auto* const actor : actors_here)
                {
                        if (actor->is_corpse())
                        {
                                actor->m_state = ActorState::destroyed;

                                map::make_blood(p);
                                map::make_gore(p);
                        }
                }

                game_time::add_mob(new terrain::ForceField(p, duration));
        }

        return ConsumeItem::no;
}

// -----------------------------------------------------------------------------
// Electric lantern
// -----------------------------------------------------------------------------
Lantern::Lantern(item::ItemData* const item_data) :
        Device(item_data),
        nr_turns_left(150),
        is_activated(false) {}

std::string Lantern::name_info_str() const
{
        std::string inf = "(" + std::to_string(nr_turns_left) + " turns";

        if (is_activated)
        {
                inf += ", Lit";
        }

        return inf + ")";
}

ConsumeItem Lantern::activate(actor::Actor* const actor)
{
        (void)actor;

        toggle();

        map::update_vision();

        game_time::tick();

        return ConsumeItem::no;
}

void Lantern::save_hook() const
{
        saving::put_int(nr_turns_left);
        saving::put_bool(is_activated);
}

void Lantern::load_hook()
{
        nr_turns_left = saving::get_int();
        is_activated = saving::get_bool();
}

void Lantern::on_pickup_hook()
{
        ASSERT(m_actor_carrying);

        // Check for existing electric lantern in inventory
        for (Item* const other : m_actor_carrying->m_inv.m_backpack)
        {
                if ((other == this) || (other->id() != id()))
                {
                        continue;
                }

                auto* other_lantern = static_cast<Lantern*>(other);

                other_lantern->nr_turns_left += nr_turns_left;

                m_actor_carrying->m_inv
                        .remove_item_in_backpack_with_ptr(this, true);

                return;
        }
}

void Lantern::toggle()
{
        const std::string toggle_str =
                is_activated
                ? "I turn off"
                : "I turn on";

        msg_log::add(toggle_str + " an Electric Lantern.");

        is_activated = !is_activated;

        // Discourage flipping on and off frequently
        if (is_activated && (nr_turns_left >= 4))
        {
                nr_turns_left -= 2;
        }

        audio::play(audio::SfxId::electric_lantern);
}

void Lantern::on_std_turn_in_inv_hook(const InvType inv_type)
{
        (void)inv_type;

        if (!is_activated)
        {
                return;
        }

        if (!(player_bon::has_trait(Trait::elec_incl) &&
              ((game_time::turn_nr() % 2) == 0)))
        {
                --nr_turns_left;
        }

        if (nr_turns_left <= 0)
        {
                msg_log::add(
                        "My Electric Lantern has expired.",
                        colors::msg_note(),
                        MsgInterruptPlayer::yes,
                        MorePromptOnMsg::yes);

                game::add_history_event("My Electric Lantern expired");

                // NOTE: The this deletes the object
                map::g_player->m_inv.remove_item_in_backpack_with_ptr(
                        this, true);
        }
}

LgtSize Lantern::lgt_size() const
{
        return is_activated
                ? LgtSize::fov
                : LgtSize::none;
}

}  // namespace device
