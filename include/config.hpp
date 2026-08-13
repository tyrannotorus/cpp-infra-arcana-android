// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>
#include <vector>

#include "browser.hpp"
#include "config_options.hpp"
#include "menu_descr_page.hpp"
#include "menu_page.hpp"
#include "state.hpp"

namespace hints
{
enum class Id;
}  // namespace hints

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

std::string font_name();
bool is_tiles_mode();
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

// Whether moving is done with the on-screen movement pad instead of by
// swiping the map (see dpad), and where the player has placed and how far
// they have scaled that pad: an offset in logical screen pixels from its
// default slot, and a percentage of its default size.
bool is_dpad_movement();
void set_dpad_movement(bool value);
int dpad_offset_px_x();
int dpad_offset_px_y();
int dpad_scale_pct();
void set_dpad_placement(int offset_px_x, int offset_px_y, int scale_pct);

// Logical gui cell size
int gui_cell_px_w();
int gui_cell_px_h();

// Logical map cell size
int map_cell_px_w();
int map_cell_px_h();

// Scale of the in-world (map) display relative to the gui (tiles mode)
int map_scale_factor();

bool display_health_bars();
bool use_trap_color_when_obscured();
int master_volume_pct();
bool is_ambient_audio_enabled();
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
HintsMode hints_mode();
bool has_seen_hint_global(hints::Id id);
void set_hint_seen_global(hints::Id id);
bool always_warn_new_mon();
int delay_projectile_draw();
int delay_explosion();

}  // namespace config

// Every setting on one page, reachable from the title screen and the
// in-game menu: the options grouped into sections by their submenu type,
// each under a header row, with the marked option's description beside the
// list. Longer than the screen - it scrolls like any menu list.
class SettingsState : public MenuDescrPageState
{
public:
        SettingsState();

        StateId id() const override;

protected:
        std::string page_title() const override;

        std::string page_hint() const override;

        std::vector<MenuPageEntry> page_entries() const override;

        int default_marked_idx() const override;

        void on_entry_selected(int idx) override;

        void on_entry_left(int idx) override;

        void on_entry_right(int idx) override;

        bool entry_plays_selection_audio(int idx) const override;

        void draw_page_content() override;

        int list_max_x1() const override;

        bool use_left_right_keys() const override;

private:
        // A list row: an option, or a section header (option = nullptr)
        struct Row
        {
                config::Option* option {nullptr};
                std::string header {};
        };

        void build_rows();

        void change_marked_option(
                int idx,
                config::OptionChangeCommand command);

        std::vector<Row> m_rows {};
};

#endif  // CONFIG_HPP
