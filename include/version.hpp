// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef VERSION_HPP
#define VERSION_HPP

#include <optional>
#include <string>

namespace version_info
{
extern const std::string g_version_str;

// Core version plus the Android port build increment
// (major.minor.patch.build) - the user facing version string
extern const std::string g_build_version_str;

extern const std::string g_copyright_str;
extern const std::string g_license_str;

std::optional<std::string> read_git_sha1_str_from_file();

}  // namespace version_info

#endif  // VERSION_HPP
