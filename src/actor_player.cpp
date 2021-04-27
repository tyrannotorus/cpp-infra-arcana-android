// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "actor_player.hpp"

#include <stddef.h>
#include <cmath>
#include <string>
#include <algorithm>
#include <ostream>
#include <vector>

#include "actor_death.hpp"
#include "actor_mon.hpp"
#include "actor_move.hpp"
#include "actor_see.hpp"
#include "attack.hpp"
#include "audio.hpp"
#include "common_text.hpp"
#include "fov.hpp"
#include "game.hpp"
#include "game_time.hpp"
#include "init.hpp"
#include "insanity.hpp"
#include "inventory.hpp"
#include "item.hpp"
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
#include "property.hpp"
#include "property_handler.hpp"
#include "query.hpp"
#include "saving.hpp"
#include "ability_values.hpp"
#include "actor_data.hpp"
#include "array2.hpp"
#include "audio_data.hpp"
#include "config.hpp"
#include "debug.hpp"
#include "item_data.hpp"
#include "pos.hpp"
#include "property_data.hpp"
#include "random.hpp"
#include "rect.hpp"
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

// -----------------------------------------------------------------------------
// actor
// -----------------------------------------------------------------------------
namespace actor
{
// -----------------------------------------------------------------------------
// Player
// -----------------------------------------------------------------------------
Player::Player()

        = default;

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
        saving::put_int(m_nr_turns_until_rspell);

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
        m_nr_turns_until_rspell = saving::get_int();

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
        if (!insanity::has_sympt(InsSymptId::masoch))
        {
                incr_shock(1, ShockSrc::misc);
        }

        const bool is_enough_dmg_for_wound = (dmg >= s_min_dmg_to_wound);
        const bool is_physical = is_physical_dmg_type(dmg_type);

        // Ghoul trait Indomitable Fury grants immunity to wounds while frenzied
        const bool is_ghoul_resist_wound =
                player_bon::has_trait(Trait::indomitable_fury) &&
                m_properties.has(PropId::frenzied);

