// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef TERRAIN_MIRROR_HPP
#define TERRAIN_MIRROR_HPP

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
class Mirror : public Terrain
{
public:
        Mirror(const P& p, const TerrainData* data);
        Mirror() = delete;
        ~Mirror() = default;

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

        bool m_is_activated {false};
};

}  // namespace terrain

#endif  // TERRAIN_MIRROR_HPP
