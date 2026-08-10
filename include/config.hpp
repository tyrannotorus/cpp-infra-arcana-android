// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>

#include "browser.hpp"
#include "menu_page.hpp"
#include "state.hpp"

namespace hints
{
enum class Id;
}  // namespace hints

namespace config
{
enum class OptionSubmenuType;
}  // namespace config

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
// Size of the source tile images in pixels. The logical map cell size is
// this value multiplied by the map scale factor (tiles are stretched when
// drawn at map scale factors above one).
inline constexpr int g_tile_img_px = 20;

void init();

InputMode input_mode();
bool is_double_click_toggle_fullscreen();
std::string font_name();
bool always_center_view_on_player();
RendererType renderer_type();
bool is_tiles_mode();
void set_fullscreen(bool value);
bool is_fullscreen();
int video_scale_factor();
int brightness_pct();

// If true, the in-game side stats panel is laid out on the left side of the
// screen instead of the right (toggled by swiping the panel on touch
// devices, to support both right and left handed play)
bool is_side_panel_left();
void set_side_panel_left(bool value);

// Action bar layout (see action_bar): comma separated action id lists.
// Empty strings mean "use the defaults".
std::string action_bar_order();
std::string action_bar_disabled();
void set_action_bar_layout(
        const std::string& order_csv,
        const std::string& disabled_csv);

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

// Scale of the in-world (map) display relative to the gui (tiles mode)
int map_scale_factor();

bool text_mode_filled_walls();
bool display_health_bars();
bool use_trap_color_when_obscured();
int master_volume_pct();
bool is_ambient_audio_enabled();
bool is_ambient_audio_preloaded();
int audio_buffer_size();
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
bool is_medical_bag_auto_choice();
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

}  // namespace config

// The options root page (Video / Audio / Input / Gameplay), reachable from
// the title screen
class OptionsState : public MenuPageState
{
public:
        OptionsState();

        StateId id() const override;

protected:
        std::string page_title() const override;

        std::vector<MenuPageEntry> page_entries() const override;

        void on_entry_selected(int idx) override;
};

// An options submenu (e.g. Video), pushed from the options root page or
// engaged directly from the in-game menu
class OptionsSubmenuState : public MenuPageState
{
public:
        OptionsSubmenuState(config::OptionSubmenuType submenu);

        StateId id() const override;

protected:
        std::string page_title() const override;

        std::string page_hint() const override;

        std::vector<MenuPageEntry> page_entries() const override;

        void on_entry_selected(int idx) override;

        void on_entry_left(int idx) override;

        void on_entry_right(int idx) override;

        bool entry_plays_selection_audio(int idx) const override;

        void draw_page_content() override;

        int list_x0(int block_w) const override;

        int list_y0(int nr_entries_shown) const override;

        int list_max_x1() const override;

        bool use_left_right_keys() const override;
};

#endif  // CONFIG_HPP
