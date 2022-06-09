// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "config.hpp"

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "audio.hpp"
#include "audio_data.hpp"
#include "browser.hpp"
#include "colors.hpp"
#include "common_text.hpp"
#include "debug.hpp"
#include "draw_box.hpp"
#include "hints.hpp"
#include "io.hpp"
#include "misc.hpp"
#include "panel.hpp"
#include "paths.hpp"
#include "pos.hpp"
#include "query.hpp"
#include "random.hpp"
#include "rect.hpp"
#include "terrain_data.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
enum class OptionToggleDirecton
{
        enter,
        left,
        right
};

static const std::vector<std::string> font_image_names = {
        "7x13_uushi.png",
        "8x17_terminus.png",
        "10x24_dejavu_sans_mono_book.png",
        "12x22_monospace_medium.png",
        "12x24_dejavu_sans_mono_book.png",
        "13x24_dejavu_sans_mono_book.png",
        "13x24_special_elite.png",
};

static const int s_opt_values_x_pos = 44;

static InputMode s_input_mode = InputMode::standard;
static std::string s_font_name;
static bool s_always_center_view_on_player = false;
static bool s_is_tiles_mode = false;
static bool s_is_fullscreen = false;
static bool s_is_2x_scale_requested = false;
static bool s_is_2x_scale_enabled = false;
static bool s_text_mode_filled_walls = true;
static bool s_warn_on_throw_valuable = false;
static bool s_warn_on_light_explosive = false;
static bool s_warn_on_drink_malign_potion = false;
static bool s_warn_on_ranged_wpn_melee = false;
static bool s_is_ranged_wpn_auto_reload = false;
static bool s_is_intro_lvl_skipped = false;
static bool s_is_intro_popup_skipped = false;
static bool s_is_any_key_confirm_more = false;
static HintsMode s_hints_mode = HintsMode::once;
static bool s_has_seen_hint_global[(size_t)hints::Id::END];
static bool s_always_warn_new_mon = false;
static int s_delay_projectile_draw = -1;
static int s_delay_shotgun = -1;
static int s_delay_explosion = -1;
static std::string s_default_player_name;
static bool s_is_bot_playing = false;
static bool s_is_stress_test = false;
static bool s_is_gj_mode = false;
static int s_master_volume_pct_option = 100;
static int s_master_volume_pct_adjusted = 100;
static bool s_is_ambient_audio_enabled = false;
static bool s_is_ambient_audio_preloaded = false;
static int s_window_px_w = -1;
static int s_window_px_h = -1;
static int s_gui_cell_px_w = -1;
static int s_gui_cell_px_h = -1;
static int s_map_cell_px_w = -1;
static int s_map_cell_px_h = -1;

static P parse_dims_from_font_name(std::string font_name)
{
        TRACE_FUNC_BEGIN;

        TRACE << "font_name: " << font_name << std::endl;

        char ch = font_name.front();

        while (ch < '0' || ch > '9') {
                font_name.erase(std::begin(font_name));

                ch = font_name.front();
        }

        TRACE << "font_name, stripped beginning: " << font_name << std::endl;

        std::string w_str;

        while (ch != 'x') {
                font_name.erase(std::begin(font_name));

                w_str += ch;

                ch = font_name.front();
        }

        TRACE << "w_str: " << w_str << std::endl;

        font_name.erase(std::begin(font_name));

        ch = font_name.front();

        std::string h_str;

        while (ch != '_' && ch != '.') {
                font_name.erase(std::begin(font_name));

                h_str += ch;

                ch = font_name.front();
        }

        TRACE << "h_str: " << h_str << std::endl;

        TRACE
                << "Parsed font image name, found dims: "
                << w_str << "x" << h_str << std::endl;

        const int w = to_int(w_str);
        const int h = to_int(h_str);

        TRACE_FUNC_END;

        return {w, h};
}

