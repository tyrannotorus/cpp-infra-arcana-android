// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef EXPLOSION_HPP
#define EXPLOSION_HPP

#include <optional>
#include <vector>

#include "colors.hpp"
#include "rect.hpp"

struct P;

namespace prop
{
class Prop;
}  // namespace prop

enum class ExplType
{
        expl,
        apply_prop
};

enum class EmitExplSnd
{
        no,
        yes
};

enum class ExplExclCenter
{
        no,
        yes
};

enum class ExplIsGas
{
        no,
        yes
};

namespace explosion
{
// TODO: The signature of this function is really ugly! Do something similar to the Sound class
// instead.

// NOTE: If "emit_expl_sound" is set to "no", this typically means that the caller should emit a
// custom sound before running the explosion (e.g. molotov explosion sound).
void run(
        const P& origin,
        ExplType expl_type,
        EmitExplSnd emit_expl_snd = EmitExplSnd::yes,
        int radi_change = 0,
        ExplExclCenter exclude_center = ExplExclCenter::no,
        const std::vector<prop::Prop*>& properties_applied = {},
        const std::optional<Color>& color_override = {},
        ExplIsGas is_gas = ExplIsGas::no);

void run_smoke_explosion_at(const P& origin, int radi_change = 0);

void run_mist_explosion_at(const P& origin, int radi_change = 0);

R explosion_area(const P& c, int radi);

R explosion_area_outside_map_allowed(const P& c, int radi);

}  // namespace explosion

#endif
