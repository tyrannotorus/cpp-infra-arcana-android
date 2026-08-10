// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "draw_map.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <vector>

#include "actor.hpp"
#include "actor_see.hpp"
#include "array2.hpp"
#include "colors.hpp"
#include "config.hpp"
#include "debug.hpp"
#include "game_time.hpp"
#include "gfx.hpp"
#include "inventory.hpp"
#include "io.hpp"
#include "io_display.hpp"
#include "item.hpp"
#include "item_data.hpp"
#include "map.hpp"
#include "misc.hpp"
#include "player_bon.hpp"
#include "pos.hpp"
#include "property_data.hpp"
#include "property_handler.hpp"
#include "rect.hpp"
#include "terrain.hpp"
#include "terrain_data.hpp"
#include "viewport.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
// Background color to draw in cases where one "object" is obscuring another,
// such as when an item is on top of a trap.
static Array2<std::optional<Color>> s_bg_color_obscured(0, 0);

// Everything the map layers want drawn this frame, resolved per cell and
// handed to SDL in one batch at the end (see io::MapDrawBuffer)
static io::MapDrawBuffer s_draw_buffer;

// The view cell area the buffer covers - the visible view plus the margin
// that viewport::is_in_drawn_view allows (partial cells at the panel edges
// and the sub-cell scroll overscan)
static R drawn_view_area()
{
        const R view = viewport::get_map_view_area();

        return {
                viewport::to_view_pos(view.p0) - 2,
                viewport::to_view_pos(view.p1) + 2};
}

// The same area in MAP coordinates, clipped to the map - what the cell
// iterating draw passes need to visit, and nothing more
static R drawn_map_area()
{
        const R view = viewport::get_map_view_area();

        return {
                {std::max(0, view.p0.x - 2), std::max(0, view.p0.y - 2)},
                {std::min(map::w() - 1, view.p1.x + 2),
                 std::min(map::h() - 1, view.p1.y + 2)}};
}

static void put(const io::MapDrawObj& obj)
{
        s_draw_buffer.put(obj);
}

static gfx::TileId get_player_tile_for_wpn_id(const item::Id item_id)
{
        switch (item_id) {
        case item::Id::axe:
                return gfx::TileId::player_axe;

        case item::Id::club:
                return gfx::TileId::player_club;

        case item::Id::dagger:
        case item::Id::shadow_dagger:
                return gfx::TileId::player_dagger;

        case item::Id::electric_gun:
                return gfx::TileId::player_electric_gun;

        case item::Id::flagellant_whip:
                return gfx::TileId::player_flagellant_whip;

        case item::Id::hammer:
                return gfx::TileId::player_hammer;

        case item::Id::hatchet:
                return gfx::TileId::player_hatchet;

        case item::Id::machete:
                return gfx::TileId::player_machete;

        case item::Id::tommy_gun:
                return gfx::TileId::player_tommy_gun;

        case item::Id::morphic_blaster:
                return gfx::TileId::player_morphic_blaster;

        case item::Id::pharaoh_staff:
                return gfx::TileId::player_pharaoh_staff;

        case item::Id::pistol:
        case item::Id::revolver:
                return gfx::TileId::player_pistol;

        case item::Id::pitchfork:
                return gfx::TileId::player_pitchfork;

        case item::Id::pump_shotgun:
                return gfx::TileId::player_pump_shotgun;

        case item::Id::rifle:
                return gfx::TileId::player_rifle;

        case item::Id::sawed_off:
                return gfx::TileId::player_sawed_off;

        case item::Id::sledgehammer:
                return gfx::TileId::player_sledgehammer;

        case item::Id::spear:
                return gfx::TileId::player_spear;

        case item::Id::spike_gun:
                return gfx::TileId::player_spike_gun;

        case item::Id::spiked_mace:
                return gfx::TileId::player_spiked_mace;

        default:
                return gfx::TileId::player_machete;
        }
}

