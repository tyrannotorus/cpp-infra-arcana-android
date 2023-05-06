// =============================================================================
// Copyright 2011-2023 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef VIEW_HPP
#define VIEW_HPP

struct P;

namespace view
{
void print_location_info_msgs(const P& pos);

void print_living_actor_info_msg(const P& pos);

}  // namespace view

#endif  // VIEW_HPP
