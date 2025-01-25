// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef TERRAIN_MIRROR_HPP
#define TERRAIN_MIRROR_HPP

#include <string>

#include "colors.hpp"
#include "global.hpp"
#include "terrain.hpp"

namespace actor
{
class Actor;
}  // namespace actor

namespace terrain
{
struct TerrainData;
}  // namespace terrain

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

        Color color_default() const override;

        std::optional<map::MinimapAppearance> minimap_appearance() const override;

        void bump(actor::Actor& actor_bumping) override;

        void hit(
                DmgType dmg_type,
                actor::Actor* actor,
                const P& from_pos,
                int dmg) override;

        bool is_activated() const
        {
                return m_is_activated;
        }

private:
        bool m_is_activated {false};
};

}  // namespace terrain

#endif  // TERRAIN_MIRROR_HPP