static void update_render_dims()
{
        TRACE_FUNC_BEGIN;

        if (s_is_tiles_mode) {
                const auto font_dims = parse_dims_from_font_name(s_font_name);

                s_gui_cell_px_w = font_dims.x;
                s_gui_cell_px_h = font_dims.y;
                s_map_cell_px_w = 20;
                s_map_cell_px_h = 20;
        }
        else {
                const auto font_dims = parse_dims_from_font_name(s_font_name);

                s_gui_cell_px_w = s_map_cell_px_w = font_dims.x;
                s_gui_cell_px_h = s_map_cell_px_h = font_dims.y;
        }

        TRACE << "GUI cell size: "
              << s_gui_cell_px_w << "x" << s_gui_cell_px_h
              << std::endl;

        TRACE << "Tile size: "
              << s_map_cell_px_w << "x" << s_map_cell_px_h
              << std::endl;

        TRACE_FUNC_END;
}

static void set_default_variables()
{
        TRACE_FUNC_BEGIN;

        s_input_mode = InputMode::standard;

        // Use the smallest font
        s_font_name = font_image_names[0];

        s_always_center_view_on_player = true;

        s_is_tiles_mode = true;

        update_render_dims();

        const P native_res = io::get_native_resolution();

        s_window_px_w = native_res.x;
        s_window_px_h = native_res.y;

#ifdef NDEBUG
        s_master_volume_pct_option = s_master_volume_pct_adjusted = 100;
#else
        // Hearing the audio all the time while debug testing gets old...
        s_master_volume_pct_option = s_master_volume_pct_adjusted = 0;
#endif  // NDEBUG

        s_is_ambient_audio_enabled = true;
        s_is_ambient_audio_preloaded = false;
        s_is_fullscreen = true;
        s_is_2x_scale_requested = true;
        s_is_2x_scale_enabled = true;
        s_text_mode_filled_walls = true;
        s_is_intro_lvl_skipped = false;
        s_is_intro_popup_skipped = false;
        s_is_any_key_confirm_more = false;
        s_hints_mode = HintsMode::once;
        s_always_warn_new_mon = true;
        s_warn_on_throw_valuable = true;
        s_warn_on_light_explosive = true;
        s_warn_on_drink_malign_potion = true;
        s_warn_on_ranged_wpn_melee = true;
        s_is_ranged_wpn_auto_reload = false;
        s_delay_projectile_draw = 50;
        s_delay_shotgun = 75;
        s_delay_explosion = 300;
        s_default_player_name = "";

        for (size_t i = 0; i < (size_t)hints::Id::END; ++i) {
                s_has_seen_hint_global[i] = false;
        }

        TRACE_FUNC_END;
}

