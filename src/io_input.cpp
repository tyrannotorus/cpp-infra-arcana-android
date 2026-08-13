// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <jni.h>
#include <ostream>
#include <string>

#include "SDL_events.h"
#include "SDL_keyboard.h"
#include "SDL_keycode.h"
#include "SDL_system.h"
#include "SDL_timer.h"
#include "SDL_video.h"
#include "config.hpp"
#include "debug.hpp"
#include "io.hpp"
#include "io_internal.hpp"
#include "pos.hpp"
#include "state.hpp"

#include "action_bar.hpp"
#include "context_pins.hpp"
#include "dpad.hpp"
#include "draw_box.hpp"
#include "io_display.hpp"
#include "msg_log.hpp"
#include "panel.hpp"
#include "popup.hpp"
#include "rect.hpp"
#include "viewport.hpp"

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

#ifndef NDEBUG
        // This is set to false in the clear_screen() call above, but in this case we really do just
        // want to show an empty screen.
        io::g_allow_render = true;
#endif  // NDEBUG

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

        case SDLK_AC_BACK: {
                // The Android back button acts as escape
                s_input.key = SDLK_ESCAPE;

                s_is_done_reading_input = true;
        } break;

        default:
        {
        } break;
        }
}

// Characters received but not yet handed to the game. A text input event
// carries a whole COMMITTED STRING, not a keystroke: an on-screen keyboard
// commits several characters at once whenever it corrects, predicts or
// glides a word, and the string arrives as one event (see SDL's
// SDLInputConnection.updateText, which sends the pending text in a single
// nativeCommitText call). The game reads one key at a time, so the rest
// wait here.
static std::string s_pending_text_input;

// Hands the next queued character to the game as a key press
static bool take_pending_text_input()
{
        if (s_pending_text_input.empty()) {
                return false;
        }

        s_input.key = (unsigned char)s_pending_text_input.front();

        s_pending_text_input.erase(std::begin(s_pending_text_input));

        s_is_done_reading_input = true;

        return true;
}

static void handle_textinput_event()
{
        const char* const text = s_sdl_event.text.text;

        const auto c = text[0];

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

        // Every character of the event is queued, and the first one is
        // handed over right away.
        //
        // NOTE: The event queue must NOT be cleared here - it holds the
        // keystrokes typed while this event was being processed, and, on a
        // touch device, any gesture in progress (which clearing input
        // abandons, see clear_input).
        for (int i = 0; text[i] != '\0'; ++i) {
                const char text_char = text[i];

                // Space is not "printable" by the test above, but it is a
                // key the game understands (SDLK_SPACE is its ASCII value),
                // and names may contain one
                if (is_printable_ascii_char(text_char) ||
                    (text_char == ' ')) {
                        s_pending_text_input.push_back(text_char);
                }
        }

        take_pending_text_input();
}

static void handle_mousebutton_event()
{
        const uint8_t clicks = s_sdl_event.button.clicks;
        const uint8_t button = s_sdl_event.button.button;

        if ((clicks == 2) && (button == 1)) {
                TRACE << "Left mouse button double-click" << std::endl;

                if (config::is_double_click_toggle_fullscreen()) {
                        config::set_fullscreen(!config::is_fullscreen());

                        io::on_user_toggle_fullscreen();

                        io::clear_input();
                }
        }
}

