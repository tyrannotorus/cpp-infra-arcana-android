// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include <stdint.h>
#include <ostream>

#include "io.hpp"
#include "io_internal.hpp"
#include "config.hpp"
#include "debug.hpp"
#include "state.hpp"
#include "SDL_events.h"
#include "SDL_keyboard.h"
#include "SDL_keycode.h"
#include "SDL_timer.h"
#include "SDL_video.h"
#include "pos.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static SDL_Event s_sdl_event;

// -----------------------------------------------------------------------------
// io
// -----------------------------------------------------------------------------
namespace io
{
void flush_input()
{
        SDL_PumpEvents();
}

void clear_events()
{
        while (SDL_PollEvent(&s_sdl_event))
        {
        }
}

InputData get()
{
        InputData input;

        SDL_StartTextInput();

        bool is_done = false;

        bool is_window_resized = false;

        uint32_t ms_at_last_window_resize = 0;

        while (!is_done)
        {
                io::sleep(1);

                const auto mod = SDL_GetModState();

                input.is_shift_held = mod & KMOD_SHIFT;
                input.is_ctrl_held = mod & KMOD_CTRL;
                input.is_alt_held = mod & KMOD_ALT;

                const bool did_poll_event = SDL_PollEvent(&s_sdl_event);

                // Handle window resizing
                if (!config::is_fullscreen())
                {
                        if (is_window_resized)
                        {
                                on_window_resized();

                                clear_screen();

                                io::update_screen();

                                clear_events();

                                is_window_resized = false;

                                ms_at_last_window_resize = SDL_GetTicks();

                                continue;
                        }

                        if (ms_at_last_window_resize != 0)
                        {
                                const auto d =
                                        SDL_GetTicks() -
                                        ms_at_last_window_resize;

                                if (d > 400)
                                {
                                        states::draw();

                                        io::update_screen();

                                        ms_at_last_window_resize = 0;
                                }
                        }
                }

                if (!did_poll_event)
                {
                        continue;
                }

                switch (s_sdl_event.type)
                {
                case SDL_WINDOWEVENT:
                {
                        switch (s_sdl_event.window.event)
                        {
                        case SDL_WINDOWEVENT_SIZE_CHANGED:
                        {
                                TRACE << "Window resized" << std::endl;

                                if (!config::is_fullscreen())
                                {
                                        is_window_resized = true;
                                }
                        }
                        break;

                        case SDL_WINDOWEVENT_RESTORED:
                        {
                                TRACE << "Window restored" << std::endl;
                        }
                        // Fallthrough
                        case SDL_WINDOWEVENT_FOCUS_GAINED:
                        {
                                TRACE << "Window gained focus" << std::endl;

                                clear_events();

                                io::sleep(100);
                        }
                        // Fallthrough
                        case SDL_WINDOWEVENT_EXPOSED:
                        {
                                TRACE << "Window exposed" << std::endl;

                                states::draw();

                                io::update_screen();
                        }
                        break;

                        default:
                        {
                        }
                        break;
                        }  // window event switch
                }
                break;  // case SDL_WINDOWEVENT

                case SDL_QUIT:
                {
                        input.key = SDLK_ESCAPE;

                        is_done = true;
                }
                break;

                case SDL_KEYDOWN:
                {
                        input.key = s_sdl_event.key.keysym.sym;

                        switch (input.key)
                        {
                        case SDLK_RETURN:
                        case SDLK_RETURN2:
                        case SDLK_KP_ENTER:
                        {
                                if (input.is_alt_held)
                                {
                                        TRACE << "Alt-Enter pressed"
                                              << std::endl;

                                        config::set_fullscreen(
                                                !config::is_fullscreen());

                                        on_fullscreen_toggled();

                                        // SDL_SetWindowSize(sdl_window_,

                                        // TODO: For some reason, the alt key
                                        // gets "stuck" after toggling
                                        // fullscreen, and must be cleared here
                                        // manually. Don't know if this is an
                                        // issue in the IA code, or an SDL bug.
                                        SDL_SetModState(KMOD_NONE);

                                        clear_events();

                                        flush_input();

                                        continue;
                                }
                                else
                                {
                                        // Alt is not held
                                        input.key = SDLK_RETURN;

                                        is_done = true;
                                }
                        }
                        break;  // case SDLK_KP_ENTER

                        case SDLK_KP_6:
                        case SDLK_KP_1:
                        case SDLK_KP_2:
                        case SDLK_KP_3:
                        case SDLK_KP_4:
                        case SDLK_KP_5:
                        case SDLK_KP_7:
                        case SDLK_KP_8:
                        case SDLK_KP_9:
                        case SDLK_KP_0:
                        case SDLK_SPACE:
                        case SDLK_BACKSPACE:
                        case SDLK_TAB:
                        case SDLK_PAGEUP:
                        case SDLK_PAGEDOWN:
                        case SDLK_END:
                        case SDLK_HOME:
                        case SDLK_INSERT:
                        case SDLK_DELETE:
                        case SDLK_LEFT:
                        case SDLK_RIGHT:
                        case SDLK_UP:
                        case SDLK_DOWN:
                        case SDLK_ESCAPE:
                        case SDLK_F1:
                        case SDLK_F2:
                        case SDLK_F3:
                        case SDLK_F4:
                        case SDLK_F5:
                        case SDLK_F6:
                        case SDLK_F7:
                        case SDLK_F8:
                        case SDLK_F9:
                        case SDLK_F10:
                        {
                                is_done = true;
                        }
                        break;

                        default:
                        {
                        }
                        break;
                        }  // Key down switch
                }
                break;  // case SDL_KEYDOWN

                case SDL_KEYUP:
                {
                        const auto sdl_keysym = s_sdl_event.key.keysym.sym;

                        switch (sdl_keysym)
                        {
                        case SDLK_LSHIFT:
                        case SDLK_RSHIFT:
                        {
                                // Shift released

                                // On Windows, when the user presses
                                // shift + a numpad key, a shift release event
                                // can be received before the numpad key event,
                                // which breaks shift + numpad combinations.
                                // As a workaround, we check for "future"
                                // numpad events here.
                                SDL_Event sdl_event_tmp;

                                while (SDL_PollEvent(&sdl_event_tmp))
                                {
                                        if (sdl_event_tmp.type != SDL_KEYDOWN)
                                        {
                                                continue;
                                        }

                                        switch (sdl_event_tmp.key.keysym.sym)
                                        {
                                        case SDLK_KP_0:
                                        case SDLK_KP_1:
                                        case SDLK_KP_2:
                                        case SDLK_KP_3:
                                        case SDLK_KP_4:
                                        case SDLK_KP_5:
                                        case SDLK_KP_6:
                                        case SDLK_KP_7:
                                        case SDLK_KP_8:
                                        case SDLK_KP_9:
                                        {
                                                input.key =
                                                        sdl_event_tmp.key.keysym
                                                                .sym;

                                                is_done = true;
                                        }
                                        break;

                                        default:
                                        {
                                        }
                                        break;
                                        }  // Key down switch
                                }  // while polling event
                        }
                        break;

                        default:
                        {
                        }
                        break;
                        }  // Key released switch
                }
                break;  // case SDL_KEYUP

                case SDL_TEXTINPUT:
                {
                        const auto c =
                                (unsigned char)s_sdl_event.text.text[0];

                        if (c == '+' || c == '-')
                        {
                                if (config::is_fullscreen() ||
                                    is_window_maximized())
                                {
                                        continue;
                                }

                                P gui_dims = sdl_window_gui_dims();

                                if (c == '+')
                                {
                                        if (input.is_ctrl_held)
                                        {
                                                ++gui_dims.y;
                                        }
                                        else
                                        {
                                                ++gui_dims.x;
                                        }
                                }
                                else if (c == '-')
                                {
                                        if (input.is_ctrl_held)
                                        {
                                                --gui_dims.y;
                                        }
                                        else
                                        {
                                                --gui_dims.x;
                                        }
                                }

                                try_set_window_gui_cells(gui_dims);

                                is_window_resized = true;

                                continue;
                        }

                        if (c >= 33 && c < 126)
                        {
                                // ASCII char entered
                                // (Decimal unicode '!' = 33, '~' = 126)

                                clear_events();

                                input.key = c;

                                is_done = true;
                        }
                        else
                        {
                                continue;
                        }
                }
                break;  // case SDL_TEXT_INPUT

                default:
                {
                }
                break;

                }  // event switch

        }  // while

        SDL_StopTextInput();

        return input;
}

}  // namespace io
