// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "attack.hpp"

#include <memory>
#include <string>
#include <vector>

#include "ability_values.hpp"
#include "actor.hpp"
#include "actor_data.hpp"
#include "actor_hit.hpp"
#include "actor_see.hpp"
#include "array2.hpp"
#include "attack_data.hpp"
#include "attack_internal.hpp"
#include "audio_data.hpp"
#include "bash.hpp"
#include "colors.hpp"
#include "debug.hpp"
#include "game_time.hpp"
#include "global.hpp"
#include "io.hpp"
#include "item.hpp"
#include "item_att_property.hpp"
#include "item_data.hpp"
#include "item_weapon.hpp"
#include "knockback.hpp"
#include "line_calc.hpp"
#include "map.hpp"
#include "msg_log.hpp"
#include "player_bon.hpp"
#include "pos.hpp"
#include "property.hpp"
#include "property_data.hpp"
#include "property_factory.hpp"
#include "property_handler.hpp"
#include "random.hpp"
#include "sound.hpp"
#include "terrain.hpp"
#include "text_format.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static void print_player_melee_miss_actor_msg()
{
        msg_log::add("I miss.");
}

static void print_mon_melee_miss_actor_msg(const MeleeAttData& att_data)
{
        if (!att_data.defender) {
                ASSERT(false);

                return;
        }

        const bool is_player_defender =
                actor::is_player(att_data.defender);

        const bool is_player_seeing_attacker =
                can_player_see_actor(*att_data.attacker);

        const bool is_player_seeing_defender =
                can_player_see_actor(*att_data.defender);

        const bool is_unseen_monsters_fighting =
                !is_player_defender &&
                !is_player_seeing_attacker &&
                !is_player_seeing_defender;

        if (is_unseen_monsters_fighting) {
                return;
        }

        std::string attacker_name;

        if (is_player_seeing_attacker) {
                attacker_name =
                        text_format::first_to_upper(
                                actor::name_the(*att_data.attacker));
        }
        else {
                attacker_name = "It";
        }

        std::string defender_name;

        if (actor::is_player(att_data.defender)) {
                defender_name = "me";
        }
        else {
                if (is_player_seeing_defender) {
                        defender_name = actor::name_the(*att_data.defender);
                }
                else {
                        defender_name = "it";
                }
        }

        const std::string msg =
                attacker_name +
                " misses " +
                defender_name +
                ".";

        const auto interrupt =
                actor::is_player(att_data.defender)
                ? MsgInterruptPlayer::yes
                : MsgInterruptPlayer::no;

        msg_log::add(msg, colors::text(), interrupt);
}

static void print_player_melee_hit_actor_msg(
        const int dmg,
        const MeleeAttData& att_data)
{
        const std::string wpn_verb =
                att_data.att_item->data().melee.attack_msgs.player;

        std::string other_name;

        if (can_player_see_actor(*att_data.defender)) {
                other_name = actor::name_the(*att_data.defender);
        }
        else {
                // Player cannot see defender
                other_name = "it";
        }

        const std::string dmg_punct = hit_size_punctuation_str(attack::relative_hit_size(dmg));

        if (att_data.is_intrinsic_att) {
                const std::string att_mod_str =
                        att_data.is_weak_attack
                        ? " feebly"
                        : "";

                msg_log::add(
                        std::string(
                                "I " +
                                wpn_verb +
                                " " +
                                other_name +
                                att_mod_str +
                                dmg_punct),
                        colors::msg_good());
        }
        else {
                // Not intrinsic attack
                std::string att_mod_str;

                if (att_data.is_weak_attack) {
                        att_mod_str = "feebly ";
                }
                else if (att_data.is_backstab) {
                        att_mod_str = "covertly ";
                }

                const Color color =
                        att_data.is_backstab
                        ? colors::light_blue()
                        : colors::msg_good();

                const std::string wpn_name_a =
                        att_data.att_item->name(
                                ItemNameType::a,
                                ItemNameInfo::none);

                msg_log::add(
                        std::string(
                                "I " +
                                wpn_verb +
                                " " +
                                other_name +
                                " " +
                                att_mod_str +
                                "with " +
                                wpn_name_a +
                                dmg_punct),
                        color);
        }
}