static gfx::TileId get_ghoul_tile_for_wpn_id(const item::Id item_id)
{
        switch (item_id) {
        case item::Id::axe:
                return gfx::TileId::player_ghoul_axe;

        case item::Id::club:
                return gfx::TileId::player_ghoul_club;

        case item::Id::dagger:
        case item::Id::shadow_dagger:
                return gfx::TileId::player_ghoul_dagger;

        case item::Id::electric_gun:
                return gfx::TileId::player_ghoul_electric_gun;

        case item::Id::hammer:
                return gfx::TileId::player_ghoul_hammer;

        case item::Id::hatchet:
                return gfx::TileId::player_ghoul_hatchet;

        case item::Id::machete:
                return gfx::TileId::player_ghoul_machete;

        case item::Id::tommy_gun:
                return gfx::TileId::player_ghoul_tommy_gun;

        case item::Id::morphic_blaster:
                return gfx::TileId::player_ghoul_morphic_blaster;

        case item::Id::pharaoh_staff:
                return gfx::TileId::player_ghoul_pharaoh_staff;

        case item::Id::pistol:
        case item::Id::revolver:
                return gfx::TileId::player_ghoul_pistol;

        case item::Id::pitchfork:
                return gfx::TileId::player_ghoul_pitchfork;

        case item::Id::pump_shotgun:
                return gfx::TileId::player_ghoul_pump_shotgun;

        case item::Id::rifle:
                return gfx::TileId::player_ghoul_rifle;

        case item::Id::sawed_off:
                return gfx::TileId::player_ghoul_sawed_off;

        case item::Id::sledgehammer:
                return gfx::TileId::player_ghoul_sledgehammer;

        case item::Id::spear:
                return gfx::TileId::player_ghoul_spear;

        case item::Id::spike_gun:
                return gfx::TileId::player_ghoul_spike_gun;

        case item::Id::spiked_mace:
                return gfx::TileId::player_ghoul_spiked_mace;

        default:
                return gfx::TileId::player_ghoul_machete;
        }

        ASSERT(false);

        return gfx::TileId::player_unarmed;
}

static gfx::TileId get_player_tile()
{
        const item::Item* const wpn = map::g_player->m_inv.item_in_slot(SlotId::wpn);

        if (player_bon::is_bg(Bg::ghoul)) {
                if (wpn) {
                        return get_ghoul_tile_for_wpn_id(wpn->id());
                }
                else {
                        return gfx::TileId::ghoul;
                }
        }
        else {
                if (wpn) {
                        return get_player_tile_for_wpn_id(wpn->id());
                }
                else {
                        return gfx::TileId::player_unarmed;
                }
        }
}

static void set_bg_color_obscured_terrain(
        const terrain::Terrain* const terrain,
        const size_t pos_idx)
{
        std::optional<Color>& value = s_bg_color_obscured.at(pos_idx);

        switch (terrain->id()) {
        case terrain::Id::liquid: {
                value = terrain->color_default();
        } break;

        case terrain::Id::chains: {
                value = terrain->color_default();
        } break;

        case terrain::Id::trap: {
                if (!terrain->is_hidden()) {
                        if (config::use_trap_color_when_obscured()) {
                                value = terrain->color_default();
                        }
                        else {
                                value = colors::yellow();
                        }
                }
        } break;

        default:
        {
        } break;
        }
}

static void set_bg_color_when_obscured_dead_actor(const actor::Actor& actor)
{
        const Color& color_default = colors::gray_brown();
        const Color& color_corpse_rises = colors::dark_teal();

        std::optional<Color>& color_here = s_bg_color_obscured.at(actor.m_pos);

        if (color_here && (color_here.value() == color_corpse_rises)) {
                // This position is colored as containing a corpse that will
                // rise again, do not change the color.
                return;
        }

        const bool is_corpse_rises =
                actor.m_properties.has(prop::Id::corpse_rises);

        const Color& new_color =
                is_corpse_rises
                ? color_corpse_rises
                : color_default;

        s_bg_color_obscured.at(actor.m_pos) = new_color;
}

static void use_bg_color_obscuring(Color& color, const P& p)
{
        color = s_bg_color_obscured.at(p).value_or(color);
}

static void adapt_color_for_lit_pos(Color& color)
{
        color.set_rgb(
                std::min(255, color.r() + 80),
                std::min(255, color.g() + 80),
                color.b());
}

static void adapt_color_for_dark_pos(Color& color)
{
        color = color.shaded(40);

        color.set_rgb(
                color.r(),
                color.g(),
                std::min(255, color.b() + 20));
}

static void adapt_color_for_light_level(Color& color, const size_t pos_idx)
{
        const terrain::Terrain* const t = map::g_terrain.at(pos_idx);

        if (!map::g_seen.at(pos_idx) ||
            !t->is_los_passable() ||
            (t->id() == terrain::Id::chasm)) {
                return;
        }

        if (map::g_light.at(pos_idx)) {
                adapt_color_for_lit_pos(color);
        }
        else if (map::g_dark.at(pos_idx)) {
                adapt_color_for_dark_pos(color);
        }
}

