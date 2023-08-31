// =============================================================================
// Copyright 2011-2023 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "actor.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

#include "ability_values.hpp"
#include "actor_data.hpp"
#include "actor_death.hpp"
#include "actor_player_state.hpp"
#include "actor_see.hpp"
#include "array2.hpp"
#include "attack.hpp"
#include "colors.hpp"
#include "common_text.hpp"
#include "config.hpp"
#include "debug.hpp"
#include "direction.hpp"
#include "fov.hpp"
#include "game_time.hpp"
#include "global.hpp"
#include "hints.hpp"
#include "init.hpp"
#include "insanity.hpp"
#include "inventory.hpp"
#include "item.hpp"
#include "item_data.hpp"
#include "item_factory.hpp"
#include "item_misc.hpp"
#include "item_weapon.hpp"
#include "map.hpp"
#include "map_parsing.hpp"
#include "minimap.hpp"
#include "msg_log.hpp"
#include "player_bon.hpp"
#include "popup.hpp"
#include "pos.hpp"
#include "property.hpp"
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
static const std::vector<std::string> m_item_feeling_messages = {
        "I feel like I should examine this place thoroughly.",
        "I feel like there is something of great interest here.",
        "I sense an object of great power here."};

static double shock_taken_for_mon_shock_lvl(const MonShockLvl shock_lvl)
{
        switch (shock_lvl) {
        case MonShockLvl::unsettling:
                return 0.04;
                break;

        case MonShockLvl::frightening:
                return 0.25;
                break;

        case MonShockLvl::terrifying:
                return 0.5;
                break;

        case MonShockLvl::mind_shattering:
                return 1.5;
                break;

        case MonShockLvl::none:
        case MonShockLvl::END:
                return 0.0;
                break;
        }

        ASSERT(false);

        return 0.0;
}

static std::string make_continue_remove_armor_query_msg()
{
        const auto& player = *map::g_player;

        auto* const item = player.m_inv.item_in_slot(SlotId::body);

        ASSERT(item);

        const auto turns_left_str =
                std::to_string(
                        actor::player_state::g_remove_armor_countdown);

        const auto armor_name =
                item->name(
                        ItemNameType::a,
                        ItemNameInfo::yes);

        return (
                "Continue taking off " +
                armor_name +
                " (" +
                turns_left_str +
                " turns left)? " +
                common_text::g_yes_or_no_hint);
}

static std::string make_continue_equip_armor_query_msg()
{
        const auto turns_left_str =
                std::to_string(
                        actor::player_state::g_equip_armor_countdown);

        const auto armor_name =
                actor::player_state::g_item_equipping->name(
                        ItemNameType::a,
                        ItemNameInfo::yes);

        return (
                "Continue putting on " +
                armor_name +
                " (" +
                turns_left_str +
                " turns left)? " +
                common_text::g_yes_or_no_hint);
}

static BinaryAnswer query_continue_equip_armor()
{
        ASSERT((actor::player_state::g_remove_armor_countdown > 0) ||
               (actor::player_state::g_equip_armor_countdown > 0));

        std::string msg;

        if (actor::player_state::g_remove_armor_countdown > 0) {
                msg = make_continue_remove_armor_query_msg();
        }
        else {
                msg = make_continue_equip_armor_query_msg();
        }

        msg_log::add(
                msg,
                colors::light_white(),
                MsgInterruptPlayer::no,
                MorePromptOnMsg::no,
                CopyToMsgHistory::no);

        const auto answer =
                query::yes_or_no(
                        std::nullopt,
                        AllowSpaceCancel::no);

        return answer;
}

