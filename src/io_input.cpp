// =============================================================================
// Copyright 2011-2024 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include <cstdint>
#include <ostream>

#include "SDL_events.h"
#include "SDL_keyboard.h"
#include "SDL_keycode.h"
#include "SDL_timer.h"
#include "SDL_video.h"
#include "config.hpp"
#include "debug.hpp"
#include "io.hpp"
#include "io_internal.hpp"
#include "pos.hpp"
#include "state.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static SDL_Event s_sdl_event;
static io::InputData s_input;

static bool s_is_done_reading_input = false;
static bool s_is_window_resized = false;

static const uint32_t s_window_resize_draw_delay_ms = 400U;
static uint32_t s_last_window_resize_ms = 0U;

static void update_input_mod_key_status()
{
        const auto mod = SDL_GetModState();

        s_input.is_shift_held = mod & KMOD_SHIFT;
        s_input.is_ctrl_held = mod & KMOD_CTRL;
        s_input.is_alt_held = mod & KMOD_ALT;
}

static void on_window_resized_signalled()
{
        io::on_window_resized();
        io::clear_screen();
        io::update_screen();
        io::clear_input();

        s_is_window_resized = false;
        s_last_window_resize_ms = SDL_GetTicks();
}

static void window_resized_delayed_draw()
{
        // If the window has been resized recently, redraw the window after a
        // certain delay.

        if (s_last_window_resize_ms == 0) {
                return;
        }

        const auto d = SDL_GetTicks() - s_last_window_resize_ms;

        if (d > s_window_resize_draw_delay_ms) {
                states::draw();
                io::update_screen();
                s_last_window_resize_ms = 0;
        }
}

static P calc_gui_dims_offset_for_window_resize_cmd(const char c)
{
        if (c == '+') {
                if (s_input.is_ctrl_held) {
                        return {0, 1};
                }
                else {
                        return {1, 0};
                }
        }
        else if (c == '-') {
                if (s_input.is_ctrl_held) {
                        return {0, -1};
                }
                else {
                        return {-1, 0};
                }
        }
        else {
                return {0, 0};
        }
}

static bool is_printable_ascii_char(const char c)
{
        // '!' = 33
        // '~' = 126

        return (c >= 33) && (c < 126);
}

static bool window_has_input_focus()
{
        uint32_t window_flags = SDL_GetWindowFlags(io::g_sdl_window);

        return (window_flags & SDL_WINDOW_INPUT_FOCUS);
}

static void on_shift_released()
{
        // On Windows, when the user presses shift + a numpad key, a shift
        // release event can be received before the numpad key event, which
        // breaks shift + numpad combinations.  As a workaround, we check for
        // "future" numpad events here.
        SDL_Event sdl_event_tmp;

        while (SDL_PollEvent(&sdl_event_tmp)) {
                if (sdl_event_tmp.type != SDL_KEYDOWN) {
                        continue;
                }

                switch (sdl_event_tmp.key.keysym.sym) {
                case SDLK_KP_0:
                case SDLK_KP_1:
                case SDLK_KP_2:
                case SDLK_KP_3:
                case SDLK_KP_4:
                case SDLK_KP_5:
                case SDLK_KP_6:
                case SDLK_KP_7:
                case SDLK_KP_8:
                case SDLK_KP_9: {
                        s_input.key = sdl_event_tmp.key.keysym.sym;
                        s_is_done_reading_input = true;
                } break;

                default:
                {
                } break;
                }  // Key down switch
        }          // while polling event
}

static void handle_window_event()
{
        switch (s_sdl_event.window.event) {
        case SDL_WINDOWEVENT_SIZE_CHANGED: {
                TRACE << "Window resized" << std::endl;

                if (!config::is_fullscreen()) {
                        s_is_window_resized = true;
                }
        } break;

        case SDL_WINDOWEVENT_RESTORED: {
                TRACE << "Window restored" << std::endl;
        } break;

        case SDL_WINDOWEVENT_FOCUS_LOST: {
        } break;

        case SDL_WINDOWEVENT_FOCUS_GAINED: {
                TRACE << "Window gained focus" << std::endl;

                states::draw();
                io::update_screen();

                io::clear_input();
                io::sleep(200);
        } break;

        case SDL_WINDOWEVENT_EXPOSED: {
                TRACE << "Window exposed" << std::endl;

                states::draw();
                io::update_screen();
        } break;

        default: {
        } break;
        }
}

static void handle_quit_event()
{
        s_input.key = SDLK_ESCAPE;

        s_is_done_reading_input = true;
}

static void handle_keydown_enter_event()
{
        if (s_input.is_alt_held) {
                TRACE << "Alt-Enter pressed" << std::endl;

                config::set_fullscreen(!config::is_fullscreen());

                io::on_user_toggle_fullscreen();

                // TODO: For some reason, the alt key gets "stuck" after
                // toggling fullscreen, and must be cleared here
                // manually. Don't know if this is an issue in the IA
                // code, or an SDL bug.
                SDL_SetModState(KMOD_NONE);

                io::clear_input();
        }
        else {
                // Alt is not held
                s_input.key = SDLK_RETURN;

                s_is_done_reading_input = true;
        }
}

