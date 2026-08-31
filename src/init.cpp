// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "init.hpp"

#include <ostream>
#include <string>

#include "actor_data.hpp"
#include "audio.hpp"
#include "bot.hpp"
#include "colors.hpp"
#include "config.hpp"
#include "debug.hpp"
#include "game.hpp"
#include "game_time.hpp"
#include "highscore.hpp"
#include "hints.hpp"
#include "insanity.hpp"
#include "io.hpp"
#include "item_curse.hpp"
#include "item_data.hpp"
#include "item_potion.hpp"
#include "item_rod.hpp"
#include "item_scroll.hpp"
#include "line_calc.hpp"
#include "map.hpp"
#include "map_templates.hpp"
#include "map_travel.hpp"
#include "messages.hpp"
#include "msg_log.hpp"
#include "panel.hpp"
#include "paths.hpp"
#include "player_bon.hpp"
#include "player_spells.hpp"
#include "popup.hpp"
#include "pos.hpp"
#include "property_data.hpp"
#include "query.hpp"
#include "saving.hpp"
#include "smell.hpp"
#include "terrain_data.hpp"
#include "terrain_pylon.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// init
// -----------------------------------------------------------------------------
namespace init
{
bool g_is_cheat_vision_enabled = false;

bool g_is_demo_mapgen = false;

// Draws and presents the boot loading frame. Called between the heavy
// init steps: on Android the render surface goes live asynchronously
// while they run, and a frame presented before that is silently dropped -
// presenting again after each step means the last one before the first
// state draw reaches the screen. The event pump (clear_input) is what
// lets the surface attach; nothing else pumps until the first input read.
// TODO: Use more creative loading messages
static void present_loading_frame()
{
        io::clear_input();

        io::clear_screen();

        io::draw_text_center(
                "Loading...",
                Panel::screen,
                panels::center(Panel::screen),
                colors::menu_dark());

        io::update_screen();
}

void init_io()
{
        TRACE_FUNC_BEGIN;

        io::init_sdl();
        io::init_sdl_audio();

        paths::init();

        config::init();
        colors::init();
        io::init_other();

        present_loading_frame();

        query::init();
        audio::init();

        present_loading_frame();

        std::queue<std::string>& paths_error_messages = paths::pending_error_messages();

        for (; !paths_error_messages.empty(); paths_error_messages.pop()) {
                const std::string msg = paths_error_messages.front();

                popup::Popup popup(popup::AddToMsgHistory::no);

                popup.set_title("Warning");

                popup.set_msg(msg);

                popup.run();

                io::sleep(250);
        }

        TRACE_FUNC_END;
}

void cleanup_io()
{
        TRACE_FUNC_BEGIN;

        audio::cleanup();
        query::cleanup();
        io::cleanup_other();
        io::cleanup_sdl_audio();
        io::cleanup_sdl();

        TRACE_FUNC_END;
}

void init_game()
{
        TRACE_FUNC_BEGIN;

        present_loading_frame();

        saving::init();
        messages::init();
        line_calc::init();
        map_templates::init();

        // One last present, held for a beat: the surface attach has had
        // the whole init to happen by now, so this one always shows - and
        // without the hold, a fast boot flashes past the text unseen
        present_loading_frame();

        io::sleep(750);

        TRACE_FUNC_END;
}

void cleanup_game()
{
        TRACE_FUNC_BEGIN;

        TRACE_FUNC_END;
}

void init_session()
{
        TRACE_FUNC_BEGIN;

        actor::init();
        terrain::init();
        prop::init();
        item::init();
        scroll::init();
        potion::init();
        rod::init();
        item_curse::init();
        terrain::pylon::init();
        game_time::init();
        map_travel::init();
        map::init();
        player_bon::init();
        insanity::init();
        msg_log::init();
        game::init();
        bot::init();
        player_spells::init();
        hints::init();
        smell::init();

        TRACE_FUNC_END;
}

void cleanup_session()
{
        TRACE_FUNC_BEGIN;

        map_templates::clear_base_room_templates_used();

        player_spells::cleanup();
        insanity::cleanup();
        map::cleanup();
        game_time::cleanup();
        item::cleanup();

        TRACE_FUNC_END;
}

}  // namespace init
