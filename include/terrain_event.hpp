// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
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
        Event(const P& pos, const TerrainData* const data) :
                Terrain(pos, data) {}

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
        EventWallCrumble(const P& p, const TerrainData* const data) :
                Event(p, data) {}

        ~EventWallCrumble() = default;

        void set_wall_positions(const std::vector<P>& wall_positions)
        {
                m_wall_positions = wall_positions;
        }

        void set_inner_positions(const std::vector<P>& inner_positions)
        {
                m_inner_positions = inner_positions;
        }

        void on_new_turn() override;

private:
        std::vector<P> m_wall_positions {};
        std::vector<P> m_inner_positions {};
};

class EventSnakeEmerge : public Event
{
public:
        EventSnakeEmerge(const P& p, const TerrainData* const data) :
                Event(p, data) {}

        ~EventSnakeEmerge() = default;

        bool try_find_p();

        void on_new_turn() override;

private:
        R allowed_emerge_rect(const P& p) const;

        bool is_ok_terrain_at(const P& p) const;

        Array2<bool> blocked_positions(const R& r) const;

        std::vector<P> emerge_p_bucket(
                const P& p,
                const Array2<bool>& blocked,
                const R& allowed_area) const;

        const Range allowed_emerge_dist_range =
                Range(2, g_fov_radi_int - 1);

        const static int m_min_nr_snakes {3};
};

class EventSpawnMonstersDelayed : public Event
{
public:
        EventSpawnMonstersDelayed(const P& p, const TerrainData* const data) :
                Event(p, data) {}

        ~EventSpawnMonstersDelayed() = default;

        void set_mon_id(const std::string& id)
        {
                m_id = id;
        }

        void set_nr_mon(const int nr)
        {
                m_nr_mon = nr;
        }

        void on_new_turn() override;

private:
        std::string m_id {};
        size_t m_nr_mon {1};

        int m_countdown {3};
};

class EventRatsInTheWallsDiscovery : public Event
{
public:
        EventRatsInTheWallsDiscovery(
                const P& p,
                const TerrainData* const data) :
                Event(p, data) {}

        void on_new_turn() override;
};

}  // namespace terrain

#endif  // TERRAIN_EVENT_HPP
