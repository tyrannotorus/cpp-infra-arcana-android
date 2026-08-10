// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef FADE_HPP
#define FADE_HPP

#include <cstdint>

// -----------------------------------------------------------------------------
// Fading the screen out, for the transitions that want a beat of nothing
// between one thing and the next.
// -----------------------------------------------------------------------------
namespace fade
{
// A fade like this is not a place to be interrupted, so it isn't
// interruptible: the wait is blocking, and input is DEAD throughout -
// events that arrive while it runs are swallowed rather than queued, so
// nothing tapped during it is acted on when it ends.
//
// Whatever the states draw right now is what fades, so call this from
// where the transition belongs - BEFORE pushing or popping whatever comes
// next. The screen is left black; the next thing drawn simply covers it.
inline constexpr uint32_t g_default_duration_ms = 3000;

void to_black(uint32_t duration_ms = g_default_duration_ms);

}  // namespace fade

#endif  // FADE_HPP
