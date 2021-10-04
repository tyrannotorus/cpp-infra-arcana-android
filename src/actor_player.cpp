// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "actor_player.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

#include "ability_values.hpp"
#include "actor_data.hpp"
#include "actor_death.hpp"
#include "actor_mon.hpp"
#include "actor_move.hpp"
#include "actor_see.hpp"
#include "array2.hpp"
#include "attack.hpp"
#include "audio.hpp"
#include "audio_data.hpp"
#include "colors.hpp"
#include "common_text.hpp"
#include "config.hpp"
#include "debug.hpp"
#include "fov.hpp"
#include "game.hpp"
#include "game_time.hpp"
#include "init.hpp"
#include "insanity.hpp"
#include "inventory.hpp"
#include "item.hpp"
#include "item_data.hpp"
#include "item_device.hpp"
#include "item_factory.hpp"
#include "map.hpp"
#include "map_parsing.hpp"
#include "minimap.hpp"
#include "misc.hpp"
#include "msg_log.hpp"
#include "player_bon.hpp"
#include "player_spells.hpp"
#include "popup.hpp"
#include "pos.hpp"
#include "property_data.hpp"
#include "property_factory.hpp"
#include "property_handler.hpp"
#include "query.hpp"
#include "random.hpp"
#include "rect.hpp"
#include "saving.hpp"
#include "sound.hpp"
#include "terrain.hpp"
#include "terrain_data.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static const int s_min_dmg_to_wound = 5;

static const std::vector<std::string> m_item_feeling_messages = {
        "I feel like I should examine this place thoroughly.",
        "I feel like there is something of great interest here.",
        "I sense an object of great power here."};

static int nr_wounds(const PropHandler& properties)
{
        if (properties.has(PropId::wound))
        {
                const auto* const prop =
                        properties.prop(PropId::wound);

                const auto* const wound =
                        static_cast<const PropWound*>(prop);

                return wound->nr_wounds();
        }
        else
        {
                return 0;
        }
}