static void print_mon_melee_hit_actor_msg(const int dmg, const MeleeAttData& att_data)
{
        if (!att_data.defender) {
                ASSERT(false);

                return;
        }

        const bool is_player_defender =
                actor::is_player(att_data.defender);

        const bool is_player_seeing_attacker =
                can_player_see_actor(*att_data.attacker);

        const bool is_player_seeing_defender =
                can_player_see_actor(*att_data.defender);

        const bool is_unseen_monsters_fighting =
                !is_player_defender &&
                !is_player_seeing_attacker &&
                !is_player_seeing_defender;

        if (is_unseen_monsters_fighting) {
                return;
        }

        std::string attacker_name;

        if (is_player_seeing_attacker) {
                attacker_name =
                        text_format::first_to_upper(
                                actor::name_the(*att_data.attacker));
        }
        else {
                attacker_name = "It";
        }

        std::string wpn_verb =
                att_data.att_item->data().melee.attack_msgs.other;

        std::string defender_name;

        if (actor::is_player(att_data.defender)) {
                defender_name = "me";
        }
        else {
                if (is_player_seeing_defender) {
                        defender_name = actor::name_the(*att_data.defender);
                }
                else {
                        defender_name = "it";
                }
        }

        const std::string dmg_punct = hit_size_punctuation_str(attack::relative_hit_size(dmg));

        std::string used_wpn_str;

        if (!att_data.att_item->data().is_intr &&
            // TODO: This is hacky
            (actor::id(*att_data.attacker) != "MON_SPECTRAL_WPN")) {
                const std::string wpn_name_a =
                        att_data.att_item->name(
                                ItemNameType::a,
                                ItemNameInfo::none,
                                ItemNameAttackInfo::none);

                used_wpn_str = " with " + wpn_name_a;
        }

        const std::string msg =
                attacker_name +
                " " +
                wpn_verb +
                " " +
                defender_name +
                used_wpn_str +
                dmg_punct;

        const Color color =
                actor::is_player(att_data.defender)
                ? colors::msg_bad()
                : colors::text();

        // NOTE: Interruption is not needed here since the player will be
        // interrupted by getting hit.
        msg_log::add(msg, color);
}

static void print_no_attacker_hit_player_melee_msg(const int dmg)
{
        const std::string dmg_punct = hit_size_punctuation_str(attack::relative_hit_size(dmg));

        // NOTE: Interruption is not needed here since the player will be
        // interrupted by getting hit.
        msg_log::add("I am hit" + dmg_punct, colors::msg_bad());
}

static void print_no_attacker_hit_mon_melee_msg(
        const int dmg,
        const MeleeAttData& att_data)
{
        const std::string other_name =
                text_format::first_to_upper(
                        actor::name_the(*att_data.defender));

        Color msg_color = colors::msg_good();

        if (map::g_player->is_leader_of(att_data.defender)) {
                // Monster is allied to player, use a neutral color
                // instead (we do not use red color here, since that
                // is reserved for player taking damage).
                msg_color = colors::white();
        }

        const std::string dmg_punct = hit_size_punctuation_str(attack::relative_hit_size(dmg));

        msg_log::add(
                other_name + " is hit" + dmg_punct,
                msg_color,
                MsgInterruptPlayer::yes);
}

static void print_melee_miss_actor_msg(const MeleeAttData& att_data)
{
        if (!att_data.attacker) {
                // TODO: It can happen that there is no actor attacking due to
                // traps (e.g. spear trap), but this should probably still print
                // some message? See also "print_melee_hit_msg", that case is
                // actually handling the lack of an attacker.

                return;
        }

        if (actor::is_player(att_data.attacker)) {
                print_player_melee_miss_actor_msg();
        }
        else {
                print_mon_melee_miss_actor_msg(att_data);
        }
}

