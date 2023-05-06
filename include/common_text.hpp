// =============================================================================
// Copyright 2011-2023 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef COMMON_TEXT_HPP
#define COMMON_TEXT_HPP

#include <string>
#include <vector>

namespace common_text
{
extern const std::string g_screen_exit_hint;
extern const std::string g_minimap_exit_hint;
extern const std::string g_game_over_summary_exit_hint;
extern const std::string g_set_option_hint;
extern const std::string g_scroll_hint;
extern const std::string g_scrollable_info_screen_hint;
extern const std::string g_next_page_up_hint;
extern const std::string g_next_page_down_hint;
extern const std::string g_cancel_hint;
extern const std::string g_confirm_hint;
extern const std::string g_confirm_drop_hint;
extern const std::string g_any_key_hint;
extern const std::string g_yes_or_no_hint;
extern const std::string g_direction_query;
extern const std::string g_disarm_no_trap;
extern const std::string g_mon_prevent_cmd;
extern const std::string g_fire_prevent_cmd;
extern const std::string g_shock_prevent_cmd;
extern const std::string g_mon_disappear;
extern const std::string g_mon_disappear_reappear;
extern const std::string g_miscast_player;
extern const std::string g_miscast_mon;

extern const std::vector<std::string> g_exorcist_purge_phrases;

}  // namespace common_text

#endif  // COMMON_TEXT_HPP
