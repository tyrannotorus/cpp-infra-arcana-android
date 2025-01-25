// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "attack.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <memory>
#include <optional>
#include <ostream>
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
#include "colors.hpp"
#include "config.hpp"
#include "debug.hpp"
#include "game_time.hpp"
#include "gfx.hpp"
#include "global.hpp"
#include "io.hpp"
#include "item.hpp"
#include "item_att_property.hpp"
#include "item_data.hpp"
#include "item_weapon.hpp"
#include "knockback.hpp"
#include "line_calc.hpp"
#include "map.hpp"
#include "misc.hpp"
#include "msg_log.hpp"
#include "pos.hpp"
#include "property_data.hpp"
#include "property_handler.hpp"
#include "random.hpp"
#include "sound.hpp"
#include "state.hpp"
#include "terrain.hpp"
#include "terrain_data.hpp"
#include "text_format.hpp"
#include "viewport.hpp"
#include "wpn_dmg.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static const int s_nr_cell_jumps_mg_projectiles = 2;

struct Projectile
{
        P pos {0, 0};
        int dmg {0};
        int path_idx {0};
        bool is_dead {false};
        int obstructed_in_path_idx {-1};
        actor::Actor* actor_hit {nullptr};
        terrain::Terrain* terrain_hit {nullptr};
        bool is_seen_by_player {false};
        io::MapDrawObj draw_obj;
        // Used for drawing a trail (e.g. a beam weapon):
        std::vector<io::MapDrawObj> drawn_trail;
        std::unique_ptr<RangedAttData> att_data {nullptr};

#ifndef NDEBUG
        friend std::ostream& operator<<(
                std::ostream& os,
                const Projectile& p)
        {
                os
                        << "POS: "
                        << "{"
                        << p.pos.x
                        << " ,"
                        << p.pos.y
                        << "}"
                        << " - PATH IDX: "
                        << p.path_idx
                        << " - OBSTRUCTED AT IDX: "
                        << p.obstructed_in_path_idx;

                const std::string live_str =
                        p.is_dead
                        ? " - DEAD"
                        : " - LIVE";

                os << live_str;

                if (p.actor_hit) {
                        os
                                << " - ACTOR HIT: "
                                << actor::name_a(*p.actor_hit);
                }

                if (p.terrain_hit) {
                        os
                                << " - TERRAIN HIT: "
                                << p.terrain_hit->name(Article::a);
                }

                return os;
        }
#endif  // NDEBUG
};

struct ProjectileFireData
{
        std::vector<Projectile> projectiles {};
        std::vector<P> path {};
        std::vector<actor::Actor*> actors_seen;
        P origin {0, 0};
        P aim_pos {0, 0};
        actor::Size aim_lvl {(actor::Size)0};
        actor::Actor* attacker = nullptr;
        item::Wpn* wpn = nullptr;
        int animation_delay = 0;
        bool projectile_animation_leaves_trail = false;

#ifndef NDEBUG
        friend std::ostream& operator<<(
                std::ostream& os,
                const ProjectileFireData& d)
        {
                os
                        << "PROJECTILE FIRE DATA:"
                        << std::endl
                        << "ORIGIN: "
                        << "{"
                        << d.origin.x
                        << " ,"
                        << d.origin.y
                        << "}"
                        << std::endl
                        << "AIM POS: "
                        << "{"
                        << d.aim_pos.x
                        << " ,"
                        << d.aim_pos.y
                        << "}"
                        << std::endl
                        << "AIM LVL: "
                        << (int)d.aim_lvl
                        << std::endl;

                os << "PATH: ";

                for (const P& p : d.path) {
                        os
                                << "{"
                                << p.x
                                << " ,"
                                << p.y
                                << "} ";
                }

                os << std::endl;

                os
                        << "PROJECTILES:"
                        << std::endl;

                for (const Projectile& proj : d.projectiles) {
                        os << "    " << proj << std::endl;
                }

                return os;
        }
#endif  // NDEBUG
};

static size_t nr_projectiles_for_ranged_weapon(const item::Wpn& wpn)
{
        size_t nr_projectiles = 1;

        if (wpn.data().ranged.is_machine_gun) {
                nr_projectiles = g_nr_mg_projectiles;
        }

        return nr_projectiles;
}