static void interrupt_equip_armor(ForceInterruptActions is_forced)
{
        bool should_continue_handling_armor = true;

        auto& player = *map::g_player;

        if (player.m_properties.has(prop::Id::burning)) {
                is_forced = ForceInterruptActions::yes;
        }

        if (is_forced == ForceInterruptActions::no) {
                const auto answer = query_continue_equip_armor();

                should_continue_handling_armor = (answer == BinaryAnswer::yes);

                msg_log::clear();
        }
        else {
                // TODO: Print message here (see MedicalBag)

                should_continue_handling_armor = false;
        }

        if (!should_continue_handling_armor) {
                actor::player_state::g_remove_armor_countdown = 0;
                actor::player_state::g_equip_armor_countdown = 0;
                actor::player_state::g_item_equipping = nullptr;
                actor::player_state::g_is_dropping_armor_from_body = false;
        }
}

static void interrupt_equip_other_item(const ForceInterruptActions is_forced)
{
        bool should_continue = true;

        if (is_forced == ForceInterruptActions::no) {
                // Query interruption.

                const auto wpn_name =
                        actor::player_state::g_item_equipping->name(
                                ItemNameType::a,
                                ItemNameInfo::yes);

                const std::string msg =
                        "Continue equipping " +
                        wpn_name +
                        "? " +
                        common_text::g_yes_or_no_hint;

                msg_log::add(
                        msg,
                        colors::light_white(),
                        MsgInterruptPlayer::no,
                        MorePromptOnMsg::no,
                        CopyToMsgHistory::no);

                should_continue =
                        (query::yes_or_no(
                                 std::nullopt,
                                 AllowSpaceCancel::no) ==
                         BinaryAnswer::yes);

                msg_log::clear();
        }
        else {
                // Forced interruption.

                // TODO: Print message here (see MedicalBag)

                should_continue = false;
        }

        if (!should_continue) {
                actor::player_state::g_item_equipping = nullptr;
        }
}

static void interrupt_equip(const ForceInterruptActions is_forced)
{
        if ((actor::player_state::g_remove_armor_countdown > 0) ||
            (actor::player_state::g_equip_armor_countdown > 0)) {
                interrupt_equip_armor(is_forced);
        }
        else if (actor::player_state::g_item_equipping) {
                interrupt_equip_other_item(is_forced);
        }
}

