// =============================================================================
// Copyright 2011-2024 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>

#include "browser.hpp"
#include "state.hpp"

namespace hints
{
enum class Id;
}  // namespace hints

enum class InputMode
{
        standard,
        vi_keys,

        END
};

enum class RendererType
{
        auto_select,
        sw,

        END
};

enum class HintsMode
{
        once_per_game,
        once,
        never,

        END
};

namespace config
{
void init();

InputMode input_mode();
std::string font_name();
bool always_center_view_on_player();
RendererType renderer_type();
bool is_tiles_mode();
void set_fullscreen(bool value);
bool is_fullscreen();
int video_scale_factor();

// Actual window size (i.e. not logical size)
void set_window_px_w(int w);
void set_window_px_h(int h);

int window_px_w();
int window_px_h();

// Logical gui cell size
int gui_cell_px_w();
int gui_cell_px_h();

// Logical map cell size
int map_cell_px_w();
int map_cell_px_h();

bool text_mode_filled_walls();
bool use_trap_color_when_obscured();
int master_volume_pct();
bool is_ambient_audio_enabled();
bool is_ambient_audio_preloaded();
bool is_bot_playing();
void enable_bot_playing();
void toggle_bot_playing();
bool is_stress_test();
void enable_stress_test();
bool is_gj_mode();
void toggle_gj_mode();
bool warn_on_throw_valuable();
bool warn_on_light_explosive();
bool warn_on_drink_malign_potion();
bool warn_on_ranged_wpn_melee();
bool is_ranged_wpn_auto_reload();
bool is_intro_lvl_skipped();
bool is_intro_popup_skipped();
bool is_any_key_confirm_more();
bool is_auto_select_menu();
HintsMode hints_mode();
bool has_seen_hint_global(hints::Id id);
void set_hint_seen_global(hints::Id id);
bool always_warn_new_mon();
int delay_projectile_draw();
int delay_explosion();
void set_default_player_name(const std::string& name);
std::string default_player_name();

}  // namespace config

class OptionsState : public State
{
public:
        OptionsState();

        void update() override;

        void draw() override;

        StateId id() const override;

private:
        MenuBrowser m_browser;
};

class OptionsSubmenuState : public State
{
public:
        OptionsSubmenuState();

        void update() override;

        void draw() override;

        StateId id() const override;

private:
        MenuBrowser m_browser;
};

#endif  // CONFIG_HPP
