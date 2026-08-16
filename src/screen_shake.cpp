// =============================================================================
// Copyright Werewolf Camp
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "screen_shake.hpp"

#include <algorithm>

#include "config.hpp"
#include "io_display.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static const uint32_t s_blow_duration_ms = 110;
static const uint32_t s_damaged_duration_ms = 180;

// The bot plays without a screen to shake
static bool is_shake_wanted()
{
        return !config::is_bot_playing();
}

// -----------------------------------------------------------------------------
// screen_shake
// -----------------------------------------------------------------------------
namespace screen_shake
{
void on_player_blow()
{
        if (!is_shake_wanted()) {
                return;
        }

        const int amplitude = std::max(2, io::max_map_shake_px() / 3);

        io::start_map_shake(amplitude, s_blow_duration_ms);
}

void on_player_damaged(const int dmg, const int max_hp)
{
        if (!is_shake_wanted() || (dmg <= 0)) {
                return;
        }

        const int max_amplitude = std::max(2, io::max_map_shake_px() / 2);

        // Full amplitude at half the player's max hp in one blow
        const int amplitude =
                std::clamp(
                        (max_amplitude * dmg * 2) / std::max(1, max_hp),
                        2,
                        max_amplitude);

        io::start_map_shake(amplitude, s_damaged_duration_ms);
}

void on_explosion(const uint32_t duration_ms)
{
        if (!is_shake_wanted()) {
                return;
        }

        io::start_map_shake(io::max_map_shake_px(), duration_ms);
}

}  // namespace screen_shake