static attack::HitSize relative_hit_size_ranged(const int dmg, const AttData& att_data)
{
        const int max_dmg = att_data.dmg_range.total_range().max;

        return attack::relative_hit_size(dmg, max_dmg);
}

static void print_player_fire_ranged_msg(const item::Wpn& wpn)
{
        const std::string attack_verb = wpn.data().ranged.attack_msgs.player;

        msg_log::add("I " + attack_verb + ".");
}

static void print_mon_fire_ranged_msg(const RangedAttData& att_data)
{
        if (!can_player_see_actor(*att_data.attacker)) {
                return;
        }

        const std::string attacker_name_the =
                text_format::first_to_upper(
                        actor::name_the(
                                *att_data.attacker));

        const std::string attack_verb =
                att_data.att_item->data().ranged.attack_msgs.other;

        std::string wpn_used_str;

        if (!att_data.att_item->data().is_intr) {
                const std::string wpn_name_a =
                        att_data.att_item->name(
                                ItemNameType::a,
                                ItemNameInfo::none,
                                ItemNameAttackInfo::none);

                wpn_used_str = " " + wpn_name_a;
        }

        const std::string msg =
                attacker_name_the +
                " " +
                attack_verb +
                wpn_used_str +
                ".";

        const auto interrupt =
                (actor::is_player(att_data.defender))
                ? MsgInterruptPlayer::yes
                : MsgInterruptPlayer::no;

        msg_log::add(msg, colors::text(), interrupt);
}

static void print_ranged_fire_msg(
        const RangedAttData& att_data,
        const item::Wpn& wpn)
{
        if (!att_data.attacker) {
                // No attacker actor (e.g. a trap firing a dart)
                return;
        }

        if (actor::is_player(att_data.attacker)) {
                print_player_fire_ranged_msg(wpn);
        }
        else {
                print_mon_fire_ranged_msg(att_data);
        }
}

static void print_projectile_hit_player_msg(const Projectile& projectile)
{
        const std::string dmg_punct =
                hit_size_punctuation_str(
                        relative_hit_size_ranged(
                                projectile.dmg,
                                *projectile.att_data));

        // NOTE: Interruption is not needed here since the player will be
        // interrupted by getting hit.
        msg_log::add("I am hit" + dmg_punct, colors::msg_bad());
}

static void print_projectile_hit_mon_msg(const Projectile& projectile)
{
        std::string other_name = "It";

        const actor::Actor& defender = *projectile.att_data->defender;

        if (can_player_see_actor(defender)) {
                other_name =
                        text_format::first_to_upper(
                                actor::name_the(defender));
        }

        const std::string dmg_punct =
                hit_size_punctuation_str(
                        relative_hit_size_ranged(
                                projectile.dmg,
                                *projectile.att_data));

        msg_log::add(other_name + " is hit" + dmg_punct, colors::msg_good());
}

static void print_projectile_hit_actor_msg(const Projectile& projectile)
{
        ASSERT(projectile.att_data->defender);

        if (actor::is_player(projectile.att_data->defender)) {
                print_projectile_hit_player_msg(projectile);
        }
        else {
                // Defender is monster
                const P& pos = projectile.att_data->defender->m_pos;

                if (!map::g_seen.at(pos)) {
                        return;
                }

                print_projectile_hit_mon_msg(projectile);
        }
}

static std::unique_ptr<Snd> ranged_fire_snd(
        const AttData& att_data,
        const item::Wpn& wpn,
        const P& origin)
{
        std::unique_ptr<Snd> snd;

        const std::string snd_msg = wpn.data().ranged.snd_msg;

        if (snd_msg.empty()) {
                return snd;
        }

        const audio::SfxId sfx = wpn.data().ranged.attack_sfx;
        const SndVol vol = wpn.data().ranged.snd_vol;

        std::string snd_msg_used = snd_msg;

        if (actor::is_player(att_data.attacker)) {
                snd_msg_used = "";
        }

        snd =
                std::make_unique<Snd>(
                        snd_msg_used,
                        sfx,
                        IgnoreMsgIfOriginSeen::yes,
                        origin,
                        att_data.attacker,
                        vol,
                        AlertsMon::yes);

        return snd;
}

