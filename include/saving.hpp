// =============================================================================
// Copyright 2011-2021 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef SAVING_HPP
#define SAVING_HPP

#include <string>

namespace saving
{
void init();

void save_game();

void load_game();

void erase_save();

bool is_save_available();

bool is_loading();

//Functions called by modules when saving and loading.
void put_str(const std::string& str);
void put_int(int v);
void put_bool(bool v);

std::string get_str();
int get_int();
bool get_bool();

}  // namespace saving

#endif  // SAVING_HPP