static void player_sets_option(
        const MenuBrowser& browser,
        const OptionToggleDirecton direction)
{
        switch (browser.y()) {
        case 0: {
                // Audio master volume

                audio::stop_ambient();

                const int step = 10;

                if ((direction == OptionToggleDirecton::enter) ||
                    (direction == OptionToggleDirecton::right)) {
                        // Enter or right
                        s_master_volume_pct_option += step;
                }
                else {
                        //Left
                        s_master_volume_pct_option -= step;
                }

                s_master_volume_pct_option =
                        std::clamp(
                                s_master_volume_pct_option,
                                0,
                                100);

                const auto f = ((double)s_master_volume_pct_option) / 100.0;

                s_master_volume_pct_adjusted = (int)((f * f) * 100.0);

                s_master_volume_pct_adjusted =
                        std::clamp(
                                s_master_volume_pct_adjusted,
                                0,
                                100);

                TRACE
                        << "Volume option: "
                        << s_master_volume_pct_option
                        << "%, adjusted: "
                        << s_master_volume_pct_adjusted
                        << "%"
                        << std::endl;

                audio::set_music_volume(s_master_volume_pct_adjusted);

                audio::play(audio::SfxId::menu_select);
        } break;

        case 1: {
                // Ambient audio
                s_is_ambient_audio_enabled = !s_is_ambient_audio_enabled;

                audio::stop_ambient();

                audio::play(audio::SfxId::menu_select);
        } break;

        case 2: {
                // Ambient audio
                s_is_ambient_audio_preloaded = !s_is_ambient_audio_preloaded;
        } break;

        case 3: {
                // Input mode
                auto input_mode_nr = (int)s_input_mode;
                const auto nr_input_modes = (int)InputMode::END;

                if ((direction == OptionToggleDirecton::enter) ||
                    (direction == OptionToggleDirecton::right)) {
                        // Enter or right
                        if (input_mode_nr < (nr_input_modes - 1)) {
                                ++input_mode_nr;
                        }
                        else {
                                input_mode_nr = 0;
                        }
                }
                else {
                        // Left
                        if (input_mode_nr > 0) {
                                --input_mode_nr;
                        }
                        else {
                                input_mode_nr = nr_input_modes - 1;
                        }
                }

                s_input_mode = (InputMode)(input_mode_nr);
        } break;

        case 4: {
                // Always center view_on_player
                s_always_center_view_on_player =
                        !s_always_center_view_on_player;
        } break;

        case 5: {
                // Tiles mode
                s_is_tiles_mode = !s_is_tiles_mode;

                // Attempt to use 2x scaling if requested
                s_is_2x_scale_enabled = s_is_2x_scale_requested;

                update_render_dims();
                io::init_other();
        } break;

        case 6: {
                // Font

                // Find current font index
                size_t font_idx = 0;

                const size_t nr_fonts = std::size(font_image_names);

                for (; font_idx < nr_fonts; ++font_idx) {
                        if (font_image_names[font_idx] == s_font_name) {
                                break;
                        }
                }

                if ((direction == OptionToggleDirecton::enter) ||
                    (direction == OptionToggleDirecton::right)) {
                        // Enter or right
                        if (font_idx < (nr_fonts - 1)) {
                                ++font_idx;
                        }
                        else {
                                font_idx = 0;
                        }
                }
                else {
                        // Left
                        if (font_idx > 0) {
                                --font_idx;
                        }
                        else {
                                font_idx = nr_fonts - 1;
                        }
                }

                s_font_name = font_image_names[font_idx];

                // Attempt to use 2x scaling if requested
                s_is_2x_scale_enabled = s_is_2x_scale_requested;

                update_render_dims();
                io::init_other();
        } break;

        case 7: {
                // Fullscreen
                config::set_fullscreen(!s_is_fullscreen);

                // Attempt to use 2x scaling if requested
                s_is_2x_scale_enabled = s_is_2x_scale_requested;

                io::on_user_toggle_fullscreen();
        } break;

        case 8: {
                // Scaling
                s_is_2x_scale_requested = !s_is_2x_scale_requested;

                // Attempt to use 2x scaling if requested
                s_is_2x_scale_enabled = s_is_2x_scale_requested;

                io::on_user_toggle_scaling();
        } break;

        case 9: {
                // Draw walls as filled rectangle in text mode
                s_text_mode_filled_walls = !s_text_mode_filled_walls;
        } break;

        case 10: {
                // Skip intro level
                s_is_intro_lvl_skipped = !s_is_intro_lvl_skipped;
        } break;

        case 11: {
                // Skip intro popup
                s_is_intro_popup_skipped = !s_is_intro_popup_skipped;
        } break;

        case 12: {
                // Confirm "more" with any key
                s_is_any_key_confirm_more = !s_is_any_key_confirm_more;
        } break;

        case 13: {
                // Display hints
                const auto current_idx = (int)s_hints_mode;
                const auto nr_modes = (int)HintsMode::END;

                s_hints_mode = (HintsMode)((current_idx + 1) % nr_modes);

                hints::init();
        } break;

        case 14: {
                // Always warn when a new monster appears
                s_always_warn_new_mon = !s_always_warn_new_mon;
        } break;

        case 15: {
                // Warn when throwing valuable items
                s_warn_on_throw_valuable = !s_warn_on_throw_valuable;
        } break;

        case 16: {
                // Warn when lighting explovies
                s_warn_on_light_explosive = !s_warn_on_light_explosive;
        } break;

        case 17: {
                // Warn when drinking known malign potions
                s_warn_on_drink_malign_potion = !s_warn_on_drink_malign_potion;
        } break;

        case 18: {
                // Print warning when melee attacking with ranged weapons
                s_warn_on_ranged_wpn_melee = !s_warn_on_ranged_wpn_melee;
        } break;

        case 19: {
                // Ranged weapon auto reload
                s_is_ranged_wpn_auto_reload = !s_is_ranged_wpn_auto_reload;
        } break;

        case 20: {
                // Projectile delay
                const Range allowed_range(0, 900);

                if (direction == OptionToggleDirecton::enter) {
                        // Enter
                        query::QueryNumberConfig query_config;

                        query_config.allowed_range = allowed_range;
                        query_config.default_value = s_delay_projectile_draw;
                        query_config.cancel_returns_default = true;

                        const int nr =
                                query::number(
                                        query_config,
                                        "Projectile delay");

                        if (nr != -1) {
                                s_delay_projectile_draw = nr;
                        }
                }
                else if (direction == OptionToggleDirecton::left) {
                        // Left
                        s_delay_projectile_draw -= 10;
                }
                else {
                        // Right
                        s_delay_projectile_draw += 10;
                }

                s_delay_projectile_draw =
                        std::clamp(
                                s_delay_projectile_draw,
                                allowed_range.min,
                                allowed_range.max);
        } break;

        case 21: {
                // Shotgun delay
                const Range allowed_range(0, 900);

                if (direction == OptionToggleDirecton::enter) {
                        // Enter
                        query::QueryNumberConfig query_config;

                        query_config.allowed_range = allowed_range;
                        query_config.default_value = s_delay_shotgun;
                        query_config.cancel_returns_default = true;

                        const int nr =
                                query::number(
                                        query_config,
                                        "Shotgun delay");

                        if (nr != -1) {
                                s_delay_shotgun = nr;
                        }
                }
                else if (direction == OptionToggleDirecton::left) {
                        // Left
                        s_delay_shotgun -= 10;
                }
                else {
                        // Right
                        s_delay_shotgun += 10;
                }

                s_delay_shotgun =
                        std::clamp(
                                s_delay_shotgun,
                                allowed_range.min,
                                allowed_range.max);
        } break;

        case 22: {
                // Explosion delay
                const Range allowed_range(0, 900);

                if (direction == OptionToggleDirecton::enter) {
                        // Enter
                        query::QueryNumberConfig query_config;

                        query_config.allowed_range = allowed_range;
                        query_config.default_value = s_delay_explosion;
                        query_config.cancel_returns_default = true;

                        const int nr =
                                query::number(
                                        query_config,
                                        "Explosion delay");

                        if (nr != -1) {
                                s_delay_explosion = nr;
                        }
                }
                else if (direction == OptionToggleDirecton::left) {
                        // Left
                        s_delay_explosion -= 10;
                }
                else {
                        // Right
                        s_delay_explosion += 10;
                }

                s_delay_explosion =
                        std::clamp(
                                s_delay_explosion,
                                allowed_range.min,
                                allowed_range.max);
        } break;

        case 23: {
                // Reset to defaults
                if (direction == OptionToggleDirecton::enter) {
                        set_default_variables();
                        update_render_dims();
                        io::init_other();
                        audio::init();
                        states::draw();
                        io::update_screen();
                }
        } break;

        default:
        {
                ASSERT(false);
        } break;
        }
}

