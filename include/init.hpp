// =============================================================================
// Copyright 2011-2023 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef INIT_HPP
#define INIT_HPP

namespace init
{
extern bool g_is_cheat_vision_enabled;
extern bool g_is_demo_mapgen;

void init_io();
void cleanup_io();

void init_game();
void cleanup_game();

void init_session();
void cleanup_session();

}  // namespace init

#endif
