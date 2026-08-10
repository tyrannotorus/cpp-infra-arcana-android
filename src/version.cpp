// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "version.hpp"

#include <fstream>

#include "debug.hpp"
#include "paths.hpp"

namespace version_info
{
// The upstream Infra Arcana core version - LOCKED to the vendored v23.0.0
// until (if ever) the port updates from upstream. NOTE: Also used for the
// user data directory (io::sdl_pref_dir) - never append the Android build
// number to this string.
const std::string g_version_str = "v23.0.0";

// The displayed version: the core version plus our own Android port build
// increment (Brogue-style major.minor.patch.build). The release workflow
// stamps the increment from the git tag; keep it and
// versionName/versionCode in android/app/build.gradle.kts in sync for
// local builds. The "-alpha" phase suffix is manual - dropping it when
// the port leaves alpha is a deliberate decision, not a tag format.
const std::string g_build_version_str = g_version_str + ".13-alpha";
const std::string g_copyright_str = "(c) 2011-2025 Martin Tornqvist";
const std::string g_license_str = "Infra Arcana is free software, see LICENSE.txt.";
const std::string g_date_str = __DATE__;

std::optional<std::string> read_git_sha1_str_from_file()
{
        const auto sha1_file_path = paths::data_dir() + "git-sha1.txt";

        std::ifstream file(sha1_file_path);

        if (!file.is_open()) {
                TRACE << "Failed to open git sha1 file at "
                      << sha1_file_path
                      << std::endl;

                return {};
        }

        std::string sha1;

        getline(file, sha1);

        file.close();

        if (sha1.empty()) {
                return {};
        }

        return sha1;
}

}  // namespace version_info