static void emit_projectile_hit_actor_snd(const P& pos)
{
        Snd snd(
                "A creature is hit.",
                audio::SfxId::hit_small,
                IgnoreMsgIfOriginSeen::yes,
                pos,
                nullptr,
                SndVol::low,
                AlertsMon::no);

        snd.run();
}

static void emit_projectile_hit_terrain_snd(
        const P& pos,
        const item::Wpn& wpn)
{
        if (wpn.data().ranged.makes_ricochet_snd) {
                // TODO: Check hit material, soft and wood should not cause
                // a ricochet sound
                Snd snd(
                        "I hear a ricochet.",
                        audio::SfxId::ricochet,
                        IgnoreMsgIfOriginSeen::yes,
                        pos,
                        nullptr,
                        SndVol::low,
                        AlertsMon::yes);

                snd_emit::run(snd);
        }
}

static actor::Actor* get_actor_hit_by_projectile(
        const ActionResult att_result,
        const Projectile& projectile)
{
        const RangedAttData& att_data = *projectile.att_data;

        if (!att_data.defender) {
                return nullptr;
        }

        const bool is_actor_aimed_for = (projectile.pos == att_data.aim_pos);

        const bool can_hit_height =
                (att_data.defender_size >= actor::Size::humanoid) ||
                is_actor_aimed_for;

        if (!projectile.is_dead &&
            (att_result >= ActionResult::success) &&
            can_hit_height) {
                return att_data.defender;
        }

        return nullptr;
}

static terrain::Terrain* get_ground_blocking_projectile(
        const Projectile& projectile)
{
        // Hit the ground?
        const RangedAttData& att_data = *projectile.att_data;

        const P& pos = projectile.pos;

        const bool has_hit_ground =
                (pos == att_data.aim_pos) &&
                (att_data.aim_lvl == actor::Size::floor);

        if (has_hit_ground) {
                return map::g_terrain.at(pos);
        }
        else {
                return nullptr;
        }
}

static void apply_ranged_attack_props(
        const Projectile& projectile,
        item::Wpn& wpn)
{
        ItemAttackProp att_prop = wpn.prop_applied_on_ranged(projectile.att_data->attacker);

        if (att_prop.prop) {
                attack::try_apply_attack_property_on_actor(
                        att_prop,
                        *projectile.actor_hit,
                        wpn.data().ranged.dmg_type);
        }
}

static void hit_actor_with_projectile(
        const Projectile& projectile,
        item::Wpn& wpn)
{
        ASSERT(projectile.actor_hit);

        if (!projectile.actor_hit) {
                return;
        }

        const RangedAttData& att_data = *projectile.att_data;

        if (actor::is_player(att_data.attacker)) {
                att_data.defender->make_player_aware_of_me();
        }

        if (projectile.dmg > 0) {
                actor::hit(
                        *projectile.actor_hit,
                        projectile.dmg,
                        wpn.data().ranged.dmg_type,
                        projectile.att_data->attacker,
                        AllowWound::yes);
        }

        // NOTE: This is run regardless of whether the defender died or not.
        wpn.on_ranged_hit(*projectile.actor_hit);

        wpn.on_projectile_blocked(projectile.pos);

        if (actor::is_alive(*projectile.actor_hit)) {
                apply_ranged_attack_props(projectile, wpn);

                // Knock-back?
                if (wpn.data().ranged.knocks_back &&
                    att_data.attacker) {
                        const auto knockback_source =
                                (wpn.data().id == item::Id::spike_gun)
                                ? knockback::KnockbackSource::spike_gun
                                : knockback::KnockbackSource::other;

                        knockback::run(
                                *(att_data.defender),
                                att_data.attacker->m_pos,
                                knockback_source);
                }
        }
}

static void hit_terrain_with_projectile(
        const P& projectile_pos,
        const P& terrain_pos,
        item::Wpn& wpn)
{
        (void)terrain_pos;

        // TODO: This was in the 'shotgun' function (but only the shotgun was
        // calling the terrain 'hit' method in attack.cpp - the normal
        // projectile firing did not do this). Reimplement somehow.
        // cell.terrain->hit(
        //         att_data.dmg,
        //         DmgType::physical,
        //         DmgMethod::shotgun,
        //         nullptr);

        wpn.on_projectile_blocked(projectile_pos);
}

