// =============================================================================
// Copyright 2011-2021 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "version.hpp"

#include <fstream>

#include "debug.hpp"
#include "paths.hpp"

#define STRINGIFY(x) #x
#define TO_STRING(x) STRINGIFY(x)

namespace version_info
{
// This shall be set when (and only when) building a tagged release. Use the
// format "vMAJOR.MINOR".
const std::string g_version_str;

const std::string g_copyright_str =
        "(c) 2011-2021 Martin Tornqvist";

const std::string g_license_str =
        "Infra Arcana is free software, see LICENSE.txt.";

const std::string g_date_str = __DATE__;

std::string read_git_sha1_str_from_file()
{
        const auto* const default_sha1 = "unknown_revision";

        const auto sha1_file_path = paths::data_dir() + "git-sha1.txt";

        std::ifstream file(sha1_file_path);

        if (!file.is_open())
        {
                TRACE << "Failed to open git sha1 file at "
                      << sha1_file_path
                      << std::endl;

                return default_sha1;
        }

        std::string sha1;

        getline(file, sha1);

        file.close();

        if (sha1.empty())
        {
                return default_sha1;
        }

        return sha1;
}

}  // namespace version_info
