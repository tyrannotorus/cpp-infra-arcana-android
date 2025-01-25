// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "paths.hpp"

#include <cstdlib>
#include <filesystem>
#include <ostream>
#include <queue>
#include <string>

#include "SDL.h"
#include "debug.hpp"
#include "ini.h"
#include "io.hpp"
#include "version.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static std::string s_user_dir;

std::queue<std::string> s_pending_error_messages;

// Can be used by the player for overriding the user data directory.
const static std::string s_user_data_ini_file_name = "user_data.ini";

// NOTE: This will also attempt to create the directories.
static bool ensure_writable_location(const std::string& path)
{
        TRACE_FUNC_BEGIN;

        TRACE
                << "Creating directories and testing if directory is writable: '"
                << path
                << "'"
                << std::endl;

        try {
                std::filesystem::path temp_file_path = std::filesystem::path(path) / "test.tmp";

                std::filesystem::create_directories(temp_file_path.parent_path());

                // Try to open the temp file for writing.
                std::ofstream temp_file(temp_file_path.string(), std::ios::out | std::ios::trunc);

                if (!temp_file.is_open()) {
                        TRACE
                                << "Unable to open temporary file "
                                << temp_file_path
                                << std::endl;

                        return false;
                }

                // Write a test string.
                temp_file << "test";
                temp_file.close();

                // OK, we can write here!

                TRACE << "Writing succeeded" << std::endl;

                // Clean up
                std::filesystem::remove(temp_file_path);

                TRACE_FUNC_END;

                return true;

        } catch (const std::exception& e) {
                TRACE_ERROR_RELEASE << "Error checking write access: " << e.what() << std::endl;

                TRACE_FUNC_END;

                return false;
        }

        TRACE_FUNC_END;
}

static std::string read_user_dir_from_ini_file()
{
        TRACE_FUNC_BEGIN;

        // TODO: It seems like the custom directory from user_data.ini is never created, but if it
        // exists files are placed in it. Why is the SDL_GetPrefPath directory automatically created
        // but not e.g. "~/foo" specified in user_data.ini?

        mINI::INIFile user_data_ini(s_user_data_ini_file_name);
        mINI::INIStructure ini;
        user_data_ini.read(ini);

        std::string path_str = ini["paths"]["user_data"];

        TRACE_FUNC_END;

        return path_str;
}

static void add_error_msg_user_dir_not_writable(const std::string& user_dir)
{
        s_pending_error_messages.push(
                "The user data directory '" +
                user_dir +
                "' specified in " +
                s_user_data_ini_file_name +
                " is not writable, falling back to calling SDL_GetPrefPath or using the game "
                "directory to store user data.");
}

static void add_error_msg_no_directory_writable()
{
        const std::string sdl_pref_path = io::sdl_pref_dir();

        s_pending_error_messages.push(
                "Could not find any writable location. Tried path set in " +
                s_user_data_ini_file_name +
                ", path retrieved from calling SDL_GetPrefPath (" +
                sdl_pref_path +
                "), and a path inside the game directory.");
}

// -----------------------------------------------------------------------------
// paths
// -----------------------------------------------------------------------------

// TODO: Use std::filesystem instead of concatenating path strings.

namespace paths
{
void init()
{
        TRACE_FUNC_BEGIN;

        while (!s_pending_error_messages.empty()) {
                s_pending_error_messages.pop();
        }

        // Attempt to decide a directory for user data (config highscore, etc) according to the
        // following prioritization:
        //
        // 1) The custom path set in the user data ini file, if any.
        // 2) The location decided by SDL_GetPrefPath.
        // 3) A path inside the game directory.
        //
        // For each directory considered, a check is made to see if the directory is writable. If
        // none of the above directories are writable, we print a warning informing the player of
        // this limitation, but still let them play the game - just without the possibility of
        // saving, keeping highscores, setting persistent options, etc.
        //

        //
        // 1) Try the custom path in the user data ini file:
        //

        TRACE
                << "Attempting to use path from "
                << s_user_data_ini_file_name
                << " for user data"
                << std::endl;

        std::string user_dir = read_user_dir_from_ini_file();

        TRACE
                << "User data path set from "
                << s_user_data_ini_file_name
                << ": '" << user_dir << "'" << std::endl;

        if (user_dir.empty()) {
                TRACE << "Path from " << s_user_data_ini_file_name << " is empty" << std::endl;
        }
        else {
                bool is_writable = ensure_writable_location(user_dir);

                if (is_writable) {
                        TRACE
                                << "Will use path from "
                                << s_user_data_ini_file_name
                                << " for user data"
                                << std::endl;

                        // TODO: This is hacky, but will not be needed if using std::filesystem.
                        user_dir += "/";
                }
                else {
                        TRACE
                                << "Could not write to path from "
                                << s_user_data_ini_file_name
                                << std::endl;

                        add_error_msg_user_dir_not_writable(user_dir);

                        user_dir = "";
                }
        }

        //
        // 2) Try using the SDL_GetPrefPath directory.
        //
        if (user_dir.empty()) {
                TRACE
                        << "Attempting to use path from SDL_GetPrefPath for user data"
                        << std::endl;

                user_dir = io::sdl_pref_dir();

                bool is_writable = ensure_writable_location(user_dir);

                if (is_writable) {
                        TRACE << "Will use path from SDL_GetPrefPath for user data" << std::endl;
                }
                else {
                        TRACE << "Could not write to path from SDL_GetPrefPath" << std::endl;

                        user_dir = "";
                }
        }

        //
        // 3) Try using path in game directory
        //
        if (user_dir.empty()) {
                TRACE
                        << "Attempting to use path in game directory for user data"
                        << std::endl;

                user_dir = "user_data/";

                bool is_writable = ensure_writable_location(user_dir);

                if (is_writable) {
                        TRACE << "Will use path in game directory for user data" << std::endl;
                }
                else {
                        // NOTE: We keep this directory set even if it's not possible to write here.

                        add_error_msg_no_directory_writable();
                }
        }

        s_user_dir = user_dir;

        TRACE_FUNC_END;
}

std::queue<std::string>& pending_error_messages()
{
        return s_pending_error_messages;
}

std::string user_dir()
{
        return s_user_dir;
}

std::string save_file_path()
{
        return user_dir() + "save";
}

std::string config_file_path()
{
        return user_dir() + "config";
}

std::string highscores_file_path()
{
        return user_dir() + "highscores";
}

std::string gfx_dir()
{
        return "gfx/";
}

std::string fonts_dir()
{
        return gfx_dir() + "/fonts/";
}

std::string tiles_dir()
{
        return gfx_dir() + "/tiles/20x20/";
}

std::string images_dir()
{
        return gfx_dir() + "/images/";
}

std::string logo_img_path()
{
        return images_dir() + "/main_menu_logo.png";
}

std::string logo_small_img_path()
{
        return images_dir() + "/main_menu_logo_small.png";
}

std::string audio_dir()
{
        return "audio/";
}

std::string data_dir()
{
        return "data/";
}

std::string messages_dir()
{
        return data_dir() + "/messages/";
}

}  // namespace paths