static void init_projectiles_gfx(ProjectileFireData& fire_data)
{
        ASSERT(fire_data.wpn);
        ASSERT(!fire_data.path.empty());

        if (!fire_data.wpn || fire_data.path.empty()) {
                return;
        }

        fire_data.projectile_animation_leaves_trail =
                fire_data.wpn->data().ranged.projectile_leaves_trail;

        char projectile_character =
                fire_data.wpn->data().ranged.projectile_character;

        if (projectile_character == '/') {
                const std::vector<P>& path = fire_data.path;
                const P& origin = fire_data.origin;

                const int i = (path.size() > 2) ? 2 : 1;

                const P& ref_pos = path[i];

                if (ref_pos.y == origin.y) {
                        projectile_character = '-';
                }
                else if (ref_pos.x == origin.x) {
                        projectile_character = '|';
                }
                else if (
                        ((ref_pos.x > origin.x) && (ref_pos.y < origin.y)) ||
                        ((ref_pos.x < origin.x) && (ref_pos.y > origin.y))) {
                        projectile_character = '/';
                }
                else if (
                        ((ref_pos.x > origin.x) && (ref_pos.y > origin.y)) ||
                        ((ref_pos.x < origin.x) && (ref_pos.y < origin.y))) {
                        projectile_character = '\\';
                }
        }

        gfx::TileId projectile_tile = fire_data.wpn->data().ranged.projectile_tile;

        if (projectile_tile == gfx::TileId::projectile_std_front_slash) {
                if (projectile_character == '-') {
                        projectile_tile = gfx::TileId::projectile_std_dash;
                }
                else if (projectile_character == '|') {
                        projectile_tile = gfx::TileId::projectile_std_vertical;
                }
                else if (projectile_character == '\\') {
                        projectile_tile = gfx::TileId::projectile_std_back_slash;
                }
        }

        const Color projectile_color = fire_data.wpn->data().ranged.projectile_color;

        for (Projectile& projectile : fire_data.projectiles) {
                projectile.draw_obj.tile = projectile_tile;
                projectile.draw_obj.character = projectile_character;
                projectile.draw_obj.color = projectile_color;
        }
}

static void init_projectiles_animation_delay(ProjectileFireData& fire_data)
{
        const auto denom = (int)fire_data.projectiles.size();

        fire_data.animation_delay = (config::delay_projectile_draw() / denom);
}

static ProjectileFireData init_projectiles_fire_data(
        actor::Actor* const attacker,
        item::Wpn& wpn,
        const P& origin,
        const P& aim_pos)
{
        ProjectileFireData fire_data;

        fire_data.attacker = attacker;
        fire_data.wpn = &wpn;
        fire_data.origin = origin;
        fire_data.aim_pos = aim_pos;

        const size_t nr_projectiles = nr_projectiles_for_ranged_weapon(wpn);

        fire_data.projectiles.resize(nr_projectiles);

        for (size_t i = 0; i < nr_projectiles; ++i) {
                Projectile& proj = fire_data.projectiles[i];

                // Projectile path indexes are initially set up so that all
                // projectiles start with a negative index (they are queued up
                // to enter the path)
                proj.path_idx =
                        -((int)i * s_nr_cell_jumps_mg_projectiles) - 1;

                proj.att_data =
                        std::make_unique<RangedAttData>(
                                fire_data.attacker,
                                fire_data.origin,
                                fire_data.aim_pos,
                                fire_data.origin,  // Current position
                                *fire_data.wpn,
                                std::nullopt);  // Undefined aim level
        }

        fire_data.aim_lvl = fire_data.projectiles[0].att_data->aim_lvl;

        const bool stop_at_target = (fire_data.aim_lvl == actor::Size::floor);

        fire_data.path =
                line_calc::calc_new_line(
                        fire_data.origin,
                        fire_data.aim_pos,
                        stop_at_target,
                        999,
                        false);

        init_projectiles_gfx(fire_data);

        init_projectiles_animation_delay(fire_data);

        return fire_data;
}