static void adapt_color_for_light_level(Color& color, const P& pos)
{
        adapt_color_for_light_level(color, map::g_terrain.pos_to_idx(pos));
}

static void adapt_color_for_distance_to_player(Color& color, const P& pos)
{
        if (map::g_light.at(pos)) {
                return;
        }

        const int dist = king_dist(pos, map::g_player->m_pos);

        const int k = std::clamp(dist - 1, 0, 4);

        if (k > 0) {
                color = color.shaded(k * 15);
        }
}

static void draw_terrains()
{
        // NOTE: Only the cells that can actually be drawn are visited. This
        // used to walk every position of the (60x60) map and cull each one
        // with viewport::is_in_drawn_view - which recomputes the view area
        // per call - to end up drawing the few hundred cells in view.
        const R area = drawn_map_area();

        for (int x = area.p0.x; x <= area.p1.x; ++x) {
                for (int y = area.p0.y; y <= area.p1.y; ++y) {
                        const P p(x, y);

                        const size_t i = map::g_terrain.pos_to_idx(p);

                        if (!map::g_seen.at(i)) {
                                continue;
                        }

                        const terrain::Terrain* const t = map::g_terrain.at(i);

                        io::MapDrawObj draw_obj;

                        draw_obj.pos = viewport::to_view_pos(p);

                        auto gore_tile = gfx::TileId::END;
                        char gore_character = 0;

                        if (t->can_have_gore()) {
                                gore_tile = t->gore_tile();
                                gore_character = t->gore_character();
                        }

                        if (gore_tile == gfx::TileId::END) {
                                draw_obj.tile = t->tile();
                                draw_obj.character = t->character();
                                draw_obj.color = t->color();
                        }
                        else {
                                draw_obj.tile = gore_tile;
                                draw_obj.character = gore_character;
                                draw_obj.color = colors::red();
                        }

                        const Color terrain_color_bg = t->color_bg();

                        if (terrain_color_bg == colors::black()) {
                                // Set background color to use if this terrain
                                // is obscured by another object (e.g. an item
                                // on a trap).
                                set_bg_color_obscured_terrain(t, i);
                        }
                        else {
                                draw_obj.color_bg = terrain_color_bg;

                                s_bg_color_obscured.at(i) = terrain_color_bg;
                        }

                        if (config::text_mode_filled_walls()) {
                                if (draw_obj.character == '#') {
                                        // Any terrain with the '#' symbol is
                                        // converted to a filled rectangle
                                        // instead.
                                        //
                                        // NOTE: No other (static) terrain
                                        // except WALLS (or terrain imitating
                                        // walls, such as hidden doors) must
                                        // use the '#' character!
                                        //
                                        draw_obj.character =
                                                io::g_filled_rect_char;
                                }
                                else if (t->id() == terrain::Id::grate) {
                                        // Since we are using filled rectangle
                                        // as wall symbol, then we can use the
                                        // '#' character for grates (looks good
                                        // for this terrain, but obviously not
                                        // if walls are also using this).
                                        draw_obj.character = '#';
                                }
                        }

                        adapt_color_for_light_level(draw_obj.color, i);

                        adapt_color_for_distance_to_player(draw_obj.color, p);

                        put(draw_obj);
                }
        }
}

static void draw_dead_actors()
{
        for (actor::Actor* actor : game_time::g_actors) {
                const P& p = actor->m_pos;

                if (!map::g_seen.at(p) || !actor::is_corpse(*actor)) {
                        continue;
                }

                if (!viewport::is_in_drawn_view(p)) {
                        continue;
                }

                io::MapDrawObj draw_obj;

                draw_obj.pos = viewport::to_view_pos(p);
                draw_obj.color = actor::color(*actor);
                draw_obj.tile = actor::tile(*actor);
                draw_obj.character = actor::character(*actor);

                adapt_color_for_light_level(draw_obj.color, p);

                use_bg_color_obscuring(draw_obj.color_bg, p);

                set_bg_color_when_obscured_dead_actor(*actor);

                put(draw_obj);
        }
}

static void draw_items()
{
        const R area = drawn_map_area();

        for (int x = area.p0.x; x <= area.p1.x; ++x) {
                for (int y = area.p0.y; y <= area.p1.y; ++y) {
                        const P p(x, y);

                        if (!map::g_seen.at(p)) {
                                continue;
                        }

                        const item::Item* const item = map::g_items.at(p);

                        if (!item) {
                                continue;
                        }

                        io::MapDrawObj draw_obj;

                        draw_obj.pos = viewport::to_view_pos(p);
                        draw_obj.color = item->color();
                        draw_obj.tile = item->tile();
                        draw_obj.character = item->character();

                        adapt_color_for_light_level(draw_obj.color, p);

                        use_bg_color_obscuring(draw_obj.color_bg, p);

                        put(draw_obj);
                }
        }
}