static void print_melee_hit_actor_msg(const int dmg, const MeleeAttData& att_data)
{
        if (!att_data.defender) {
                ASSERT(false);

                return;
        }

        if (att_data.attacker) {
                if (actor::is_player(att_data.attacker)) {
                        print_player_melee_hit_actor_msg(dmg, att_data);
                }
                else {
                        print_mon_melee_hit_actor_msg(dmg, att_data);
                }
        }
        else {
                // No attacker (e.g. trap attack)
                if (actor::is_player(att_data.defender)) {
                        print_no_attacker_hit_player_melee_msg(dmg);
                }
                else if (can_player_see_actor(*att_data.defender)) {
                        print_no_attacker_hit_mon_melee_msg(dmg, att_data);
                }
        }
}

static audio::SfxId melee_hit_sfx(const int dmg, const MeleeAttData& att_data)
{
        const attack::HitSize hit_size = attack::relative_hit_size(dmg);

        switch (hit_size) {
        case attack::HitSize::minor:
                return att_data.att_item->data().melee.hit_small_sfx;

        case attack::HitSize::medium:
                return att_data.att_item->data().melee.hit_medium_sfx;

        case attack::HitSize::major:
                return att_data.att_item->data().melee.hit_hard_sfx;
        }

        return audio::SfxId::END;
}

static AlertsMon is_melee_snd_alerting_mon(
        const actor::Actor* const attacker,
        const item::Item& wpn)
{
        const bool is_wpn_noisy = wpn.data().melee.is_noisy;

        if (!is_wpn_noisy) {
                return AlertsMon::no;
        }

        if (actor::is_player(attacker) &&
            player_bon::has_trait(Trait::silent)) {
                return AlertsMon::no;
        }

        return AlertsMon::yes;
}

static void print_melee_attack_actor_msg(
        const ActionResult att_result,
        const int dmg,
        const MeleeAttData& att_data)
{
        if (!att_data.defender) {
                ASSERT(false);

                return;
        }

        if (att_result <= ActionResult::fail) {
                print_melee_miss_actor_msg(att_data);
        }
        else {
                print_melee_hit_actor_msg(dmg, att_data);
        }
}

static std::string melee_snd_msg(const MeleeAttData& att_data)
{
        std::string snd_msg;

        // Only print a message if player is not involved
        if (!actor::is_player(att_data.defender) &&
            !actor::is_player(att_data.attacker)) {
                snd_msg = "I hear fighting.";
        }

        return snd_msg;
}

static void emit_melee_snd(
        const ActionResult att_result,
        const int dmg,
        const MeleeAttData& att_data)
{
        if (!att_data.defender) {
                ASSERT(false);

                return;
        }

        const AlertsMon snd_alerts_mon =
                is_melee_snd_alerting_mon(
                        att_data.attacker,
                        *att_data.att_item);

        auto sfx = audio::SfxId::END;

        if (att_result <= ActionResult::fail) {
                sfx = att_data.att_item->data().melee.miss_sfx;
        }
        else {
                sfx = melee_hit_sfx(dmg, att_data);
        }

        const P origin =
                att_data.attacker
                ? att_data.attacker->m_pos
                : att_data.defender->m_pos;

        std::string snd_msg;

        if (att_data.attacker &&
            !actor::can_player_see_actor(*att_data.attacker) &&
            !actor::can_player_see_actor(*att_data.defender)) {
                snd_msg = melee_snd_msg(att_data);
        }

        auto ignore_msg_if_origin_seeen = IgnoreMsgIfOriginSeen::yes;

        if (att_data.attacker) {
                const bool is_player_defender =
                        actor::is_player(att_data.defender);

                const bool is_player_seeing_defender =
                        can_player_see_actor(*att_data.defender);

                const bool is_player_seeing_attacker =
                        can_player_see_actor(*att_data.attacker);

                const bool is_unseen_monsters_fighting =
                        !is_player_defender &&
                        !is_player_seeing_attacker &&
                        !is_player_seeing_defender;

                if (is_unseen_monsters_fighting) {
                        // Two unseen monsters fighting each other - always
                        // include the sound message.
                        ignore_msg_if_origin_seeen = IgnoreMsgIfOriginSeen::no;
                }
        }

        Snd snd(
                snd_msg,
                sfx,
                ignore_msg_if_origin_seeen,
                origin,
                att_data.attacker,
                SndVol::low,
                snd_alerts_mon);

        snd.run();
}

