// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "version.hpp"

#include <fstream>

#include "debug.hpp"
#include "paths.hpp"

namespace version_info
{
const std::string g_version_str = "v21.0.1.beta2";

const std::string g_copyright_str =
        "(c) 2011-2022 Martin Tornqvist";

const std::string g_license_str =
        "Infra Arcana is free software, see LICENSE.txt.";

const std::string g_date_str = __DATE__;

std::optional<std::string> read_git_sha1_str_from_file()
{
        const auto sha1_file_path = paths::data_dir() + "git-sha1.txt";

        std::ifstream file(sha1_file_path);

        if (!file.is_open())
        {
                TRACE << "Failed to open git sha1 file at "
                      << sha1_file_path
                      << std::endl;

                return {};
        }

        std::string sha1;

        getline(file, sha1);

        file.close();

        if (sha1.empty())
        {
                return {};
        }

        return sha1;
}

}  // namespace version_info