static void read_file(std::vector<std::string>& lines)
{
        std::ifstream file;
        file.open(paths::config_file_path());

        if (file.is_open()) {
                std::string line;

                while (getline(file, line)) {
                        lines.push_back(line);
                }

                file.close();
        }
}

static void set_variables_from_lines(std::vector<std::string>& lines)
{
        TRACE_FUNC_BEGIN;

        s_master_volume_pct_option = to_int(lines.front());
        lines.erase(std::begin(lines));

        s_master_volume_pct_adjusted = to_int(lines.front());
        lines.erase(std::begin(lines));

        s_is_ambient_audio_enabled = lines.front() == "1";
        lines.erase(std::begin(lines));

        s_is_ambient_audio_preloaded = lines.front() == "1";
        lines.erase(std::begin(lines));

        s_input_mode = (InputMode)to_int(lines.front());
        lines.erase(std::begin(lines));

        s_window_px_w = to_int(lines.front());
        lines.erase(std::begin(lines));

        s_window_px_h = to_int(lines.front());
        lines.erase(std::begin(lines));

        s_always_center_view_on_player = lines.front() == "1";
        lines.erase(std::begin(lines));

        s_is_tiles_mode = lines.front() == "1";
        lines.erase(std::begin(lines));

        s_font_name = lines.front();
        lines.erase(std::begin(lines));

        update_render_dims();

        s_is_fullscreen = lines.front() == "1";
        lines.erase(std::begin(lines));

        s_is_2x_scale_requested = lines.front() == "1";
        lines.erase(std::begin(lines));

        s_is_2x_scale_enabled = lines.front() == "1";
        lines.erase(std::begin(lines));

        s_text_mode_filled_walls = lines.front() == "1";
        lines.erase(std::begin(lines));

        s_is_intro_lvl_skipped = lines.front() == "1";
        lines.erase(std::begin(lines));

        s_is_intro_popup_skipped = lines.front() == "1";
        lines.erase(std::begin(lines));

        s_is_any_key_confirm_more = lines.front() == "1";
        lines.erase(std::begin(lines));

        s_hints_mode = (HintsMode)to_int(lines.front());
        lines.erase(std::begin(lines));

        s_always_warn_new_mon = lines.front() == "1";
        lines.erase(std::begin(lines));

        s_warn_on_throw_valuable = lines.front() == "1";
        lines.erase(std::begin(lines));

        s_warn_on_light_explosive = lines.front() == "1";
        lines.erase(std::begin(lines));

        s_warn_on_drink_malign_potion = lines.front() == "1";
        lines.erase(std::begin(lines));

        s_warn_on_ranged_wpn_melee = lines.front() == "1";
        lines.erase(std::begin(lines));

        s_is_ranged_wpn_auto_reload = lines.front() == "1";
        lines.erase(std::begin(lines));

        s_delay_projectile_draw = to_int(lines.front());
        lines.erase(std::begin(lines));

        s_delay_shotgun = to_int(lines.front());
        lines.erase(std::begin(lines));

        s_delay_explosion = to_int(lines.front());
        lines.erase(std::begin(lines));

        s_default_player_name = "";

        const bool has_default_name = lines.front() == "1";

        lines.erase(std::begin(lines));

        if (has_default_name) {
                s_default_player_name = lines.front();

                lines.erase(std::begin(lines));
        }

        for (size_t i = 0; i < (size_t)hints::Id::END; ++i) {
                s_has_seen_hint_global[i] = lines.front() == "1";

                lines.erase(std::begin(lines));
        }

        ASSERT(lines.empty());

        TRACE_FUNC_END;
}

