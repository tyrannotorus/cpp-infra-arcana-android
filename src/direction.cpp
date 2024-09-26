// =============================================================================
// Copyright 2011-2024 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "direction.hpp"

#include <cmath>

#include "debug.hpp"
#include "pos.hpp"
#include "random.hpp"

namespace dir_utils
{
const std::vector<P> g_cardinal_list {
        {-1, 0},
        {1, 0},
        {0, -1},
        {0, 1}};

const std::vector<P> g_cardinal_list_w_center {
        {0, 0},
        {-1, 0},
        {1, 0},
        {0, -1},
        {0, 1}};

const std::vector<P> g_dir_list {
        {-1, 0},
        {1, 0},
        {0, -1},
        {0, 1},
        {-1, -1},
        {-1, 1},
        {1, -1},
        {1, 1}};

const std::vector<P> g_dir_list_w_center {
        {0, 0},
        {-1, 0},
        {1, 0},
        {0, -1},
        {0, 1},
        {-1, -1},
        {-1, 1},
        {1, -1},
        {1, 1}};

static const std::string g_compass_dir_names[3][3] = {
        {"NW", "N", "NE"},
        {"W", "", "E"},
        {"SW", "S", "SE"}};

static const double s_pi_db = 3.14159265;
static const double s_angle_45_db = 2 * s_pi_db / 8;
static const double s_angle_45_half_db = s_angle_45_db / 2.0;

static const double edge[4] =
        {
                s_angle_45_half_db + (s_angle_45_db * 0),
                s_angle_45_half_db + (s_angle_45_db * 1),
                s_angle_45_half_db + (s_angle_45_db * 2),
                s_angle_45_half_db + (s_angle_45_db * 3)};

Dir dir(const P& offset)
{
        ASSERT(offset.x >= -1 &&
               offset.y >= -1 &&
               offset.x <= 1 &&
               offset.y <= 1);

        Dir dir = Dir::END;

        if (offset.y == -1) {
                if (offset.x == -1) {
                        dir = Dir::up_left;
                }
                else if (offset.x == 0) {
                        dir = Dir::up;
                }
                else if (offset.x == 1) {
                        dir = Dir::up_right;
                }
        }
        else if (offset.y == 0) {
                if (offset.x == -1) {
                        dir = Dir::left;
                }
                else if (offset.x == 0) {
                        dir = Dir::center;
                }
                else if (offset.x == 1) {
                        dir = Dir::right;
                }
        }
        else if (offset.y == 1) {
                if (offset.x == -1) {
                        dir = Dir::down_left;
                }
                else if (offset.x == 0) {
                        dir = Dir::down;
                }
                else if (offset.x == 1) {
                        dir = Dir::down_right;
                }
        }

        return dir;
}

Dir reversed_dir(const Dir dir)
{
        switch (dir) {
        case Dir::down_left:
                return Dir::up_right;

        case Dir::down:
                return Dir::up;

        case Dir::down_right:
                return Dir::up_left;

        case Dir::left:
                return Dir::right;

        case Dir::center:
                return Dir::center;

        case Dir::right:
                return Dir::left;

        case Dir::up_left:
                return Dir::down_right;

        case Dir::up:
                return Dir::down;

        case Dir::up_right:
                return Dir::down_left;

        case Dir::END:
                break;
        }

        ASSERT(false);

        return Dir::right;
}

P offset(const Dir dir)
{
        ASSERT(dir != Dir::END);

        switch (dir) {
        case Dir::down_left:
                return {-1, 1};

        case Dir::down:
                return {0, 1};

        case Dir::down_right:
                return {1, 1};

        case Dir::left:
                return {-1, 0};

        case Dir::center:
                return {0, 0};

        case Dir::right:
                return {1, 0};

        case Dir::up_left:
                return {-1, -1};

        case Dir::up:
                return {0, -1};

        case Dir::up_right:
                return {1, -1};

        case Dir::END:
                return {0, 0};
        }

        return {0, 0};
}

P rnd_adj_pos(const P& origin, const bool is_center_allowed)
{
        const std::vector<P>* vec = nullptr;

        if (is_center_allowed) {
                vec = &g_dir_list_w_center;
        }
        else {
                // Center not allowed
                vec = &g_dir_list;
        }

        return origin + rnd::element(*vec);
}

std::string compass_dir_name(const P& from_pos, const P& to_pos)
{
        std::string name;

        const P offset(to_pos - from_pos);

        const double angle_db = atan2(-offset.y, offset.x);

        if (angle_db < -edge[2] && angle_db > -edge[3]) {
                name = "SW";
        }
        else if (angle_db <= -edge[1] && angle_db >= -edge[2]) {
                name = "S";
        }
        else if (angle_db < -edge[0] && angle_db > -edge[1]) {
                name = "SE";
        }
        else if (angle_db >= -edge[0] && angle_db <= edge[0]) {
                name = "E";
        }
        else if (angle_db > edge[0] && angle_db < edge[1]) {
                name = "NE";
        }
        else if (angle_db >= edge[1] && angle_db <= edge[2]) {
                name = "N";
        }
        else if (angle_db > edge[2] && angle_db < edge[3]) {
                name = "NW";
        }
        else {
                name = "W";
        }

        return name;
}

std::string compass_dir_name(const Dir dir)
{
        const P& o = offset(dir);

        return g_compass_dir_names[o.x + 1][o.y + 1];
}

std::string compass_dir_name(const P& offs)
{
        return g_compass_dir_names[offs.x + 1][offs.y + 1];
}

}  // namespace dir_utils
