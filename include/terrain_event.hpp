// =============================================================================
// Copyright 2011-2021 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef TERRAIN_EVENT_HPP
#define TERRAIN_EVENT_HPP

#include <string>
#include <vector>

#include "array2.hpp"
#include "colors.hpp"
#include "global.hpp"
#include "pos.hpp"
#include "random.hpp"
#include "rect.hpp"
#include "terrain.hpp"
#include "terrain_data.hpp"

// TODO: Events should probably not be terrain

namespace terrain
{
class Event : public Terrain
{
public:
        Event(const P& pos) :
                Terrain(pos) {}

        virtual ~Event() = default;

        void on_new_turn() override = 0;

        std::string name(const Article article) const final
        {
                (void)article;
                return "";
        }

        Color color() const final
        {
                return colors::black();
        }
};

class EventWallCrumble : public Event
{
public:
        EventWallCrumble(
                const P& p,
                std::vector<P>& walls,
                std::vector<P>& inner);

        EventWallCrumble(const P& p) :
                Event(p) {}

        ~EventWallCrumble() = default;

        Id id() const override
        {
                return Id::event_wall_crumble;
        }

        void on_new_turn() override;

private:
        std::vector<P> m_wall_cells;
        std::vector<P> m_inner_cells;
};

class EventSnakeEmerge : public Event
{
public:
        EventSnakeEmerge();

        EventSnakeEmerge(const P& p) :
                Event(p) {}

        ~EventSnakeEmerge() = default;

        Id id() const override
        {
                return Id::event_snake_emerge;
        }

        bool try_find_p();

        void on_new_turn() override;

private:
        R allowed_emerge_rect(const P& p) const;

        bool is_ok_terrain_at(const P& p) const;

        Array2<bool> blocked_cells(const R& r) const;

        std::vector<P> emerge_p_bucket(
                const P& p,
                const Array2<bool>& blocked,
                const R& allowed_area) const;

        const Range allowed_emerge_dist_range =
                Range(2, g_fov_radi_int - 1);

        const int m_min_nr_snakes = 3;
};

class EventRatsInTheWallsDiscovery : public Event
{
public:
        EventRatsInTheWallsDiscovery(const P& terrain_pos);

        Id id() const override
        {
                return Id::event_rat_cave_discovery;
        }

        void on_new_turn() override;
};

}  // namespace terrain

#endif  // TERRAIN_EVENT_HPP