static void write_lines_to_file(const std::vector<std::string>& lines)
{
        std::ofstream file;
        file.open(paths::config_file_path(), std::ios::trunc);

        for (size_t i = 0; i < lines.size(); ++i) {
                file << lines[i];

                if (i != (lines.size() - 1)) {
                        file << std::endl;
                }
        }

        file.close();
}

static std::vector<std::string> lines_from_variables()
{
        TRACE_FUNC_BEGIN;

        std::vector<std::string> lines;

        lines.emplace_back(std::to_string(s_master_volume_pct_option));
        lines.emplace_back(std::to_string(s_master_volume_pct_adjusted));
        lines.emplace_back(s_is_ambient_audio_enabled ? "1" : "0");
        lines.emplace_back(s_is_ambient_audio_preloaded ? "1" : "0");
        lines.push_back(std::to_string((int)s_input_mode));
        lines.push_back(std::to_string(s_window_px_w));
        lines.push_back(std::to_string(s_window_px_h));
        lines.emplace_back(s_always_center_view_on_player ? "1" : "0");
        lines.emplace_back(s_is_tiles_mode ? "1" : "0");
        lines.push_back(s_font_name);
        lines.emplace_back(s_is_fullscreen ? "1" : "0");
        lines.emplace_back(s_is_2x_scale_requested ? "1" : "0");
        lines.emplace_back(s_is_2x_scale_enabled ? "1" : "0");
        lines.emplace_back(s_text_mode_filled_walls ? "1" : "0");
        lines.emplace_back(s_is_intro_lvl_skipped ? "1" : "0");
        lines.emplace_back(s_is_intro_popup_skipped ? "1" : "0");
        lines.emplace_back(s_is_any_key_confirm_more ? "1" : "0");
        lines.emplace_back(std::to_string((int)s_hints_mode));
        lines.emplace_back(s_always_warn_new_mon ? "1" : "0");
        lines.emplace_back(s_warn_on_throw_valuable ? "1" : "0");
        lines.emplace_back(s_warn_on_light_explosive ? "1" : "0");
        lines.emplace_back(s_warn_on_drink_malign_potion ? "1" : "0");
        lines.emplace_back(s_warn_on_ranged_wpn_melee ? "1" : "0");
        lines.emplace_back(s_is_ranged_wpn_auto_reload ? "1" : "0");
        lines.push_back(std::to_string(s_delay_projectile_draw));
        lines.push_back(std::to_string(s_delay_shotgun));
        lines.push_back(std::to_string(s_delay_explosion));

        if (s_default_player_name.empty()) {
                lines.emplace_back("0");
        }
        else {
                // Default player name has been set
                lines.emplace_back("1");

                lines.push_back(s_default_player_name);
        }

        for (size_t i = 0; i < (size_t)hints::Id::END; ++i) {
                lines.emplace_back(s_has_seen_hint_global[i] ? "1" : "0");
        }

        TRACE_FUNC_END;

        return lines;
}