static bool is_all_projectiles_dead(
        const std::vector<Projectile>& projectiles)
{
        return (
                std::all_of(
                        std::cbegin(projectiles),
                        std::cend(projectiles),
                        [](const Projectile& projectile) {
                                return projectile.is_dead;
                        }));
}

static void run_projectile_hits(ProjectileFireData& fire_data)
{
        for (Projectile& projectile : fire_data.projectiles) {
                if (projectile.is_dead) {
                        continue;
                }

                if (projectile.actor_hit) {
                        hit_actor_with_projectile(projectile, *fire_data.wpn);

                        projectile.is_dead = true;

                        continue;
                }

                if (projectile.terrain_hit) {
                        const int prev_idx = projectile.path_idx - 1;

                        ASSERT(prev_idx >= 0);

                        if (prev_idx < 0) {
                                continue;
                        }

                        P current_pos;
                        P terrain_pos;

                        if (projectile.terrain_hit->is_projectile_passable()) {
                                // The terrain does not block projectiles -
                                // assuming this is floor
                                current_pos = projectile.pos;
                                terrain_pos = current_pos;
                        }
                        else {
                                // The terrain blocks projectiles (e.g. a wall),
                                // do not consider the projectile to be inside
                                // the terrain, but in the cell before it
                                current_pos = fire_data.path[prev_idx];
                                terrain_pos = projectile.pos;
                        }

                        hit_terrain_with_projectile(
                                current_pos,
                                terrain_pos,
                                *fire_data.wpn);

                        projectile.is_dead = true;

                        continue;
                }
        }
}

static void advance_projectiles_on_path(ProjectileFireData& fire_data)
{
        for (Projectile& projectile : fire_data.projectiles) {
                if (projectile.is_dead) {
                        continue;
                }

                if (projectile.path_idx == (int)fire_data.path.size() - 1) {
                        // Projectile is already at the maximum path index.
                        // This situation is unexpected - consider it a critical
                        // error in debug mode, and just kill the projectile in
                        // release mode.
#ifndef NDEBUG
                        TRACE << fire_data;

                        ASSERT(false);
#endif  // NDEBUG

                        projectile.is_dead = true;

                        continue;
                }

                ++projectile.path_idx;

                if (projectile.path_idx >= 0) {
                        projectile.pos = fire_data.path[projectile.path_idx];
                }
        }
}

static terrain::Terrain* get_terrain_blocking_projectile(const P& pos)
{
        terrain::Terrain* terrain = map::g_terrain.at(pos);

        if (!terrain->is_projectile_passable()) {
                return terrain;
        }

        for (terrain::Terrain* const mob : game_time::g_mobs) {
                if (mob->pos() == pos && !mob->is_projectile_passable()) {
                        return mob;
                }
        }

        return nullptr;
}

static void update_projectile_states(ProjectileFireData& fire_data)
{
        advance_projectiles_on_path(fire_data);

        for (Projectile& projectile : fire_data.projectiles) {
                if (projectile.is_dead ||
                    (projectile.path_idx < 1)) {
                        continue;
                }

                const P& projectile_pos = fire_data.path[projectile.path_idx];

                projectile.att_data =
                        std::make_unique<RangedAttData>(
                                fire_data.attacker,
                                fire_data.origin,
                                fire_data.aim_pos,
                                projectile_pos,
                                *fire_data.wpn,
                                fire_data.aim_lvl);

                if (projectile.att_data->defender) {
                        // The projectile is at a potential target to attack,
                        // store information about an encountered actor.
                        fire_data.actors_seen.push_back(
                                projectile.att_data->defender);
                }

                const ActionResult att_result =
                        ability_roll::roll(projectile.att_data->hit_chance_tot);

                projectile.dmg = projectile.att_data->dmg_range.total_range().roll();

                projectile.is_seen_by_player = map::g_seen.at(projectile_pos);

                // Projectile out of weapon max range?
                const int max_range = fire_data.wpn->data().ranged.max_range;

                if (king_dist(fire_data.origin, projectile_pos) > max_range) {
                        projectile.is_dead = true;
                        projectile.obstructed_in_path_idx = projectile.path_idx;

                        continue;
                }

                projectile.actor_hit = get_actor_hit_by_projectile(att_result, projectile);

                if (projectile.actor_hit) {
                        projectile.obstructed_in_path_idx = projectile.path_idx;
                        projectile.draw_obj.color = colors::light_red();

                        continue;
                }

                projectile.terrain_hit = get_terrain_blocking_projectile(projectile.pos);

                if (projectile.terrain_hit) {
                        projectile.obstructed_in_path_idx = projectile.path_idx;
                        projectile.draw_obj.color = colors::yellow();

                        continue;
                }

                projectile.terrain_hit =
                        get_ground_blocking_projectile(projectile);

                if (projectile.terrain_hit) {
                        projectile.obstructed_in_path_idx = projectile.path_idx;

                        const terrain::Id terrain_id = projectile.terrain_hit->id();

                        if (terrain_id == terrain::Id::liquid) {
                                projectile.draw_obj.color =
                                        projectile.terrain_hit->color();
                        }
                        else {
                                projectile.draw_obj.color =
                                        colors::yellow();
                        }

                        continue;
                }
        }
}

