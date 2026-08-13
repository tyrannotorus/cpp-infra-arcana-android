// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "config.hpp"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <regex>
#include <string>
#include <vector>

#include "audio.hpp"
#include "audio_data.hpp"
#include "browser.hpp"
#include "colors.hpp"
#include "common_text.hpp"
#include "config_options.hpp"
#include "debug.hpp"
#include "draw_box.hpp"
#include "hints.hpp"
#include "ini.h"
#include "io.hpp"
#include "misc.hpp"
#include "msg_log.hpp"
#include "panel.hpp"
#include "paths.hpp"
#include "popup.hpp"
#include "pos.hpp"
#include "query.hpp"
#include "random.hpp"
#include "rect.hpp"
#include "terrain_data.hpp"
#include "text.hpp"
#include "text_format.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
// Owning container of all available options (globally).
static std::vector<std::unique_ptr<config::Option>> s_options {};

// Options shown in the current submenu.
static std::vector<config::Option*> s_current_options {};

static config::OptionSubmenuType s_current_submenu = config::OptionSubmenuType::END;

static std::vector<std::string> s_font_image_names;

// The font a fresh install starts with (see set_default_variables)
static const std::string s_default_font_name = "13x24_typewriter.png";

static const int s_video_scale_factor_max = 4;
static const int s_map_scale_factor_max = 4;

static InputMode s_input_mode = InputMode::standard;
static bool s_is_double_click_toggle_fullscreen = true;
static std::string s_font_name;
static bool s_always_center_view_on_player = false;
static bool s_is_tiles_mode = false;
static RendererType s_renderer_type = RendererType::auto_select;
static bool s_is_fullscreen = false;
static int s_video_scale_factor = 1;
static bool s_side_panel_left = false;
static std::string s_action_bar_order;
static std::string s_action_bar_disabled;
static bool s_is_dpad_movement = false;
static int s_dpad_offset_px_x = 0;
static int s_dpad_offset_px_y = 0;
static int s_dpad_scale_pct = 100;

// Scale of the in-world (map) display, relative to the gui. Applied in tiles
// mode only (scaling text mode would require stretched glyph rendering). On
// touch devices the map is zoomed in for playability; a later iteration will
// make this adjustable by pinch zooming.
static int s_map_scale_factor = 2;
static int s_brightness_pct = 100;
static bool s_text_mode_filled_walls = true;
static bool s_display_health_bars = true;
static bool s_use_trap_color_when_obscured = true;
static bool s_warn_on_throw_valuable = false;
static bool s_warn_on_light_explosive = false;
static bool s_warn_on_drink_malign_potion = false;
static bool s_warn_on_ranged_wpn_melee = false;
static bool s_is_medical_bag_auto_choice = false;
static bool s_is_ranged_wpn_auto_reload = false;
static bool s_is_intro_lvl_skipped = false;
static bool s_is_intro_popup_skipped = false;
static bool s_is_any_key_confirm_more = false;
static bool s_auto_select_menu = false;
static HintsMode s_hints_mode = HintsMode::once;
static bool s_has_seen_hint_global[(size_t)hints::Id::END];
static bool s_always_warn_new_mon = false;
static int s_delay_projectile_draw = -1;
static int s_delay_explosion = -1;
static bool s_is_bot_playing = false;
static bool s_is_stress_test = false;
static bool s_is_gj_mode = false;
static int s_master_volume_pct_option = 100;
static int s_master_volume_pct_adjusted = 100;
static bool s_is_ambient_audio_enabled = false;
static bool s_is_ambient_audio_preloaded = false;
static int s_audio_buffer_size = 512;
static int s_window_px_w = -1;
static int s_window_px_h = -1;
static int s_gui_cell_px_w = -1;
static int s_gui_cell_px_h = -1;
static int s_map_cell_px_w = -1;
static int s_map_cell_px_h = -1;

static std::string submenu_name(config::OptionSubmenuType submenu_type)
{
        switch (submenu_type) {
        case config::OptionSubmenuType::video:
                return "Video";

        case config::OptionSubmenuType::audio:
                return "Audio";

        case config::OptionSubmenuType::input:
                return "Input";

        case config::OptionSubmenuType::gameplay:
                return "Gameplay";

        case config::OptionSubmenuType::END:
                break;
        }

        PANIC;

        return "";
}

static std::string submenu_name_with_key_shortcut(
        config::OptionSubmenuType submenu_type)
{
        switch (submenu_type) {
        case config::OptionSubmenuType::video:
                return "(V)ideo";

        case config::OptionSubmenuType::audio:
                return "(A)udio";

        case config::OptionSubmenuType::input:
                return "(I)nput";

        case config::OptionSubmenuType::gameplay:
                return "(G)ameplay";

        case config::OptionSubmenuType::END:
                break;
        }

        PANIC;

        return "";
}

static void filter_current_options(config::OptionSubmenuType submenu_type)
{
        s_current_options.clear();

        for (const auto& option : s_options) {
                if (option->submenu_type() == submenu_type) {
                        s_current_options.push_back(option.get());
                }
        }
}

static P parse_dims_from_font_name(const std::string& font_name)
{
        std::regex pattern(R"(^(\d+)x(\d+))");

        std::smatch match;

        const bool is_name_ok = std::regex_search(font_name, match, pattern);

        if (!is_name_ok) {
                TRACE << "Invalid font name: '" << font_name << "'" << std::endl;

                return {-1, -1};
        }

        int w = std::stoi(match[1].str());
        int h = std::stoi(match[2].str());

        return {w, h};
}

static void find_fonts_from_filesystem()
{
        TRACE_FUNC_BEGIN;

        TRACE << "Finding font images at '" << paths::fonts_dir() << "'" << std::endl;

        for (const auto& entry : std::filesystem::directory_iterator(paths::fonts_dir())) {
                if (!entry.is_regular_file()) {
                        continue;
                }

                // TODO: Should more formats be supported?
                if (entry.path().extension() != ".png") {
                        continue;
                }

                const std::string filename = entry.path().filename().string();

                const P dims = parse_dims_from_font_name(filename);

                if (dims.x == -1) {
                        continue;
                }

                s_font_image_names.push_back(filename);
        }

        std::sort(
                std::begin(s_font_image_names),
                std::end(s_font_image_names),
                [](const std::string& s1, const std::string& s2) {
                        const P dims_1 = parse_dims_from_font_name(s1);
                        const P dims_2 = parse_dims_from_font_name(s2);

                        // Primarily sort by width, secondarily be height.
                        if (dims_1.x == dims_2.x) {
                                // Same width, sort by height.
                                return dims_1.y < dims_2.y;
                        }
                        else {
                                // Different width, sort by width.
                                return dims_1.x < dims_2.x;
                        }
                });

#ifndef NDEBUG
        for (const std::string& font_name : s_font_image_names) {
                const P dims = parse_dims_from_font_name(font_name);

                TRACE
                        << "Found valid font '" << font_name << "'"
                        << " widh dimensions '" << dims.x << "x" << dims.y << "'"
                        << std::endl;
        }
#endif  // NDEBUG

        TRACE_FUNC_END;
}

