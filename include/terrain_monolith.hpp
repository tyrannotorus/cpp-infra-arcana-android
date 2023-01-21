// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef TERRAIN_MONOLITH_HPP
#define TERRAIN_MONOLITH_HPP

#include <string>

#include "colors.hpp"
#include "global.hpp"
#include "terrain.hpp"
#include "terrain_data.hpp"

namespace actor
{
class Actor;
}  // namespace actor
struct P;

namespace terrain
{
class Monolith : public Terrain
{
public:
        Monolith(const P& p, const TerrainData* data);
        Monolith() = delete;
        ~Monolith() = default;

        std::string name(Article article) const override;

        void bump(actor::Actor& actor_bumping) override;

        void hit(
                DmgType dmg_type,
                actor::Actor* actor,
                const P& from_pos,
                int dmg) override;

private:
        Color color_default() const override;

        void activate();

        bool m_is_activated;
};

}  // namespace terrain

#endif  // TERRAIN_MONOLITH_HPP
