// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "explosion.hpp"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>

#include "actor.hpp"
#include "actor_hit.hpp"
#include "array2.hpp"
#include "audio_data.hpp"
#include "colors.hpp"
#include "config.hpp"
#include "game.hpp"
#include "game_time.hpp"
#include "gfx.hpp"
#include "global.hpp"
#include "inventory.hpp"
#include "io.hpp"
#include "item.hpp"
#include "item_data.hpp"
#include "item_explosive.hpp"
#include "line_calc.hpp"
#include "map.hpp"
#include "map_parsing.hpp"
#include "misc.hpp"
#include "msg_log.hpp"
#include "panel.hpp"
#include "pos.hpp"
#include "property.hpp"
#include "property_data.hpp"
#include "property_factory.hpp"
#include "property_handler.hpp"
#include "random.hpp"
#include "screen_shake.hpp"
#include "sound.hpp"
#include "state.hpp"
#include "terrain.hpp"
#include "terrain_data.hpp"
#include "terrain_factory.hpp"
#include "terrain_mob.hpp"
#include "viewport.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static bool is_player_protected_from_gas()
{
        // Do not apply effect if wearing Asbestos Suite
        // or Gas Mask, and this is a gas explosion
        const auto* const body_item =
                map::g_player->m_inv.item_in_slot(
                        SlotId::body);

        if (body_item && (body_item->id() == item::Id::armor_asb_suit)) {
                return true;
        }

        const auto* const head_item =
                map::g_player->m_inv.item_in_slot(
                        SlotId::head);

        if (head_item && (head_item->id() == item::Id::gas_mask)) {
                return true;
        }

        return false;
}

static bool is_actor_gas_immune(const actor::Actor& actor)
{
        if (actor.m_properties.has(prop::Id::r_breath)) {
                return true;
        }

        if (actor::is_player(&actor) && is_player_protected_from_gas()) {
                return true;
        }

        return false;
}

static std::vector<std::vector<P>> positions_reached(
        const R& area,
        const P& origin,
        const ExplExclCenter exclude_center,
        const Array2<bool>& blocked)
{
        std::vector<std::vector<P>> out;

        for (int y = area.p0.y; y <= area.p1.y; ++y) {
                for (int x = area.p0.x; x <= area.p1.x; ++x) {
                        const P pos(x, y);

                        if (exclude_center == ExplExclCenter::yes &&
                            pos == origin) {
                                continue;
                        }

                        const int dist = king_dist(pos, origin);

                        bool is_reached = true;

                        if (dist > 1) {
                                const auto path =
                                        line_calc::calc_new_line(
                                                origin,
                                                pos,
                                                true,
                                                999,
                                                false);

                                for (const P& path_pos : path) {
                                        if (blocked.at(path_pos)) {
                                                is_reached = false;
                                                break;
                                        }
                                }
                        }

                        if (is_reached) {
                                if ((int)out.size() <= dist) {
                                        out.resize(dist + 1);
                                }

                                out[dist].push_back(pos);
                        }
                }
        }

        return out;
}

static void draw(
        const std::vector<std::vector<P>>& pos_lists,
        const Array2<bool>& blocked,
        const std::optional<Color>& color_override)
{
        const auto color_inner = color_override.value_or(colors::yellow());
        const auto color_outer = color_override.value_or(colors::light_red());

        const bool is_tiles = config::is_tiles_mode();

        const int nr_anim_steps = is_tiles ? 2 : 1;

        bool is_any_cell_seen_by_player = false;

        for (int i_anim = 0; i_anim < nr_anim_steps; i_anim++) {
                states::draw();

                const gfx::TileId tile =
                        (i_anim == 0)
                        ? gfx::TileId::blast1
                        : gfx::TileId::blast2;

                const int nr_outer = (int)pos_lists.size();

                for (int i_outer = 0; i_outer < nr_outer; i_outer++) {
                        const Color& color =
                                (i_outer == nr_outer - 1)
                                ? color_outer
                                : color_inner;

                        const std::vector<P>& inner = pos_lists[i_outer];

                        for (const P& pos : inner) {
                                if (!map::g_seen.at(pos) ||
                                    blocked.at(pos) ||
                                    !viewport::is_in_view(pos)) {
                                        continue;
                                }

                                is_any_cell_seen_by_player = true;

                                io::MapDrawObj render_obj;
                                render_obj.tile = tile;
                                render_obj.character = '*';
                                render_obj.pos = viewport::to_view_pos(pos);
                                render_obj.color = color;

                                render_obj.draw();
                        }
                }

                if (is_any_cell_seen_by_player) {
                        if (i_anim == 0) {
                                // The ground shakes with the blast. It runs
                                // through the delay below (io::sleep steps
                                // it), so the shake costs no time of its
                                // own - and it settles at about the same
                                // moment the last blast frame clears.
                                screen_shake::on_explosion(
                                        (uint32_t)config::delay_explosion());
                        }

                        io::update_screen();

                        io::sleep(config::delay_explosion() / nr_anim_steps);
                }
        }
}

