// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef DEBUG_HPP
#define DEBUG_HPP

#include <cstdlib>
#include <iostream>

// -----------------------------------------------------------------------------
// ASSERT macro
// -----------------------------------------------------------------------------
#ifdef NDEBUG

// Release mode

#define ASSERT(check)

#else

// Debug mode

#define ASSERT(check) assert_impl(check, #check, __FILE__, __LINE__, __func__)

#endif

// -----------------------------------------------------------------------------
// TRACE macros
// -----------------------------------------------------------------------------
// Print an error, for both debug and release builds.
#define TRACE_ERROR_RELEASE std::cerr << "ERROR: "

#if defined(NDEBUG) || defined(IA_TEST)

// Release mode or test mode - do not output anything.

// NOTE: For release mode or test mode, the TRACE functionality never do anything. The if/else here
// is a trick to support writing e.g.:
//
// TRACE << "foo" << std::endl;
//
// ...which will be evaluated to:
//
// if (1) ; else std::cerr << "foo" << std::endl;
//
#define TRACE \
        if (1) \
                ; \
        else \
                std::cerr

#define TRACE_FUNC_BEGIN \
        if (1) \
                ; \
        else \
                std::cerr

#define TRACE_FUNC_END \
        if (1) \
                ; \
        else \
                std::cerr

#define PANIC exit(EXIT_FAILURE)

#else

// Debug mode

#define TRACE \
        std::cerr \
                << "DEBUG: " \
                << __FILE__ << ", " \
                << __LINE__ << ", " \
                << __func__ << "(): "

#define TRACE_FUNC_BEGIN \
        std::cerr \
                << "DEBUG: " \
                << __FILE__ << ", " \
                << __LINE__ << ", " \
                << __func__ << "() [BEGIN]" \
                << std::endl

#define TRACE_FUNC_END \
        std::cerr \
                << "DEBUG: " \
                << __FILE__ << ", " \
                << __LINE__ << ", " \
                << __func__ << "() [END]" \
                << std::endl

#define PANIC ASSERT(false)

#endif

//------------------------------------------------------------------------------
// Custom assert
// NOTE: Never call this function directly, use the "ASSERT" macro above
//------------------------------------------------------------------------------
void assert_impl(
        bool check,
        const char* check_str,
        const char* file,
        int line,
        const char* func);

#endif  // DEBUG_HPP
