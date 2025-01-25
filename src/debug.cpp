// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "debug.hpp"

#include <cassert>
#include <iostream>

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static void on_assert_failed(
        const char* const check_str,
        const char* const file,
        const int line,
        const char* const func)
{
        std::cerr << std::endl
                  << file << ", "
                  << line << ", "
                  << func << "():"
                  << std::endl
                  << std::endl
                  << "*** ASSERTION FAILED! ***"
                  << std::endl
                  << std::endl
                  << "Check that failed:"
                  << std::endl
                  << "\"" << check_str << "\""
                  << std::endl
                  << std::endl;

        assert(false);
}

// -----------------------------------------------------------------------------
// Public
// -----------------------------------------------------------------------------
void assert_impl(
        const bool check,
        const char* const check_str,
        const char* const file,
        const int line,
        const char* const func)
{
        if (!check) {
                on_assert_failed(check_str, file, line, func);
        }
}
