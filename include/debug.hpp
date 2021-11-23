// =============================================================================
// Copyright 2011-2021 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef DEBUG_HPP
#define DEBUG_HPP

#include <cstdlib>
#include <iostream>

//------------------------------------------------------------------------------
// Trace level (use -D build flag to override)
//------------------------------------------------------------------------------
// Lvl of TRACE output in debug mode
// 0 : Disabled
// 1 : Standard
// 2 : Verbose
#ifndef TRACE_LVL

#define TRACE_LVL 1

#endif  // TRACE_LVL

//------------------------------------------------------------------------------
// Custom trace output and assert functionality
//------------------------------------------------------------------------------
#ifdef NDEBUG  // Release mode

#define ASSERT(check)

// For release mode, the TRACE functionality never do anything. The if/else here
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
#define TRACE_VERBOSE \
        if (1) \
                ; \
        else \
                std::cerr
#define TRACE_FUNC_BEGIN_VERBOSE \
        if (1) \
                ; \
        else \
                std::cerr
#define TRACE_FUNC_END_VERBOSE \
        if (1) \
                ; \
        else \
                std::cerr

#define PANIC exit(EXIT_FAILURE)

#else  // Debug mode

#define ASSERT(check) assert_impl(check, #check, __FILE__, __LINE__, __func__)

#define TRACE \
        if (TRACE_LVL < 1) \
                ; \
        else \
                std::cerr \
                        << "DEBUG: " \
                        << __FILE__ << ", " \
                        << __LINE__ << ", " \
                        << __func__ << "(): "

#define TRACE_FUNC_BEGIN \
        if (TRACE_LVL < 1) \
                ; \
        else \
                std::cerr \
                        << "DEBUG: " \
                        << __FILE__ << ", " \
                        << __LINE__ << ", " \
                        << __func__ << "() [BEGIN]" \
                        << std::endl

#define TRACE_FUNC_END \
        if (TRACE_LVL < 1) \
                ; \
        else \
                std::cerr \
                        << "DEBUG: " \
                        << __FILE__ << ", " \
                        << __LINE__ << ", " \
                        << __func__ << "() [END]" \
                        << std::endl

#define TRACE_VERBOSE \
        if (TRACE_LVL < 2) \
                ; \
        else \
                TRACE
#define TRACE_FUNC_BEGIN_VERBOSE \
        if (TRACE_LVL < 2) \
                ; \
        else \
                TRACE_FUNC_BEGIN
#define TRACE_FUNC_END_VERBOSE \
        if (TRACE_LVL < 2) \
                ; \
        else \
                TRACE_FUNC_END

#define PANIC ASSERT(false)

#endif  // NDEBUG

// Print an error, for both debug and release builds
#define TRACE_ERROR_RELEASE std::cerr << "ERROR: "

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