static void update_render_dims()
{
        TRACE_FUNC_BEGIN;

        if (s_is_tiles_mode) {
                const auto font_dims = parse_dims_from_font_name(s_font_name);

                s_gui_cell_px_w = font_dims.x;
                s_gui_cell_px_h = font_dims.y;
                s_map_cell_px_w = config::g_tile_img_px * s_map_scale_factor;
                s_map_cell_px_h = config::g_tile_img_px * s_map_scale_factor;
        }
        else {
                const auto font_dims = parse_dims_from_font_name(s_font_name);

                s_gui_cell_px_w = s_map_cell_px_w = font_dims.x;
                s_gui_cell_px_h = s_map_cell_px_h = font_dims.y;
        }

        TRACE << "GUI cell size: " << s_gui_cell_px_w << "x" << s_gui_cell_px_h << std::endl;

        TRACE << "Tile size: " << s_map_cell_px_w << "x" << s_map_cell_px_h << std::endl;

        TRACE_FUNC_END;
}

static int calc_default_video_scale_factor(const P& native_res)
{
        // Set the video scale factor based on the user's native resolution.
        // Examples:
        // 1920 x 1080  : 2
        // 2560 x 1440  : 3
        // 3840 x 2160  : 4
        // 7680 x 4320  : 4

        int f = (native_res.x + 500) / 1000;

        // Mobile default is at least 2x (readability on handheld screens)
        f = std::clamp(f, 2, s_video_scale_factor_max);

        TRACE
                << "Calculated a default video scale factor of "
                << "'" << f << "', "
                << "based on native resolution of "
                << "'" << native_res.x << "x" << native_res.y << "' "
                << "(f = (x_resolution + 500) / 1000, "
                << "limited to " << s_video_scale_factor_max << ")"
                << std::endl;

        return f;
}

static void set_default_variables()
{
        TRACE_FUNC_BEGIN;

        s_input_mode = InputMode::standard;

        s_is_double_click_toggle_fullscreen = true;

        // The typewriter face at 13x24 - the interface is laid out to be
        // read at that cell size on a phone. NOTE: The font list is sorted
        // by cell size, so entry 0 is the smallest one available - the
        // fallback if the intended font is not installed.
        const auto default_font =
                std::find(
                        std::cbegin(s_font_image_names),
                        std::cend(s_font_image_names),
                        s_default_font_name);

        s_font_name =
                (default_font == std::cend(s_font_image_names))
                ? s_font_image_names[0]
                : *default_font;

        s_always_center_view_on_player = true;

        s_is_tiles_mode = true;

        update_render_dims();

        const P native_res = io::get_native_resolution();

        s_window_px_w = native_res.x;
        s_window_px_h = native_res.y;

        s_master_volume_pct_option = s_master_volume_pct_adjusted = 100;
        s_is_ambient_audio_enabled = true;
        s_is_ambient_audio_preloaded = false;
        s_audio_buffer_size = 512;
        s_renderer_type = RendererType::auto_select;
        s_is_fullscreen = true;

        // Side stats panel on the LEFT by default - which puts the action
        // bar's buttons and the [ x ] close control on the right, under the
        // right thumb (swiping toward the screen edge flips it, see
        // io_input). The map's centering follows the panel side, and
        // nothing else - see viewport's visible_view_center.
        s_side_panel_left = true;

        s_map_scale_factor = 2;

        s_action_bar_order = "";
        s_action_bar_disabled = "";

        // Swiping the map is what moves by default - the pad is offered to
        // players who would rather press keys than aim a gesture
        s_is_dpad_movement = false;
        s_dpad_offset_px_x = 0;
        s_dpad_offset_px_y = 0;
        s_dpad_scale_pct = 100;

        s_video_scale_factor = calc_default_video_scale_factor(native_res);
        s_brightness_pct = 100;
        s_text_mode_filled_walls = true;
        s_display_health_bars = true;
        s_use_trap_color_when_obscured = false;
        s_is_intro_lvl_skipped = false;
        s_is_intro_popup_skipped = false;
        s_is_any_key_confirm_more = false;
        s_auto_select_menu = false;
        s_hints_mode = HintsMode::once;
        s_always_warn_new_mon = true;
        s_warn_on_throw_valuable = true;
        s_warn_on_light_explosive = true;
        s_warn_on_drink_malign_potion = true;
        s_warn_on_ranged_wpn_melee = true;
        s_is_medical_bag_auto_choice = false;
        s_is_ranged_wpn_auto_reload = false;
        s_delay_projectile_draw = 25;
        s_delay_explosion = 250;

        for (size_t i = 0; i < (size_t)hints::Id::END; ++i) {
                s_has_seen_hint_global[i] = false;
        }

        TRACE_FUNC_END;
}

static bool read_config_file()
{
        TRACE_FUNC_BEGIN;

        mINI::INIFile file(paths::config_file_path() + ".ini");
        mINI::INIStructure ini;

        if (!file.read(ini)) {
                return false;
        }

        if (!ini.has("config")) {
                return false;
        }

        auto config = ini["config"];

        // The config file is written as a complete set of keys - if
        // fundamental keys are missing, the file is empty or truncated (e.g.
        // the process was killed in the middle of writing it, which can
        // happen on Android). Then ignore the whole file and use defaults,
        // instead of reading garbage values (e.g. a video scale factor of
        // zero, which would crash the window setup).
        if (!config.has("video_scale_factor") ||
            !config.has("delay_explosion")) {
                TRACE_ERROR_RELEASE
                        << "Config file is empty or truncated, "
                        << "falling back to default values"
                        << std::endl;

                return false;
        }

        s_master_volume_pct_option = to_int(config["master_volume_pct_option"]);
        s_master_volume_pct_adjusted = to_int(config["master_volume_pct_adjusted"]);
        s_is_ambient_audio_enabled = config["is_ambient_audio_enabled"] == "1";
        s_is_ambient_audio_preloaded = config["is_ambient_audio_preloaded"] == "1";
        s_audio_buffer_size = to_int(config["audio_buffer_size"]);
        s_input_mode = (InputMode)to_int(config["input_mode"]);
        s_is_double_click_toggle_fullscreen = config["is_double_click_toggle_fullscreen"] == "1";
        s_window_px_w = to_int(config["window_px_w"]);
        s_window_px_h = to_int(config["window_px_h"]);
        s_is_fullscreen = config["is_fullscreen"] == "1";
        s_side_panel_left = config["side_panel_left"] == "1";

        if (config.has("map_scale_factor")) {
                s_map_scale_factor =
                        std::clamp(
                                to_int(config["map_scale_factor"]),
                                1,
                                s_map_scale_factor_max);
        }

        s_action_bar_order = config["action_bar_order"];
        s_action_bar_disabled = config["action_bar_disabled"];

        // NOTE: Guarded - a config file written before the movement pad
        // existed has none of these keys, and reading them as zeroes would
        // scale the pad down to nothing
        if (config.has("dpad_movement")) {
                s_is_dpad_movement = config["dpad_movement"] == "1";
                s_dpad_offset_px_x = to_int(config["dpad_offset_px_x"]);
                s_dpad_offset_px_y = to_int(config["dpad_offset_px_y"]);
                s_dpad_scale_pct = to_int(config["dpad_scale_pct"]);
        }

        s_video_scale_factor =
                std::clamp(
                        to_int(config["video_scale_factor"]),
                        1,
                        s_video_scale_factor_max);

        std::string video_scale_str = config["video_scale_factor"];
        TRACE << "Read video_scale_factor: '" << video_scale_str << "'" << std::endl;

        s_brightness_pct = to_int(config["brightness_pct"]);
        s_renderer_type = (RendererType)to_int(config["renderer_type"]);

        s_always_center_view_on_player = config["always_center_view_on_player"] == "1";
        s_is_tiles_mode = config["is_tiles_mode"] == "1";
        s_font_name = config["font_name"];

        if (
                std::find(
                        std::cbegin(s_font_image_names),
                        std::cend(s_font_image_names),
                        s_font_name) == std::cend(s_font_image_names)) {
                TRACE
                        << "Font name in config file not found in available fonts "
                           "(removed from disk?), reverting to smallest font"
                        << std::endl;

                s_font_name = s_font_image_names[0];
        }

        update_render_dims();

        s_text_mode_filled_walls = config["text_mode_filled_walls"] == "1";
        s_display_health_bars = config["display_health_bars"] == "1";
        s_use_trap_color_when_obscured = config["use_trap_color_when_obscured"] == "1";
        s_is_intro_lvl_skipped = config["is_intro_lvl_skipped"] == "1";
        s_is_intro_popup_skipped = config["is_intro_popup_skipped"] == "1";
        s_is_any_key_confirm_more = config["is_any_key_confirm_more"] == "1";
        s_auto_select_menu = config["auto_select_menu"] == "1";
        s_hints_mode = (HintsMode)to_int(config["hints_mode"]);
        s_always_warn_new_mon = config["always_warn_new_mon"] == "1";
        s_warn_on_throw_valuable = config["warn_on_throw_valuable"] == "1";
        s_warn_on_light_explosive = config["warn_on_light_explosive"] == "1";
        s_warn_on_drink_malign_potion = config["warn_on_drink_malign_potion"] == "1";
        s_warn_on_ranged_wpn_melee = config["warn_on_ranged_wpn_melee"] == "1";
        s_is_medical_bag_auto_choice = config["is_medical_bag_auto_choice"] == "1";
        s_is_ranged_wpn_auto_reload = config["is_ranged_wpn_auto_reload"] == "1";
        s_delay_projectile_draw = to_int(config["delay_projectile_draw"]);
        s_delay_explosion = to_int(config["delay_explosion"]);

        for (size_t i = 0; i < (size_t)hints::Id::END; ++i) {
                s_has_seen_hint_global[i] =
                        config["has_seen_hint_" + std::to_string(i)] == "1";
        }

        TRACE_FUNC_END;

        return true;
}