// -----------------------------------------------------------------------------
// actor
// -----------------------------------------------------------------------------
namespace actor
{
// -----------------------------------------------------------------------------
// Player
// -----------------------------------------------------------------------------
void Actor::save() const
{
        m_properties.save();

        saving::put_int(player_state::g_insanity);
        saving::put_int((int)player_state::g_shock);
        saving::put_int(m_hp);
        saving::put_int(m_base_max_hp);
        saving::put_int(m_sp);
        saving::put_int(m_base_max_sp);
        saving::put_int(m_pos.x);
        saving::put_int(m_pos.y);
        saving::put_int(player_state::g_nr_turns_until_r_spell);
        saving::put_int(player_state::g_nr_turns_until_meditative_focused);

        ASSERT(player_state::g_unarmed_wpn.get());

        saving::put_int((int)player_state::g_unarmed_wpn->id());

        for (int i = 0; i < (int)AbilityId::END; ++i) {
                const int v = m_data->ability_values.raw_val((AbilityId)i);

                saving::put_int(v);
        }

        saving::put_int((int)player_state::g_player_total_shock_taken);

        for (size_t src_idx = 0; src_idx < (size_t)ShockSrc::END; ++src_idx) {
                saving::put_int((int)player_state::g_player_total_shock_from_src[src_idx]);
        }
}

void Actor::load()
{
        m_properties.load();

        player_state::g_insanity = saving::get_int();
        player_state::g_shock = double(saving::get_int());
        m_hp = saving::get_int();
        m_base_max_hp = saving::get_int();
        m_sp = saving::get_int();
        m_base_max_sp = saving::get_int();
        m_pos.x = saving::get_int();
        m_pos.y = saving::get_int();
        player_state::g_nr_turns_until_r_spell = saving::get_int();
        player_state::g_nr_turns_until_meditative_focused = saving::get_int();

        const auto unarmed_wpn_id = (item::Id)saving::get_int();

        ASSERT(unarmed_wpn_id < item::Id::END);

        auto* const unarmed_item = item::make(unarmed_wpn_id);

        ASSERT(unarmed_item);

        player_state::g_unarmed_wpn.reset(static_cast<item::Wpn*>(unarmed_item));

        for (int i = 0; i < (int)AbilityId::END; ++i) {
                const int v = saving::get_int();

                m_data->ability_values.set_val((AbilityId)i, v);
        }

        player_state::g_player_total_shock_taken = saving::get_int();

        for (int src_idx = 0; src_idx < (int)ShockSrc::END; ++src_idx) {
                player_state::g_player_total_shock_from_src[src_idx] = saving::get_int();
        }
}

int Actor::enc_percent() const
{
        const int total_w = m_inv.total_item_weight();
        const int max_w = carry_weight_lmt();

        return (int)(((double)total_w / (double)max_w) * 100.0);
}

int Actor::carry_weight_lmt() const
{
        int carry_weight_mod = 0;

        if (player_bon::has_trait(Trait::strong_backed)) {
                carry_weight_mod += 50;
        }

        if (m_properties.has(prop::Id::weakened)) {
                carry_weight_mod -= g_weakened_carry_weight_penalty;
        }

        return (g_player_carry_weight_base * (carry_weight_mod + 100)) / 100;
}

int Actor::shock_resistance(const ShockSrc shock_src) const
{
        int res = 0;

        if (player_bon::has_trait(Trait::cool_headed)) {
                res += 20;
        }

        if (player_bon::has_trait(Trait::courageous)) {
                res += 20;
        }

        if (player_bon::has_trait(Trait::fearless)) {
                res += 10;
        }

        switch (shock_src) {
        case ShockSrc::use_strange_item:
        case ShockSrc::cast_intr_spell_clairvoyance:
        case ShockSrc::cast_intr_spell_enchantment:
        case ShockSrc::cast_intr_spell_invocation:
        case ShockSrc::cast_intr_spell_transmutation:
        case ShockSrc::cast_intr_spell_general:
                if (player_bon::is_bg(Bg::occultist)) {
                        res += 50;
                }
                break;

        case ShockSrc::cast_intr_spell_blood:
                if (player_bon::is_bg(Bg::occultist)) {
                        res += 50;
                }
                else if (player_bon::is_bg(Bg::flagellant)) {
                        res += 25;
                }
                break;

        case ShockSrc::see_mon:
                if (player_bon::bg() == Bg::ghoul) {
                        res += 50;
                }
                break;

        case ShockSrc::take_damage:
                if (player_bon::is_bg(Bg::flagellant)) {
                        res = 100;
                }
                break;

        case ShockSrc::time:
        case ShockSrc::misc:
        case ShockSrc::END:
                break;
        }

        return std::clamp(res, 0, 100);
}

double Actor::shock_taken_after_mods(
        const double base_shock,
        const ShockSrc shock_src) const
{
        const auto shock_res_db = (double)shock_resistance(shock_src);

        return (base_shock * (100.0 - shock_res_db)) / 100.0;
}

void Actor::incr_shock(double shock, ShockSrc shock_src)
{
        if (m_properties.has(prop::Id::r_shock)) {
                // Player is shock resistant.
                return;
        }

        shock = shock_taken_after_mods(shock, shock_src);

        player_state::g_player_total_shock_taken += shock;
        player_state::g_player_total_shock_from_src[(size_t)shock_src] += shock;

        player_state::g_shock += shock;

        player_state::g_shock = std::max(0.0, player_state::g_shock);
}

void Actor::restore_shock(
        const int amount_restored,
        const bool is_temp_shock_restored)
{
        player_state::g_shock = std::max(0.0, player_state::g_shock - amount_restored);

        if (is_temp_shock_restored) {
                player_state::g_shock_tmp = 0.0;
        }
}

void Actor::incr_insanity()
{
        TRACE << "Increasing insanity" << std::endl;

        if (!config::is_bot_playing()) {
                const int ins_incr = rnd::range(10, 15);

                player_state::g_insanity += ins_incr;
        }

        if (insanity() >= 100) {
                const std::string msg =
                        "My mind can no longer withstand what it has grasped. "
                        "I am hopelessly lost.";

                popup::Popup(popup::AddToMsgHistory::yes)
                        .set_msg(msg)
                        .set_title("Insane!")
                        .run();

                kill(
                        *this,
                        IsDestroyed::yes,
                        AllowGore::no,
                        AllowDropItems::no);

                return;
        }

        // This point reached means insanity is below 100%
        insanity::run_sympt();

        restore_shock(999, true);
}

void Actor::item_feeling()
{
        if ((player_bon::bg() != Bg::rogue) ||
            !rnd::percent(80)) {
                return;
        }

        bool print_feeling = false;

        auto is_nice = [](const item::Item& item) {
                return item.data().value == item::Value::supreme_treasure;
        };

        const size_t nr_positions = map::nr_positions();
        for (size_t i = 0; i < nr_positions; ++i) {
                // Nice item on the floor, which is not seen by the player?
                const auto* const floor_item = map::g_items.at(i);
                const bool is_seen = map::g_seen.at(i);

                if (floor_item && is_nice(*floor_item) && !is_seen) {
                        print_feeling = true;

                        break;
                }

                // Nice item in container?
                const auto* const terrain = map::g_terrain.at(i);
                const auto& items = terrain->m_item_container.items();

                for (const auto* const item : items) {
                        if (is_nice(*item)) {
                                print_feeling = true;

                                break;
                        }
                }
        }

        if (print_feeling) {
                const std::string msg =
                        rnd::element(m_item_feeling_messages);

                msg_log::add(
                        msg,
                        colors::light_cyan(),
                        MsgInterruptPlayer::no,
                        MorePromptOnMsg::yes);

                return;
        }
}

void Actor::on_new_dlvl_reached()
{
        mon_feeling();

        item_feeling();

        for (auto& slot : m_inv.m_slots) {
                if (slot.item) {
                        slot.item->on_player_reached_new_dlvl();
                }
        }

        for (auto* const item : m_inv.m_backpack) {
                item->on_player_reached_new_dlvl();
        }

        m_properties.on_new_dlvl();
}

void Actor::mon_feeling() const
{
        if (player_bon::bg() != Bg::rogue) {
                return;
        }

        bool print_unique_mon_feeling = false;

        for (Actor* actor : game_time::g_actors) {
                if (actor::is_player(actor) ||
                    map::g_player->is_leader_of(actor) ||
                    !actor->is_alive()) {
                        // Not a hostile living monster
                        continue;
                }

                // Print monster feeling for monsters spawned during the level?
                // (We do the actual printing once, after the loop, so that we
                // don't print something like "A chill runs down my spine (x2)")
                if (actor->m_data->is_unique &&
                    actor->m_mon_aware_state.is_player_feeling_msg_allowed) {
                        print_unique_mon_feeling = true;

                        actor->m_mon_aware_state
                                .is_player_feeling_msg_allowed = false;
                }
        }

        if (print_unique_mon_feeling && rnd::percent(80)) {
                std::vector<std::string> msg_bucket {
                        "A chill runs down my spine.",
                        "I sense a great danger.",
                };

                // This message only makes sense if the player is fearful
                if (!player_bon::has_trait(Trait::fearless) &&
                    !m_properties.has(prop::Id::frenzied)) {
                        msg_bucket.emplace_back("I feel anxious.");
                }

                const auto msg = rnd::element(msg_bucket);

                msg_log::add(
                        msg,
                        colors::msg_note(),
                        MsgInterruptPlayer::no,
                        MorePromptOnMsg::yes);
        }
}

void Actor::set_auto_move(const Dir dir)
{
        ASSERT(dir != Dir::END);

        player_state::g_auto_move_dir = dir;
        player_state::g_has_taken_auto_move_step = false;
}

bool Actor::is_busy() const
{
        return (
                is_busy_queryable_action() ||
                (player_state::g_wait_turns_left > 0) ||
                (player_state::g_auto_move_dir != Dir::END));
}

bool Actor::is_busy_queryable_action() const
{
        return (
                player_state::g_active_medical_bag ||
                (player_state::g_remove_armor_countdown > 0) ||
                (player_state::g_equip_armor_countdown > 0) ||
                player_state::g_item_equipping);
}

void Actor::add_shock_from_seen_monsters()
{
        if (!m_properties.allow_see()) {
                return;
        }

        double val = 0.0;

        for (Actor* actor : game_time::g_actors) {
                if (actor::is_player(actor) ||
                    !actor->is_alive() ||
                    (is_leader_of(actor))) {
                        continue;
                }

                if (!actor->is_player_aware_of_me()) {
                        continue;
                }

                auto shock_lvl = MonShockLvl::none;

                if (can_player_see_actor(*actor)) {
                        shock_lvl = actor->m_data->mon_shock_lvl;
                }
                else if (map::g_seen.at(actor->m_pos)) {
                        // Player is aware of the monster, and the map position
                        // is seen - this is an invisible monster, how spooky!
                        shock_lvl = MonShockLvl::terrifying;
                }

                val += shock_taken_for_mon_shock_lvl(shock_lvl);
        }

        // Dampen the progression (it doesn't seem right that e.g. 8 monsters
        // are twice as scary as 4 monsters).
        val = std::sqrt(val);

        // Cap the value
        const double cap = 5.0;

        val = std::min(cap, val);

        incr_shock(val, ShockSrc::see_mon);
}

double Actor::increased_tmp_chock_on_blind() const
{
        auto* const blind = m_properties.prop(prop::Id::blind);

        const int blind_shock = std::min(blind->nr_turns_active(), 30);

        return shock_taken_after_mods((double)blind_shock, ShockSrc::misc);
}

double Actor::increased_tmp_shock_from_dark() const
{
        if (!map::g_dark.at(m_pos) || map::g_light.at(m_pos)) {
                return 0.0;
        }

        double shock =
                insanity::has_sympt(InsSymptId::phobia_dark)
                ? 30.0
                : 20.0;

        // Ghoul characters take half shock from darkness.
        if (player_bon::is_bg(Bg::ghoul)) {
                shock /= 2.0;
        }

        return shock_taken_after_mods(shock, ShockSrc::misc);
}

double Actor::reduced_tmp_shock_from_light() const
{
        if (!map::g_light.at(m_pos)) {
                return 0.0;
        }

        double reduced_shock = 20.0;

        // Ghoul characters have halved shock reduction from light.
        if (player_bon::is_bg(Bg::ghoul)) {
                reduced_shock /= 2.0;
        }

        return reduced_shock;
}

double Actor::increased_tmp_shock_from_adjacent_terrain() const
{
        double shock = 0.0;

        for (const P& d : dir_utils::g_dir_list_w_center) {
                const P p = m_pos + d;

                const terrain::Terrain* const t = map::g_terrain.at(p);

                const int terrain_shock = t->shock_when_adj();

                shock += shock_taken_after_mods((double)terrain_shock, ShockSrc::misc);

                // HACK: It is convenient to show the hint here since we are
                // searching surrounding terrain anyway. But it makes this
                // function less pure.
                if (t->has_gore()) {
                        hints::display(hints::Id::temporary_and_permanent_shock);
                }
        }

        return shock;
}

void Actor::update_tmp_shock()
{
        double increased_tmp_shock = 0.0;
        double reduced_tmp_shock = 0.0;

        if (insanity::has_sympt(InsSymptId::sadism)) {
                increased_tmp_shock += (double)g_shock_from_obsession;
        }

        if (m_properties.has(prop::Id::blind)) {
                // NOTE: Here we assume that blindness is the ONLY property that
                // prevents the player from seeing, that should cause shock
                // (fainting also prevents seeing, but should not cause shock).

                increased_tmp_shock += increased_tmp_chock_on_blind();
        }
        else if (m_properties.allow_see()) {
                // Visual things that might affect shock.

                increased_tmp_shock += increased_tmp_shock_from_dark();

                reduced_tmp_shock += reduced_tmp_shock_from_light();

                increased_tmp_shock += increased_tmp_shock_from_adjacent_terrain();
        }

        if (m_properties.has(prop::Id::r_shock)) {
                // Player is shock resistant, only allow reducing shock.
                increased_tmp_shock = 0.0;
        }

        player_state::g_shock_tmp = increased_tmp_shock - reduced_tmp_shock;
}

int Actor::shock_tot() const
{
        double shock_tot_db =
                player_state::g_shock +
                player_state::g_shock_tmp;

        shock_tot_db = std::max(0.0, shock_tot_db);

        shock_tot_db = std::floor(shock_tot_db);

        int result = (int)shock_tot_db;

        result += m_properties.player_extra_min_shock();

        return result;
}

int Actor::insanity() const
{
        int result = player_state::g_insanity;

        result = std::min(100, result);

        return result;
}

void Actor::on_log_msg_printed()
{
        // NOTE: There cannot be any calls to msg_log::add() in this function,
        // as that would cause infinite recursion!

        // All messages abort waiting
        player_state::g_wait_turns_left = -1;

        // All messages abort quick move
        player_state::g_auto_move_dir = Dir::END;
}

void Actor::interrupt_actions(const ForceInterruptActions is_forced)
{
        if (player_state::g_active_medical_bag) {
                player_state::g_active_medical_bag->interrupted(is_forced);
        }

        interrupt_equip(is_forced);

        player_state::g_wait_turns_left = -1;

        player_state::g_auto_move_dir = Dir::END;
}

item::Wpn* Actor::make_kick_wpn(const Actor& mon_kicked) const
{
        const ActorData& d = *mon_kicked.m_data;

        if ((d.actor_size == Size::floor) &&
            mon_kicked.m_properties.has(prop::Id::small_crawling)) {
                return static_cast<item::Wpn*>(item::make(item::Id::player_stomp));
        }
        else {
                return static_cast<item::Wpn*>(item::make(item::Id::player_kick));
        }
}

void Actor::kick_mon(Actor& defender)
{
        std::unique_ptr<item::Wpn> kick_wpn(make_kick_wpn(defender));

        attack::melee(this, m_pos, defender.m_pos, *kick_wpn);
}

item::Wpn& Actor::unarmed_wpn() const
{
        ASSERT(player_state::g_unarmed_wpn.get());

        return *player_state::g_unarmed_wpn;
}

void Actor::set_unarmed_wpn(item::Wpn* wpn) const
{
        player_state::g_unarmed_wpn.reset(wpn);
}

void Actor::update_fov()
{
        const size_t nr_map_positions = map::nr_positions();

        for (size_t i = 0; i < nr_map_positions; ++i) {
                map::g_seen.at(i) = false;

                LosResult& los = map::g_los.at(i);
                los.is_blocked_hard = true;
                los.is_blocked_by_dark = false;
        }

        const bool has_darkvision = m_properties.has(prop::Id::darkvision);

        if (m_properties.allow_see()) {
                Array2<bool> hard_blocked(map::dims());

                const auto fov_lmt = fov::fov_rect(m_pos, map::dims());

                map_parsers::BlocksLos()
                        .run(hard_blocked,
                             fov_lmt,
                             MapParseMode::overwrite);

                FovMap fov_map;
                fov_map.hard_blocked = &hard_blocked;
                fov_map.light = &map::g_light;
                fov_map.dark = &map::g_dark;

                const Array2<LosResult> fov_result = fov::run(m_pos, fov_map);

                for (int x = fov_lmt.p0.x; x <= fov_lmt.p1.x; ++x) {
                        for (int y = fov_lmt.p0.y; y <= fov_lmt.p1.y; ++y) {
                                const LosResult& los_result =
                                        fov_result.at(x, y);

                                LosResult& los_to_update = map::g_los.at(x, y);

                                map::g_seen.at(x, y) =
                                        !los_result.is_blocked_hard &&
                                        (!los_result.is_blocked_by_dark ||
                                         has_darkvision);

                                los_to_update = los_result;

#ifndef NDEBUG
                                // Sanity check - if the cell is ONLY blocked by
                                // darkness (i.e. not by a wall or other
                                // blocking terrain), it should NOT be lit
                                if (!los_result.is_blocked_hard &&
                                    los_result.is_blocked_by_dark) {
                                        ASSERT(!map::g_light.at(x, y));
                                }
#endif  // NDEBUG
                        }
                }

                fov_hack();
        }

        // The player's current cell is always seen.
        map::g_seen.at(m_pos) = true;

        // Cheat vision
        if (init::g_is_cheat_vision_enabled) {
                Array2<bool> blocked_projectiles(map::dims());

                // Show all cells adjacent to cells which can be shot through or
                // seen through
                map_parsers::BlocksProjectiles()
                        .run(blocked_projectiles, blocked_projectiles.rect());

                map_parsers::BlocksLos()
                        .run(
                                blocked_projectiles,
                                blocked_projectiles.rect(),
                                MapParseMode::append);

                for (auto& reveal_cell : blocked_projectiles) {
                        reveal_cell = !reveal_cell;
                }

                const auto reveal_expanded =
                        map_parsers::expand(
                                blocked_projectiles,
                                blocked_projectiles.rect());

                for (size_t i = 0; i < nr_map_positions; ++i) {
                        if (reveal_expanded.at(i)) {
                                map::g_seen.at(i) = true;
                        }
                }
        }

        minimap::update();
}

void Actor::fov_hack() const
{
        Array2<bool> blocked_los(map::dims());

        map_parsers::BlocksLos()
                .run(blocked_los, blocked_los.rect());

        Array2<bool> blocked(map::dims());

        map_parsers::BlocksWalking(ParseActors::no)
                .run(blocked, blocked.rect());

        const std::vector<terrain::Id> free_terrains = {
                terrain::Id::chasm};

        for (int x = 0; x < blocked.w(); ++x) {
                for (int y = 0; y < blocked.h(); ++y) {
                        const P p(x, y);

                        if (map_parsers::IsAnyOfTerrains(free_terrains).run(p)) {
                                blocked.at(p) = false;
                        }
                }
        }

        const bool has_darkvision = m_properties.has(prop::Id::darkvision);

        for (int x = 0; x < map::w(); ++x) {
                for (int y = 0; y < map::h(); ++y) {
                        if (!blocked_los.at(x, y) || !blocked.at(x, y)) {
                                continue;
                        }

                        const P p(x, y);

                        for (const auto& d : dir_utils::g_dir_list) {
                                const auto p_adj = p + d;

                                if (!map::is_pos_inside_map(p_adj) ||
                                    !map::g_seen.at(p_adj)) {
                                        continue;
                                }

                                const bool allow_explore =
                                        (!map::g_dark.at(p_adj) ||
                                         map::g_light.at(p_adj) ||
                                         has_darkvision) &&
                                        !blocked.at(p_adj);

                                if (!allow_explore) {
                                        continue;
                                }

                                map::g_seen.at(x, y) = true;

                                map::g_los.at(x, y).is_blocked_hard = false;

                                break;
                        }
                }
        }
}

void Actor::update_mon_awareness() const
{
        const std::vector<Actor*> my_seen_actors = seen_actors(*this);

        for (Actor* const actor : my_seen_actors) {
                actor->make_player_aware_of_me();
        }
}

}  // namespace actor
