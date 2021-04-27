// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include <memory>
#include <string>

#include "init.hpp"
#include "config.hpp"
#include "debug.hpp"
#include "main_menu.hpp"
#include "random.hpp"
#include "state.hpp"

#ifdef _WIN32
#undef main
#endif
int main(int argc, char** argv)
{
        TRACE_FUNC_BEGIN;

        rnd::seed();

        init::init_io();

        for (int arg_nr = 0; arg_nr < argc; ++arg_nr)
        {
                const std::string arg_str = std::string(argv[arg_nr]);

#ifndef NDEBUG
                if (arg_str == "--demo-mapgen")
                {
                        init::g_is_demo_mapgen = true;
                }

                if (arg_str == "--bot")
                {
                        config::toggle_bot_playing();
                }
#endif  // NDEBUG

                // Extra challenge for user "GJ" from the Discord chat ;-)
                if (arg_str == "--gj")
                {
                        config::toggle_gj_mode();
                }
        }

        init::init_game();

        states::push(std::make_unique<MainMenuState>());

        states::run();

        init::cleanup_session();
        init::cleanup_game();
        init::cleanup_io();

        return 0;
}