        if ((allow_wound == AllowWound::yes) &&
            (m_hp - dmg) > 0 &&
            is_enough_dmg_for_wound &&
            is_physical &&
            !is_ghoul_resist_wound &&
            !config::is_bot_playing())
        {
                auto* const prop = new PropWound();

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

double Player::shock_lvl_to_value(const ShockLvl shock_lvl) const
{
        double shock_value = 0.0;

        switch (shock_lvl)
        {
        case ShockLvl::unsettling:
                shock_value = 2.0;
                break;

        case ShockLvl::frightening:
                shock_value = 4.0;
                break;

        case ShockLvl::terrifying:
                shock_value = 12.0;
                break;

        case ShockLvl::mind_shattering:
                shock_value = 50.0;
                break;

        case ShockLvl::none:
        case ShockLvl::END:
                break;
        }

        return shock_value;
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

void Player::incr_shock(const ShockLvl shock_lvl, ShockSrc shock_src)
{
        double shock_value = shock_lvl_to_value(shock_lvl);

        if (shock_value > 0.0)
        {
                incr_shock(shock_value, shock_src);
        }
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
        return m_active_medical_bag ||
                (m_remove_armor_countdown > 0) ||
                (m_equip_armor_countdown > 0) ||
                m_item_equipping ||
                (m_wait_turns_left > 0) ||
                (m_auto_move_dir != Dir::END);
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

                Mon* mon = static_cast<Mon*>(actor);

                if (!mon->is_player_aware_of_me())
                {
                        continue;
                }

                ShockLvl shock_lvl = ShockLvl::none;

                if (can_player_see_actor(*mon))
                {
                        shock_lvl = mon->m_data->mon_shock_lvl;
                }
                else
                {
                        // Monster cannot be seen
                        const auto mon_p = mon->m_pos;

                        if (map::g_seen.at(mon_p))
                        {
                                // There is an invisible monster here!
                                shock_lvl = ShockLvl::terrifying;
                        }
                }

                switch (shock_lvl)
                {
                case ShockLvl::unsettling:
                        val += 0.05;
                        break;

                case ShockLvl::frightening:
                        val += 0.3;
                        break;

                case ShockLvl::terrifying:
                        val += 0.6;
                        break;

                case ShockLvl::mind_shattering:
                        val += 1.75;
                        break;

                case ShockLvl::none:
                case ShockLvl::END:
                        break;
                }
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

        if (m_properties.allow_see())
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

                        const int terrain_shock = (double)t->shock_when_adj();

                        increased_tmp_shock +=
                                shock_taken_after_mods(
                                        (double)terrain_shock,
                                        ShockSrc::misc);
                }
        }
        // Is blind
        else if (!m_properties.has(PropId::fainted))
        {
                increased_tmp_shock +=
                        shock_taken_after_mods(
                                30.0,
                                ShockSrc::misc);
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

void Player::interrupt_actions()
{
        // Abort healing
        if (m_active_medical_bag)
        {
                m_active_medical_bag->interrupted();
                m_active_medical_bag = nullptr;
        }

        // Abort taking off/wearing armor?
        if ((m_remove_armor_countdown > 0) ||
            (m_equip_armor_countdown > 0))
        {
                bool should_continue_handling_armor =
                        !m_properties.has(PropId::burning);

                if (should_continue_handling_armor)
                {
                        std::string msg;

                        if (m_remove_armor_countdown > 0)
                        {
                                auto* const item =
                                        m_inv.item_in_slot(SlotId::body);

                                ASSERT(item);

                                const auto turns_left_str =
                                        std::to_string(
                                                m_remove_armor_countdown);

                                const auto armor_name =
                                        item->name(
                                                ItemRefType::a,
                                                ItemRefInf::yes);

                                msg =
                                        "Continue taking off " +
                                        armor_name +
                                        " (" +
                                        turns_left_str +
                                        " turns left)?";
                        }
                        else if (m_equip_armor_countdown > 0)
                        {
                                const auto turns_left_str =
                                        std::to_string(
                                                m_equip_armor_countdown);

                                const auto armor_name =
                                        m_item_equipping->name(
                                                ItemRefType::a,
                                                ItemRefInf::yes);

                                msg =
                                        "Continue putting on " +
                                        armor_name +
                                        " (" +
                                        turns_left_str +
                                        " turns left)?";
                        }

                        msg_log::add(
                                msg + " " + common_text::g_yes_or_no_hint,
                                colors::light_white(),
                                MsgInterruptPlayer::no,
                                MorePromptOnMsg::no,
                                CopyToMsgHistory::no);

                        should_continue_handling_armor =
                                query::yes_or_no(-1, AllowSpaceCancel::no) ==
                                BinaryAnswer::yes;

                        msg_log::clear();
                }

                if (!should_continue_handling_armor)
                {
                        m_remove_armor_countdown = 0;
                        m_equip_armor_countdown = 0;
                        m_item_equipping = nullptr;
                        m_is_dropping_armor_from_body_slot = false;
                }
        }
        // Abort equipping other item than armor?
        else if (m_item_equipping)
        {
                const auto wpn_name =
                        m_item_equipping->name(
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

                const bool should_continue =
                        (query::yes_or_no(-1, AllowSpaceCancel::no) ==
                         BinaryAnswer::yes);

                msg_log::clear();

                if (!should_continue)
                {
                        m_item_equipping = nullptr;
                }
        }

        // Abort waiting
        m_wait_turns_left = -1;

        // Abort quick move
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

        const audio::SfxId sfx = snd.sfx();
        const std::string& msg = snd.msg();
        const bool has_snd_msg = !msg.empty() && msg != " ";

        if (has_snd_msg)
        {
                msg_log::add(msg, colors::text(), MsgInterruptPlayer::no);
        }

        // Play audio after message to ensure sync between audio and animation.
        audio::play(sfx, dir_to_origin, percent_audible_distance);

        if (has_snd_msg)
        {
                Actor* const actor_who_made_snd = snd.actor_who_made_sound();

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

        const auto* const lantern_item = m_inv.item_in_backpack(item::Id::lantern);

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

                const R fov_lmt = fov::fov_rect(m_pos, hard_blocked.dims());

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

        // The player's current cell is always seen - mostly to update item info
        // while blind (i.e. when you pick up an item you should see it
        // disappear)
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

        // Explore
        for (int x = 0; x < map::w(); ++x)
        {
                for (int y = 0; y < map::h(); ++y)
                {
                        const P p(x, y);

                        if (!map::g_seen.at(p))
                        {
                                continue;
                        }

                        const bool is_dark =
                                map::g_dark.at(p);

                        const bool blocks_los =
                                map_parsers::BlocksLos().run(p);

                        const bool blocks_walking =
                                map_parsers::BlocksWalking(ParseActors::no)
                                        .run(p);

                        const bool allow_explore =
                                !is_dark ||
                                blocks_los ||
                                blocks_walking ||
                                has_darkvision;

                        if (allow_explore)
                        {
                                map::g_explored.at(p) = true;
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