static void apply_explosion_on_pos(
        const P& pos,
        const int current_dist,
        actor::Actor* living_actor,
        const std::vector<actor::Actor*>& corpses_here)
{
        const Range dmg_range(
                g_expl_dmg_min - current_dist,
                g_expl_dmg_max - (current_dist * 5));

        const int dmg = dmg_range.roll();

        // Damage environment
        map::g_terrain.at(pos)->hit(DmgType::explosion, nullptr);

        // Damage living actor
        if (living_actor) {
                if (actor::is_player(living_actor)) {
                        msg_log::add(
                                "I am hit by an explosion!",
                                colors::msg_bad());
                }

                actor::hit(
                        *living_actor,
                        dmg,
                        DmgType::explosion,
                        nullptr);

                if (actor::is_alive(*living_actor) &&
                    actor::is_player(living_actor)) {
                        // Player survived being hit by an explosion, that's
                        // pretty cool!
                        game::add_history_event("Survived an explosion");
                }
        }

        // Damage dead actors
        for (auto* corpse : corpses_here) {
                actor::hit(
                        *corpse,
                        dmg,
                        DmgType::explosion,
                        nullptr);
        }

        // Add smoke
        if (rnd::fraction(6, 10)) {
                auto* const smoke =
                        static_cast<terrain::Smoke*>(
                                terrain::make(terrain::Id::smoke, pos));

                smoke->set_nr_turns(rnd::range(2, 4));

                game_time::add_mob(smoke);
        }

        // The blast sets off any unlit explosives lying here. Done last,
        // so that this cell is fully resolved (e.g. a carrier killed above
        // has dropped their items) before the chained explosion runs.
        item::trigger_explosives_on_ground_at(pos);
}

static void apply_explosion_property_on_pos(
        const P& pos,
        prop::Prop* property,
        actor::Actor* living_actor,
        const std::vector<actor::Actor*>& corpses_here,
        const ExplIsGas is_gas)
{
        // TODO: Add a test that checks gas immunity
        const bool should_apply_on_living_actor =
                living_actor &&
                !((is_gas == ExplIsGas::yes) &&
                  is_actor_gas_immune(*living_actor));

        if (should_apply_on_living_actor) {
                prop::Prop* const prop_cpy = prop::make(property->id());

                prop_cpy->set_duration(property->nr_turns_left());

                living_actor->m_properties.apply(prop_cpy);
        }

        // If property is burning, also apply it to corpses and the environment
        if (property->id() == prop::Id::burning) {
                map::g_terrain.at(pos)->hit(DmgType::fire, nullptr);

                for (auto* corpse : corpses_here) {
                        auto* const prop_cpy =
                                prop::make(property->id());

                        prop_cpy->set_duration(property->nr_turns_left());

                        corpse->m_properties.apply(prop_cpy);
                }

                // The flames set off any unlit explosives lying here
                item::trigger_explosives_on_ground_at(pos);
        }
}

// I.e. smoke/mist.
//
// TODO: Could be used for lingering gas effects in the future.
//
// TODO: Instead of using a function pointer, all of the explosion stuff should be handled by some
// class based/composition system instead.
//
static void run_explosion_with_lingering_terrain_at(
        const P& origin,
        const int radi_change,
        terrain::Terrain* (*terrain_builder)(const P& pos))
{
        const int radi = g_expl_std_radi + radi_change;

        const R area = explosion::explosion_area(origin, radi);

        Array2<bool> blocked(map::dims());

        map_parsers::BlocksProjectiles().run(blocked, area);

        std::vector<std::vector<P>> pos_lists =
                positions_reached(
                        area,
                        origin,
                        ExplExclCenter::no,
                        blocked);

        // TODO: Sound message?
        Snd snd("",
                audio::SfxId::END,
                IgnoreMsgIfOriginSeen::yes,
                origin,
                nullptr,
                SndVol::low,
                AlertsMon::yes);

        snd_emit::run(snd);

        for (const std::vector<P>& inner : pos_lists) {
                for (const P& pos : inner) {
                        if (blocked.at(pos)) {
                                continue;
                        }

                        terrain::Terrain* const terrain = terrain_builder(pos);

                        game_time::add_mob(terrain);
                }
        }

        map::update_vision();
}