static void run_projectiles_messages_and_sounds(
        const ProjectileFireData& fire_data)
{
        for (const Projectile& projectile : fire_data.projectiles) {
                if (projectile.is_dead) {
                        continue;
                }

                // Projectile entering the path this update?
                if (projectile.path_idx == 0) {
                        // NOTE: The initial attack sound(s) must NOT alert
                        // monsters, since this would immediately make them
                        // aware before any attack data is set. This would
                        // result in the player not getting ranged attack
                        // bonuses against unaware monsters.
                        // An extra sound is run when the attack ends (without a
                        // message or audio), which may alert monsters.
                        std::unique_ptr<Snd> snd =
                                ranged_fire_snd(
                                        *projectile.att_data,
                                        *fire_data.wpn,
                                        fire_data.origin);

                        if (snd) {
                                snd->set_alerts_mon(AlertsMon::no);

                                snd->run();
                        }

                        continue;
                }

                if (projectile.actor_hit) {
                        emit_projectile_hit_actor_snd(projectile.pos);

                        print_projectile_hit_actor_msg(projectile);

                        continue;
                }

                if (projectile.terrain_hit) {
                        emit_projectile_hit_terrain_snd(
                                projectile.pos,
                                *fire_data.wpn);
                }
        }
}

static void draw_projectile(const Projectile& projectile)
{
        if (projectile.draw_obj.tile == gfx::TileId::END) {
                return;
        }

        if (!viewport::is_in_view(projectile.pos)) {
                return;
        }

        projectile.draw_obj.draw();
}

static void draw_previous_trail(const std::vector<io::MapDrawObj>& draw_objs)
{
        for (const io::MapDrawObj& draw_obj : draw_objs) {
                draw_obj.draw();
        }
}

static bool should_draw_projectile_as_travelling(const Projectile& projectile)
{
        return (
                !projectile.is_dead &&
                projectile.is_seen_by_player &&
                (projectile.path_idx >= 1) &&
                (projectile.obstructed_in_path_idx < 0));
}

static bool should_draw_projectile_as_hit(const Projectile& projectile)
{
        return (
                !projectile.is_dead &&
                projectile.is_seen_by_player &&
                (projectile.obstructed_in_path_idx >= 0));
}

static void draw_projectiles(ProjectileFireData& fire_data)
{
        states::draw();

        for (Projectile& projectile : fire_data.projectiles) {
                projectile.draw_obj.pos = viewport::to_view_pos(projectile.pos);

                if (should_draw_projectile_as_travelling(projectile)) {
                        // Draw travelling projectile.
                        draw_projectile(projectile);

                        if (fire_data.projectile_animation_leaves_trail) {
                                draw_previous_trail(projectile.drawn_trail);
                        }

                        projectile.drawn_trail.push_back(projectile.draw_obj);
                }
                else if (should_draw_projectile_as_hit(projectile)) {
                        // Draw projectile hit.
                        projectile.draw_obj.tile =
                                (projectile.draw_obj.tile == gfx::TileId::blast1)
                                ? gfx::TileId::blast2
                                : gfx::TileId::blast1;

                        projectile.draw_obj.character = '*';

                        draw_projectile(projectile);
                }
        }

        io::update_screen();
}

