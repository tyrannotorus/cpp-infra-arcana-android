// =============================================================================
// Copyright 2011-2023 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "actor_player_state.hpp"

#include "item.hpp"
#include "item_explosive.hpp"
#include "item_misc.hpp"
#include "item_weapon.hpp"

namespace actor::player_state
{
item::MedicalBag* g_active_medical_bag {nullptr};
int g_equip_armor_countdown {0};
int g_remove_armor_countdown {0};
bool g_is_dropping_armor_from_body {false};
item::Item* g_item_equipping {nullptr};
std::unique_ptr<item::Explosive> g_active_explosive {};
item::Item* g_last_thrown_item {nullptr};
std::unique_ptr<item::Wpn> g_unarmed_wpn {};

Actor* g_target {nullptr};

int g_wait_turns_left {-1};

Dir g_auto_move_dir {Dir::END};
bool g_has_taken_auto_move_step {false};

int g_insanity {0};
double g_shock {0.0};
double g_shock_tmp {0.0};
int g_nr_turns_until_insanity {-1};

double g_player_total_shock_taken = 0.0;
double g_player_total_shock_from_src[(size_t)ShockSrc::END] {};

int g_nr_turns_until_r_spell {-1};
int g_nr_turns_until_meditative_focused {-1};

Color g_lantern_color {};

Actor* g_seen_mon_to_warn_about {nullptr};
bool g_allow_print_mon_warning {false};

bool g_did_warn_encumbered {false};

void init()
{
        g_active_medical_bag = nullptr;
        g_equip_armor_countdown = 0;
        g_remove_armor_countdown = 0;
        g_is_dropping_armor_from_body = false;
        g_item_equipping = nullptr;
        g_active_explosive.reset();
        g_last_thrown_item = nullptr;
        g_unarmed_wpn.reset();

        g_target = nullptr;

        g_wait_turns_left = -1;

        g_auto_move_dir = Dir::END;
        g_has_taken_auto_move_step = false;

        g_insanity = 0;
        g_shock = 0.0;
        g_shock_tmp = 0.0;
        g_nr_turns_until_insanity = -1;

        g_player_total_shock_taken = 0.0;

        for (size_t src_idx = 0; src_idx < (size_t)ShockSrc::END; ++src_idx) {
                g_player_total_shock_from_src[src_idx] = 0.0;
        }

        g_nr_turns_until_r_spell = -1;
        g_nr_turns_until_meditative_focused = -1;

        g_lantern_color = {};

        g_seen_mon_to_warn_about = nullptr;
        g_allow_print_mon_warning = false;

        g_did_warn_encumbered = false;
}

}  // namespace actor::player_state