static double shock_taken_for_mon_shock_lvl(const MonShockLvl shock_lvl)
{
        switch (shock_lvl)
        {
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
                        player.m_remove_armor_countdown);

        const auto armor_name =
                item->name(
                        ItemRefType::a,
                        ItemRefInf::yes);

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
        const auto& player = *map::g_player;

        const auto turns_left_str =
                std::to_string(
                        player.m_equip_armor_countdown);

        const auto armor_name =
                player.m_item_equipping->name(
                        ItemRefType::a,
                        ItemRefInf::yes);

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
        const auto& player = *map::g_player;

        ASSERT((player.m_remove_armor_countdown > 0) ||
               (player.m_equip_armor_countdown > 0));

        std::string msg;

        if (player.m_remove_armor_countdown > 0)
        {
                msg = make_continue_remove_armor_query_msg();
        }
        else
        {
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

        if (player.m_properties.has(PropId::burning))
        {
                is_forced = ForceInterruptActions::yes;
        }

        if (is_forced == ForceInterruptActions::no)
        {
                const auto answer = query_continue_equip_armor();

                should_continue_handling_armor = (answer == BinaryAnswer::yes);

                msg_log::clear();
        }
        else
        {
                // TODO: Print message here (see MedicalBag)

                should_continue_handling_armor = false;
        }

        if (!should_continue_handling_armor)
        {
                player.m_remove_armor_countdown = 0;
                player.m_equip_armor_countdown = 0;
                player.m_item_equipping = nullptr;
                player.m_is_dropping_armor_from_body_slot = false;
        }
}

static void interrupt_equip_other_item(const ForceInterruptActions is_forced)
{
        bool should_continue = true;

        auto& player = *map::g_player;

        if (is_forced == ForceInterruptActions::no)
        {
                // Query interruption.

                const auto wpn_name =
                        player.m_item_equipping->name(
                                ItemRefType::a,
                                ItemRefInf::yes);

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
        else
        {
                // Forced interruption.

                // TODO: Print message here (see MedicalBag)

                should_continue = false;
        }

        if (!should_continue)
        {
                player.m_item_equipping = nullptr;
        }
}

static void interrupt_equip(const ForceInterruptActions is_forced)
{
        auto& player = *map::g_player;

        if ((player.m_remove_armor_countdown > 0) ||
            (player.m_equip_armor_countdown > 0))
        {
                interrupt_equip_armor(is_forced);
        }
        else if (player.m_item_equipping)
        {
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
Player::Player() = default;

Player::~Player()
{
        delete m_active_explosive;
        delete m_unarmed_wpn;
}

void Player::save() const
{
        m_properties.save();

        saving::put_int(m_ins);
        saving::put_int((int)m_shock);
        saving::put_int(m_hp);
        saving::put_int(m_base_max_hp);
        saving::put_int(m_sp);
        saving::put_int(m_base_max_sp);
        saving::put_int(m_pos.x);
        saving::put_int(m_pos.y);
        saving::put_int(m_nr_turns_until_r_spell);
        saving::put_int(m_nr_turns_until_meditative_focused);

        ASSERT(m_unarmed_wpn);

        saving::put_int((int)m_unarmed_wpn->id());

        for (int i = 0; i < (int)AbilityId::END; ++i)
        {
                const int v = m_data->ability_values.raw_val((AbilityId)i);

                saving::put_int(v);
        }
}

void Player::load()
{
        m_properties.load();

        m_ins = saving::get_int();
        m_shock = double(saving::get_int());
        m_hp = saving::get_int();
        m_base_max_hp = saving::get_int();
        m_sp = saving::get_int();
        m_base_max_sp = saving::get_int();
        m_pos.x = saving::get_int();
        m_pos.y = saving::get_int();
        m_nr_turns_until_r_spell = saving::get_int();
        m_nr_turns_until_meditative_focused = saving::get_int();

        const auto unarmed_wpn_id = (item::Id)saving::get_int();

        ASSERT(unarmed_wpn_id < item::Id::END);

        delete m_unarmed_wpn;
        m_unarmed_wpn = nullptr;

        auto* const unarmed_item = item::make(unarmed_wpn_id);

        ASSERT(unarmed_item);

        m_unarmed_wpn = static_cast<item::Wpn*>(unarmed_item);

        for (int i = 0; i < (int)AbilityId::END; ++i)
        {
                const int v = saving::get_int();

                m_data->ability_values.set_val((AbilityId)i, v);
        }
}

void Player::on_hit(
        const int dmg,
        const DmgType dmg_type,
        const AllowWound allow_wound)
{
        map::g_player->interrupt_actions(ForceInterruptActions::yes);

        if (!insanity::has_sympt(InsSymptId::masoch))
        {
                incr_shock(1.0, ShockSrc::misc);
        }

        const bool is_enough_dmg_for_wound = (dmg >= s_min_dmg_to_wound);
        const bool is_physical = is_physical_dmg_type(dmg_type);

        // Ghoul trait Indomitable Fury grants immunity to wounds while frenzied
        const bool is_ghoul_resist_wound =
                player_bon::has_trait(Trait::indomitable_fury) &&
                m_properties.has(PropId::frenzied);

        const bool is_wounded =
                (allow_wound == AllowWound::yes) &&
                (m_hp - dmg) > 0 &&
                is_enough_dmg_for_wound &&
                is_physical &&
                !is_ghoul_resist_wound &&
                !config::is_bot_playing();

        if (is_wounded)
        {
                auto* const prop = property_factory::make(PropId::wound);

                prop->set_indefinite();

                const int nr_wounds_before = nr_wounds(m_properties);

                m_properties.apply(prop);

                const int nr_wounds_after = nr_wounds(m_properties);

                if (nr_wounds_after > nr_wounds_before)
                {
                        if (insanity::has_sympt(InsSymptId::masoch))
                        {
                                game::add_history_event(
                                        "Experienced a very pleasant wound");

                                msg_log::add("Hehehe...");

                                const double shock_restored = 10.0;

                                m_shock = std::max(
                                        0.0,
                                        m_shock - shock_restored);
                        }
                        else
                        {
                                // Not masochistic
                                game::add_history_event(
                                        "Sustained a severe wound");
                        }
                }
        }
}

int Player::enc_percent() const
{
        const int total_w = m_inv.total_item_weight();
        const int max_w = carry_weight_lmt();

        return (int)(((double)total_w / (double)max_w) * 100.0);
}

int Player::carry_weight_lmt() const
{
        int carry_weight_mod = 0;

        if (player_bon::has_trait(Trait::strong_backed))
        {
                carry_weight_mod += 50;
        }

        if (m_properties.has(PropId::weakened))
        {
                carry_weight_mod -= 15;
        }

        return (g_player_carry_weight_base * (carry_weight_mod + 100)) / 100;
}

int Player::shock_resistance(const ShockSrc shock_src) const
{
        int res = 0;

        if (player_bon::has_trait(Trait::cool_headed))
        {
                res += 20;
        }

        if (player_bon::has_trait(Trait::courageous))
        {
                res += 20;
        }

        if (player_bon::has_trait(Trait::fearless))
        {
                res += 10;
        }

        switch (shock_src)
        {
        case ShockSrc::use_strange_item:
        case ShockSrc::cast_intr_spell:
                if (player_bon::bg() == Bg::occultist)
                {
                        res += 50;
                }
                break;

        case ShockSrc::see_mon:
                if (player_bon::bg() == Bg::ghoul)
                {
                        res += 50;
                }
                break;

        case ShockSrc::time:
        case ShockSrc::misc:
        case ShockSrc::END:
                break;
        }

        return std::clamp(res, 0, 100);
}

double Player::shock_taken_after_mods(
        const double base_shock,
        const ShockSrc shock_src) const
{
        const auto shock_res_db = (double)shock_resistance(shock_src);

        return (base_shock * (100.0 - shock_res_db)) / 100.0;
}

void Player::incr_shock(double shock, ShockSrc shock_src)
{
        if (m_properties.has(PropId::r_shock))
        {
                // Player is shock resistant
                return;
        }

        shock = shock_taken_after_mods(shock, shock_src);

        m_shock += shock;

        m_shock = std::max(0.0, m_shock);
}

void Player::restore_shock(
        const int amount_restored,
        const bool is_temp_shock_restored)
{
        m_shock = std::max(0.0, m_shock - amount_restored);

        if (is_temp_shock_restored)
        {
                m_shock_tmp = 0.0;
        }
}

void Player::incr_insanity()
{
        TRACE << "Increasing insanity" << std::endl;

        if (!config::is_bot_playing())
        {
                const int ins_incr = rnd::range(10, 15);

                m_ins += ins_incr;
        }

        if (ins() >= 100)
        {
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

void Player::item_feeling()
{
        if ((player_bon::bg() != Bg::rogue) ||
            !rnd::percent(80))
        {
                return;
        }

        bool print_feeling = false;

        auto is_nice = [](const item::Item& item) {
                return item.data().value == item::Value::supreme_treasure;
        };

        const size_t nr_positions = map::nr_positions();
        for (size_t i = 0; i < nr_positions; ++i)
        {
                // Nice item on the floor, which is not seen by the player?
                const auto* const floor_item = map::g_items.at(i);
                const bool is_seen = map::g_seen.at(i);

                if (floor_item && is_nice(*floor_item) && !is_seen)
                {
                        print_feeling = true;

                        break;
                }

                // Nice item in container?
                const auto* const terrain = map::g_terrain.at(i);
                const auto& items = terrain->m_item_container.items();

                for (const auto* const item : items)
                {
                        if (is_nice(*item))
                        {
                                print_feeling = true;

                                break;
                        }
                }
        }

        if (print_feeling)
        {
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

void Player::on_new_dlvl_reached()
{
        mon_feeling();

        item_feeling();

        for (auto& slot : m_inv.m_slots)
        {
                if (slot.item)
                {
                        slot.item->on_player_reached_new_dlvl();
                }
        }

        for (auto* const item : m_inv.m_backpack)
        {
                item->on_player_reached_new_dlvl();
        }

        m_properties.on_new_dlvl();
}

void Player::mon_feeling()
{
        if (player_bon::bg() != Bg::rogue)
        {
                return;
        }

        bool print_unique_mon_feeling = false;

        for (Actor* actor : game_time::g_actors)
        {
                if (actor->is_player() ||
                    map::g_player->is_leader_of(actor) ||
                    !actor->is_alive())
                {
                        // Not a hostile living monster
                        continue;
                }

                // Print monster feeling for monsters spawned during the level?
                // (We do the actual printing once, after the loop, so that we
                // don't print something like "A chill runs down my spine (x2)")
                if (actor->m_data->is_unique &&
                    actor->m_mon_aware_state.is_player_feeling_msg_allowed)
                {
                        print_unique_mon_feeling = true;

                        actor->m_mon_aware_state
                                .is_player_feeling_msg_allowed = false;
                }
        }

        if (print_unique_mon_feeling &&
            rnd::percent(80))
        {
                std::vector<std::string> msg_bucket {
                        "A chill runs down my spine.",
                        "I sense a great danger.",
                };

                // This message only makes sense if the player is fearful
                if (!player_bon::has_trait(Trait::fearless) &&
                    !m_properties.has(PropId::frenzied))
                {
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

void Player::set_auto_move(const Dir dir)
{
        ASSERT(dir != Dir::END);

        m_auto_move_dir = dir;

        m_has_taken_auto_move_step = false;
}

bool Player::is_busy() const
{
        return (
                is_busy_queryable_action() ||
                (m_wait_turns_left > 0) ||
                (m_auto_move_dir != Dir::END));
}

bool Player::is_busy_queryable_action() const
{
        return (
                m_active_medical_bag ||
                (m_remove_armor_countdown > 0) ||
                (m_equip_armor_countdown > 0) ||
                m_item_equipping);
}

void Player::add_shock_from_seen_monsters()
{
        if (!m_properties.allow_see())
        {
                return;
        }

        double val = 0.0;

        for (Actor* actor : game_time::g_actors)
        {
                if (actor->is_player() ||
                    !actor->is_alive() ||
                    (is_leader_of(actor)))
                {
                        continue;
                }

                if (!actor->is_player_aware_of_me())
                {
                        continue;
                }

                auto shock_lvl = MonShockLvl::none;

                if (can_player_see_actor(*actor))
                {
                        shock_lvl = actor->m_data->mon_shock_lvl;
                }
                else if (map::g_seen.at(actor->m_pos))
                {
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

void Player::update_tmp_shock()
{
        double increased_tmp_shock = 0.0;
        double reduced_tmp_shock = 0.0;

        // "Obessions" raise temporary shock
        if (insanity::has_sympt(InsSymptId::sadism) ||
            insanity::has_sympt(InsSymptId::masoch))
        {
                increased_tmp_shock += (double)g_shock_from_obsession;
        }

        if (m_properties.has(PropId::blind))
        {
                // NOTE: Here we assume that blindness is the ONLY property that
                // prevents the player from seeing, that should cause shock
                // (fainting also prevents seeing, but should not cause shock).

                auto* const blind = m_properties.prop(PropId::blind);

                const int blind_shock = std::min(blind->nr_turns_active(), 30);

                increased_tmp_shock +=
                        shock_taken_after_mods(
                                (double)blind_shock,
                                ShockSrc::misc);
        }
        else
        {
                const bool is_ghoul = player_bon::is_bg(Bg::ghoul);

                // Shock reduction from light?
                if (map::g_light.at(m_pos))
                {
                        reduced_tmp_shock += 20.0;
                }
                // Not lit - shock from darkness?
                else if (map::g_dark.at(m_pos) && !is_ghoul)
                {
                        double tmp_shock_dark = 20.0;

                        if (insanity::has_sympt(InsSymptId::phobia_dark))
                        {
                                tmp_shock_dark = 30.0;
                        }

                        increased_tmp_shock +=
                                shock_taken_after_mods(
                                        tmp_shock_dark,
                                        ShockSrc::misc);
                }

                // Temporary shock from seen terrains?
                for (const auto& d : dir_utils::g_dir_list_w_center)
                {
                        const auto p(m_pos + d);

                        const auto* const t = map::g_terrain.at(p);

                        const int terrain_shock = t->shock_when_adj();

                        increased_tmp_shock +=
                                shock_taken_after_mods(
                                        (double)terrain_shock,
                                        ShockSrc::misc);
                }
        }

        if (m_properties.has(PropId::r_shock))
        {
                // Player is shock resistant, only allow reducing shock
                increased_tmp_shock = 0.0;
        }

        m_shock_tmp = increased_tmp_shock - reduced_tmp_shock;
}

int Player::shock_tot() const
{
        double shock_tot_db = m_shock + m_shock_tmp;

        shock_tot_db = std::max(0.0, shock_tot_db);

        shock_tot_db = std::floor(shock_tot_db);

        int result = (int)shock_tot_db;

        result = m_properties.affect_shock(result);

        return result;
}

int Player::ins() const
{
        int result = m_ins;

        result = std::min(100, result);

        return result;
}

void Player::on_log_msg_printed()
{
        // NOTE: There cannot be any calls to msg_log::add() in this function,
        // as that would cause infinite recursion!

        // All messages abort waiting
        m_wait_turns_left = -1;

        // All messages abort quick move
        m_auto_move_dir = Dir::END;
}

void Player::interrupt_actions(const ForceInterruptActions is_forced)
{
        if (m_active_medical_bag)
        {
                m_active_medical_bag->interrupted(is_forced);
        }

        interrupt_equip(is_forced);

        m_wait_turns_left = -1;

        m_auto_move_dir = Dir::END;
}

void Player::hear_sound(
        const Snd& snd,
        const bool is_origin_seen_by_player,
        const Dir dir_to_origin,
        const int percent_audible_distance)
{
        (void)is_origin_seen_by_player;

        if (m_properties.has(PropId::deaf))
        {
                return;
        }

        const auto sfx = snd.sfx();
        const std::string& msg = snd.msg();
        const bool has_snd_msg = !msg.empty() && msg != " ";

        if (has_snd_msg)
        {
                const auto should_interrupt =
                        (is_origin_seen_by_player)
                        ? MsgInterruptPlayer::no
                        : MsgInterruptPlayer::yes;

                msg_log::add(msg, colors::text(), should_interrupt);
        }

        // Play audio after message to ensure sync between audio and animation.
        audio::play(sfx, dir_to_origin, percent_audible_distance);

        if (has_snd_msg)
        {
                auto* const actor_who_made_snd = snd.actor_who_made_sound();

                if (actor_who_made_snd && (actor_who_made_snd != this))
                {
                        static_cast<Mon*>(actor_who_made_snd)
                                ->set_player_aware_of_me();
                }
        }

        snd.on_heard(*this);
}

Color Player::color() const
{
        if (!is_alive())
        {
                return colors::red();
        }

        if (m_active_explosive)
        {
                return colors::yellow();
        }

        Color tmp_color;

        if (m_properties.affect_actor_color(tmp_color))
        {
                return tmp_color;
        }

        const auto* const lantern_item =
                m_inv.item_in_backpack(item::Id::lantern);

        if (lantern_item)
        {
                const auto* const lantern =
                        static_cast<const device::Lantern*>(lantern_item);

                if (lantern->is_activated)
                {
                        return colors::yellow();
                }
        }

        if (shock_tot() >= 75)
        {
                return colors::magenta();
        }

        if (m_properties.has(PropId::invis) ||
            m_properties.has(PropId::cloaked))
        {
                return colors::gray();
        }

        if (map::g_dark.at(m_pos))
        {
                tmp_color = m_data->color;

                tmp_color = tmp_color.shaded(40);

                tmp_color.set_rgb(
                        tmp_color.r(),
                        tmp_color.g(),
                        std::min(255, tmp_color.b() + 20));

                return tmp_color;
        }

        return m_data->color;
}

SpellSkill Player::spell_skill(const SpellId id) const
{
        return player_spells::spell_skill(id);
}

void Player::auto_melee()
{
        if (m_tgt &&
            (m_tgt->m_state == ActorState::alive) &&
            is_pos_adj(m_pos, m_tgt->m_pos, false) &&
            can_player_see_actor(*m_tgt))
        {
                move(*this, dir_utils::dir(m_tgt->m_pos - m_pos));

                return;
        }

        // If this line reached, there is no adjacent cur target.
        for (const P& d : dir_utils::g_dir_list)
        {
                Actor* const actor = map::first_actor_at_pos(m_pos + d);

                if (actor &&
                    !is_leader_of(actor) &&
                    can_player_see_actor(*actor))
                {
                        m_tgt = actor;

                        move(*this, dir_utils::dir(d));

                        return;
                }
        }
}

void Player::kick_mon(Actor& defender)
{
        item::Wpn* kick_wpn = nullptr;

        const ActorData& d = *defender.m_data;

        // TODO: This is REALLY hacky, it should be done another way. Why even
        // have a "stomp" attack?? Why not just kick them as well?
        // TODO: For a slightly better way, check for the "small crawling"
        // property instead.
        if ((d.actor_size == Size::floor) &&
            (d.is_spider ||
             d.is_rat ||
             d.is_snake ||
             (d.id == Id::worm_mass) ||
             (d.id == Id::mind_worms) ||
             (d.id == Id::crawling_intestines) ||
             (d.id == Id::crawling_hand) ||
             (d.id == Id::thing)))
        {
                kick_wpn = static_cast<item::Wpn*>(
                        item::make(item::Id::player_stomp));
        }
        else
        {
                kick_wpn = static_cast<item::Wpn*>(
                        item::make(item::Id::player_kick));
        }

        attack::melee(this, m_pos, defender, *kick_wpn);

        delete kick_wpn;
}

item::Wpn& Player::unarmed_wpn() const
{
        ASSERT(m_unarmed_wpn);

        return *m_unarmed_wpn;
}

void Player::hand_att(Actor& defender)
{
        item::Wpn& wpn = unarmed_wpn();

        attack::melee(this, m_pos, defender, wpn);
}

void Player::add_light_hook(Array2<bool>& light_map) const
{
        auto lgt_size = LgtSize::none;

        if (m_active_explosive)
        {
                if (m_active_explosive->data().id == item::Id::flare)
                {
                        lgt_size = LgtSize::fov;
                }
        }

        if (lgt_size != LgtSize::fov)
        {
                for (auto* const item : m_inv.m_backpack)
                {
                        const auto item_lgt_size = item->lgt_size();

                        if ((int)lgt_size < (int)item_lgt_size)
                        {
                                lgt_size = item_lgt_size;
                        }
                }
        }

        switch (lgt_size)
        {
        case LgtSize::single:
        {
                light_map.at(m_pos) = true;
        }
        break;

        case LgtSize::small:
        {
                for (const auto d : dir_utils::g_dir_list_w_center)
                {
                        light_map.at(m_pos + d) = true;
                }
        }
        break;

        case LgtSize::fov:
        {
                Array2<bool> hard_blocked(map::dims());

                const auto fov_lmt = fov::fov_rect(m_pos, hard_blocked.dims());

                map_parsers::BlocksLos()
                        .run(hard_blocked,
                             fov_lmt,
                             MapParseMode::overwrite);

                FovMap fov_map;
                fov_map.hard_blocked = &hard_blocked;
                fov_map.light = &map::g_light;
                fov_map.dark = &map::g_dark;

                const auto actor_fov = fov::run(m_pos, fov_map);

                for (int x = fov_lmt.p0.x; x <= fov_lmt.p1.x; ++x)
                {
                        for (int y = fov_lmt.p0.y; y <= fov_lmt.p1.y; ++y)
                        {
                                if (!actor_fov.at(x, y).is_blocked_hard)
                                {
                                        light_map.at(x, y) = true;
                                }
                        }
                }
        }
        break;

        case LgtSize::none:
        {
        }
        break;
        }
}

void Player::update_fov()
{
        const size_t nr_map_positions = map::nr_positions();

        for (size_t i = 0; i < nr_map_positions; ++i)
        {
                map::g_seen.at(i) = false;

                auto& los = map::g_los.at(i);
                los.is_blocked_hard = true;
                los.is_blocked_by_dark = false;
        }

        const bool has_darkvision = m_properties.has(PropId::darkvision);

        if (m_properties.allow_see())
        {
                Array2<bool> hard_blocked(map::dims());

                const auto fov_lmt = fov::fov_rect(m_pos, hard_blocked.dims());

                map_parsers::BlocksLos()
                        .run(hard_blocked,
                             fov_lmt,
                             MapParseMode::overwrite);

                FovMap fov_map;
                fov_map.hard_blocked = &hard_blocked;
                fov_map.light = &map::g_light;
                fov_map.dark = &map::g_dark;

                const auto fov_result = fov::run(m_pos, fov_map);

                for (int x = fov_lmt.p0.x; x <= fov_lmt.p1.x; ++x)
                {
                        for (int y = fov_lmt.p0.y; y <= fov_lmt.p1.y; ++y)
                        {
                                const auto& los_result = fov_result.at(x, y);

                                auto& los_to_update = map::g_los.at(x, y);

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
                                    los_result.is_blocked_by_dark)
                                {
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
        if (init::g_is_cheat_vision_enabled)
        {
                // Show all cells adjacent to cells which can be shot through or
                // seen through
                Array2<bool> reveal(map::dims());

                map_parsers::BlocksProjectiles()
                        .run(reveal, reveal.rect());

                map_parsers::BlocksLos()
                        .run(reveal,
                             reveal.rect(),
                             MapParseMode::append);

                for (auto& reveal_cell : reveal)
                {
                        reveal_cell = !reveal_cell;
                }

                const auto reveal_expanded =
                        map_parsers::expand(reveal, reveal.rect());

                for (size_t i = 0; i < nr_map_positions; ++i)
                {
                        if (reveal_expanded.at(i))
                        {
                                map::g_seen.at(i) = true;
                        }
                }
        }

        minimap::update();
}

void Player::fov_hack()
{
        Array2<bool> blocked_los(map::dims());

        map_parsers::BlocksLos()
                .run(blocked_los, blocked_los.rect());

        Array2<bool> blocked(map::dims());

        map_parsers::BlocksWalking(ParseActors::no)
                .run(blocked, blocked.rect());

        const std::vector<terrain::Id> free_terrains = {
                terrain::Id::liquid_deep,
                terrain::Id::chasm};

        for (int x = 0; x < blocked.w(); ++x)
        {
                for (int y = 0; y < blocked.h(); ++y)
                {
                        const P p(x, y);

                        if (map_parsers::IsAnyOfTerrains(free_terrains).run(p))
                        {
                                blocked.at(p) = false;
                        }
                }
        }

        const bool has_darkvision = m_properties.has(PropId::darkvision);

        for (int x = 0; x < map::w(); ++x)
        {
                for (int y = 0; y < map::h(); ++y)
                {
                        if (!blocked_los.at(x, y) || !blocked.at(x, y))
                        {
                                continue;
                        }

                        const P p(x, y);

                        for (const auto& d : dir_utils::g_dir_list)
                        {
                                const auto p_adj = p + d;

                                if (!map::is_pos_inside_map(p_adj) ||
                                    !map::g_seen.at(p_adj))
                                {
                                        continue;
                                }

                                const bool allow_explore =
                                        (!map::g_dark.at(p_adj) ||
                                         map::g_light.at(p_adj) ||
                                         has_darkvision) &&
                                        !blocked.at(p_adj);

                                if (!allow_explore)
                                {
                                        continue;
                                }

                                map::g_seen.at(x, y) = true;

                                map::g_los.at(x, y).is_blocked_hard = false;

                                break;
                        }
                }
        }
}

void Player::update_mon_awareness()
{
        const auto my_seen_actors = seen_actors(*this);

        for (auto* const actor : my_seen_actors)
        {
                static_cast<Mon*>(actor)->set_player_aware_of_me();
        }
}

bool Player::is_leader_of(const Actor* const actor) const
{
        if (!actor || (actor == this))
        {
                return false;
        }

        // Actor is monster

        return static_cast<const Mon*>(actor)->m_leader == this;
}

bool Player::is_actor_my_leader(const Actor* const actor) const
{
        (void)actor;

        // Should never happen
        ASSERT(false);

        return false;
}

}  // namespace actor
