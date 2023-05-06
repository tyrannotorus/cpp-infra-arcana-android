// =============================================================================
// Copyright 2011-2023 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef MAP_CONTROLLER_HPP
#define MAP_CONTROLLER_HPP

#include <memory>

// -----------------------------------------------------------------------------
// MapController
// -----------------------------------------------------------------------------
class MapController
{
public:
        MapController() = default;

        virtual ~MapController() = default;

        virtual void on_start() {}

        virtual void on_std_turn() {}
};

class MapControllerStd : public MapController
{
public:
        MapControllerStd() = default;

        void on_start() override;

        void on_std_turn() override;
};

class MapControllerBoss : public MapController
{
public:
        MapControllerBoss() = default;

        void on_start() override;

        void on_std_turn() override;
};

// -----------------------------------------------------------------------------
// map_control
// -----------------------------------------------------------------------------
namespace map_control
{
extern std::unique_ptr<MapController> g_controller;

}  // namespace map_control

#endif  // MAP_CONTROLLER_HPP