static void draw_mobiles()
{
        for (terrain::Terrain* mob : game_time::g_mobs) {
                const P& p = mob->pos();
                const gfx::TileId mob_tile = mob->tile();
                const char mob_character = mob->character();

                if (!map::g_seen.at(p) ||
                    (mob_tile == gfx::TileId::END) ||
                    (mob_character == 0) ||
                    (mob_character == ' ')) {
                        continue;
                }

                if (!viewport::is_in_drawn_view(p)) {
                        continue;
                }

                io::MapDrawObj draw_obj;

                draw_obj.pos = viewport::to_view_pos(p);
                draw_obj.color = mob->color();
                draw_obj.tile = mob_tile;
                draw_obj.character = mob_character;

                adapt_color_for_light_level(draw_obj.color, p);

                put(draw_obj);
        }
}

static void draw_living_seen_monster(const actor::Actor& mon)
{
        const gfx::TileId mon_tile = actor::tile(mon);
        const char mon_char = actor::character(mon);

        if ((mon_tile == gfx::TileId::END) ||
            (mon_char == 0) ||
            (mon_char == ' ')) {
                return;
        }

        io::MapDrawObj draw_obj;

        draw_obj.pos = viewport::to_view_pos(mon.m_pos);
        draw_obj.color = actor::color(mon);
        draw_obj.tile = actor::tile(mon);
        draw_obj.character = actor::character(mon);

        if (map::g_player->is_leader_of(&mon)) {
                // The monster is player-friendly
                draw_obj.color_bg = colors::mon_allied();
        }
        else {
                // The monster is hostile
                if (actor::is_aware_of_player(mon)) {
                        // Monster is aware of player
                        if (mon.m_properties.has_temporary_negative_prop_mon()) {
                                draw_obj.color_bg = colors::mon_temp_property();
                        }
                        else if (mon.m_properties.has(prop::Id::frenzied)) {
                                draw_obj.color_bg = colors::red();
                        }
                }
                else {
                        // Monster is not aware of the player
                        draw_obj.color_bg = colors::mon_unaware();
                }
        }

        adapt_color_for_light_level(draw_obj.color, mon.m_pos);

        put(draw_obj);
}

static void draw_living_hidden_monster(const actor::Actor& mon)
{
        if (!actor::is_player_aware_of_me(mon)) {
                return;
        }

        io::MapDrawObj draw_obj;

        const Color color_bg =
                map::g_player->is_leader_of(&mon)
                ? colors::mon_allied()
                : colors::dark_gray();

        draw_obj.pos = viewport::to_view_pos(mon.m_pos);
        draw_obj.tile = gfx::TileId::excl_mark;
        draw_obj.character = '!';
        draw_obj.color = colors::white();
        draw_obj.color_bg = color_bg;

        adapt_color_for_light_level(draw_obj.color, mon.m_pos);

        put(draw_obj);
}

static void draw_living_monsters()
{
        for (actor::Actor* actor : game_time::g_actors) {
                if (actor::is_player(actor) || !actor::is_alive(*actor)) {
                        continue;
                }

                if (!viewport::is_in_drawn_view(actor->m_pos)) {
                        continue;
                }

                if (can_player_see_actor(*actor)) {
                        draw_living_seen_monster(*actor);
                }
                else {
                        draw_living_hidden_monster(*actor);
                }
        }
}

static io::MapDrawObj player_memory_to_draw_obj(
        const map::PlayerMemoryAppearance& d)
{
        io::MapDrawObj draw_obj;

        draw_obj.tile = d.tile;
        draw_obj.color = d.color;
        draw_obj.color_bg = colors::black();
        draw_obj.character = d.character;

        return draw_obj;
}

