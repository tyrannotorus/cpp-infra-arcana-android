// =============================================================================
// Copyright Werewolf Camp
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef EASING_HPP
#define EASING_HPP

// Shaping curves for animation. Each takes and returns progress from 0 to 1.
namespace ease
{
// Creeps, then rushes
inline float in(const float t)
{
        return t * t;
}

// Moves at once, then settles
inline float out(const float t)
{
        const float u = 1.0f - t;

        return 1.0f - (u * u);
}

// As out, but settling harder - the last of the motion is very slow
inline float cubic_out(const float t)
{
        const float u = 1.0f - t;

        return 1.0f - (u * u * u);
}

}  // namespace ease

#endif  // EASING_HPP