static terrain::Terrain* make_smoke(const P& pos)
{
        auto* const smoke =
                static_cast<terrain::Smoke*>(
                        terrain::make(terrain::Id::smoke, pos));

        smoke->set_nr_turns(rnd::range(25, 30));

        return smoke;
}

static terrain::Terrain* make_mist(const P& pos)
{
        auto* const mist =
                static_cast<terrain::Mist*>(
                        terrain::make(terrain::Id::mist, pos));

        mist->set_nr_turns(rnd::range(25, 30));

        return mist;
}

// -----------------------------------------------------------------------------
// explosion
// -----------------------------------------------------------------------------
namespace explosion
{
void run(
        const P& origin,
        const ExplType expl_type,
        const EmitExplSnd emit_expl_snd,
        const int radi_change,
        const ExplExclCenter exclude_center,
        const std::vector<prop::Prop*>& properties_applied,
        const std::optional<Color>& color_override,
        const ExplIsGas is_gas)
{
        const int radi = g_expl_std_radi + radi_change;

        const R area = explosion_area(origin, radi);

        Array2<bool> blocked(map::dims());

        map_parsers::BlocksProjectiles().run(blocked, area);

        auto pos_lists =
                positions_reached(
                        area,
                        origin,
                        exclude_center,
                        blocked);

        if (emit_expl_snd == EmitExplSnd::yes) {
                Snd snd("I hear an explosion!",
                        audio::SfxId::explosion,
                        IgnoreMsgIfOriginSeen::yes,
                        origin,
                        nullptr,
                        SndVol::high,
                        AlertsMon::yes);

                snd_emit::run(snd);
        }

        draw(pos_lists, blocked, color_override);

        // Do damage, apply effect
        Array2<actor::Actor*> living_actors(map::dims());

        Array2<std::vector<actor::Actor*>> corpses(map::dims());

        const size_t nr_positions = map::nr_positions();
        for (size_t i = 0; i < nr_positions; ++i) {
                living_actors.at(i) = nullptr;

                corpses.at(i).clear();
        }

        for (auto* actor : game_time::g_actors) {
                const P& pos = actor->m_pos;

                if (actor::is_alive(*actor)) {
                        living_actors.at(pos) = actor;
                }
                else if (actor::is_corpse(*actor)) {
                        corpses.at(pos).push_back(actor);
                }
        }

        const int nr_outer = (int)pos_lists.size();

        for (int dist = 0; dist < nr_outer; ++dist) {
                const std::vector<P>& positions_at_dist = pos_lists[dist];

                for (const P& pos : positions_at_dist) {
                        actor::Actor* living_actor = living_actors.at(pos);

                        auto corpses_here = corpses.at(pos);

                        if (expl_type == ExplType::expl) {
                                apply_explosion_on_pos(
                                        pos,
                                        dist,
                                        living_actor,
                                        corpses_here);
                        }

                        for (auto* property : properties_applied) {
                                apply_explosion_property_on_pos(
                                        pos,
                                        property,
                                        living_actor,
                                        corpses_here,
                                        is_gas);
                        }
                }
        }

        for (auto* prop : properties_applied) {
                delete prop;
        }

        map::update_vision();

}  // run

void run_smoke_explosion_at(const P& origin, const int radi_change)
{
        run_explosion_with_lingering_terrain_at(origin, radi_change, make_smoke);
}

void run_mist_explosion_at(const P& origin, const int radi_change)
{
        run_explosion_with_lingering_terrain_at(origin, radi_change, make_mist);
}

R explosion_area(const P& c, const int radi)
{
        R area = explosion_area_outside_map_allowed(c, radi);

        area.p0.x = std::max(area.p0.x, 1);
        area.p0.y = std::max(area.p0.y, 1);

        area.p1.x = std::min(area.p1.x, map::w() - 2);
        area.p1.y = std::min(area.p1.y, map::h() - 2);

        return area;
}

R explosion_area_outside_map_allowed(const P& c, const int radi)
{
        return {c - radi, c + radi};
}

}  // namespace explosion
