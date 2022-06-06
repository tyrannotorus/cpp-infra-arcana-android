// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef ACTOR_PLAYER_STATE
#define ACTOR_PLAYER_STATE

#include <memory>

#include "colors.hpp"
#include "direction.hpp"

namespace item
{
class Explosive;
class Item;
class MedicalBag;
class Wpn;
}  // namespace item

namespace actor
{
class Actor;

namespace player_state
{
// Inventory and item handling state
extern item::MedicalBag* g_active_medical_bag;
extern int g_equip_armor_countdown;
extern int g_remove_armor_countdown;
extern bool g_is_dropping_armor_from_body;
extern item::Item* g_item_equipping;
extern std::unique_ptr<item::Explosive> g_active_explosive;
extern item::Item* g_last_thrown_item;
extern std::unique_ptr<item::Wpn> g_unarmed_wpn;

// Player target
extern Actor* g_target;

// "Five turn waiting" (long wait command) state
extern int g_wait_turns_left;

// Auto-move command state
extern Dir g_auto_move_dir;
extern bool g_has_taken_auto_move_step;

// Shock and insanity state
extern int g_insanity;
extern double g_shock;
extern double g_shock_tmp;
extern int g_nr_turns_until_insanity;

// Cooldowns to regain effects
extern int g_nr_turns_until_r_spell;
extern int g_nr_turns_until_meditative_focused;

// Current color for lantern flickering effect
extern Color g_lantern_color;

// State for keeping track of if a monster should be warned about
// ("[...] is in my view!").
extern Actor* g_seen_mon_to_warn_about;
extern bool g_allow_print_mon_warning;

void init();

}  // namespace player_state

}  // namespace actor

#endif  // ACTOR_PLAYER_STATE