static void handle_mousewheel_event()
{
        State* curr_st = states::current_state();
        if (curr_st && curr_st->id() == StateId::game) {
                return;
        }

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

static Uint32 hide_cursor_callback(Uint32, void*)
{
        return SDL_ShowCursor(SDL_FALSE);
}

static void handle_mousemotion_event()
{
        SDL_ShowCursor(SDL_TRUE);
        static SDL_TimerID timer = 0;
        SDL_RemoveTimer(timer);
        timer = SDL_AddTimer(1000, hide_cursor_callback, nullptr);
}

// -----------------------------------------------------------------------------
// Android touch input
//
// The game is entirely key-driven (there is no pointer support to hook into),
// so touch gestures are translated to keys the game already understands:
//
//   swipe          -> movement key in the swiped direction (numpad 1-9)
//   tap            -> return (confirm/select)
//   hold + drag    -> pan the map (the finger held down briefly and then
//                     dragged - a quick flick is a movement swipe instead)
//   two-finger tap -> show/hide the on-screen keyboard, for letter commands
//                     ('i' for inventory, 'f' for aiming, etc.)
//
// NOTE: There is deliberately no long-press gesture (cancelling is done via
// the [ x ] border control, the action bar, or the device back button)
// -----------------------------------------------------------------------------
static bool s_touch_gesture_active = false;
static bool s_touch_gesture_consumed = false;
static SDL_FingerID s_touch_finger_id = 0;
static uint32_t s_touch_start_ms = 0;
static float s_touch_start_x = 0.0f;
static float s_touch_start_y = 0.0f;

// Two finger gesture state (keyboard toggle on a two finger tap).
// Normalized [0, 1] finger positions, indexed 0 = the first finger of the
// gesture, 1 = the second.
static bool s_two_finger_active = false;
static SDL_FingerID s_two_finger_ids[2] = {};
static float s_two_finger_x[2] = {};
static float s_two_finger_y[2] = {};

// Sub-cell map scroll in logical pixels (the fractional part of panning -
// whole cells shift the viewport, see viewport::pan)
static float s_map_scroll_x = 0.0f;
static float s_map_scroll_y = 0.0f;

// Total midpoint travel (window pixels), to distinguish taps from drags
static float s_two_finger_travel_px = 0.0f;

// Single finger direct content dragging - ONLY from explicit, narrow
// handle zones (the action config row handles, the description column
// scrollbar), so engaging is unambiguous and immediate. Content areas
// themselves are never drag zones (swipes over them must stay swipes).
static bool s_touch_drag_active = false;

// The gesture's maximum travel (window pixels) from the start position -
// used to classify taps on release (a flick that moved and came back is
// NOT a tap)
static float s_touch_max_travel_px = 0.0f;

// Single finger map panning (game and marker states) - engaged when the
// finger has been held down briefly AND dragged beyond the tap distance. A
// quick flick releases before the time gate and is handled as a movement
// swipe on release instead, so rapid movement swiping never moves the map.
static bool s_map_pan_active = false;

// The pan engaged WITHOUT the hold delay (targeting states, see
// handle_single_finger_motion) - the gesture may still turn out to be a
// movement swipe, which is decided on release
static bool s_map_pan_provisional = false;

static float s_map_pan_last_x = 0.0f;
static float s_map_pan_last_y = 0.0f;

// Distance thresholds as fractions of the window's smallest dimension
static const float s_touch_tap_max_dist = 0.03f;
static const float s_touch_swipe_min_dist = 0.06f;

// Minimum finger-down time before a drag counts as map panning (a quicker
// release is a swipe)
static const uint32_t s_map_pan_engage_ms = 230U;

// Holding still on an action bar button this long lifts it for reordering
// (see action_bar::try_begin_reorder_drag). Clearly longer than the map
// pan delay above, and in the range platform long presses use.
static const uint32_t s_bar_lift_ms = 450U;

// An action bar button has been lifted and follows the finger
static bool s_bar_drag_active = false;

// Holding still on the movement pad lifts it for arranging (see
// dpad::begin_edit_mode). Longer than the bar's own lift, and longer than a
// platform long press: a finger resting on a movement key between steps is
// normal, and must not be taken for a request to rearrange the interface.
static const uint32_t s_dpad_edit_hold_ms = 1000U;

// The movement pad is being moved or scaled by the finger
static bool s_dpad_drag_active = false;

// Maximum total duration of a swipe - a swipe is a QUICK directional
// gesture. A slower gesture (e.g. a drag that missed its handle) is not a
// swipe and does nothing. (Standard touch libraries bound swipes by time
// or velocity; common max-duration values are 150-500 ms.)
static const uint32_t s_touch_swipe_max_ms = 350U;

static void reset_touch_gesture()
{
        s_touch_gesture_active = false;
        s_touch_gesture_consumed = false;
        s_touch_max_travel_px = 0.0f;

        s_two_finger_active = false;
        s_two_finger_travel_px = 0.0f;

        s_touch_drag_active = false;

        if (s_bar_drag_active) {
                // The gesture was interrupted (e.g. input cleared by a
                // state change) - drop the lifted button where it is
                action_bar::end_reorder_drag();

                s_bar_drag_active = false;
        }

        if (s_dpad_drag_active) {
                // Likewise for the movement pad - it stays where the
                // gesture left it (still lifted, so it can be adjusted
                // further or put down with a tap)
                dpad::end_edit_drag();

                s_dpad_drag_active = false;
        }

        s_map_pan_active = false;
        s_map_pan_provisional = false;

        // Abandon any unsettled sub-cell map scroll (e.g. the gesture was
        // interrupted by input clearing)
        if ((s_map_scroll_x != 0.0f) || (s_map_scroll_y != 0.0f)) {
                s_map_scroll_x = 0.0f;
                s_map_scroll_y = 0.0f;

                io::set_map_scroll_px_offset({0, 0});
        }
}

static int touch_swipe_dir_key(const float dx_px, const float dy_px)
{
        // Eight 45 degree sectors, centered on the compass directions (the y
        // delta is negated since screen y grows downwards).
        const double pi = 3.14159265358979323846;

        const double angle = std::atan2(-(double)dy_px, (double)dx_px);

        const int sector = (int)std::lround(angle / (pi / 4.0));

        switch (sector) {
        case 0:
                return SDLK_KP_6;
        case 1:
                return SDLK_KP_9;
        case 2:
                return SDLK_KP_8;
        case 3:
                return SDLK_KP_7;
        case 4:
        case -4:
                return SDLK_KP_4;
        case -3:
                return SDLK_KP_1;
        case -2:
                return SDLK_KP_2;
        case -1:
                return SDLK_KP_3;
        default:
                return SDLK_KP_5;
        }
}

// Screen pixels covered by the on-screen keyboard, as observed by the
// activity's window inset listener (see IAActivity.java). Returns 0 when
// the keyboard is down, and also when the height cannot be determined
// (before Android 11 there is no IME inset).
static int android_screen_keyboard_px_h()
{
        auto* const env = (JNIEnv*)SDL_AndroidGetJNIEnv();

        if (!env) {
                return 0;
        }

        auto* const activity = (jobject)SDL_AndroidGetActivity();

        if (!activity) {
                return 0;
        }

        jclass cls = env->GetObjectClass(activity);

        const jmethodID method_id =
                env->GetStaticMethodID(cls, "screenKeyboardHeightPx", "()I");

        int px_h = 0;

        if (method_id) {
                px_h = (int)env->CallStaticIntMethod(cls, method_id);
        }

        env->DeleteLocalRef(cls);
        env->DeleteLocalRef(activity);

        return px_h;
}

// Asks the activity for a haptic tick (see IAActivity.performHapticTick).
// The activity posts it to the view's own thread - this is called from the
// game thread.
static void android_haptic_tick(const bool is_long_press)
{
        auto* const env = (JNIEnv*)SDL_AndroidGetJNIEnv();

        if (!env) {
                return;
        }

        auto* const activity = (jobject)SDL_AndroidGetActivity();

        if (!activity) {
                return;
        }

        jclass cls = env->GetObjectClass(activity);

        const jmethodID method_id =
                env->GetStaticMethodID(cls, "performHapticTick", "(Z)V");

        if (method_id) {
                env->CallStaticVoidMethod(
                        cls,
                        method_id,
                        (jboolean)is_long_press);
        }

        env->DeleteLocalRef(cls);
        env->DeleteLocalRef(activity);
}

static void toggle_screen_keyboard()
{
        // NOTE: The on-screen keyboard state must be queried with
        // SDL_IsScreenKeyboardShown - SDL_IsTextInputActive only tells
        // whether text EVENTS are enabled, which SDL turns on during video
        // init and which therefore says nothing about the keyboard
        if (SDL_IsScreenKeyboardShown(io::g_sdl_window)) {
                io::hide_screen_keyboard();
        }
        else {
                io::show_screen_keyboard();
        }
}

// Logical pixel rectangle fully covering a rectangle of screen gui cells
static R gui_area_to_logical_px_rect(const R& gui_area)
{
        const P p0 = io::gui_to_px_coords(gui_area.p0);

        const P p1 = io::gui_to_px_coords(gui_area.p1 + 1) - 1;

        return {p0, p1};
}

// Handles a swipe starting on the side stats panel while playing - swiping
// toward the nearest screen edge slides the panel to the other side of the
// screen (left/right handedness support). Returns true if the swipe was
// consumed (i.e. it started on the side panel).
static bool try_handle_side_panel_swipe(
        const P& logical_start_px,
        const float dx_px,
        const float dy_px)
{
        const State* const state = states::current_state();

        if (!state || (state->id() != StateId::game)) {
                return false;
        }

        const auto side_px_rect =
                io::panel_logical_px_rect(Panel::map_gui_stats_border);

        if (!side_px_rect.is_pos_inside(logical_start_px)) {
                return false;
        }

        const bool is_horizontal =
                std::fabs(dx_px) > std::fabs(dy_px);

        if (is_horizontal) {
                const bool is_left = config::is_side_panel_left();

                const bool is_toward_edge =
                        is_left
                        ? (dx_px < 0.0f)
                        : (dx_px > 0.0f);

                if (is_toward_edge) {
                        io::run_side_panel_slide_animation();

                        io::clear_input();
                }
        }

        // Swipes starting on the side panel never generate movement keys
        return true;
}

static void handle_finger_down_event()
{
        if (s_touch_gesture_active) {
                // A second finger joined - begin a two finger gesture (a
                // keyboard toggle if it turns out to be a tap - decided on
                // release). Additional fingers beyond the second are
                // ignored.
                // NOTE: Not while panning the map - a provisional pan
                // leaves the gesture unconsumed, but a second finger
                // landing mid-pan is not a two finger gesture
                if (!s_two_finger_active &&
                    !s_touch_gesture_consumed &&
                    !s_map_pan_active &&
                    (s_sdl_event.tfinger.fingerId != s_touch_finger_id)) {
                        s_two_finger_active = true;

                        s_two_finger_ids[0] = s_touch_finger_id;
                        s_two_finger_x[0] = s_touch_start_x;
                        s_two_finger_y[0] = s_touch_start_y;

                        s_two_finger_ids[1] = s_sdl_event.tfinger.fingerId;
                        s_two_finger_x[1] = s_sdl_event.tfinger.x;
                        s_two_finger_y[1] = s_sdl_event.tfinger.y;

                        s_two_finger_travel_px = 0.0f;
                }

                return;
        }

        s_touch_gesture_active = true;
        s_touch_gesture_consumed = false;
        s_touch_max_travel_px = 0.0f;
        s_touch_finger_id = s_sdl_event.tfinger.fingerId;
        s_touch_start_ms = SDL_GetTicks();
        s_touch_start_x = s_sdl_event.tfinger.x;
        s_touch_start_y = s_sdl_event.tfinger.y;
}

// Applies the current fractional map scroll: whole cells are transferred to
// the viewport (redrawing the map content), the sub-cell remainder becomes
// the map display's composite scroll offset. At the pan limits, the
// fractional part is clamped so the view never slides beyond the map area.
static void apply_map_scroll()
{
        const float cell_w = (float)config::map_cell_px_w();
        const float cell_h = (float)config::map_cell_px_h();

        // Transfer whole cells to the viewport (positive scroll = the source
        // window moves right/down = the content appears moved left/up)
        const int cells_x = (int)(s_map_scroll_x / cell_w);
        const int cells_y = (int)(s_map_scroll_y / cell_h);

        bool did_step = false;

        if ((cells_x != 0) || (cells_y != 0)) {
                viewport::pan({cells_x, cells_y});

                s_map_scroll_x -= (float)cells_x * cell_w;
                s_map_scroll_y -= (float)cells_y * cell_h;

                did_step = true;
        }

        // Clamp the fractional part when the viewport is at a pan limit
        const auto limits = viewport::pan_limits();
        const auto origin = viewport::origin();

        if (origin.x <= limits.p0.x) {
                s_map_scroll_x = std::max(0.0f, s_map_scroll_x);
        }

        if (origin.x >= limits.p1.x) {
                s_map_scroll_x = std::min(0.0f, s_map_scroll_x);
        }

        if (origin.y <= limits.p0.y) {
                s_map_scroll_y = std::max(0.0f, s_map_scroll_y);
        }

        if (origin.y >= limits.p1.y) {
                s_map_scroll_y = std::min(0.0f, s_map_scroll_y);
        }

        if (did_step) {
                // E.g. the free look marker follows the view center
                State* const state = states::current_state();

                if (state) {
                        state->on_map_panned();
                }

                states::draw();
        }

        io::set_map_scroll_px_offset(
                {(int)std::lround(s_map_scroll_x),
                 (int)std::lround(s_map_scroll_y)});

        io::update_screen();
}

// Eases the remaining sub-cell scroll back to zero when the finger is
// released, so that the cell under the pin stays EXACTLY the cell the
// user landed on. NOTE: The viewport must NOT step to the nearest cell
// boundary here: the pin/reticle slides along with the map content
// between whole-cell crossings, so a user releasing with the reticle on
// their target tile can be up to a full cell past the last crossing -
// rounding to the nearest boundary then snaps the pin one tile further
// in the drag direction (very noticeable horizontally, where map cells
// are narrower than they are tall).
static void settle_map_scroll()
{
        const float start_x = s_map_scroll_x;
        const float start_y = s_map_scroll_y;

        if ((start_x == 0.0f) && (start_y == 0.0f)) {
                return;
        }

        const uint32_t duration_ms = 90U;

        const uint32_t start_ms = SDL_GetTicks();

        while (true) {
                const uint32_t elapsed_ms = SDL_GetTicks() - start_ms;

                if (elapsed_ms >= duration_ms) {
                        break;
                }

                const float t = (float)elapsed_ms / (float)duration_ms;

                const float u = 1.0f - t;

                const float remaining = u * u * u;  // Cubic.out

                io::set_map_scroll_px_offset(
                        {(int)std::lround(start_x * remaining),
                         (int)std::lround(start_y * remaining)});

                io::update_screen();

                SDL_Delay(10U);
        }

        s_map_scroll_x = 0.0f;
        s_map_scroll_y = 0.0f;

        io::set_map_scroll_px_offset({0, 0});

        io::update_screen();
}

// Folds one finger motion event into the two finger gesture state (finger
// positions and total midpoint travel - used for telling a two finger tap
// from a drag on release)
static void update_two_finger_travel(const SDL_TouchFingerEvent& event)
{
        const auto finger_id = event.fingerId;

        int idx;

        if (finger_id == s_two_finger_ids[0]) {
                idx = 0;
        }
        else if (finger_id == s_two_finger_ids[1]) {
                idx = 1;
        }
        else {
                return;
        }

        const float mid_x_before =
                (s_two_finger_x[0] + s_two_finger_x[1]) / 2.0f;

        const float mid_y_before =
                (s_two_finger_y[0] + s_two_finger_y[1]) / 2.0f;

        s_two_finger_x[idx] = event.x;
        s_two_finger_y[idx] = event.y;

        const float mid_x = (s_two_finger_x[0] + s_two_finger_x[1]) / 2.0f;
        const float mid_y = (s_two_finger_y[0] + s_two_finger_y[1]) / 2.0f;

        P window_px_dims;

        SDL_GetWindowSize(
                io::g_sdl_window,
                &window_px_dims.x,
                &window_px_dims.y);

        const float dx_px = (mid_x - mid_x_before) * (float)window_px_dims.x;
        const float dy_px = (mid_y - mid_y_before) * (float)window_px_dims.y;

        s_two_finger_travel_px +=
                std::sqrt((dx_px * dx_px) + (dy_px * dy_px));
}

// The logical screen position of a normalized touch coordinate
static P touch_logical_px(const float x, const float y)
{
        P window_px_dims;

        SDL_GetWindowSize(
                io::g_sdl_window,
                &window_px_dims.x,
                &window_px_dims.y);

        return io::window_px_to_logical_px(
                P((int)(x * (float)window_px_dims.x),
                  (int)(y * (float)window_px_dims.y)));
}

// Whether the gesture is still a press: down, uncommitted, and the finger
// has not strayed from where it landed.
//
// NOTE: The two things a press engages (lifting a bar button, arranging the
// movement pad) are timed from the input loop rather than from motion
// events - a finger resting still produces no events at all.
static bool is_touch_still_pressing()
{
        if (!s_touch_gesture_active ||
            s_touch_gesture_consumed ||
            s_two_finger_active ||
            s_touch_drag_active ||
            s_map_pan_active ||
            s_bar_drag_active ||
            s_dpad_drag_active) {
                return false;
        }

        P window_px_dims;

        SDL_GetWindowSize(
                io::g_sdl_window,
                &window_px_dims.x,
                &window_px_dims.y);

        const float min_window_dim =
                (float)std::min(window_px_dims.x, window_px_dims.y);

        // The finger has strayed - this is a drag or a swipe, not a press
        return s_touch_max_travel_px <=
                (s_touch_tap_max_dist * min_window_dim);
}

// Advances a position to where the gesture's finger is RIGHT NOW, taking
// the last of the queued motion events (and dropping the rest).
//
// NOTE: Touch events can arrive faster than frames can be presented, and
// handling one motion per presented frame builds an ever growing backlog -
// the content lags behind the finger, badly on slow devices. Every drag
// therefore folds all queued motion into one step, and renders ONCE.
static void take_latest_finger_motion(float& x, float& y)
{
        SDL_PumpEvents();

        SDL_Event queued_event;

        while (SDL_PeepEvents(
                       &queued_event,
                       1,
                       SDL_GETEVENT,
                       SDL_FINGERMOTION,
                       SDL_FINGERMOTION) > 0) {
                if (queued_event.tfinger.fingerId == s_touch_finger_id) {
                        x = queued_event.tfinger.x;
                        y = queued_event.tfinger.y;
                }
        }
}

// A finger held still on the movement pad lifts it for arranging: dragging
// on from there moves it, and its cells stop taking movement steps until it
// is put down again.
static void step_touch_dpad_hold()
{
        if (dpad::is_edit_active() ||
            ((SDL_GetTicks() - s_touch_start_ms) < s_dpad_edit_hold_ms) ||
            !is_touch_still_pressing()) {
                return;
        }

        const P start_logical_px =
                touch_logical_px(s_touch_start_x, s_touch_start_y);

        if (!dpad::is_pos_on_pad(start_logical_px)) {
                return;
        }

        dpad::begin_edit_mode(start_logical_px);

        s_dpad_drag_active = true;

        // The release must not also take a step in that cell's direction
        s_touch_gesture_consumed = true;

        states::draw();

        io::update_screen();
}

// A finger held still on an action bar button lifts it for reordering
static void step_touch_bar_lift()
{
        if (((SDL_GetTicks() - s_touch_start_ms) < s_bar_lift_ms) ||
            !is_touch_still_pressing()) {
                return;
        }

        const bool did_lift =
                action_bar::try_begin_reorder_drag(
                        touch_logical_px(s_touch_start_x, s_touch_start_y));

        if (!did_lift) {
                return;
        }

        s_bar_drag_active = true;

        // The release must not also engage the button's action
        s_touch_gesture_consumed = true;

        states::draw();

        io::update_screen();
}

// Single finger dragging pans the map (game and marker states), or scrolls
// scrollable content (e.g. info screens), with pixel precision - the content
// follows the finger.
static void handle_single_finger_motion()
{
        State* const state = states::current_state();

        if (!state) {
                return;
        }

        const float x = s_sdl_event.tfinger.x;
        const float y = s_sdl_event.tfinger.y;

        P window_px_dims;

        SDL_GetWindowSize(
                io::g_sdl_window,
                &window_px_dims.x,
                &window_px_dims.y);

        // Track the gesture's maximum travel from the start position - the
        // tap classification on release uses this, so that a swipe whose
        // release happens to land near the start point never counts as a
        // tap (see handle_finger_up_event)
        {
                const float travel_dx_px =
                        (x - s_touch_start_x) * (float)window_px_dims.x;

                const float travel_dy_px =
                        (y - s_touch_start_y) * (float)window_px_dims.y;

                const float travel_px =
                        std::sqrt(
                                (travel_dx_px * travel_dx_px) +
                                (travel_dy_px * travel_dy_px));

                s_touch_max_travel_px =
                        std::max(s_touch_max_travel_px, travel_px);
        }

        if (msg_log::is_waiting_prompt()) {
                // A "more" prompt or a log query owns the input until it is
                // answered - nothing may pan, scroll or drag meanwhile (the
                // gesture's travel is still tracked above, so that a drag
                // is not mistaken for a tap and answers the prompt by
                // accident). Taps still go through and answer it, and so do
                // swipes, which are classified on release - that is how the
                // direction query is answered.
                return;
        }

        if (dpad::is_edit_active() && !s_bar_drag_active) {
                // The movement pad is being arranged - it owns every
                // gesture until it is put down, and nothing under it (the
                // map above all) reacts to any of them
                if (!s_dpad_drag_active) {
                        const P start_logical_px =
                                touch_logical_px(
                                        s_touch_start_x,
                                        s_touch_start_y);

                        if (!dpad::is_pos_on_pad(start_logical_px)) {
                                return;
                        }

                        // Scaling or moving, decided by where the finger
                        // came down (see dpad::begin_edit_drag)
                        dpad::begin_edit_drag(start_logical_px);

                        s_dpad_drag_active = true;

                        s_touch_gesture_consumed = true;
                }
        }

        if (s_dpad_drag_active) {
                // Carrying (or scaling) the pad
                float drag_x = s_sdl_event.tfinger.x;
                float drag_y = y;

                take_latest_finger_motion(drag_x, drag_y);

                dpad::edit_drag_move(touch_logical_px(drag_x, drag_y));

                states::draw();

                io::update_screen();

                return;
        }

        if (s_bar_drag_active) {
                // Carrying a lifted action bar button - the bar reorders
                // itself under it
                float drag_x = s_sdl_event.tfinger.x;
                float drag_y = y;

                take_latest_finger_motion(drag_x, drag_y);

                action_bar::reorder_drag_move(
                        touch_logical_px(drag_x, drag_y));

                states::draw();

                io::update_screen();

                return;
        }

        const float scale = (float)config::video_scale_factor();

        if (s_touch_drag_active) {
                // Direct content dragging (e.g. row reordering)
                float drag_x = s_sdl_event.tfinger.x;
                float drag_y = y;

                take_latest_finger_motion(drag_x, drag_y);

                const P window_px(
                        (int)(drag_x * (float)window_px_dims.x),
                        (int)(drag_y * (float)window_px_dims.y));

                state->on_touch_drag_move(
                        io::window_px_to_logical_px(window_px));

                states::draw();

                io::update_screen();

                return;
        }

        if (s_map_pan_active) {
                // Map panning - the content follows the finger with pixel
                // precision, like dragging the map surface itself. Whole
                // cells are transferred to the viewport underneath; the
                // sub-cell remainder slides the composited map content. The
                // pan is kept on release (settling on the nearest cell),
                // and the camera snaps back to the player on any play
                // movement (see viewport::should_auto_center).
                float latest_x = x;
                float latest_y = y;

                take_latest_finger_motion(latest_x, latest_y);

                const float dx_px =
                        (latest_x - s_map_pan_last_x) *
                        (float)window_px_dims.x;

                const float dy_px =
                        (latest_y - s_map_pan_last_y) *
                        (float)window_px_dims.y;

                s_map_pan_last_x = latest_x;
                s_map_pan_last_y = latest_y;

                // The content follows the finger: a finger movement to the
                // right moves the source window to the left
                s_map_scroll_x -= dx_px / scale;
                s_map_scroll_y -= dy_px / scale;

                apply_map_scroll();

                return;
        }

        {
                const auto state_id = state->id();

                const bool is_targeting = (state_id == StateId::marker);

                if ((state_id == StateId::game) || is_targeting) {
                        const P start_logical_px =
                                touch_logical_px(
                                        s_touch_start_x,
                                        s_touch_start_y);

                        if (action_bar::is_pos_on_bar(start_logical_px)) {
                                // A gesture that started on the bar belongs
                                // to the bar (holding lifts a button for
                                // reordering, see step_touch_bar_lift) -
                                // the map is never panned from there
                                return;
                        }

                        if (context_pins::is_pos_on_pins(start_logical_px)) {
                                // Same for the pin row sitting on top of
                                // the bar - a finger that came down on a
                                // pin is reaching for that pin, and a
                                // slightly sloppy tap must not drag the
                                // map out from under it
                                return;
                        }

                        if (dpad::is_pos_on_pad(start_logical_px)) {
                                // And for the movement pad - a gesture that
                                // came down on it belongs to it (holding it
                                // lifts it for arranging, see
                                // step_touch_dpad_hold)
                                return;
                        }

                        // Map panning engages once the finger has been down
                        // for a short time AND has moved beyond the tap
                        // distance. A quick flick releases before the time
                        // gate and becomes a movement swipe (decided on
                        // release), so rapid movement swiping never moves
                        // the map.
                        //
                        // While TARGETING (a marker state) there is no hold
                        // delay: aiming is what the mode is for, so the
                        // reticle follows the finger from the first pixel.
                        // Such a pan is provisional - a quick flick is still
                        // classified as a movement swipe on release (which
                        // cancels the targeting and moves, see
                        // MarkerState::update; popping the marker undoes the
                        // pan).
                        const float dx_from_start_px =
                                (x - s_touch_start_x) *
                                (float)window_px_dims.x;

                        const float dy_from_start_px =
                                (y - s_touch_start_y) *
                                (float)window_px_dims.y;

                        const float travel_px =
                                std::sqrt(
                                        (dx_from_start_px *
                                         dx_from_start_px) +
                                        (dy_from_start_px *
                                         dy_from_start_px));

                        const float min_window_dim =
                                (float)std::min(
                                        window_px_dims.x,
                                        window_px_dims.y);

                        const uint32_t held_ms =
                                SDL_GetTicks() - s_touch_start_ms;

                        if ((is_targeting ||
                             (held_ms >= s_map_pan_engage_ms)) &&
                            (travel_px >
                             (s_touch_tap_max_dist * min_window_dim))) {
                                s_map_pan_active = true;
                                s_map_pan_provisional = is_targeting;

                                // A provisional pan must stay eligible for
                                // the release-time swipe classification
                                s_touch_gesture_consumed = !is_targeting;

                                // Manual panning takes over the map
                                // scroll offset
                                io::cancel_map_follow_tween();

                                // The pan starts from the current finger
                                // position - the travel so far is NOT
                                // applied, so the content does not jump
                                s_map_pan_last_x = x;
                                s_map_pan_last_y = y;
                        }

                        return;
                }

                // Engage scrolling once the finger has moved a small distance
                // vertically (so that taps are still recognized as taps)
                const float dy_from_start_px =
                        std::fabs(y - s_touch_start_y) *
                        (float)window_px_dims.y;

                const float engage_threshold_px =
                        0.01f * (float)std::min(
                                        window_px_dims.x,
                                        window_px_dims.y);

                if (dy_from_start_px < engage_threshold_px) {
                        return;
                }

                // Direct content dragging takes precedence over scrolling.
                // Drags engage immediately, but ONLY from explicit, narrow
                // handle zones (the action config row handles, a
                // scrollbar) - content areas are NEVER drag zones.
                //
                // Text containers were briefly draggable, arbitrated against
                // swipes by time (a swipe is quick, so a finger still down
                // past the swipe window had to be a drag). On a menu page a
                // swipe over the description still ended up both changing
                // the entry AND scrolling the text, so the ambiguity is gone
                // for good: over a text container a gesture is a swipe, and
                // scrolling is the scrollbar's job.
                const P start_window_px(
                        (int)(s_touch_start_x * (float)window_px_dims.x),
                        (int)(s_touch_start_y * (float)window_px_dims.y));

                if (state->try_begin_touch_drag(
                            io::window_px_to_logical_px(start_window_px))) {
                        s_touch_drag_active = true;
                        s_touch_gesture_consumed = true;
                }
        }
}

static void handle_finger_motion_event()
{
        if (s_two_finger_active) {
                if (!s_touch_gesture_consumed) {
                        update_two_finger_travel(s_sdl_event.tfinger);
                }

                return;
        }

        if (s_touch_gesture_active &&
            (s_sdl_event.tfinger.fingerId == s_touch_finger_id) &&
            (!s_touch_gesture_consumed ||
             s_touch_drag_active ||
             s_map_pan_active ||
             s_bar_drag_active ||
             s_dpad_drag_active)) {
                handle_single_finger_motion();
        }
}

static void handle_finger_up_event()
{
        if (!s_touch_gesture_active) {
                return;
        }

        if (s_two_finger_active) {
                // First finger of the pair released - a tap (barely any
                // movement) toggles the on-screen keyboard; anything else is
                // a dead gesture. Remaining finger-ups of this gesture are
                // ignored via the consumed flag.
                if ((s_sdl_event.tfinger.fingerId == s_two_finger_ids[0]) ||
                    (s_sdl_event.tfinger.fingerId == s_two_finger_ids[1])) {
                        if (!s_touch_gesture_consumed) {
                                P window_px_dims;

                                SDL_GetWindowSize(
                                        io::g_sdl_window,
                                        &window_px_dims.x,
                                        &window_px_dims.y);

                                const float min_window_dim =
                                        (float)std::min(
                                                window_px_dims.x,
                                                window_px_dims.y);

                                const bool is_tap =
                                        s_two_finger_travel_px <=
                                        (s_touch_tap_max_dist *
                                         min_window_dim);

                                if (is_tap) {
                                        toggle_screen_keyboard();
                                }

                                s_touch_gesture_consumed = true;
                        }

                        // If the finger that started the gesture is
                        // released, the whole gesture ends (a still-held
                        // second finger is ignored from here on)
                        if (s_sdl_event.tfinger.fingerId ==
                            s_touch_finger_id) {
                                reset_touch_gesture();
                        }
                }

                return;
        }

        if (s_sdl_event.tfinger.fingerId != s_touch_finger_id) {
                return;
        }

        if (s_bar_drag_active) {
                // Drop the lifted action bar button where it now sits
                action_bar::end_reorder_drag();

                s_bar_drag_active = false;

                reset_touch_gesture();

                states::draw();

                io::update_screen();

                return;
        }

        if (s_dpad_drag_active) {
                // The movement pad keeps its new place and size, and stays
                // lifted - a tap outside it is what puts it down (see the
                // tap handling below)
                reset_touch_gesture();

                states::draw();

                io::update_screen();

                return;
        }

        if (s_touch_drag_active) {
                State* const state = states::current_state();

                if (state) {
                        state->on_touch_drag_end();

                        states::draw();

                        io::update_screen();
                }

                reset_touch_gesture();

                return;
        }

        P window_px_dims;

        SDL_GetWindowSize(
                io::g_sdl_window,
                &window_px_dims.x,
                &window_px_dims.y);

        // Finger coordinates are normalized to [0, 1]
        const float dx_px =
                (s_sdl_event.tfinger.x - s_touch_start_x) *
                (float)window_px_dims.x;

        const float dy_px =
                (s_sdl_event.tfinger.y - s_touch_start_y) *
                (float)window_px_dims.y;

        const float dist_px =
                std::sqrt((dx_px * dx_px) + (dy_px * dy_px));

        const float min_window_dim =
                (float)std::min(window_px_dims.x, window_px_dims.y);

        const uint32_t gesture_ms = SDL_GetTicks() - s_touch_start_ms;

        // Far enough AND quick - the movement swipe test (see the swipe
        // branch at the end of this function)
        const bool is_swipe =
                (dist_px >= (s_touch_swipe_min_dist * min_window_dim)) &&
                (gesture_ms <= s_touch_swipe_max_ms);

        if (s_map_pan_active) {
                // A provisional pan (targeting, no hold delay) that turned
                // out to be a quick flick is a movement swipe after all -
                // fall through to the classification below. The aiming
                // state pops on the movement, which snaps the camera back,
                // so the panning done meanwhile undoes itself.
                if (!s_map_pan_provisional || !is_swipe) {
                        // The panned view is kept - ease the sub-cell
                        // remainder to the nearest cell boundary
                        settle_map_scroll();

                        reset_touch_gesture();

                        return;
                }
        }

        const bool was_consumed = s_touch_gesture_consumed;

        const float max_travel_px = s_touch_max_travel_px;

        reset_touch_gesture();

        if (was_consumed) {
                return;
        }

        const P start_window_px(
                (int)(s_touch_start_x * (float)window_px_dims.x),
                (int)(s_touch_start_y * (float)window_px_dims.y));

        const P end_window_px(
                (int)(s_sdl_event.tfinger.x * (float)window_px_dims.x),
                (int)(s_sdl_event.tfinger.y * (float)window_px_dims.y));

        const P start_logical_px =
                io::window_px_to_logical_px(start_window_px);

        const P end_logical_px =
                io::window_px_to_logical_px(end_window_px);

        // A tap means the finger never strayed far from the start point -
        // classified by the gesture's MAXIMUM travel, so that a swipe whose
        // release lands near its start (a flick that bounced back) is never
        // mistaken for a tap and can never select anything. Gestures between
        // the tap and swipe distances do nothing (a deliberate dead zone -
        // sloppy taps must neither select nor move anything).
        const bool is_tap =
                max_travel_px <= (s_touch_tap_max_dist * min_window_dim);

        if (is_tap) {
                // Tap on an action bar button?
                const auto bar_action = action_bar::action_at(end_logical_px);

                if (bar_action) {
                        if (bar_action->is_keyboard_toggle) {
                                toggle_screen_keyboard();
                        }
                        else {
                                s_input.key = bar_action->key;

                                s_is_done_reading_input = true;
                        }

                        return;
                }

                if (dpad::is_edit_active()) {
                        // While the movement pad is lifted, a tap anywhere
                        // but on the pad itself puts it down. Either way
                        // the tap goes no further - it must not also reach
                        // the map as a "travel to" or select anything.
                        if (!dpad::is_pos_on_pad(end_logical_px)) {
                                dpad::exit_edit_mode();
                        }

                        states::draw();

                        io::update_screen();

                        return;
                }

                State* const state = states::current_state();

                // The standard [ x ] close control of fullscreen screens
                if (state &&
                    state->has_close_button() &&
                    screen_close_button_hit_px_rect()
                            .is_pos_inside(end_logical_px)) {
                        s_input.key = SDLK_ESCAPE;

                        s_is_done_reading_input = true;

                        return;
                }

                // Tappable context pins above the action bar (e.g. the
                // [ yes ] / [ no ] answers of a waiting query, or
                // contextual actions such as [ describe ])
                const int pin_key = context_pins::key_at(end_logical_px);

                if (pin_key != 0) {
                        s_input.key = pin_key;

                        s_is_done_reading_input = true;

                        return;
                }

                // A cell of the movement pad - the same movement keys a
                // swipe would have produced (see dpad).
                //
                // NOTE: Not while a "more" prompt is up. That prompt is
                // answered by tapping, and a tap that landed on the pad
                // must answer it rather than send a movement key it may
                // not accept - the same rule the swipe branch below
                // follows. A QUERY is different: the direction query is
                // answered BY the movement keys.
                const int dpad_key =
                        msg_log::is_waiting_more_prompt()
                        ? 0
                        : dpad::key_at(end_logical_px);

                if (dpad_key != 0) {
                        s_input.key = dpad_key;

                        s_is_done_reading_input = true;

                        return;
                }

                // The state may handle the tap directly (e.g. toggling a
                // tapped row). A menu state may also mark the tapped row
                // WITHOUT handling the tap - the confirm key synthesized
                // below then selects it.
                if (state && state->try_tap(end_logical_px)) {
                        states::draw();

                        io::update_screen();

                        return;
                }

                // Popup ok/cancel buttons (hit areas are expanded by one row
                // vertically for easier tapping)
                if (state &&
                    ((state->id() == StateId::popup) ||
                     (state->id() == StateId::query_number))) {
                        auto expand = [](R area) {
                                area.p0 = area.p0.with_offsets(-1, -1);
                                area.p1 = area.p1.with_offsets(1, 1);

                                return area;
                        };

                        const auto ok_area = popup::ok_button_area();

                        if ((ok_area.p0.x >= 0) &&
                            gui_area_to_logical_px_rect(expand(ok_area))
                                    .is_pos_inside(end_logical_px)) {
                                s_input.key = SDLK_RETURN;

                                s_is_done_reading_input = true;

                                return;
                        }

                        const auto cancel_area = popup::cancel_button_area();

                        if ((cancel_area.p0.x >= 0) &&
                            gui_area_to_logical_px_rect(expand(cancel_area))
                                    .is_pos_inside(end_logical_px)) {
                                s_input.key = SDLK_ESCAPE;

                                s_is_done_reading_input = true;

                                return;
                        }
                }

                // NOTE: Any other tap acts as confirm/select on the
                // highlighted entry (menus are cancelled via the [ x ]
                // control, not by tapping outside)
                s_input.key = SDLK_RETURN;

                s_is_done_reading_input = true;
        }
        else if (is_swipe) {
                // A swipe: far enough, and QUICK - slower gestures (e.g. a
                // drag that missed its handle) do nothing

                if (msg_log::is_waiting_more_prompt()) {
                        // A "more" prompt is answered by tapping, and must
                        // not be swiped past into a move.
                        //
                        // NOTE: A QUERY must NOT be caught here. The
                        // direction query (kick, close, disarm, ...) is
                        // answered BY swiping - suppressing swipes for it
                        // left "Which direction?" with no answer but cancel.
                        return;
                }

                if (dpad::is_edit_active() ||
                    dpad::is_pos_on_pad(start_logical_px)) {
                        // The movement pad is being arranged, or the swipe
                        // came off the pad itself - either way it belongs
                        // to the pad, which takes steps by tapping
                        return;
                }

                // Swipes starting on the action bar are ignored - only
                // while the bar is shown (play states). Its panel rect
                // still exists on menu screens, where it must not eat
                // swipes (menus use the full screen height).
                if (action_bar::is_visible()) {
                        const auto bar_px_rect =
                                io::panel_logical_px_rect(Panel::action_bar);

                        if (bar_px_rect.is_pos_inside(start_logical_px)) {
                                return;
                        }
                }

                // Swiping the side stats panel slides it to the other side
                // of the screen. NOTE: Before the movement mode is
                // consulted - changing hands is not moving, and stays
                // available whichever way the player moves.
                if (try_handle_side_panel_swipe(
                            start_logical_px,
                            dx_px,
                            dy_px)) {
                        return;
                }

                if (dpad::is_visible()) {
                        // Moving is the pad's job now. The two must not
                        // coexist: the pad is held under the thumb, and a
                        // swipe leaving it would step the player twice.
                        //
                        // NOTE: Only where the pad IS the movement input -
                        // i.e. while playing. Every other screen keeps its
                        // swipes: menus are browsed by swiping, and the
                        // movement mode has nothing to say about that.
                        return;
                }

                s_input.key = touch_swipe_dir_key(dx_px, dy_px);

                s_is_done_reading_input = true;
        }
}

static void handle_render_device_reset_event()
{
        // The GPU context was lost (e.g. the app was backgrounded and
        // resumed) - all textures are invalid. Recreate the window, renderer,
        // and textures, and redraw (on_user_toggle_fullscreen does exactly
        // this - it does not itself change the fullscreen setting).
        io::on_user_toggle_fullscreen();
}

static void handle_polled_event()
{
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

        case SDL_TEXTINPUT: {
                handle_textinput_event();
        } break;

        case SDL_MOUSEBUTTONDOWN: {
                handle_mousebutton_event();
        } break;

        case SDL_MOUSEWHEEL: {
                handle_mousewheel_event();
        } break;

        case SDL_MOUSEMOTION: {
                handle_mousemotion_event();
        } break;

        case SDL_FINGERDOWN: {
                handle_finger_down_event();
        } break;

        case SDL_FINGERMOTION: {
                handle_finger_motion_event();
        } break;

        case SDL_FINGERUP: {
                handle_finger_up_event();
        } break;

        case SDL_RENDER_DEVICE_RESET: {
                handle_render_device_reset_event();
        } break;

        default:
        {
        } break;
        }
}