// -----------------------------------------------------------------------------
// Config
// -----------------------------------------------------------------------------
namespace config
{
void init()
{
        s_font_name = "";
        s_is_bot_playing = false;
        s_is_stress_test = false;
        s_is_gj_mode = false;

        set_default_variables();

        std::vector<std::string> lines;

        // Load config file, if it exists
        read_file(lines);

        if (lines.empty()) {
                // No previous config file exists, create one.
                lines = lines_from_variables();

                write_lines_to_file(lines);
        }
        else {
                // A config file exists, set values from parsed config lines
                set_variables_from_lines(lines);
        }

        update_render_dims();
}

InputMode input_mode()
{
        return s_input_mode;
}

bool always_center_view_on_player()
{
        return s_always_center_view_on_player;
}

bool is_tiles_mode()
{
        return s_is_tiles_mode;
}

std::string font_name()
{
        return s_font_name;
}

bool is_fullscreen()
{
        return s_is_fullscreen;
}

bool is_2x_scale_requested()
{
        return s_is_2x_scale_requested;
}

bool is_2x_scale_enabled()
{
        return s_is_2x_scale_enabled;
}

void set_window_px_w(const int w)
{
        s_window_px_w = w;

        const auto lines = lines_from_variables();

        write_lines_to_file(lines);
}

void set_window_px_h(const int h)
{
        s_window_px_h = h;

        const auto lines = lines_from_variables();

        write_lines_to_file(lines);
}

int window_px_w()
{
        return s_window_px_w;
}

int window_px_h()
{
        return s_window_px_h;
}

int gui_cell_px_w()
{
        return s_gui_cell_px_w;
}

int gui_cell_px_h()
{
        return s_gui_cell_px_h;
}

int map_cell_px_w()
{
        return s_map_cell_px_w;
}

int map_cell_px_h()
{
        return s_map_cell_px_h;
}

bool text_mode_filled_walls()
{
        return s_text_mode_filled_walls;
}

int master_volume_pct()
{
        return s_master_volume_pct_adjusted;
}

bool is_ambient_audio_enabled()
{
        return s_is_ambient_audio_enabled;
}

bool is_ambient_audio_preloaded()
{
        return s_is_ambient_audio_preloaded;
}

bool is_bot_playing()
{
        return s_is_bot_playing;
}

void enable_bot_playing()
{
        s_is_bot_playing = true;
}

void toggle_bot_playing()
{
        s_is_bot_playing = !s_is_bot_playing;
}

bool is_stress_test()
{
        return s_is_stress_test;
}

void enable_stress_test()
{
        s_is_stress_test = true;
}

bool is_gj_mode()
{
        return s_is_gj_mode;
}

void toggle_gj_mode()
{
        s_is_gj_mode = !s_is_gj_mode;
}

bool warn_on_throw_valuable()
{
        return s_warn_on_throw_valuable;
}

bool warn_on_light_explosive()
{
        return s_warn_on_light_explosive;
}

bool warn_on_drink_malign_potion()
{
        return s_warn_on_drink_malign_potion;
}

bool warn_on_ranged_wpn_melee()
{
        return s_warn_on_ranged_wpn_melee;
}

bool is_ranged_wpn_auto_reload()
{
        return s_is_ranged_wpn_auto_reload;
}

bool is_intro_lvl_skipped()
{
        return s_is_intro_lvl_skipped;
}

bool is_intro_popup_skipped()
{
        return s_is_intro_popup_skipped;
}

bool is_any_key_confirm_more()
{
        return s_is_any_key_confirm_more;
}

HintsMode hints_mode()
{
        return s_hints_mode;
}

bool has_seen_hint_global(const hints::Id id)
{
        if (id == hints::Id::END) {
                ASSERT(false);

                return false;
        }

        return s_has_seen_hint_global[(size_t)id];
}

void set_hint_seen_global(const hints::Id id)
{
        if (id == hints::Id::END) {
                ASSERT(false);
        }

        s_has_seen_hint_global[(size_t)id] = true;

        const auto lines = lines_from_variables();
        write_lines_to_file(lines);
}

bool always_warn_new_mon()
{
        return s_always_warn_new_mon;
}

int delay_projectile_draw()
{
        return s_delay_projectile_draw;
}
int delay_shotgun()
{
        return s_delay_shotgun;
}

int delay_explosion()
{
        return s_delay_explosion;
}

void set_default_player_name(const std::string& name)
{
        s_default_player_name = name;

        const auto lines = lines_from_variables();

        write_lines_to_file(lines);
}

std::string default_player_name()
{
        return s_default_player_name;
}

void set_fullscreen(const bool value)
{
        s_is_fullscreen = value;

        const auto lines = lines_from_variables();

        write_lines_to_file(lines);
}

void set_2x_scale_enabled(const bool value)
{
        s_is_2x_scale_enabled = value;

        const auto lines = lines_from_variables();

        write_lines_to_file(lines);
}

}  // namespace config

