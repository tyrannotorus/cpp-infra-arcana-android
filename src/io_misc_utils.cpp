// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include <algorithm>
#include <vector>

#include "actor.hpp"
#include "array2.hpp"
#include "colors.hpp"
#include "config.hpp"
#include "debug.hpp"
#include "gfx.hpp"
#include "io.hpp"
#include "map.hpp"
#include "panel.hpp"
#include "pos.hpp"
#include "state.hpp"
#include "text_format.hpp"
#include "viewport.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// io
// -----------------------------------------------------------------------------
namespace io
{
void draw_symbol(
        const gfx::TileId tile,
        const char character,
        const Panel panel,
        const P pos,
        const Color& color,
        const DrawBg draw_bg,
        const Color& color_bg)
{
        if (config::is_tiles_mode())
        {
                draw_tile(
                        tile,
                        panel,
                        pos,
                        color,
                        draw_bg,
                        color_bg);
        }
        else
        {
                draw_character(
                        character,
                        panel,
                        pos,
                        color,
                        draw_bg,
                        color_bg);
        }
}

void draw_blast_at_cells(const std::vector<P>& positions, const Color& color)
{
        TRACE_FUNC_BEGIN;

        if (!panels::is_valid())
        {
                TRACE_FUNC_END;

                return;
        }

        states::draw();

        for (const P& pos : positions)
        {
                if (!viewport::is_in_view(pos))
                {
                        continue;
                }

                draw_symbol(
                        gfx::TileId::blast1,
                        '*',
                        Panel::map,
                        viewport::to_view_pos(pos),
                        color);
        }

        update_screen();

        io::sleep(config::delay_explosion() / 2);

        for (const P& pos : positions)
        {
                if (!viewport::is_in_view(pos))
                {
                        continue;
                }

                draw_symbol(
                        gfx::TileId::blast2,
                        '*',
                        Panel::map,
                        viewport::to_view_pos(pos),
                        color);
        }

        update_screen();

        io::sleep(config::delay_explosion() / 2);

        TRACE_FUNC_END;
}

void draw_blast_at_seen_cells(
        const std::vector<P>& positions,
        const Color& color)
{
        if (!panels::is_valid())
        {
                return;
        }

        std::vector<P> positions_with_vision;

        for (const P& p : positions)
        {
                if (map::g_seen.at(p))
                {
                        positions_with_vision.push_back(p);
                }
        }

        if (!positions_with_vision.empty())
        {
                io::draw_blast_at_cells(positions_with_vision, color);
        }
}

void draw_blast_at_seen_actors(
        const std::vector<actor::Actor*>& actors,
        const Color& color)
{
        if (!panels::is_valid())
        {
                return;
        }

        std::vector<P> positions;

        positions.reserve(actors.size());
        for (auto* const actor : actors)
        {
                positions.push_back(actor->m_pos);
        }

        draw_blast_at_seen_cells(positions, color);
}

}  // namespace io