static void draw_unseen_cells_from_player_memory()
{
        R view = viewport::get_map_view_area();

        // Also draw a little bit outside the viewport - partial tiles can be
        // shown when the map panel size is not a whole number of map tiles,
        // and the map display renders one cell of overscan beyond each panel
        // edge for smooth sub-cell scrolling. The drawing is clipped to the
        // map panel plus the overscan ring.
        view.p0 = view.p0.with_offsets(-2, -2);
        view.p1 = view.p1.with_offsets(2, 2);

        for (int x = view.p0.x; x < view.p1.x; ++x) {
                for (int y = view.p0.y; y < view.p1.y; ++y) {
                        const P p(x, y);

                        if (!map::is_pos_inside_map(p)) {
                                continue;
                        }

                        if (map::g_seen.at(p)) {
                                continue;
                        }

                        io::MapDrawObj draw_obj;

                        const map::PlayerMemoryTerrain& terrain_memory =
                                map::g_terrain_memory.at(p);

                        const map::PlayerMemoryItem& item_memory =
                                map::g_item_memory.at(p);

                        if (terrain_memory.appearance.is_defined()) {
                                draw_obj =
                                        player_memory_to_draw_obj(
                                                terrain_memory.appearance);
                        }

                        if (item_memory.appearance.is_defined()) {
                                draw_obj =
                                        player_memory_to_draw_obj(
                                                item_memory.appearance);
                        }

                        draw_obj.pos = viewport::to_view_pos(p);

                        draw_obj.color = draw_obj.color.shaded(80);

                        put(draw_obj);
                }
        }
}

static void draw_player_character()
{
        const actor::Actor& player = *map::g_player;

        // NOTE: One cell outside the view is also drawn (the overscan ring
        // used for smooth sub-cell scrolling)
        const auto view_area = viewport::get_map_view_area();

        const R overscan_area(
                view_area.p0 - 1,
                view_area.p1 + 1);

        if (!overscan_area.is_pos_inside(player.m_pos)) {
                return;
        }

        const Color color = actor::color(player);
        const Color color_bg = colors::black();

        gfx::TileId tile = get_player_tile();

        io::MapDrawObj draw_obj;

        const char character = '@';

        draw_obj.pos = viewport::to_view_pos(player.m_pos);
        draw_obj.tile = tile;
        draw_obj.character = character;
        draw_obj.color = color;
        draw_obj.color_bg = color_bg;

        put(draw_obj);
}

// -----------------------------------------------------------------------------
// draw_map
// -----------------------------------------------------------------------------
namespace draw_map
{
void run()
{
        // NOTE: Array2::resize frees and reallocates - only do it when the
        // map size actually changed (i.e. on a new level), and clear the
        // existing storage otherwise.
        if (s_bg_color_obscured.dims() == map::dims()) {
                std::fill(
                        std::begin(s_bg_color_obscured),
                        std::end(s_bg_color_obscured),
                        std::optional<Color>());
        }
        else {
                s_bg_color_obscured.resize(map::dims());
        }

        s_draw_buffer.reset(drawn_view_area());

        draw_unseen_cells_from_player_memory();
        draw_terrains();
        draw_dead_actors();
        draw_items();
        draw_mobiles();
        draw_living_monsters();

        draw_player_character();

        s_draw_buffer.draw();

#ifndef NDEBUG
        io::g_allow_render = true;
#endif  // NDEBUG
}

void draw_reticle(const P& view_pos, const Color& color)
{
        io::set_display_for_panel(Panel::map);

        const P cell_dims(
                config::map_cell_px_w(),
                config::map_cell_px_h());

        const P p0 = io::map_to_px_coords(Panel::map, view_pos);
        const P p1 = p0 + cell_dims - 1;

        // Bracket arm length and line thickness, scaled to the cell size
        const int arm = std::max(3, cell_dims.x / 3);
        const int thickness = std::max(1, cell_dims.x / 8);

        auto bracket_part = [&color](
                                    const int x0,
                                    const int y0,
                                    const int x1,
                                    const int y1) {
                io::draw_rectangle_filled({P(x0, y0), P(x1, y1)}, color);
        };

        // Top left corner
        bracket_part(p0.x, p0.y, p0.x + arm - 1, p0.y + thickness - 1);
        bracket_part(p0.x, p0.y, p0.x + thickness - 1, p0.y + arm - 1);

        // Top right corner
        bracket_part(p1.x - arm + 1, p0.y, p1.x, p0.y + thickness - 1);
        bracket_part(p1.x - thickness + 1, p0.y, p1.x, p0.y + arm - 1);

        // Bottom left corner
        bracket_part(p0.x, p1.y - thickness + 1, p0.x + arm - 1, p1.y);
        bracket_part(p0.x, p1.y - arm + 1, p0.x + thickness - 1, p1.y);

        // Bottom right corner
        bracket_part(p1.x - arm + 1, p1.y - thickness + 1, p1.x, p1.y);
        bracket_part(p1.x - thickness + 1, p1.y - arm + 1, p1.x, p1.y);
}

}  // namespace draw_map