// -----------------------------------------------------------------------------
// Config state
// -----------------------------------------------------------------------------
ConfigState::ConfigState() :

        m_browser(24)
{
        m_browser.enable_left_right_keys();
}

StateId ConfigState::id() const
{
        return StateId::config;
}

void ConfigState::update()
{
        // Do not play selection audio if we are at the audio settings - these
        // will play a custom audio when they are toggled.
        //
        // TODO: This is *very* hacky, but there's no easy way to improve this
        // without refactoring the whole option handling (which should be done).
        const bool enable_selection_audio = (m_browser.y() >= 2);

        m_browser.set_selection_audio_enabled(enable_selection_audio);

        const auto input = io::read_input();

        const MenuAction action =
                m_browser.read(
                        input,
                        MenuInputMode::scrolling);

        bool did_set_option = false;

        switch (action) {
        case MenuAction::esc:
        case MenuAction::space: {
                // Since text mode wall symbol may have changed, we need to
                // redefine the terrain data list
                terrain::init();

                states::pop();

                return;
        } break;

        case MenuAction::selected: {
                player_sets_option(m_browser, OptionToggleDirecton::enter);
                did_set_option = true;
        } break;

        case MenuAction::left: {
                player_sets_option(m_browser, OptionToggleDirecton::left);
                did_set_option = true;
        } break;

        case MenuAction::right: {
                player_sets_option(m_browser, OptionToggleDirecton::right);
                did_set_option = true;
        } break;

        default:
                break;
        }

        if (did_set_option) {
                const auto lines = lines_from_variables();

                write_lines_to_file(lines);

                io::flush_input();
        }
}