// Sleeps between passes of the input/render loop.
//
// The loop used to delay exactly one millisecond per pass, i.e. it woke up a
// thousand times a second to step a handful of timers whose shortest gate is
// twelve milliseconds. On a desktop that is merely wasteful; on a battery
// powered device it keeps the CPU out of its idle states permanently, and
// the heat that produces is thermal throttling - which costs frame rate
// everywhere else. Eight milliseconds is still ~120 passes a second, far
// finer than any touch input can be perceived to lag.
static void idle_between_input_passes()
{
        if (config::is_bot_playing()) {
                // The bot runs the loop as fast as it can
                return;
        }

        // A camera tween or a map shake is stepped from here, and moves the
        // composited map every pass - those want a finer cadence
        const bool is_map_animating =
                io::is_map_follow_tween_active() ||
                io::is_map_shake_active();

        SDL_Delay(is_map_animating ? 2U : 8U);
}

static void run_handle_event_cycle()
{
        update_input_mod_key_status();

        // Characters left over from a committed string are handed out one
        // per read, before anything new is taken from the queue
        if (take_pending_text_input()) {
                return;
        }

        // NOTE: The queue is drained, not sampled one event per pass. Taking
        // a single event per iteration only kept up because the loop spun at
        // a thousand iterations a second; now that it sleeps between passes,
        // a burst of touch events has to get through in one pass or the
        // input visibly lags behind the finger.
        //
        // The loop stops the moment the input is answered (or a resize is
        // signalled) - whatever is still queued belongs to whatever runs
        // next, exactly as it did before.
        while (!s_is_done_reading_input && !s_is_window_resized) {
                if (!SDL_PollEvent(&s_sdl_event)) {
                        return;
                }

                handle_polled_event();
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

        // Characters of a committed string that were still waiting their
        // turn belong to whatever was just abandoned
        s_pending_text_input.clear();

        // Any in-progress touch gesture just had its finger-up flushed
        reset_touch_gesture();
}

void haptic_feedback(const HapticFeedback kind)
{
        android_haptic_tick(kind == HapticFeedback::press);
}

void show_screen_keyboard()
{
        // NOTE: This is called unconditionally - it must NOT be guarded on
        // SDL_IsTextInputActive(). SDL enables text input during video
        // init (with the screen keyboard hint temporarily off), so text
        // input is "active" from startup onward, and such a guard would
        // silently drop every request to raise the keyboard. Calling
        // SDL_StartTextInput again when the keyboard is already up is
        // harmless.
        SDL_StartTextInput();
}

void hide_screen_keyboard()
{
        SDL_StopTextInput();

        // SDL_StopTextInput also disables text events, but letter input
        // arrives ONLY as text events (see handle_textinput_event), so a
        // hardware keyboard would go dead for the rest of the session -
        // re-enable the events, only the on-screen keyboard should hide
        SDL_EventState(SDL_TEXTINPUT, SDL_ENABLE);
        SDL_EventState(SDL_TEXTEDITING, SDL_ENABLE);
}

int screen_keyboard_covered_px_h()
{
        const int screen_px_h = panel_px_h(Panel::screen);

        // The inset is authoritative when it is there (it also tracks the
        // keyboard's open/close animation)
        const int keyboard_window_px_h = android_screen_keyboard_px_h();

        if (keyboard_window_px_h <= 0) {
                // No inset: either the keyboard is down, or its height is
                // unknown (no IME inset before Android 11, and none has
                // been dispatched yet on the first frames). If it is up
                // regardless, assume the usual half of the screen rather
                // than reporting a clear screen that is not clear.
                return SDL_IsScreenKeyboardShown(g_sdl_window)
                        ? (screen_px_h / 2)
                        : 0;
        }

        P window_px_dims;

        SDL_GetWindowSize(
                g_sdl_window,
                &window_px_dims.x,
                &window_px_dims.y);

        // The window is not resized for the keyboard - the covered height
        // is the distance from the keyboard's top edge (in logical pixels)
        // down to the bottom of the screen
        const P keyboard_top_logical_px =
                window_px_to_logical_px(
                        P(0, window_px_dims.y - keyboard_window_px_h));

        return std::clamp(
                screen_px_h - keyboard_top_logical_px.y,
                0,
                screen_px_h);
}

InputData read_input()
{

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
                idle_between_input_passes();

                if (!config::is_fullscreen()) {
                        if (s_is_window_resized) {
                                on_window_resized_signalled();

                                continue;
                        }

                        window_resized_delayed_draw();
                }

                // The camera follow tween and the map shake only move the
                // composite offset of the map display - no state redraw
                // needed
                const bool did_step_follow_tween = step_map_follow_tween();

                const bool did_step_shake = step_map_shake();

                bool should_redraw_cycling = false;

                // Do not cycle graphics if window has been resized recently,
                // or while the map is sliding or shaking: cycling redraws
                // the whole state, which on a slow device takes longer than
                // the animation itself and would leave it stepping instead
                // of moving. Tile animation just waits those moments out.
                if ((s_last_window_resize_ms == 0) &&
                    !is_map_follow_tween_active() &&
                    !is_map_shake_active()) {
                        should_redraw_cycling = step_graphics_cycling();
                }

                bool should_redraw_flash = step_flash_animations();

                if (should_redraw_cycling || should_redraw_flash) {
                        // Both of these only change map content - the tile
                        // cycling walks terrain and actors, and flashes are
                        // drawn over the map. So redraw ONLY the map
                        // display and leave the stats panel, the log and the
                        // action bar textures as they are; rebuilding the
                        // whole interface several times a second to animate
                        // a few tiles is what this used to cost.
                        //
                        // NOTE: Falls back to a full draw whenever anything
                        // on screen cannot be refreshed that way.
                        if (!states::draw_map_display()) {
                                states::draw();
                        }

                        update_screen();
                }
                else if (did_step_follow_tween || did_step_shake) {
                        update_screen();
                }

                // NOTE: Before the event cycle - a finger resting on a
                // button (or on the movement pad) sends no events, so the
                // press is timed here
                step_touch_bar_lift();

                step_touch_dpad_hold();

                run_handle_event_cycle();
        }


        return s_input;
}

}  // namespace io