static void write_config_file()
{
        TRACE_FUNC_BEGIN;

        const std::string file_path = paths::config_file_path() + ".ini";
        const std::string tmp_file_path = file_path + ".tmp";

        mINI::INIFile ini_file(tmp_file_path);
        mINI::INIStructure ini;

        auto& config = ini["config"];

        config["master_volume_pct_option"] = std::to_string(s_master_volume_pct_option);
        config["master_volume_pct_adjusted"] = std::to_string(s_master_volume_pct_adjusted);
        config["is_ambient_audio_enabled"] = s_is_ambient_audio_enabled ? "1" : "0";
        config["is_ambient_audio_preloaded"] = s_is_ambient_audio_preloaded ? "1" : "0";
        config["audio_buffer_size"] = std::to_string(s_audio_buffer_size);
        config["input_mode"] = std::to_string((int)s_input_mode);
        config["s_is_double_click_toggle_fullscreen"] =
                s_is_double_click_toggle_fullscreen ? "1" : "0";
        config["window_px_w"] = std::to_string(s_window_px_w);
        config["window_px_h"] = std::to_string(s_window_px_h);
        config["is_fullscreen"] = s_is_fullscreen ? "1" : "0";
        config["side_panel_left"] = s_side_panel_left ? "1" : "0";
        config["map_scale_factor"] = std::to_string(s_map_scale_factor);
        config["action_bar_order"] = s_action_bar_order;
        config["action_bar_disabled"] = s_action_bar_disabled;
        config["dpad_movement"] = s_is_dpad_movement ? "1" : "0";
        config["dpad_offset_px_x"] = std::to_string(s_dpad_offset_px_x);
        config["dpad_offset_px_y"] = std::to_string(s_dpad_offset_px_y);
        config["dpad_scale_pct"] = std::to_string(s_dpad_scale_pct);
        config["video_scale_factor"] = std::to_string(s_video_scale_factor);
        config["brightness_pct"] = std::to_string(s_brightness_pct);
        config["renderer_type"] = std::to_string((int)s_renderer_type);
        config["always_center_view_on_player"] = s_always_center_view_on_player ? "1" : "0";
        config["is_tiles_mode"] = s_is_tiles_mode ? "1" : "0";
        config["font_name"] = s_font_name;
        config["text_mode_filled_walls"] = s_text_mode_filled_walls ? "1" : "0";
        config["display_health_bars"] = s_display_health_bars ? "1" : "0";
        config["use_trap_color_when_obscured"] = s_use_trap_color_when_obscured ? "1" : "0";
        config["is_intro_lvl_skipped"] = s_is_intro_lvl_skipped ? "1" : "0";
        config["is_intro_popup_skipped"] = s_is_intro_popup_skipped ? "1" : "0";
        config["is_any_key_confirm_more"] = s_is_any_key_confirm_more ? "1" : "0";
        config["auto_select_menu"] = s_auto_select_menu ? "1" : "0";
        config["hints_mode"] = std::to_string((int)s_hints_mode);
        config["always_warn_new_mon"] = s_always_warn_new_mon ? "1" : "0";
        config["warn_on_throw_valuable"] = s_warn_on_throw_valuable ? "1" : "0";
        config["warn_on_light_explosive"] = s_warn_on_light_explosive ? "1" : "0";
        config["warn_on_drink_malign_potion"] = s_warn_on_drink_malign_potion ? "1" : "0";
        config["warn_on_ranged_wpn_melee"] = s_warn_on_ranged_wpn_melee ? "1" : "0";
        config["is_medical_bag_auto_choice"] = s_is_medical_bag_auto_choice ? "1" : "0";
        config["is_ranged_wpn_auto_reload"] = s_is_ranged_wpn_auto_reload ? "1" : "0";
        config["delay_projectile_draw"] = std::to_string(s_delay_projectile_draw);
        config["delay_explosion"] = std::to_string(s_delay_explosion);

        for (size_t i = 0; i < (size_t)hints::Id::END; ++i) {
                config["has_seen_hint_" + std::to_string(i)] =
                        s_has_seen_hint_global[i] ? "1" : "0";
        }

        ini_file.generate(ini);

        // Atomically replace the previous config file - the process may be
        // killed at any time (especially on Android), and a truncated config
        // file must never be left behind
        std::error_code error;

        std::filesystem::rename(tmp_file_path, file_path, error);

        if (error) {
                TRACE_ERROR_RELEASE
                        << "Failed to replace config file: "
                        << error.message()
                        << std::endl;
        }

        TRACE_FUNC_END;
}

static std::vector<std::string> make_user_data_info_lines()
{
        std::string str = "User data location: " + paths::user_dir();

        return text_format::split(str, panels::w(Panel::screen) - 1);
}

static void draw_user_data_info()
{
        std::vector<std::string> lines = make_user_data_info_lines();

        int y = panels::y1(Panel::screen) - (int)lines.size();

        for (size_t i = 0; i < lines.size(); ++i) {
                const std::string& line = lines[i];

                io::draw_text(line, Panel::screen, {1, y}, colors::text());

                ++y;
        }
}