static void handle_keydown_event()
{
        s_input.key = s_sdl_event.key.keysym.sym;

        switch (s_input.key) {
        case SDLK_RETURN:
        case SDLK_RETURN2:
        case SDLK_KP_ENTER: {
                handle_keydown_enter_event();
        } break;

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
        case SDLK_F10: {
                s_is_done_reading_input = true;
        } break;

        default:
        {
        } break;
        }
}

static void handle_keyup_event()
{
        const auto sdl_keysym = s_sdl_event.key.keysym.sym;

        switch (sdl_keysym) {
        case SDLK_LSHIFT:
        case SDLK_RSHIFT: {
                on_shift_released();
        } break;

        default:
        {
        } break;
        }
}

static void handle_textinput_event()
{
        const auto c = s_sdl_event.text.text[0];

        if (c == '+' || c == '-') {
                if (config::is_fullscreen() || io::is_window_maximized()) {
                        return;
                }

                P gui_dims = io::sdl_window_gui_dims();

                gui_dims += calc_gui_dims_offset_for_window_resize_cmd(c);

                io::try_set_window_gui_cells(gui_dims);

                s_is_window_resized = true;

                return;
        }

        if (is_printable_ascii_char(c)) {
                io::clear_input();

                s_input.key = (unsigned char)c;

                s_is_done_reading_input = true;
        }
        else {
                return;
        }
}

static void handle_mousebutton_event()
{
        const auto clicks = s_sdl_event.button.clicks;
        const auto button = s_sdl_event.button.button;

        if ((clicks == 2) &&
            (button == 1)) {
                TRACE << "Left mouse button double-click" << std::endl;

                config::set_fullscreen(!config::is_fullscreen());

                io::on_user_toggle_fullscreen();

                io::clear_input();
        }
}

static void handle_mousewheel_event()
{
        auto delta_y = s_sdl_event.wheel.y;
        const auto direction = s_sdl_event.wheel.direction;

        if (!delta_y) {
                return;
        }

        if (direction == SDL_MOUSEWHEEL_FLIPPED) {
                delta_y = -delta_y;
        }

        SDL_Event ev = {};
        ev.type = SDL_KEYDOWN;
        ev.key.keysym.sym = (delta_y > 0) ? SDLK_UP : SDLK_DOWN;

        SDL_PushEvent(&ev);
}

static void run_handle_event_cycle()
{
        update_input_mod_key_status();

        const bool did_poll_event = SDL_PollEvent(&s_sdl_event);

        if (!did_poll_event) {
                return;
        }

        switch (s_sdl_event.type) {
        case SDL_WINDOWEVENT: {
                handle_window_event();
        } break;

        case SDL_QUIT: {
                handle_quit_event();
        } break;

        case SDL_KEYDOWN: {
                // NOTE: Apparently (surprisingly?) when the window regains
                // focus (e.g. when alt-tabbing back to the game), SDL_KEYDOWN
                // events can be received *before* SDL_WINDOWEVENT_FOCUS_GAINED.
                // This can cause things like accidentally registering window
                // manager commands like "alt-tab" as game input commands,
                // resulting in game actions that the player never intended,
                // like melee attacking a monster.
                //
                // Therefore we only handle keydown events as game commands if
                // the window has input focus (meaning the window is fully
                // restored and the restore event has been received).
                //
                if (window_has_input_focus()) {
                        handle_keydown_event();
                }
        } break;

        case SDL_KEYUP: {
                handle_keyup_event();
        } break;

        case SDL_TEXTINPUT: {
                handle_textinput_event();
        } break;

        case SDL_MOUSEBUTTONDOWN: {
                handle_mousebutton_event();
        } break;

        case SDL_MOUSEWHEEL: {
                handle_mousewheel_event();
        } break;

        default:
        {
        } break;
        }
}

// -----------------------------------------------------------------------------
// io
// -----------------------------------------------------------------------------
namespace io
{
void init_input()
{
}

void clear_input()
{
        SDL_PumpEvents();
        SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
        s_input = {};
}

InputData read_input()
{
        SDL_StartTextInput();

        s_input = {};
        s_is_done_reading_input = false;
        s_is_window_resized = false;
        s_last_window_resize_ms = 0;

        //
        // TODO: This loop handles too much details concerning rendering, it is weird that the input
        // processing loop has so much responsibility over this. It should be moved elsewhere, and
        // this loop should only call a function that processes the rendering stuff.
        //
        // When this is done, it can probably be called similarly from other places such as when
        // running projectile animations (so that flash animations are processed simultaneously for
        // example).
        //
        while (!s_is_done_reading_input) {
                sleep(1);

                if (!config::is_fullscreen()) {
                        if (s_is_window_resized) {
                                on_window_resized_signalled();

                                continue;
                        }

                        window_resized_delayed_draw();
                }

                bool should_redraw_cycling = false;

                // Do not cycle graphics if window has been resized recently.
                if (s_last_window_resize_ms == 0) {
                        should_redraw_cycling = step_graphics_cycling();
                }

                bool should_redraw_flash = step_flash_animations();

                if (should_redraw_cycling || should_redraw_flash) {
                        states::draw();
                        update_screen();
                }

                run_handle_event_cycle();
        }

        SDL_StopTextInput();

        return s_input;
}

}  // namespace io
