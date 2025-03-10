// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef QUERY_HPP
#define QUERY_HPP

#include <optional>
#include <string>

#include "direction.hpp"
#include "random.hpp"

namespace io
{
struct InputData;
}  // namespace io

enum class BinaryAnswer
{
        no,
        yes,
        special
};

enum class AllowCenter
{
        no,
        yes
};

enum class AllowSpaceCancel
{
        no,
        yes
};

namespace query
{
struct QueryNumberConfig
{
        Range allowed_range {};
        int default_value {0};
        bool cancel_returns_default {false};
};

void init();

void cleanup();

void wait_for_key_press();

BinaryAnswer yes_or_no(
        std::optional<char> key_for_special_event = std::nullopt,
        AllowSpaceCancel allow_space_cancel = AllowSpaceCancel::yes);

Dir dir(AllowCenter allow_center);

void wait_for_msg_more();

void wait_for_confirm();

io::InputData letter(bool accept_enter);

int number(
        const QueryNumberConfig& config,
        const std::string& title,
        const std::string& msg = "");

}  // namespace query

#endif  // QUERY_HPP
