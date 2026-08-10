// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "item_misc.hpp"

#include <memory>
#include <optional>
#include <vector>

#include "actor.hpp"
#include "actor_player_state.hpp"
#include "actor_see.hpp"
#include "array2.hpp"
#include "audio.hpp"
#include "audio_data.hpp"
#include "colors.hpp"
#include "common_text.hpp"
#include "config.hpp"
#include "create_character.hpp"
#include "debug.hpp"
#include "fade.hpp"
#include "game.hpp"
#include "game_over.hpp"
#include "game_time.hpp"
#include "inventory.hpp"
#include "map.hpp"
#include "msg_log.hpp"
#include "player_bon.hpp"
#include "popup.hpp"
#include "pos.hpp"
#include "property.hpp"
#include "property_data.hpp"
#include "property_factory.hpp"
#include "property_handler.hpp"
#include "query.hpp"
#include "random.hpp"
#include "saving.hpp"
#include "state.hpp"
#include "terrain.hpp"
#include "terrain_data.hpp"
#include "terrain_trap.hpp"
#include "text_format.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// item
// -----------------------------------------------------------------------------
namespace item
{
struct ItemData;

Trapezohedron::Trapezohedron(ItemData* item_data) :
        Item(item_data)
{
}

ItemPrePickResult Trapezohedron::pre_pickup_hook()
{
        game::add_history_event("Beheld The Shining Trapezohedron");

        // The world goes out before the ending is told. NOTE: This is
        // drawn over the MAP, so it happens while the game state is still
        // the one on top - before any of the popping and pushing below.
        fade::to_black();

        saving::erase_save();

        states::pop();

        game::set_is_win();

        on_game_over();

        states::push(std::make_unique<WinGameState>());

        return ItemPrePickResult::do_nothing;
}

MedicalBag::MedicalBag(ItemData* const item_data) :
        Item(item_data) {}

void MedicalBag::save_hook() const
{
        saving::put_int(m_nr_supplies);
}

void MedicalBag::load_hook()
{
        m_nr_supplies = saving::get_int();
}

void MedicalBag::randomize_nr_supplies()
{
        const int nr_supplies_max = m_max_starting_supplies;
        const int nr_supplies_min = nr_supplies_max - (nr_supplies_max / 3);

        m_nr_supplies = rnd::range(nr_supplies_min, nr_supplies_max);
}

void MedicalBag::on_pickup_hook()
{
        ASSERT(m_actor_carrying);

        if (!actor::is_player(m_actor_carrying)) {
                return;
        }

        // Check for existing medical bag in inventory
        for (Item* const other : m_actor_carrying->m_inv.m_backpack) {
                if ((other != this) &&
                    (other->id() == id())) {
                        static_cast<MedicalBag*>(other)->m_nr_supplies +=
                                m_nr_supplies;

                        m_actor_carrying->m_inv
                                .remove_item_in_backpack_with_ptr(this, true);

                        return;
                }
        }
}

ConsumeItem MedicalBag::activate(actor::Actor* const actor)
{
        (void)actor;

        if (player_bon::bg() == Bg::ghoul) {
                msg_log::add("It is of no use to me.");

                m_current_action = MedBagAction::END;

                return ConsumeItem::no;
        }

        // OK, we are allowed to use the medical bag

        if (config::is_medical_bag_auto_choice()) {
                m_current_action = choose_action_auto();
        }
        else {
                m_current_action = choose_action_manual();
        }

        if (m_current_action == MedBagAction::END) {
                return ConsumeItem::no;
        }

        const int nr_supplies_needed = tot_suppl_for_action(m_current_action);
        const bool is_enough_supplies = m_nr_supplies >= nr_supplies_needed;

        if (!is_enough_supplies) {
                msg_log::add("I do not have enough medical supplies.");

                m_current_action = MedBagAction::END;

                return ConsumeItem::no;
        }

        // Action can be done
        actor::player_state::g_active_medical_bag = this;

        m_nr_turns_left_action = tot_turns_for_action(m_current_action);

        std::string start_msg;

        switch (m_current_action) {
        case MedBagAction::quick_patch_up:
                start_msg = "I patch up some minor injuries.";
                break;

        case MedBagAction::treat_wound:
                start_msg = "I start treating a wound";
                break;

        case MedBagAction::sanitize_infection:
                start_msg = "I start to sanitize an infection";
                break;

        case MedBagAction::END:
                ASSERT(false);
                break;
        }

        start_msg +=
                " (" +
                std::to_string(m_nr_turns_left_action) +
                " turns)...";

        msg_log::add(start_msg);

        game_time::tick();

        return ConsumeItem::no;
}

MedBagAction MedicalBag::choose_action_auto() const
{
        // Infection?
        if (map::g_player->m_properties.has(prop::Id::infected)) {
                return MedBagAction::sanitize_infection;
        }

        // Wound?
        if (map::g_player->m_properties.has(prop::Id::wound)) {
                return MedBagAction::treat_wound;
        }

        // Quick patch-up?
        if (map::g_player->m_hp < actor::max_hp(*map::g_player)) {
                return MedBagAction::quick_patch_up;
        }

        msg_log::clear();

        msg_log::add("There is nothing to treat.");

        return MedBagAction::END;
}

MedBagAction MedicalBag::choose_action_manual() const
{
        auto cost_info_hint = [this](const MedBagAction action) {
                const int cost = tot_suppl_for_action(action);
                const int turns = tot_turns_for_action(action);

                return (
                        "(supplies: " +
                        std::to_string(cost) +
                        ", turns: " +
                        std::to_string(turns) +
                        ")");
        };

        const std::vector<std::string> choices = {
                ("(q) Quick patch-up, restores " +
                 std::to_string(m_hp_restored_by_quick_patch_up) +
                 " hit points " +
                 cost_info_hint(MedBagAction::quick_patch_up)),
                "(s) Sanitize infection " + cost_info_hint(MedBagAction::sanitize_infection),
                "(w) Treat wound " + cost_info_hint(MedBagAction::treat_wound),
        };

        int choice = -1;

        const std::string title = "Select treatment";

        popup::Popup(popup::AddToMsgHistory::no)
                .set_title(title)
                .set_msg("Available supplies: " + std::to_string(m_nr_supplies))
                .setup_menu_mode(
                        choices,
                        &choice)
                .run();

        switch (choice) {
        case 0: {
                if (map::g_player->m_hp < actor::max_hp(*map::g_player)) {
                        return MedBagAction::quick_patch_up;
                }
                else {
                        msg_log::add("There are no minor injuries to patch up.");
                }
        } break;

        case 1:
                if (map::g_player->m_properties.has(prop::Id::infected)) {
                        return MedBagAction::sanitize_infection;
                }
                else {
                        msg_log::add("I am not infected.");
                }
                break;

        case 2:
                if (map::g_player->m_properties.has(prop::Id::wound)) {
                        return MedBagAction::treat_wound;
                }
                else {
                        msg_log::add("I have no wounds to treat.");
                }
                break;
        }

        return MedBagAction::END;
}

void MedicalBag::continue_action()
{
        ASSERT(m_current_action != MedBagAction::END);

        // Check if current action should be stopped.
        switch (m_current_action) {
        case MedBagAction::quick_patch_up: {
                if (map::g_player->m_hp >= actor::max_hp(*map::g_player)) {
                        // Player already has full HP, presumably they were
                        // healed by something else, or regenerated health.
                        stop_action();

                        return;
                }
        } break;

        case MedBagAction::treat_wound: {
                if (!map::g_player->m_properties.has(prop::Id::wound)) {
                        // Player is no longer wounded, presumably they were
                        // healed by something else.
                        stop_action();

                        return;
                }
        } break;

        case MedBagAction::sanitize_infection: {
                if (!map::g_player->m_properties.has(prop::Id::infected)) {
                        // Player is no longer infected, presumably it was
                        // healed by something else.
                        stop_action();

                        return;
                }
        } break;

        case MedBagAction::END: {
        } break;
        }

        --m_nr_turns_left_action;

        if (m_nr_turns_left_action <= 0) {
                finish_current_action();
        }
        else {
                // Time still remaining on the current action
                game_time::tick();
        }
}

void MedicalBag::finish_current_action()
{
        actor::player_state::g_active_medical_bag = nullptr;

        switch (m_current_action) {
        case MedBagAction::quick_patch_up: {
                actor::restore_hp(*map::g_player, m_hp_restored_by_quick_patch_up);
        } break;

        case MedBagAction::treat_wound: {
                auto* const prop =
                        map::g_player->m_properties.prop(prop::Id::wound);

                if (!prop) {
                        ASSERT(false);

                        stop_action();

                        return;
                }

                auto* const wound = static_cast<prop::Wound*>(prop);

                wound->heal_one_wound();
        } break;

        case MedBagAction::sanitize_infection: {
                if (!map::g_player->m_properties.has(prop::Id::infected)) {
                        ASSERT(false);

                        stop_action();

                        return;
                }

                map::g_player->m_properties.end_prop(prop::Id::infected);
        } break;

        case MedBagAction::END:
                ASSERT(false);
                break;
        }

        m_nr_supplies -= tot_suppl_for_action(m_current_action);

        m_current_action = MedBagAction::END;

        if (m_nr_supplies <= 0) {
                map::g_player->m_inv.remove_item_in_backpack_with_ptr(this, true);

                game::add_history_event("Ran out of medical supplies");
        }
}

void MedicalBag::interrupted(const ForceInterruptActions is_forced)
{
        bool should_continue = true;

        if (is_forced == ForceInterruptActions::no) {
                // Query interruption.
                const std::string item_name =
                        name(
                                ItemNameType::plain,
                                ItemNameInfo::none);

                const std::string msg =
                        "Continue using " +
                        item_name +
                        " (" +
                        std::to_string(m_nr_turns_left_action) +
                        " turns left)? " +
                        common_text::g_yes_or_no_hint;

                msg_log::add(
                        msg,
                        colors::light_white(),
                        MsgInterruptPlayer::no,
                        MorePromptOnMsg::no,
                        CopyToMsgHistory::no);

                const BinaryAnswer query_result =
                        query::yes_or_no(
                                std::nullopt,
                                AllowSpaceCancel::no);

                should_continue = (query_result == BinaryAnswer::yes);

                msg_log::clear();
        }
        else {
                // Forced interruption.
                msg_log::add("My healing is disrupted.");

                should_continue = false;
        }

        if (!should_continue) {
                stop_action();
        }
}

int MedicalBag::tot_suppl_for_action(const MedBagAction action) const
{
        int cost = 0;

        switch (action) {
        case MedBagAction::quick_patch_up:
                cost = 2;
                break;

        case MedBagAction::treat_wound:
                cost = 8;
                break;

        case MedBagAction::sanitize_infection:
                cost = 2;
                break;

        case MedBagAction::END:
                ASSERT(false);
                break;
        }

        if (player_bon::has_trait(Trait::healer)) {
                cost /= 2;
        }

        return cost;
}

int MedicalBag::tot_turns_for_action(const MedBagAction action) const
{
        int nr_turns = 0;

        switch (action) {
        case MedBagAction::quick_patch_up:
                nr_turns = 4;
                break;

        case MedBagAction::treat_wound:
                nr_turns = 16;
                break;

        case MedBagAction::sanitize_infection:
                nr_turns = 8;
                break;

        case MedBagAction::END:
                ASSERT(false);
                break;
        }

        if (player_bon::has_trait(Trait::healer)) {
                nr_turns /= 2;
        }

        return nr_turns;
}

std::string MedicalBag::name_info_str(const ItemNameIdentified id_type) const
{
        (void)id_type;

        return "(" + std::to_string(m_nr_supplies) + " supplies)";
}

void MedicalBag::stop_action()
{
        m_nr_turns_left_action = -1;
        m_current_action = MedBagAction::END;

        actor::player_state::g_active_medical_bag = nullptr;
}

Lantern::Lantern(item::ItemData* const item_data) :
        Item(item_data) {}

void Lantern::randomize_duration()
{
        const int duration_max = m_max_turns_left;
        const int duration_min = duration_max / 2;

        m_nr_turns_left = rnd::range(duration_min, duration_max);
}

std::string Lantern::name_info_str(const ItemNameIdentified id_type) const
{
        (void)id_type;

        std::string inf = "(" + std::to_string(m_nr_turns_left) + " turns";

        if (m_is_activated) {
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
        saving::put_int(m_nr_turns_left);
        saving::put_bool(m_is_activated);
}

void Lantern::load_hook()
{
        m_nr_turns_left = saving::get_int();
        m_is_activated = saving::get_bool();
}

void Lantern::on_pickup_hook()
{
        ASSERT(m_actor_carrying);

        // Check for existing electric lantern in inventory
        for (Item* const other : m_actor_carrying->m_inv.m_backpack) {
                if ((other == this) || (other->id() != id())) {
                        continue;
                }

                auto* other_lantern = static_cast<Lantern*>(other);

                other_lantern->m_nr_turns_left += m_nr_turns_left;

                m_actor_carrying->m_inv
                        .remove_item_in_backpack_with_ptr(this, true);

                return;
        }
}

void Lantern::toggle()
{
        const std::string toggle_str =
                m_is_activated
                ? "I turn off"
                : "I turn on";

        msg_log::add(toggle_str + " an Electric Lantern.");

        m_is_activated = !m_is_activated;

        // Discourage flipping on and off frequently
        if (m_is_activated && (m_nr_turns_left >= 4)) {
                m_nr_turns_left -= 2;
        }

        audio::play(audio::SfxId::electric_lantern);
}

void Lantern::on_std_turn_in_inv_hook(const InvType inv_type)
{
        (void)inv_type;

        if (!m_is_activated) {
                return;
        }

        if (!(player_bon::has_trait(Trait::elec_incl) &&
              ((game_time::turn_nr() % 2) == 0))) {
                --m_nr_turns_left;
        }

        if (m_nr_turns_left <= 0) {
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

LightSize Lantern::light_size() const
{
        return m_is_activated ? LightSize::fov : LightSize::none;
}

ReflTalisman::ReflTalisman(ItemData* const item_data) :
        Item(item_data)
{
}

void ReflTalisman::on_pickup_hook()
{
        auto* prop = prop::make(prop::Id::spell_reflect);

        prop->set_indefinite();

        add_carrier_prop(prop, Verbose::no);
}

void ReflTalisman::on_removed_from_inv_hook()
{
        clear_carrier_props();
}

ResurrectTalisman::ResurrectTalisman(ItemData* const item_data) :
        Item(item_data)
{
}

TeleCtrlTalisman::TeleCtrlTalisman(ItemData* const item_data) :
        Item(item_data)
{
}

void TeleCtrlTalisman::on_pickup_hook()
{
        auto* prop = prop::make(prop::Id::tele_ctrl);

        prop->set_indefinite();

        add_carrier_prop(prop, Verbose::no);
}

void TeleCtrlTalisman::on_removed_from_inv_hook()
{
        clear_carrier_props();
}

void HornOfMaliceHeard::run(actor::Actor& actor) const
{
        if (!actor::is_player(&actor)) {
                actor.m_properties.apply(
                        prop::make(prop::Id::conflict));
        }
}

HornOfMalice::HornOfMalice(ItemData* const item_data) :
        Item(item_data),
        m_charges(rnd::range(4, 6))
{
}

std::string HornOfMalice::name_info_str(const ItemNameIdentified id_type) const
{
        (void)id_type;

        return "(" + std::to_string(m_charges) + " uses)";
}

void HornOfMalice::save_hook() const
{
        saving::put_int(m_charges);
}

void HornOfMalice::load_hook()
{
        m_charges = saving::get_int();
}

ConsumeItem HornOfMalice::activate(actor::Actor* const actor)
{
        (void)actor;

        if (m_charges <= 0) {
                msg_log::add("It makes no sound.");

                return ConsumeItem::no;
        }

        auto effect =
                std::shared_ptr<SndHeardEffect>(
                        new HornOfMaliceHeard);

        Snd snd(
                "The Horn of Malice resounds!",
                audio::SfxId::horn,
                IgnoreMsgIfOriginSeen::no,
                map::g_player->m_pos,
                map::g_player,
                SndVol::high,
                AlertsMon::yes,
                effect);

        snd.run();

        --m_charges;

        game_time::tick();

        return ConsumeItem::no;
}

void HornOfBanishmentHeard::run(actor::Actor& actor) const
{
        if (actor.m_properties.has(prop::Id::summoned)) {
                if (actor::can_player_see_actor(actor)) {
                        const std::string name_the =
                                text_format::first_to_upper(
                                        actor::name_the(actor));

                        msg_log::add(
                                name_the +
                                " " +
                                common_text::g_mon_disappear);
                }

                actor.m_state = ActorState::destroyed;
        }
}

HornOfBanishment::HornOfBanishment(ItemData* const item_data) :
        Item(item_data),
        m_charges(rnd::range(4, 6))
{
}

std::string HornOfBanishment::name_info_str(const ItemNameIdentified id_type) const
{
        (void)id_type;

        return "(" + std::to_string(m_charges) + " uses)";
}

void HornOfBanishment::save_hook() const
{
        saving::put_int(m_charges);
}

void HornOfBanishment::load_hook()
{
        m_charges = saving::get_int();
}

ConsumeItem HornOfBanishment::activate(actor::Actor* const actor)
{
        (void)actor;

        if (m_charges <= 0) {
                msg_log::add("It makes no sound.");

                return ConsumeItem::no;
        }

        auto effect = std::shared_ptr<SndHeardEffect>(
                new HornOfBanishmentHeard);

        Snd snd(
                "The Horn of Banishment resounds!",
                audio::SfxId::horn,
                IgnoreMsgIfOriginSeen::no,
                map::g_player->m_pos,
                map::g_player,
                SndVol::high,
                AlertsMon::yes,
                effect);

        snd.run();

        --m_charges;

        game_time::tick();

        return ConsumeItem::no;
}

HolySymbol::HolySymbol(ItemData* item_data) :
        Item(item_data)
{
}

ConsumeItem HolySymbol::activate(actor::Actor* actor)
{
        (void)actor;

        if (!map::g_player->m_properties.allow_pray(Verbose::yes)) {
                return ConsumeItem::no;
        }

        if (m_has_failed_attempt) {
                msg_log::add("I have no faith that this would help me at the moment.");

                return ConsumeItem::no;
        }

        // Is not in failed state

        const std::string my_name = name(ItemNameType::plain, ItemNameInfo::none);

        std::string pray_msg;

        if (map::g_player->m_properties.has(prop::Id::terrified)) {
                pray_msg = "With trembling hands ";
        }

        pray_msg += "I make a prayer over the " + my_name + "...";

        msg_log::add(pray_msg);

        // If the item is still charging, roll for success
        if ((m_nr_charge_turns_left > 0) && !rnd::percent(25)) {
                // Failed!
                m_has_failed_attempt = true;

                // Set a recharge duration that is longer than normal
                m_nr_charge_turns_left = nr_turns_to_recharge().max;

                const int duration_pct = rnd::range(175, 200);

                m_nr_charge_turns_left = (m_nr_charge_turns_left * duration_pct) / 100;

                msg_log::more_prompt();
                msg_log::add("This feels useless!");

                map::g_player->incr_shock(4.0, ShockSrc::misc);

                game_time::tick();

                return ConsumeItem::no;
        }

        // Item can be used

        m_nr_charge_turns_left = nr_turns_to_recharge().roll();

        run_effect();

        game_time::tick();

        return ConsumeItem::no;
}

void HolySymbol::on_std_turn_in_inv_hook(InvType inv_type)
{
        (void)inv_type;

        // Already fully charged?
        if (m_nr_charge_turns_left == 0) {
                return;
        }

        ASSERT(m_nr_charge_turns_left > 0);

        --m_nr_charge_turns_left;

        if (m_nr_charge_turns_left == 0) {
                m_has_failed_attempt = false;

                const std::string my_name =
                        name(
                                ItemNameType::plain,
                                ItemNameInfo::none);

                msg_log::add(
                        "I feel like praying over the " +
                        my_name +
                        " would be beneficent again.");
        }
}

std::string HolySymbol::name_info_str(const ItemNameIdentified id_type) const
{
        (void)id_type;

        if (m_nr_charge_turns_left <= 0) {
                return "";
        }

        const auto turns_left_str = std::to_string(m_nr_charge_turns_left);

        std::string str = "(" + turns_left_str + " turns";

        if (m_has_failed_attempt) {
                str += ", failed";
        }

        str += ")";

        return str;
}

void HolySymbol::save_hook() const
{
        saving::put_int(m_nr_charge_turns_left);
        saving::put_bool(m_has_failed_attempt);
}

void HolySymbol::load_hook()
{
        m_nr_charge_turns_left = saving::get_int();
        m_has_failed_attempt = saving::get_bool();
}

void HolySymbol::run_effect()
{
        actor::restore_sp(
                *map::g_player,
                rnd::range(1, 4),
                actor::AllowRestoreAboveMax::yes);

        const int prop_duration = rnd::range(6, 12);

        prop::Prop* r_fear = prop::make(prop::Id::r_fear);
        prop::Prop* r_shock = prop::make(prop::Id::r_shock);

        r_fear->set_duration(prop_duration);
        r_shock->set_duration(prop_duration);

        map::g_player->m_properties.apply(r_fear);
        map::g_player->m_properties.apply(r_shock);
}

Range HolySymbol::nr_turns_to_recharge() const
{
        return {125, 175};
}

Clockwork::Clockwork(ItemData* const item_data) :
        Item(item_data),
        m_charges(rnd::range(4, 6))
{
}

std::string Clockwork::name_info_str(const ItemNameIdentified id_type) const
{
        (void)id_type;

        return "(" + std::to_string(m_charges) + " uses)";
}

void Clockwork::save_hook() const
{
        saving::put_int(m_charges);
}

void Clockwork::load_hook()
{
        m_charges = saving::get_int();
}

ConsumeItem Clockwork::activate(actor::Actor* const actor)
{
        (void)actor;

        if (m_charges <= 0) {
                msg_log::add("Nothing happens.");

                return ConsumeItem::no;
        }

        if (map::g_player->m_properties.has(prop::Id::extra_hasted)) {
                msg_log::add("It will not move.");

                return ConsumeItem::no;
        }

        msg_log::add("I wind up the clockwork.");

        map::g_player->incr_shock(12.0, ShockSrc::use_strange_item);

        if (!actor::is_alive(*map::g_player)) {
                return ConsumeItem::no;
        }

        map::g_player->m_properties.apply(
                prop::make(
                        prop::Id::extra_hasted));

        --m_charges;

        game_time::tick();

        return ConsumeItem::no;
}

OrbOfLife::OrbOfLife(ItemData* const item_data) :
        Item(item_data)
{
}

void OrbOfLife::on_pickup_hook()
{
        actor::change_max_hp(*map::g_player, 4, Verbose::yes);

        auto* prop_r_poison = prop::make(prop::Id::r_poison);
        prop_r_poison->set_indefinite();
        add_carrier_prop(prop_r_poison, Verbose::yes);

        auto* prop_r_disease = prop::make(prop::Id::r_disease);
        prop_r_disease->set_indefinite();
        add_carrier_prop(prop_r_disease, Verbose::yes);
}

void OrbOfLife::on_removed_from_inv_hook()
{
        actor::change_max_hp(*map::g_player, -4, Verbose::yes);

        clear_carrier_props();
}

Necronomicon::Necronomicon(ItemData* const item_data) :
        Item(item_data)
{
}

void Necronomicon::on_std_turn_in_inv_hook(const InvType inv_type)
{
        (void)inv_type;

        if (rnd::percent(2)) {
                Snd snd(
                        "",
                        audio::SfxId::END,
                        IgnoreMsgIfOriginSeen::yes,
                        map::g_player->m_pos,
                        map::g_player,
                        SndVol::high,
                        AlertsMon::yes);

                snd.run();
        }
}

ItemPrePickResult Necronomicon::pre_pickup_hook()
{
        if (player_bon::is_bg(Bg::exorcist)) {
                msg_log::add("I destroy the profane text!");

                game::incr_player_xp(g_xp_on_exorcist_destroy_necronomicon);

                actor::restore_sp(
                        *map::g_player,
                        999,
                        actor::AllowRestoreAboveMax::no,
                        Verbose::no);

                actor::restore_exorcist_fervor(g_exorcist_fervor_destroy_necronomicon);

                return ItemPrePickResult::destroy_item;
        }
        else {
                // Not Exorcist.
                return ItemPrePickResult::do_pickup;
        }
}

void Necronomicon::on_pickup_hook()
{
}

ConsumeItem WitchEye::activate(actor::Actor* actor)
{
        (void)actor;

        const auto item_name = name(ItemNameType::plain);

        msg_log::add("I clutch the " + item_name + "...");

        auto* const search =
                static_cast<prop::MagicSearching*>(
                        prop::make(prop::Id::magic_searching));

        search->set_range(g_fov_radi_int);

        search->set_allow_reveal_items();
        search->set_allow_reveal_creatures();

        search->set_duration(rnd::range(60, 80));

        map::g_player->m_properties.apply(search);

        map::g_player->incr_shock(12.0, ShockSrc::use_strange_item);

        if (rnd::one_in(3)) {
                msg_log::add("The eye decomposes.");

                return ConsumeItem::yes;
        }
        else {
                return ConsumeItem::no;
        }
}

ConsumeItem BoneCharm::activate(actor::Actor* actor)
{
        (void)actor;

        prop::Prop* const r_spell = prop::make(prop::Id::r_spell);

        r_spell->set_duration(rnd::range(6, 12));

        map::g_player->m_properties.apply(r_spell);

        for (terrain::Terrain* const terrain : map::g_terrain) {
                const P p = terrain->pos();

                if (!map::g_seen.at(p)) {
                        continue;
                }

                if (terrain->id() != terrain::Id::trap) {
                        continue;
                }

                auto* const trap = static_cast<terrain::Trap*>(terrain);

                if (!trap->is_magical()) {
                        continue;
                }

                trap->disarm();

                actor::restore_sp(
                        *map::g_player,
                        rnd::range(1, 6),
                        actor::AllowRestoreAboveMax::yes);
        }

        return ConsumeItem::yes;
}

// ConsumeItem FlaskOfDamning::activate(actor::Actor* actor)
// {
//         // TODO

//         (void)actor;

//         return ConsumeItem::no;
// }

// ConsumeItem ObsidianCharm::activate(actor::Actor* actor)
// {
//         // TODO

//         (void)actor;

//         return ConsumeItem::no;
// }

ConsumeItem FluctuatingMaterial::activate(actor::Actor* actor)
{
        (void)actor;

        const auto item_name = name(ItemNameType::plain);

        msg_log::add(
                ("I stare into the " +
                 item_name +
                 ", and feel myself changing..."),
                colors::text(),
                MsgInterruptPlayer::no,
                MorePromptOnMsg::yes);

        // First lose one trait, then gain one trait

        states::push(
                std::make_unique<PickTraitState>(
                        "Which trait do you gain?",
                        IsCharacterCreationTraitPick::no));

        states::push(std::make_unique<RemoveTraitState>());

        map::g_player->incr_shock(12.0, ShockSrc::use_strange_item);

        // TODO: Print a message that the item disappears

        return ConsumeItem::yes;
}

// ConsumeItem BatWingSalve::activate(actor::Actor* actor)
// {
//         // TODO

//         (void)actor;

//         return ConsumeItem::no;
// }

ConsumeItem AstralOpium::activate(actor::Actor* actor)
{
        (void)actor;

        const auto item_name = name(ItemNameType::plain);

        msg_log::add("I use the " + item_name + "...");

        map::g_player->m_properties.apply(
                prop::make(
                        prop::Id::astral_opium_addiction));

        map::g_player->m_properties.end_prop(
                prop::Id::frenzied);

        map::g_player->m_properties.apply(
                prop::make(
                        prop::Id::r_shock));

        map::g_player->m_properties.apply(
                prop::make(
                        prop::Id::r_fear));

        map::g_player->restore_shock(999, false);

        auto* const halluc =
                prop::make(
                        prop::Id::hallucinating);

        map::g_player->m_properties.apply(halluc);

        return ConsumeItem::yes;
}

}  // namespace item
