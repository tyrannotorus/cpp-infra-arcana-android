// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef PATHS_HPP
#define PATHS_HPP

#include <string>

namespace paths
{
std::string gfx_dir();

std::string fonts_dir();
std::string tiles_dir();
std::string images_dir();

std::string logo_img_path();
std::string skull_img_path();

std::string audio_dir();

std::string data_dir();

std::string user_dir();

std::string save_file_path();

std::string config_file_path();

std::string highscores_file_path();

}  // namespace paths

#endif  // PATHS_HPP