static void apply_melee_attack_props(
        actor::Actor& defender,
        const actor::Actor* const attacker,
        item::Wpn& wpn)
{
        ItemAttackProp& att_prop = wpn.prop_applied_on_melee(attacker);

        if (att_prop.prop) {
                attack::try_apply_attack_property_on_actor(
                        att_prop,
                        defender,
                        wpn.data().melee.dmg_type);
        }

        // NOTE: The 'can_bleed' flag is used as a condition here for
        // which monsters can be weakened by crippling strikes - it
        // should be a good enough rule so that crippling strikes can
        // only be applied against monsters where it makes sense.
        if (actor::is_player(attacker) &&
            player_bon::has_trait(Trait::crippling_strikes) &&
            defender.m_data->can_bleed &&
            // TODO: This prevents applying on Worm Masses, but it's
            // hacky, and only makes sense *right now*, there should be
            // some better attribute to control this.
            !defender.m_properties.has(prop::Id::splits_on_death) &&
            rnd::percent(60)) {
                prop::Prop* weak = prop::make(prop::Id::weakened);

                weak->set_duration(rnd::range(2, 3));

                defender.m_properties.apply(weak);
        }
}

static void knock_back_target(actor::Actor& defender, const P& attacker_origin)
{
        const P d = defender.m_pos - attacker_origin;

        const P knock_origin = defender.m_pos - d.signs();

        knockback::run(defender, knock_origin, knockback::KnockbackSource::other);
}

static void melee_hit_actor(
        const int dmg,
        actor::Actor& defender,
        actor::Actor* const attacker,
        const P& attacker_origin,
        item::Wpn& wpn)
{
        if (wpn.is_resisting_weapon_special(defender)) {
                return;
        }

        const bool is_ranged_wpn = wpn.data().type == ItemType::ranged_wpn;

        const auto allow_wound = is_ranged_wpn ? AllowWound::no : AllowWound::yes;

        const DmgType dmg_type = wpn.data().melee.dmg_type;

        if (dmg > 0) {
                actor::hit(
                        defender,
                        dmg,
                        dmg_type,
                        attacker,
                        allow_wound);

                if (defender.m_data->can_bleed &&
                    (is_physical_dmg_type(dmg_type) ||
                     (dmg_type == DmgType::pure) ||
                     (dmg_type == DmgType::light))) {
                        terrain::make_blood(defender.m_pos);
                }
        }

        wpn.on_melee_hit(defender, dmg);

        if (actor::is_alive(defender)) {
                apply_melee_attack_props(defender, attacker, wpn);

                if (wpn.data().melee.knocks_back) {
                        knock_back_target(defender, attacker_origin);
                }
        }
        else {
                // Defender was killed
                wpn.on_melee_kill(defender);
        }
}