// -----------------------------------------------------------------------------
// Config
// -----------------------------------------------------------------------------
namespace config
{
void init()
{
        //
        // Register options here.
        //
        // NOTE: Options will appear in this order in the game!
        //

        // Video
        s_options.emplace_back(std::make_unique<TilesModeOption>());
        s_options.emplace_back(std::make_unique<FontOption>());
        s_options.emplace_back(std::make_unique<RendererTypeOption>());
        s_options.emplace_back(std::make_unique<FullscreenOption>());
        s_options.emplace_back(std::make_unique<VideoScaleOption>());
        s_options.emplace_back(std::make_unique<GameScaleOption>());
        s_options.emplace_back(std::make_unique<BrightnessOption>());
        s_options.emplace_back(std::make_unique<TextModeFilledWallsOption>());

        // Audio
        s_options.emplace_back(std::make_unique<MasterVolumeOption>());
        s_options.emplace_back(std::make_unique<AmbientAudioEnabledOption>());
        s_options.emplace_back(std::make_unique<PreloadAmbientAudioOption>());
        s_options.emplace_back(std::make_unique<AudioBufferSizeOption>());

        // Input
        s_options.emplace_back(std::make_unique<MovementModeOption>());
        s_options.emplace_back(std::make_unique<InputModeOption>());
        s_options.emplace_back(std::make_unique<DoubleClickTogglesFullscreenOption>());
        s_options.emplace_back(std::make_unique<AnyKeyConfirmMoreOption>());
        s_options.emplace_back(std::make_unique<AutoSelectMenuOption>());
        s_options.emplace_back(std::make_unique<MedicalBagAutoChoiceOption>());
        s_options.emplace_back(std::make_unique<AutoReloadOption>());

        // Gameplay
        s_options.emplace_back(std::make_unique<AlwaysCenterViewOption>());
        s_options.emplace_back(std::make_unique<SkipIntroLevelOption>());
        s_options.emplace_back(std::make_unique<SkipIntroPopupOption>());
        s_options.emplace_back(std::make_unique<DisplayHintsOption>());
        s_options.emplace_back(std::make_unique<UseTrapColorWhenObscuredOption>());
        s_options.emplace_back(std::make_unique<DisplayHealthBarsOption>());
        s_options.emplace_back(std::make_unique<AlwaysWarnMonsterOption>());
        s_options.emplace_back(std::make_unique<WarnThrowValuableOption>());
        s_options.emplace_back(std::make_unique<WarnLightExplosivesOption>());
        s_options.emplace_back(std::make_unique<WanDrinkMalignPotionOption>());
        s_options.emplace_back(std::make_unique<WarnRangedWeaponMeleeOption>());
        s_options.emplace_back(std::make_unique<ProjectileDelayOption>());
        s_options.emplace_back(std::make_unique<ExplosionDelayOption>());

        // Find available fonts
        find_fonts_from_filesystem();

        // Reset to defaults
        //
        // TODO: Consider how to handle this. Should there be such an option?
        //
        // s_options.emplace_back(std::make_unique<ResetDefaultsOption>());

        s_font_name = "";
        s_is_bot_playing = false;
        s_is_stress_test = false;
        s_is_gj_mode = false;

        set_default_variables();

        // Load config file, if it exists
        const bool did_read_config_file = read_config_file();

        if (!did_read_config_file) {
                // No previous config file exists, create one.
                write_config_file();
        }

        update_render_dims();
}

InputMode input_mode()
{
        return s_input_mode;
}

bool is_double_click_toggle_fullscreen()
{
        return s_is_double_click_toggle_fullscreen;
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

RendererType renderer_type()
{
        return s_renderer_type;
}

bool is_fullscreen()
{
        return s_is_fullscreen;
}

int video_scale_factor()
{
        return s_video_scale_factor;
}

int map_scale_factor()
{
        return s_map_scale_factor;
}

bool is_side_panel_left()
{
        return s_side_panel_left;
}

void set_side_panel_left(const bool value)
{
        s_side_panel_left = value;

        write_config_file();
}

std::string action_bar_order()
{
        return s_action_bar_order;
}

std::string action_bar_disabled()
{
        return s_action_bar_disabled;
}

void set_action_bar_layout(
        const std::string& order_csv,
        const std::string& disabled_csv)
{
        s_action_bar_order = order_csv;
        s_action_bar_disabled = disabled_csv;

        write_config_file();
}

bool is_dpad_movement()
{
        return s_is_dpad_movement;
}

void set_dpad_movement(const bool value)
{
        s_is_dpad_movement = value;

        write_config_file();
}

int dpad_offset_px_x()
{
        return s_dpad_offset_px_x;
}

int dpad_offset_px_y()
{
        return s_dpad_offset_px_y;
}

int dpad_scale_pct()
{
        return s_dpad_scale_pct;
}

void set_dpad_placement(
        const int offset_px_x,
        const int offset_px_y,
        const int scale_pct)
{
        // NOTE: The pad persists its placement whenever a drag ends, which
        // includes every drag that changed nothing - do not rewrite the
        // config file for those
        if ((offset_px_x == s_dpad_offset_px_x) &&
            (offset_px_y == s_dpad_offset_px_y) &&
            (scale_pct == s_dpad_scale_pct)) {
                return;
        }

        s_dpad_offset_px_x = offset_px_x;
        s_dpad_offset_px_y = offset_px_y;
        s_dpad_scale_pct = scale_pct;

        write_config_file();
}

int brightness_pct()
{
        return s_brightness_pct;
}

void set_window_px_w(const int w)
{
        s_window_px_w = w;

        write_config_file();
}

void set_window_px_h(const int h)
{
        s_window_px_h = h;

        write_config_file();
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

bool display_health_bars()
{
        return s_display_health_bars;
}

bool use_trap_color_when_obscured()
{
        return s_use_trap_color_when_obscured;
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

int audio_buffer_size()
{
        return s_audio_buffer_size;
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

bool is_medical_bag_auto_choice()
{
        return s_is_medical_bag_auto_choice;
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

bool is_auto_select_menu()
{
        return s_auto_select_menu;
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

        write_config_file();
}

bool always_warn_new_mon()
{
        return s_always_warn_new_mon;
}

int delay_projectile_draw()
{
        return s_delay_projectile_draw;
}

int delay_explosion()
{
        return s_delay_explosion;
}

void set_fullscreen(const bool value)
{
        s_is_fullscreen = value;

        write_config_file();
}

// -----------------------------------------------------------------------------
// User options
// -----------------------------------------------------------------------------
std::string MasterVolumeOption::name() const
{
        return "Audio volume level";
}

std::string MasterVolumeOption::descr() const
{
        return (
                "Master volume control (0-100%).");
}

std::string MasterVolumeOption::value_str() const
{
        std::string str(11, '-');

        str[s_master_volume_pct_option / 10] = '|';

        str += " " + std::to_string(s_master_volume_pct_option) + "%";

        return str;
}

OptionSubmenuType MasterVolumeOption::submenu_type() const
{
        return OptionSubmenuType::audio;
}

void MasterVolumeOption::change(const OptionChangeCommand command) const
{
        audio::stop_ambient();

        const int step = 10;

        if ((command == OptionChangeCommand::enter) ||
            (command == OptionChangeCommand::right)) {
                // Enter or right
                s_master_volume_pct_option += step;
        }
        else {
                // Left
                s_master_volume_pct_option -= step;
        }

        s_master_volume_pct_option = std::clamp(s_master_volume_pct_option, 0, 100);

        const auto f = ((double)s_master_volume_pct_option) / 100.0;

        s_master_volume_pct_adjusted = (int)((f * f) * 100.0);

        s_master_volume_pct_adjusted = std::clamp(s_master_volume_pct_adjusted, 0, 100);

        TRACE
                << "Volume option: "
                << s_master_volume_pct_option
                << "%, adjusted: "
                << s_master_volume_pct_adjusted
                << "%"
                << std::endl;

        audio::set_music_volume(s_master_volume_pct_adjusted);

        audio::play(audio::SfxId::menu_select);
}

std::string AmbientAudioEnabledOption::name() const
{
        return "Play ambient sounds";
}

std::string AmbientAudioEnabledOption::descr() const
{
        return (
                "If enabled, ambient sound effects such as howling wind or "
                "dripping water will occasionally be played.");
}

std::string AmbientAudioEnabledOption::value_str() const
{
        return s_is_ambient_audio_enabled ? "Yes" : "No";
}

OptionSubmenuType AmbientAudioEnabledOption::submenu_type() const
{
        return OptionSubmenuType::audio;
}

void AmbientAudioEnabledOption::change(const OptionChangeCommand command) const
{
        (void)command;

        s_is_ambient_audio_enabled = !s_is_ambient_audio_enabled;

        audio::stop_ambient();

        audio::play(audio::SfxId::menu_select);
}

std::string PreloadAmbientAudioOption::name() const
{
        return "Preload ambient sounds";
}

std::string PreloadAmbientAudioOption::descr() const
{
        return (
                "Load all ambient sound clips on game startup, otherwise load "
                "each sound individually when played for the first time "
                "(a small delay may occur when this happens).");
}

std::string PreloadAmbientAudioOption::value_str() const
{
        return s_is_ambient_audio_preloaded ? "Yes" : "No";
}

OptionSubmenuType PreloadAmbientAudioOption::submenu_type() const
{
        return OptionSubmenuType::audio;
}

void PreloadAmbientAudioOption::change(const OptionChangeCommand command) const
{
        (void)command;

        s_is_ambient_audio_preloaded = !s_is_ambient_audio_preloaded;
}

std::string AudioBufferSizeOption::name() const
{
        return "Audio buffer size";
}

std::string AudioBufferSizeOption::descr() const
{
        return (
                "Lower values decrease latency, but may cause "
                "sound crackling (audio buffer underruns) "
                "on devices with slow CPU.\n"
                "Increase it to get smoother sound, "
                "but with the cost of increasing sound delay.");
}

std::string AudioBufferSizeOption::value_str() const
{
        return std::to_string(s_audio_buffer_size);
}

OptionSubmenuType AudioBufferSizeOption::submenu_type() const
{
        return OptionSubmenuType::audio;
}

void AudioBufferSizeOption::change(const OptionChangeCommand command) const
{
        const Range allowed_range(512, 4096);

        if ((command == OptionChangeCommand::enter) ||
            (command == OptionChangeCommand::right)) {
                // Enter or right
                s_audio_buffer_size <<= 1;
        }
        else {
                // Left
                s_audio_buffer_size >>= 1;
        }

        s_audio_buffer_size =
                std::clamp(
                        s_audio_buffer_size,
                        allowed_range.min,
                        allowed_range.max);

        TRACE
                << "Audio buffer size: "
                << s_audio_buffer_size
                << std::endl;

        io::init_sdl_audio();
}

std::string MovementModeOption::name() const
{
        return "Movement";
}

std::string MovementModeOption::descr() const
{
        return (
                "Move by swiping the map in the direction to go, or with an "
                "on-screen movement pad. The pad is placed in the bottom "
                "corner opposite the stats panel - hold it to move or resize "
                "it, and tap elsewhere to put it down.");
}

std::string MovementModeOption::value_str() const
{
        return s_is_dpad_movement ? "D-pad" : "Swipe";
}

OptionSubmenuType MovementModeOption::submenu_type() const
{
        return OptionSubmenuType::input;
}

void MovementModeOption::change(const OptionChangeCommand command) const
{
        (void)command;

        s_is_dpad_movement = !s_is_dpad_movement;
}

std::string InputModeOption::name() const
{
        return "Input mode";
}

std::string InputModeOption::descr() const
{
        return (
                "Use default input mode (numerical keypad or arrow keys), or "
                "\"Vi-keys\". See the game manual for more information.");
}

std::string InputModeOption::value_str() const
{
        switch (s_input_mode) {
        case InputMode::standard:
                return "Default";

        case InputMode::vi_keys:
                return "Vi-keys";

        case InputMode::END:
                break;
        }

        return "";

        ASSERT(false);
}

OptionSubmenuType InputModeOption::submenu_type() const
{
        return OptionSubmenuType::input;
}

void InputModeOption::change(const OptionChangeCommand command) const
{
        auto input_mode_nr = (int)s_input_mode;

        const auto nr_input_modes = (int)InputMode::END;

        if ((command == OptionChangeCommand::enter) ||
            (command == OptionChangeCommand::right)) {
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
}

std::string DoubleClickTogglesFullscreenOption::name() const
{
        return "Double click fullscreen";
}

std::string DoubleClickTogglesFullscreenOption::descr() const
{
        return "Double click anywhere to toggle fullscreen?";
}

std::string DoubleClickTogglesFullscreenOption::value_str() const
{
        return s_is_double_click_toggle_fullscreen ? "Yes" : "No";
}

OptionSubmenuType DoubleClickTogglesFullscreenOption::submenu_type() const
{
        return OptionSubmenuType::input;
}

void DoubleClickTogglesFullscreenOption::change(const OptionChangeCommand command) const
{
        (void)command;

        s_is_double_click_toggle_fullscreen = !s_is_double_click_toggle_fullscreen;
}

std::string AlwaysCenterViewOption::name() const
{
        return "Always center view";
}

std::string AlwaysCenterViewOption::descr() const
{
        return (
                "Keep the view centered on the player, otherwise only center "
                "when the player is near the edge of the view.");
}

std::string AlwaysCenterViewOption::value_str() const
{
        return s_always_center_view_on_player ? "Yes" : "No";
}

OptionSubmenuType AlwaysCenterViewOption::submenu_type() const
{
        return OptionSubmenuType::gameplay;
}

void AlwaysCenterViewOption::change(OptionChangeCommand command) const
{
        (void)command;

        s_always_center_view_on_player = !s_always_center_view_on_player;
}

std::string TilesModeOption::name() const
{
        return "Use tile set";
}

std::string TilesModeOption::descr() const
{
        return (
                "Use tile graphics to represent the game environment, "
                "otherwise use text symbols only.");
}

std::string TilesModeOption::value_str() const
{
        return s_is_tiles_mode ? "Yes" : "No";
}

OptionSubmenuType TilesModeOption::submenu_type() const
{
        return OptionSubmenuType::video;
}

void TilesModeOption::change(OptionChangeCommand command) const
{
        (void)command;

        s_is_tiles_mode = !s_is_tiles_mode;

        update_render_dims();
        io::init_other();
}

std::string FontOption::name() const
{
        return "Font";
}

std::string FontOption::descr() const
{
        return (
                "Font used for text.");
}

std::string FontOption::value_str() const
{
        return s_font_name;
}

OptionSubmenuType FontOption::submenu_type() const
{
        return OptionSubmenuType::video;
}

void FontOption::change(OptionChangeCommand command) const
{
        // Find current font index
        size_t font_idx = 0;

        const size_t nr_fonts = std::size(s_font_image_names);

        for (; font_idx < nr_fonts; ++font_idx) {
                if (s_font_image_names[font_idx] == s_font_name) {
                        break;
                }
        }

        if ((command == OptionChangeCommand::enter) ||
            (command == OptionChangeCommand::right)) {
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

        s_font_name = s_font_image_names[font_idx];

        update_render_dims();
        io::init_other();
}

std::string FullscreenOption::name() const
{
        return "Fullscreen";
}

std::string FullscreenOption::descr() const
{
        return (
                "Run in fullscreen mode (borderless fullscreen window) or "
                "use resizable window. This can also be toggled by pressing "
                "Alt-Enter.");
}

std::string FullscreenOption::value_str() const
{
        return s_is_fullscreen ? "Yes" : "No";
}

OptionSubmenuType FullscreenOption::submenu_type() const
{
        return OptionSubmenuType::video;
}

void FullscreenOption::change(OptionChangeCommand command) const
{
        (void)command;

        config::set_fullscreen(!s_is_fullscreen);

        io::on_user_toggle_fullscreen();
}

std::string RendererTypeOption::name() const
{
        return "Use hardware acceleration";
}

std::string RendererTypeOption::descr() const
{
        return (
                "Use automatic selection (attempt to use hardware "
                "acceleration first, or use software fallback), or "
                "force using software renderer. "
                "If you experience any graphical issues, setting this option "
                "to \"Software\" may fix the problem.");
}

std::string RendererTypeOption::value_str() const
{
        switch (s_renderer_type) {
        case RendererType::auto_select:
                return "Auto select";

        case RendererType::sw:
                return "Software";

        case RendererType::END:
                break;
        }

        ASSERT(false);

        return "";
}

OptionSubmenuType RendererTypeOption::submenu_type() const
{
        return OptionSubmenuType::video;
}

void RendererTypeOption::change(OptionChangeCommand command) const
{
        auto renderer_type_nr = (int)s_renderer_type;
        const auto nr_renderer_types = (int)RendererType::END;

        if ((command == OptionChangeCommand::enter) ||
            (command == OptionChangeCommand::right)) {
                // Enter or right
                if (renderer_type_nr < (nr_renderer_types - 1)) {
                        ++renderer_type_nr;
                }
                else {
                        renderer_type_nr = 0;
                }
        }
        else {
                // Left
                if (renderer_type_nr > 0) {
                        --renderer_type_nr;
                }
                else {
                        renderer_type_nr = nr_renderer_types - 1;
                }
        }

        s_renderer_type = (RendererType)(renderer_type_nr);

        io::init_other();
}

std::string VideoScaleOption::name() const
{
        return "Menu scale";
}

std::string VideoScaleOption::descr() const
{
        return (
                "Scale of the menus and interface (the side panel, message "
                "log, and menu screens). The game world is additionally "
                "scaled by the \"Game scale\" option.");
}

std::string VideoScaleOption::value_str() const
{
        return std::to_string(s_video_scale_factor) + "x";
}

OptionSubmenuType VideoScaleOption::submenu_type() const
{
        return OptionSubmenuType::video;
}

void VideoScaleOption::change(OptionChangeCommand command) const
{
        const int scale_factor_before = s_video_scale_factor;

        switch (command) {
        case OptionChangeCommand::enter: {
                if (s_video_scale_factor < s_video_scale_factor_max) {
                        ++s_video_scale_factor;
                }
                else {
                        s_video_scale_factor = 1;
                }
        } break;

        case OptionChangeCommand::right: {
                if (s_video_scale_factor < s_video_scale_factor_max) {
                        ++s_video_scale_factor;
                }
        } break;

        case OptionChangeCommand::left: {
                if (s_video_scale_factor > 1) {
                        --s_video_scale_factor;
                }
        } break;
        }

        if (s_video_scale_factor != scale_factor_before) {
                io::on_user_toggle_scaling();
        }
}

std::string GameScaleOption::name() const
{
        return "Game scale";
}

std::string GameScaleOption::descr() const
{
        return (
                "Scale of the game world (the map), independent of the "
                "menu scale. Applied when using the tile set.");
}

std::string GameScaleOption::value_str() const
{
        return std::to_string(s_map_scale_factor) + "x";
}

OptionSubmenuType GameScaleOption::submenu_type() const
{
        return OptionSubmenuType::video;
}

void GameScaleOption::change(OptionChangeCommand command) const
{
        const int scale_factor_before = s_map_scale_factor;

        switch (command) {
        case OptionChangeCommand::enter: {
                if (s_map_scale_factor < s_map_scale_factor_max) {
                        ++s_map_scale_factor;
                }
                else {
                        s_map_scale_factor = 1;
                }
        } break;

        case OptionChangeCommand::right: {
                if (s_map_scale_factor < s_map_scale_factor_max) {
                        ++s_map_scale_factor;
                }
        } break;

        case OptionChangeCommand::left: {
                if (s_map_scale_factor > 1) {
                        --s_map_scale_factor;
                }
        } break;
        }

        if (s_map_scale_factor != scale_factor_before) {
                update_render_dims();

                io::init_other();

                states::draw();

                io::update_screen();
        }
}

std::string BrightnessOption::name() const
{
        return "Brightness";
}

std::string BrightnessOption::descr() const
{
        return ("Change brightness of colors.");
}

std::string BrightnessOption::value_str() const
{
        std::string str(11, '-');

        str[(s_brightness_pct - 20) / 20] = '|';

        str += " " + std::to_string(s_brightness_pct) + "%";

        return str;
}

OptionSubmenuType BrightnessOption::submenu_type() const
{
        return OptionSubmenuType::video;
}

void BrightnessOption::change(const OptionChangeCommand command) const
{
        const int step = 20;

        if ((command == OptionChangeCommand::enter) ||
            (command == OptionChangeCommand::right)) {
                // Enter or right
                s_brightness_pct += step;
        }
        else {
                // Left
                s_brightness_pct -= step;
        }

        s_brightness_pct = std::clamp(s_brightness_pct, 20, 220);

        colors::init();
}

std::string TextModeFilledWallsOption::name() const
{
        return "Text mode wall symbol";
}

std::string TextModeFilledWallsOption::descr() const
{
        return (
                "Symbol used for representing walls in text mode "
                "(when tiles are not used).");
}

std::string TextModeFilledWallsOption::value_str() const
{
        return s_text_mode_filled_walls ? "Filled rectangle" : "Hash sign";
}

OptionSubmenuType TextModeFilledWallsOption::submenu_type() const
{
        return OptionSubmenuType::video;
}

void TextModeFilledWallsOption::change(OptionChangeCommand command) const
{
        (void)command;

        s_text_mode_filled_walls = !s_text_mode_filled_walls;

        // Redefine the terrain data list.
        terrain::init();
}

std::string DisplayHealthBarsOption::name() const
{
        return "Display health bars";
}

std::string DisplayHealthBarsOption::descr() const
{
        return "Show health bars under creatures when not at full health.";
}

std::string DisplayHealthBarsOption::value_str() const
{
        return s_display_health_bars ? "Yes" : "No";
}

OptionSubmenuType DisplayHealthBarsOption::submenu_type() const
{
        return OptionSubmenuType::gameplay;
}

void DisplayHealthBarsOption::change(OptionChangeCommand command) const
{
        (void)command;

        s_display_health_bars = !s_display_health_bars;
}

std::string UseTrapColorWhenObscuredOption::name() const
{
        return "Use trap color when obscured";
}

std::string UseTrapColorWhenObscuredOption::descr() const
{
        return (
                "If a trap is obscured by another object (e.g. an item), use "
                "the color of the specific trap type as background color to "
                "signify that there is a trap underneath the objct, "
                "otherwise use the same background color regardless of "
                "trap type.");
}

std::string UseTrapColorWhenObscuredOption::value_str() const
{
        return s_use_trap_color_when_obscured ? "Yes" : "No";
}

OptionSubmenuType UseTrapColorWhenObscuredOption::submenu_type() const
{
        return OptionSubmenuType::gameplay;
}

void UseTrapColorWhenObscuredOption::change(
        OptionChangeCommand command) const
{
        (void)command;

        s_use_trap_color_when_obscured = !s_use_trap_color_when_obscured;
}

std::string SkipIntroLevelOption::name() const
{
        return "Skip intro level";
}

std::string SkipIntroLevelOption::descr() const
{
        return (
                "Skip the introduction level. This level merely serves to "
                "provide a backstory and to set up the atmosphere, there are "
                "no gameplay benefits such as items or experience points that "
                "can be gained from playing through it.");
}

std::string SkipIntroLevelOption::value_str() const
{
        return s_is_intro_lvl_skipped ? "Yes" : "No";
}

OptionSubmenuType SkipIntroLevelOption::submenu_type() const
{
        return OptionSubmenuType::gameplay;
}

void SkipIntroLevelOption::change(OptionChangeCommand command) const
{
        (void)command;

        s_is_intro_lvl_skipped = !s_is_intro_lvl_skipped;
}

std::string SkipIntroPopupOption::name() const
{
        return "Skip intro popup";
}

std::string SkipIntroPopupOption::descr() const
{
        return (
                "Skip the story popup on the introduction level.");
}

std::string SkipIntroPopupOption::value_str() const
{
        return s_is_intro_popup_skipped ? "Yes" : "No";
}

OptionSubmenuType SkipIntroPopupOption::submenu_type() const
{
        return OptionSubmenuType::gameplay;
}

void SkipIntroPopupOption::change(OptionChangeCommand command) const
{
        (void)command;

        s_is_intro_popup_skipped = !s_is_intro_popup_skipped;
}

std::string AnyKeyConfirmMoreOption::name() const
{
        return "Any key to proceed";
}

std::string AnyKeyConfirmMoreOption::descr() const
{
        return (
                "Any key confirms \"" +
                msg_log::g_more_str +
                "\" prompts in the message log (which can happen for example "
                "when a monster appears as a warning to the player), "
                "otherwise only space (and a few other keys) confirms these "
                "prompts. Keeping the option disabled is safer.");
}

std::string AnyKeyConfirmMoreOption::value_str() const
{
        return s_is_any_key_confirm_more ? "Yes" : "No";
}

OptionSubmenuType AnyKeyConfirmMoreOption::submenu_type() const
{
        return OptionSubmenuType::input;
}

void AnyKeyConfirmMoreOption::change(OptionChangeCommand command) const
{
        (void)command;

        s_is_any_key_confirm_more = !s_is_any_key_confirm_more;
}

std::string AutoSelectMenuOption::name() const
{
        return "Auto select menu entries";
}

std::string AutoSelectMenuOption::descr() const
{
        return (
                "If disabled, pressing a letter key while in a menu only jumps "
                "to that position, and a second press (or enter) is required "
                "to select it (this is only applicable for menus where the options "
                "have descriptions, such as the trait selection menu). "
                "If enabled, pressing a letter immediately selects that entry.");
}

std::string AutoSelectMenuOption::value_str() const
{
        return s_auto_select_menu ? "Yes" : "No";
}

OptionSubmenuType AutoSelectMenuOption::submenu_type() const
{
        return OptionSubmenuType::input;
}

void AutoSelectMenuOption::change(OptionChangeCommand command) const
{
        (void)command;

        s_auto_select_menu = !s_auto_select_menu;
}

std::string DisplayHintsOption::name() const
{
        return "Display hints";
}

std::string DisplayHintsOption::descr() const
{
        return (
                "Controls when in-game hints should be displayed. "
                "\n\n{COLOR_LIGHT_WHITE}Once per game:{color_reset} "
                "Show all hints once in each game session "
                "(and repeat them again in other sessions). "
                "\n\n{COLOR_LIGHT_WHITE}Once:{color_reset} "
                "Only show each hint once across all game sessions. "
                "\n\n{COLOR_LIGHT_WHITE}Never:{color_reset} "
                "Completely disable hints.");
}

std::string DisplayHintsOption::value_str() const
{
        switch (s_hints_mode) {
        case HintsMode::once_per_game:
                return "Once per game";

        case HintsMode::once:
                return "Once";

        case HintsMode::never:
                return "Never";

        case HintsMode::END:
                break;
        }

        ASSERT(false);

        return "";
}

OptionSubmenuType DisplayHintsOption::submenu_type() const
{
        return OptionSubmenuType::gameplay;
}

void DisplayHintsOption::change(OptionChangeCommand command) const
{
        (void)command;

        const auto current_idx = (int)s_hints_mode;
        const auto nr_modes = (int)HintsMode::END;

        s_hints_mode = (HintsMode)((current_idx + 1) % nr_modes);

        hints::init();
}

std::string AlwaysWarnMonsterOption::name() const
{
        return "Always warn monster appears";
}

std::string AlwaysWarnMonsterOption::descr() const
{
        return (
                "Always warn when a monster appears in view (and no other "
                "monster is already seen), otherwise only warn while "
                "performing long actions like treating wounds.");
}

std::string AlwaysWarnMonsterOption::value_str() const
{
        return s_always_warn_new_mon ? "Yes" : "No";
}

OptionSubmenuType AlwaysWarnMonsterOption::submenu_type() const
{
        return OptionSubmenuType::gameplay;
}

void AlwaysWarnMonsterOption::change(OptionChangeCommand command) const
{
        (void)command;

        s_always_warn_new_mon = !s_always_warn_new_mon;
}

std::string WarnThrowValuableOption::name() const
{
        return "Warn throwing valuable item";
}

std::string WarnThrowValuableOption::descr() const
{
        return (
                "Warn before throwing \"valuable\" items such as potions.");
}

std::string WarnThrowValuableOption::value_str() const
{
        return s_warn_on_throw_valuable ? "Yes" : "No";
}

OptionSubmenuType WarnThrowValuableOption::submenu_type() const
{
        return OptionSubmenuType::gameplay;
}

void WarnThrowValuableOption::change(OptionChangeCommand command) const
{
        (void)command;

        s_warn_on_throw_valuable = !s_warn_on_throw_valuable;
}

std::string WarnLightExplosivesOption::name() const
{
        return "Warn lighting explosive";
}

std::string WarnLightExplosivesOption::descr() const
{
        return (
                "Warn before lighting explosives");
}

std::string WarnLightExplosivesOption::value_str() const
{
        return s_warn_on_light_explosive ? "Yes" : "No";
}

OptionSubmenuType WarnLightExplosivesOption::submenu_type() const
{
        return OptionSubmenuType::gameplay;
}

void WarnLightExplosivesOption::change(OptionChangeCommand command) const
{
        (void)command;

        s_warn_on_light_explosive = !s_warn_on_light_explosive;
}

std::string WanDrinkMalignPotionOption::name() const
{
        return "Warn malign potion";
}

std::string WanDrinkMalignPotionOption::descr() const
{
        return (
                "Warn before drinking potions that are known to be malignant.");
}

std::string WanDrinkMalignPotionOption::value_str() const
{
        return s_warn_on_drink_malign_potion ? "Yes" : "No";
}

OptionSubmenuType WanDrinkMalignPotionOption::submenu_type() const
{
        return OptionSubmenuType::gameplay;
}

void WanDrinkMalignPotionOption::change(OptionChangeCommand command) const
{
        (void)command;

        s_warn_on_drink_malign_potion = !s_warn_on_drink_malign_potion;
}

std::string WarnRangedWeaponMeleeOption::name() const
{
        return "Warn ranged weapon melee";
}

std::string WarnRangedWeaponMeleeOption::descr() const
{
        return (
                "Warn if attempting to perform a close combat melee attack "
                "with a ranged weapon (such as a pistol), which is possible "
                "but perhaps unintended.");
}

std::string WarnRangedWeaponMeleeOption::value_str() const
{
        return s_warn_on_ranged_wpn_melee ? "Yes" : "No";
}

OptionSubmenuType WarnRangedWeaponMeleeOption::submenu_type() const
{
        return OptionSubmenuType::gameplay;
}

void WarnRangedWeaponMeleeOption::change(OptionChangeCommand command) const
{
        (void)command;

        s_warn_on_ranged_wpn_melee = !s_warn_on_ranged_wpn_melee;
}

std::string MedicalBagAutoChoiceOption::name() const
{
        return "Auto medical bag choice";
}

std::string MedicalBagAutoChoiceOption::descr() const
{
        return (
                "Automatically choose Medical Bag action, "
                "according to the following priority: "
                "\n1) Treat infection"
                "\n2) Treat wound"
                "\n3) Quick patch-up"
                "\nIf this option is disabled, the action is chosen in a popup menu instead.");
}

std::string MedicalBagAutoChoiceOption::value_str() const
{
        return s_is_medical_bag_auto_choice ? "Yes" : "No";
}

OptionSubmenuType MedicalBagAutoChoiceOption::submenu_type() const
{
        return OptionSubmenuType::input;
}

void MedicalBagAutoChoiceOption::change(OptionChangeCommand command) const
{
        (void)command;

        s_is_medical_bag_auto_choice = !s_is_medical_bag_auto_choice;
}

std::string AutoReloadOption::name() const
{
        return "Auto reload weapons";
}

std::string AutoReloadOption::descr() const
{
        return (
                "Automatically perform a reload action instead if attempting "
                "to fire a ranged weapon with no ammo loaded.");
}

std::string AutoReloadOption::value_str() const
{
        return s_is_ranged_wpn_auto_reload ? "Yes" : "No";
}

OptionSubmenuType AutoReloadOption::submenu_type() const
{
        return OptionSubmenuType::input;
}

void AutoReloadOption::change(OptionChangeCommand command) const
{
        (void)command;

        s_is_ranged_wpn_auto_reload = !s_is_ranged_wpn_auto_reload;
}

std::string ProjectileDelayOption::name() const
{
        return "Projectile delay (ms)";
}

std::string ProjectileDelayOption::descr() const
{
        return (
                "Number of milliseconds per step when running "
                "projectile animations.");
}

std::string ProjectileDelayOption::value_str() const
{
        return std::to_string(s_delay_projectile_draw);
}

OptionSubmenuType ProjectileDelayOption::submenu_type() const
{
        return OptionSubmenuType::gameplay;
}

void ProjectileDelayOption::change(OptionChangeCommand command) const
{
        const Range allowed_range(0, 900);

        if (command == OptionChangeCommand::enter) {
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
        else if (command == OptionChangeCommand::left) {
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
}

std::string ExplosionDelayOption::name() const
{
        return "Explosion delay (ms)";
}

std::string ExplosionDelayOption::descr() const
{
        return (
                "Duration of explosion animations (milliseconds).");
}

std::string ExplosionDelayOption::value_str() const
{
        return std::to_string(s_delay_explosion);
}

OptionSubmenuType ExplosionDelayOption::submenu_type() const
{
        return OptionSubmenuType::gameplay;
}

void ExplosionDelayOption::change(OptionChangeCommand command) const
{
        const Range allowed_range(0, 900);

        if (command == OptionChangeCommand::enter) {
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
        else if (command == OptionChangeCommand::left) {
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
}

// std::string ResetDefaultsOption::name() const
// {
//         return "Reset to defaults";
// }

// std::string ResetDefaultsOption::descr() const
// {
//         return (
//                 "");
// }

// std::string ResetDefaultsOption::value_str() const
// {
//         return "";
// }

// OptionSubmenuType ResetDefaultsOption::submenu_type() const
// {
//         return OptionSubmenuType::gameplay;
// }

// void ResetDefaultsOption::change(OptionChangeCommand command) const
// {
//         if (command == OptionChangeCommand::enter) {
//                 set_default_variables();
//                 update_render_dims();
//                 io::init_other();
//                 audio::init();
//                 states::draw();
//                 io::update_screen();
//         }
// }

}  // namespace config

// -----------------------------------------------------------------------------
// Options state
// -----------------------------------------------------------------------------
OptionsState::OptionsState() = default;

StateId OptionsState::id() const
{
        return StateId::options;
}

std::string OptionsState::page_title() const
{
        return "Options";
}

std::vector<MenuPageEntry> OptionsState::page_entries() const
{
        std::vector<MenuPageEntry> entries;

        for (int i = 0; i < (int)config::OptionSubmenuType::END; ++i) {
                entries.push_back(
                        {submenu_name_with_key_shortcut(
                                 (config::OptionSubmenuType)i),
                         ""});
        }

        return entries;
}

void OptionsState::on_entry_selected(const int idx)
{
        states::push(
                std::make_unique<OptionsSubmenuState>(
                        (config::OptionSubmenuType)idx));
}

// -----------------------------------------------------------------------------
// Options submenu state
// -----------------------------------------------------------------------------
static void change_current_option(
        const int idx,
        const config::OptionChangeCommand command)
{
        s_current_options[idx]->change(command);

        write_config_file();

        io::clear_input();
}

OptionsSubmenuState::OptionsSubmenuState(
        const config::OptionSubmenuType submenu)
{
        s_current_submenu = submenu;

        filter_current_options(submenu);
}

StateId OptionsSubmenuState::id() const
{
        return StateId::options_submenu;
}

std::string OptionsSubmenuState::page_title() const
{
        return submenu_name(s_current_submenu);
}

std::string OptionsSubmenuState::page_hint() const
{
        return common_text::g_set_option_hint;
}

std::vector<MenuPageEntry> OptionsSubmenuState::page_entries() const
{
        std::vector<MenuPageEntry> entries;

        entries.reserve(s_current_options.size());

        for (const config::Option* const option : s_current_options) {
                entries.push_back({option->name(), option->value_str()});
        }

        return entries;
}

void OptionsSubmenuState::on_entry_selected(const int idx)
{
        change_current_option(idx, config::OptionChangeCommand::enter);
}

void OptionsSubmenuState::on_entry_left(const int idx)
{
        change_current_option(idx, config::OptionChangeCommand::left);
}

void OptionsSubmenuState::on_entry_right(const int idx)
{
        change_current_option(idx, config::OptionChangeCommand::right);
}

bool OptionsSubmenuState::entry_plays_selection_audio(const int idx) const
{
        // Some options play a custom selection audio, so they must disable
        // the one played by the menu browser
        return s_current_options[idx]->allow_browser_selection_audio();
}

void OptionsSubmenuState::draw_page_content()
{
        const config::Option* const selected_option =
                s_current_options[m_browser.y()];

        const std::string descr_str = selected_option->descr();

        if (!descr_str.empty()) {
                Text text(descr_str);

                text.set_w(panels::w(Panel::options_descr));

                io::draw_text(
                        text,
                        Panel::options_descr,
                        {0, 0},
                        colors::text());
        }
}

int OptionsSubmenuState::list_x0(const int block_w) const
{
        (void)block_w;

        return panels::p0(Panel::options).x;
}

int OptionsSubmenuState::list_y0(const int nr_entries_shown) const
{
        (void)nr_entries_shown;

        return panels::p0(Panel::options).y;
}

int OptionsSubmenuState::list_max_x1() const
{
        return panels::p0(Panel::options_descr).x - 2;
}

bool OptionsSubmenuState::use_left_right_keys() const
{
        return true;
}
