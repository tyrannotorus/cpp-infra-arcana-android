// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "common_text.hpp"

namespace common_text
{
const std::string g_screen_exit_hint =
        // Screens close via the [ x ] control
        "";

const std::string g_minimap_exit_hint =
        "";

const std::string g_set_option_hint =
        // Options are stepped by swiping sideways (or tapping the row)
        "swipe left/right to set option";

const std::string g_menu_select_hint =
        // The standard footer hint of pages with a selectable list
        "swipe/tap to select";

const std::string g_scrollable_info_screen_hint =
        // Content scrolls with its scrollbar; screens close via the [ x ]
        // control - neither needs a footer hint
        "";

const std::string g_cancel_hint =
        // Cancelling is done via [ cancel ] context pins, the [ x ]
        // close control, or the device back button - there is no esc
        // action bar button, and no key hint text
        "";

const std::string g_confirm_hint =
        // Tapping the screen sends enter, and the action bar has esc
        "tap to continue";

const std::string g_any_key_hint =
        "tap to continue";

const std::string g_yes_or_no_hint =
        // Yes/no queries show tappable [ yes ] / [ no ] buttons after the
        // question instead of a key hint (see msg_log)
        "";

const std::string g_direction_query =
        "Which direction?";

const std::string g_disarm_no_trap =
        "I find nothing there to disarm.";

const std::string g_mon_prevent_cmd =
        "Not while an enemy is near.";

const std::string g_shock_prevent_cmd =
        "Not while insanity is near.";

const std::string g_fire_prevent_cmd =
        "Fire is spreading!";

const std::string g_burning_prevent_attack_ranged =
        "Not while burning.";

const std::string g_mon_disappear =
        "suddenly disappears!";

const std::string g_mon_disappear_reappear =
        "suddenly disappears and reappears!";

const std::string g_miscast_player =
        "I fail to concentrate!";

const std::string g_miscast_mon =
        "fails to concentrate.";

const std::vector<std::string> g_exorcist_purge_phrases = {
        "This place feels more serene now.",
        "The sanctity of this place has been somewhat restored.",
        "A great wickedness has been extinguished.",
        "I sense a stillness permeating throughout the area."};

}  // namespace common_text