static void bump_awareness_after_melee_attack(
        actor::Actor& attacker,
        actor::Actor& defender,
        const MeleeAttData& att_data)
{
        const bool attacker_is_player = actor::is_player(&attacker);
        const bool defender_is_player = actor::is_player(&defender);

        if (defender_is_player) {
                // A monster attacked the player.
                att_data.attacker->make_player_aware_of_me();
        }
        else {
                // A monster was attacked (by player or another monster).
                if (attacker_is_player ||
                    attacker.is_actor_my_leader(map::g_player)) {
                        // The player (or a monster allied to the player)
                        // attacked a monster. Make the defender monster aware
                        // of the player.
                        defender.become_aware_player(
                                actor::AwareSource::attacked);
                }

                if (attacker_is_player) {
                        // Player attacked monster, make player aware of the
                        // monster.
                        defender.make_player_aware_of_me();
                }
                else {
                        // A monster attacked a monster.

                        if (actor::can_player_see_actor(attacker) ||
                            (actor::can_player_see_actor(defender))) {
                                // Player saw either the attacker or the
                                // defender. Bump player awareness of both
                                // monsters.
                                attacker.make_player_aware_of_me();
                                defender.make_player_aware_of_me();
                        }
                }
        }
}

static terrain::Terrain* get_blocking_terrain_on_path_to_target(const P& origin, const P& target)
{
        // Terrain on the path to the target is considered blocking if it blocks
        // projectiles.
        //
        // Rationale: We do not want to check for "open terrain" (floor-like
        // terrain) here, since the player should be able to attack through some
        // non-open terrain such as barred gate. It is probably a good enough
        // approximation that you can attack with a long reach weapon through
        // terrain that you can also shoot through.
        //

        std::vector<P> line = line_calc::calc_new_line(origin, target, true, 999, false);

        // Do not travel into the target position, only check positions on the
        // way to the target.
        if (line.empty()) {
                ASSERT(false);

                return nullptr;
        }

        line.pop_back();

        for (const P& p : line) {
                if (p == origin) {
                        // If the attacker is standing inside blocking terrain
                        // for some reason, this terrain should not be hit or
                        // block the attack.
                        continue;
                }

                terrain::Terrain* const terrain = map::g_terrain.at(p);

                if (!terrain->is_projectile_passable()) {
                        return terrain;
                }
        }

        return nullptr;
}

static void print_attack_terrain_not_allowed_msg(
        terrain::Terrain& terrain,
        const item::Item& wpn)
{
        const std::string terrain_name = terrain.name(Article::the);

        const std::string wpn_name =
                wpn.name(
                        ItemNameType::a,
                        ItemNameInfo::none,
                        ItemNameAttackInfo::none);

        msg_log::add(
                "Attacking " +
                terrain_name +
                " with " +
                wpn_name +
                " would be useless.");
}

static void attack_actor(
        actor::Actor* const attacker,
        const P& origin,
        actor::Actor& defender,
        item::Wpn& wpn)
{
        const MeleeAttData att_data(attacker, defender, wpn);
        const ActionResult att_result = ability_roll::roll(att_data.hit_chance_tot);

        print_melee_attack_actor_msg(att_result, att_data.dmg, att_data);

        emit_melee_snd(att_result, att_data.dmg, att_data);

        if (att_result >= ActionResult::success) {
                if (actor::can_player_see_actor(defender)) {
                        io::flash_at_actor(defender, colors::light_red());
                }

                melee_hit_actor(att_data.dmg, defender, attacker, origin, wpn);
        }

        if (attacker) {
                bump_awareness_after_melee_attack(*attacker, defender, att_data);

                // Attacking ends cloaking and sanctuary.
                attacker->m_properties.end_prop(prop::Id::cloaked);
                attacker->m_properties.end_prop(prop::Id::sanctuary);

                attacker->m_properties.on_melee_attack();

                game_time::tick();
        }
}

