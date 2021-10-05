// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "paths.hpp"

#include <ostream>

#include "debug.hpp"
#include "io.hpp"

namespace paths
{
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

std::string user_dir()
{
        auto path_str = io::sdl_pref_dir();

        TRACE << "User data directory: " << path_str << std::endl;

        return path_str;
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

}  // namespace paths
