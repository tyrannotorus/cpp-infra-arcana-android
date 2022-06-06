// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include <memory>
#include <string>

#include "config.hpp"
#include "debug.hpp"
#include "init.hpp"
#include "main_menu.hpp"
#include "random.hpp"
#include "state.hpp"

#ifdef _WIN32
#undef main
#endif

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
#ifndef NDEBUG
static void handle_args(int argc, char** argv)
{
        for (int arg_nr = 0; arg_nr < argc; ++arg_nr) {
                const std::string arg_str = std::string(argv[arg_nr]);

                if (arg_str == "--demo-mapgen") {
                        init::g_is_demo_mapgen = true;
                }

                if (arg_str == "--bot") {
                        config::enable_bot_playing();
                }

                if (arg_str == "--stress-test") {
                        rnd::seed(0);
                        config::enable_stress_test();
                        config::enable_bot_playing();
                }

                // Extra challenge for user "GJ" from the Discord chat ;-)
                if (arg_str == "--gj") {
                        config::toggle_gj_mode();
                }
        }
}
#endif  // NDEBUG

// -----------------------------------------------------------------------------
// main
// -----------------------------------------------------------------------------
int main(int argc, char** argv)
{
        TRACE_FUNC_BEGIN;

        rnd::seed();

        init::init_io();

#ifdef NDEBUG
        (void)argc;
        (void)argv;
#else
        handle_args(argc, argv);
#endif  // NDEBUG

        init::init_game();

        states::push(std::make_unique<MainMenuState>());

        states::run();

        init::cleanup_session();
        init::cleanup_game();
        init::cleanup_io();

        return 0;
}