void ConfigState::draw()
{
        draw_box(panels::area(Panel::screen));

        io::draw_text_center(
                " Options ",
                Panel::screen,
                {panels::center_x(Panel::screen), 0},
                colors::title(),
                io::DrawBg::yes,
                colors::black(),
                true);  // Allow pixel-level adjustment

        io::draw_text_center(
                std::string(
                        " " +
                        common_text::g_set_option_hint +
                        " " +
                        common_text::g_screen_exit_hint +
                        " "),
                Panel::screen,
                {panels::center_x(Panel::screen), panels::y1(Panel::screen)},
                colors::title(),
                io::DrawBg::yes,
                colors::black(),
                true);  // Allow pixel-level adjustment

        std::string font_disp_name = s_font_name;

        std::string input_mode_value_str;

        switch (s_input_mode) {
        case InputMode::standard:
                input_mode_value_str = "Default (numpad or arrows)";
                break;

        case InputMode::vi_keys:
                input_mode_value_str = "Vi-keys";
                break;

        case InputMode::END:
                PANIC;
                break;
        }

        std::string master_volume_str(11, '-');

        master_volume_str[s_master_volume_pct_option / 10] = '|';

        master_volume_str +=
                " " +
                std::to_string(s_master_volume_pct_option) + "%";

        std::string hints_mode_str;

        if (s_hints_mode == HintsMode::once_per_game) {
                hints_mode_str = "Once per game";
        }
        else if (s_hints_mode == HintsMode::once) {
                hints_mode_str = "Once";
        }
        else {
                hints_mode_str = "Never";
        }

        const std::vector<std::pair<std::string, std::string>> labels = {
                {"Audio volume level",
                 master_volume_str},

                {"Play ambient sounds",
                 s_is_ambient_audio_enabled
                         ? "Yes"
                         : "No"},

                {"Preload ambient sounds at game startup",
                 s_is_ambient_audio_preloaded
                         ? "Yes"
                         : "No"},

                {"Input mode",
                 input_mode_value_str},

                {"Always center view on player",
                 s_always_center_view_on_player
                         ? "Yes"
                         : "No"},

                {"Use tile set",
                 s_is_tiles_mode
                         ? "Yes"
                         : "No"},

                {"Font", font_disp_name},

                {"Fullscreen",
                 s_is_fullscreen
                         ? "Yes"
                         : "No"},

                {"Scale graphics 2x",
                 s_is_2x_scale_requested
                         ? "Yes"
                         : "No"},

                {"Text mode wall symbol",
                 s_text_mode_filled_walls
                         ? "Filled rectangle"
                         : "Hash sign"},

                {"Skip intro level",
                 s_is_intro_lvl_skipped
                         ? "Yes"
                         : "No"},

                {"Skip intro popup",
                 s_is_intro_popup_skipped
                         ? "Yes"
                         : "No"},

                {"Any key confirms \"[space]\" prompts",
                 s_is_any_key_confirm_more
                         ? "Yes"
                         : "No"},

                {"Display hints",
                 hints_mode_str},

                {"Always warn when new monster is seen",
                 s_always_warn_new_mon
                         ? "Yes"
                         : "No"},

                {"Warn when throwing \"valuable\" items",
                 s_warn_on_throw_valuable
                         ? "Yes"
                         : "No"},

                {"Warn when lighting explosives",
                 s_warn_on_light_explosive
                         ? "Yes"
                         : "No"},

                {"Warn when drinking malign potions",
                 s_warn_on_drink_malign_potion
                         ? "Yes"
                         : "No"},

                {"Ranged weapon melee attack warning",
                 s_warn_on_ranged_wpn_melee
                         ? "Yes"
                         : "No"},

                {"Ranged weapon auto reload",
                 s_is_ranged_wpn_auto_reload
                         ? "Yes"
                         : "No"},

                {"Projectile delay (ms)",
                 std::to_string(s_delay_projectile_draw)},

                {"Shotgun delay (ms)",
                 std::to_string(s_delay_shotgun)},

                {"Explosion delay (ms)",
                 std::to_string(s_delay_explosion)},

                {"Reset to defaults",
                 ""}};

        auto y = 0;

        for (auto i = 0; i < (int)labels.size(); ++i) {
                const auto label = labels[i];

                // Create some distance to "reset to defaults"
                if (i == ((int)labels.size() - 1)) {
                        ++y;
                }

                const auto& color =
                        (m_browser.is_at_idx((int)i))
                        ? colors::menu_highlight()
                        : colors::menu_dark();

                io::draw_text(
                        label.first,
                        Panel::info_screen_content,
                        {0, y},
                        color);

                if (!label.second.empty()) {
                        io::draw_text(
                                ":",
                                Panel::info_screen_content,
                                {s_opt_values_x_pos - 2, y},
                                color);

                        io::draw_text(
                                label.second,
                                Panel::info_screen_content,
                                {s_opt_values_x_pos, y},
                                color);
                }

                ++y;
        }
}