static bool is_any_projectile_seen(const std::vector<Projectile>& projectiles)
{
        return (
                std::any_of(
                        std::cbegin(projectiles),
                        std::cend(projectiles),
                        [](const Projectile& projectile) {
                                return projectile.is_seen_by_player;
                        }));
}

static ProjectileFireData fire_projectiles(
        actor::Actor* const attacker,
        item::Wpn& wpn,
        const P& origin,
        const P& aim_pos)
{
        ProjectileFireData fire_data =
                init_projectiles_fire_data(
                        attacker,
                        wpn,
                        origin,
                        aim_pos);

        print_ranged_fire_msg(
                *fire_data.projectiles[0].att_data,
                *fire_data.wpn);

        while (true) {
                if (is_all_projectiles_dead(fire_data.projectiles)) {
                        // Run a sound without message or audio, which can alert
                        // monsters (the initial fire sound is not allowed to
                        // alert monsters, since this would prevent ranged
                        // attack bonuses against unaware monsters)
                        const RangedAttData& att_data = *fire_data.projectiles[0].att_data;

                        std::unique_ptr<Snd> snd =
                                ranged_fire_snd(
                                        att_data,
                                        *fire_data.wpn,
                                        fire_data.origin);

                        if (snd) {
                                snd->clear_msg();

                                snd->clear_sfx();

                                snd->run();
                        }

                        return fire_data;
                }

                update_projectile_states(fire_data);

                run_projectiles_messages_and_sounds(fire_data);

                // NOTE: Here we draw the projectiles twice and sleep twice -
                // each draw call will progress hit animations (the animation
                // has two steps).
                for (int i = 0; i <= 1; ++i) {
                        draw_projectiles(fire_data);

                        if (is_any_projectile_seen(fire_data.projectiles)) {
                                io::sleep(fire_data.animation_delay / 2);
                        }
                }

                run_projectile_hits(fire_data);
        }

        return {};
}

// -----------------------------------------------------------------------------
// attack
// -----------------------------------------------------------------------------
namespace attack
{
DidAction ranged(
        actor::Actor* const attacker,
        const P& origin,
        const P& aim_pos,
        item::Wpn& wpn)
{
        map::update_vision();

        DidAction did_attack = DidAction::no;

        const bool has_inf_ammo = wpn.data().ranged.has_infinite_ammo;

        const int nr_projectiles = (int)nr_projectiles_for_ranged_weapon(wpn);

        ProjectileFireData projectile_data;

        if ((wpn.m_ammo_loaded >= nr_projectiles) || has_inf_ammo) {
                wpn.pre_ranged_attack();

                projectile_data = fire_projectiles(attacker, wpn, origin, aim_pos);

                did_attack = DidAction::yes;

                if (!has_inf_ammo) {
                        wpn.m_ammo_loaded -= nr_projectiles;
                }

                // Player could have for example fired an explosive weapon into
                // a wall and killed themselves - if so, abort early.
                if (!actor::is_alive(*map::g_player)) {
                        return DidAction::yes;
                }
        }

        states::draw();

        if ((did_attack == DidAction::yes) && attacker) {
                // Attacking ends cloaking and sanctuary.
                attacker->m_properties.end_prop(prop::Id::cloaked);
                attacker->m_properties.end_prop(prop::Id::sanctuary);

                if (actor::is_player(attacker) ||
                    attacker->is_actor_my_leader(map::g_player)) {
                        // Attacker is player, or a monster allied to the
                        // player, alert all encountered monsters.
                        for (actor::Actor* const actor : projectile_data.actors_seen) {
                                if (actor::is_player(actor)) {
                                        continue;
                                }

                                actor->become_aware_player(actor::AwareSource::attacked);
                        }
                }

                if (!actor::is_player(attacker)) {
                        // A monster attacked, bump its awareness.
                        attacker->become_aware_player(actor::AwareSource::other);
                }

                game_time::tick();
        }

        return did_attack;
}

}  // namespace attack