static void do_melee_player_attacker(
        actor::Actor* const attacker,
        const P& origin,
        const P& aim_pos,
        item::Wpn& wpn)
{
        map::update_vision();

        actor::Actor* const defender = map::living_actor_at(aim_pos);

        terrain::Terrain* const blocking_terrain =
                get_blocking_terrain_on_path_to_target(origin, aim_pos);

        terrain::Terrain* const terrain_at_aim_pos = map::g_terrain.at(aim_pos);

        // --- Handle blocking terrain on the path to the position aimed at. ---
        // (This is applicable when using long reach melee weapons).
        if (blocking_terrain) {
                const bool can_use_wpn_on_terrain =
                        bash::is_allowed_use_wpn_on_terrain(wpn, *blocking_terrain);

                if (can_use_wpn_on_terrain) {
                        bash::attack_terrain(blocking_terrain->pos(), wpn);
                }
                else if (map::g_seen.at(blocking_terrain->pos())) {
                        print_attack_terrain_not_allowed_msg(*blocking_terrain, wpn);
                }
                else {
                        bash::do_fake_attack_on_unseen_terrain(aim_pos, wpn);
                }

                return;
        }

        // --- Attack known actor? ---
        if (defender && actor::is_player_aware_of_me(*defender)) {
                const bool is_melee_allowed =
                        map::g_player->m_properties.allow_attack_melee(
                                Verbose::yes);

                if (is_melee_allowed) {
                        attack_actor(attacker, origin, *defender, wpn);
                }

                return;
        }

        // --- Attack terrain that can be attacked with this weapon? ---
        if (bash::is_allowed_use_wpn_on_terrain(wpn, *terrain_at_aim_pos)) {
                bash::attack_terrain(aim_pos, wpn);

                return;
        }

        // --- Attack seen corpse? ---
        if (wpn.data().melee.can_attack_corpse && map::g_seen.at(aim_pos)) {
                actor::Actor* const corpse = bash::get_corpse_to_bash_at(aim_pos);

                if (corpse) {
                        bash::attack_corpse(*corpse, wpn);

                        return;
                }
        }

        // --- Attack living actor that the player is UNAWARE of? ---

        // NOTE: This is only allowed on some terrain, and only if the creature
        // is at least humanoid size - the player cannot melee attack a monster
        // on the floor that they are unaware of (it is assumed that the player
        // attacks into the air, not down at the floor - except possibly when
        // bashing a corpse, but supporting bashing a corpse and accidentally
        // hitting a creature on the floor doesn't really seem necessary).

        if (defender &&
            (defender->m_data->actor_size >= actor::Size::humanoid) &&
            bash::is_open_terrain(*terrain_at_aim_pos)) {
                const bool is_melee_allowed =
                        map::g_player->m_properties.allow_attack_melee(
                                Verbose::no);

                if (is_melee_allowed) {
                        attack_actor(attacker, origin, *defender, wpn);

                        return;
                }
        }

        // --- Swing into open terrain? ---
        if (bash::is_open_terrain(*terrain_at_aim_pos)) {
                // Attack "the air" (regardless of whether the position is seen
                // or not).
                bash::attack_air();

                return;
        }

        // --- Player knows there is nothing to attack here? ---
        if (map::g_seen.at(aim_pos)) {
                print_attack_terrain_not_allowed_msg(*terrain_at_aim_pos, wpn);

                return;
        }

        // --- Unseen position not possible to attack uknown monsters on. ---
        {
                bash::do_fake_attack_on_unseen_terrain(aim_pos, wpn);
        }
}

// NOTE: This is also used when there is no attacker (e.g. spear traps).
static void do_melee_non_player_attacker(
        actor::Actor* const attacker,
        const P& origin,
        const P& aim_pos,
        item::Wpn& wpn)
{
        if (attacker) {
                // A monster attacked, bump monster awareness.
                attacker->become_aware_player(actor::AwareSource::other);
        }

        actor::Actor* const defender = map::living_actor_at(aim_pos);

        if (!defender) {
                ASSERT(false);

                return;
        }

        map::update_vision();

        attack_actor(attacker, origin, *defender, wpn);
}

// -----------------------------------------------------------------------------
// attack
// -----------------------------------------------------------------------------
namespace attack
{
void melee(
        actor::Actor* const attacker,
        const P& origin,
        const P& aim_pos,
        item::Wpn& wpn)
{
        if (actor::is_player(attacker)) {
                do_melee_player_attacker(attacker, origin, aim_pos, wpn);
        }
        else {
                do_melee_non_player_attacker(attacker, origin, aim_pos, wpn);
        }
}

}  // namespace attack
