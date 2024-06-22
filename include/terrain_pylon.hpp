// =============================================================================
// Copyright 2011-2024 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef TERRAIN_PYLON_HPP
#define TERRAIN_PYLON_HPP

#include <memory>
#include <string>
#include <vector>

#include "colors.hpp"
#include "global.hpp"
#include "pos.hpp"
#include "terrain.hpp"
#include "terrain_data.hpp"

namespace actor
{
class Actor;
}  // namespace actor
template <typename T>
class Array2;

namespace terrain
{
class Pylon;

namespace pylon
{
enum class PylonId
{
        slow,
        haste,
        terrify,
        invis,
        knockback,
        teleport,
        END,
};

void init();

void save();
void load();

// -----------------------------------------------------------------------------
// Pylon implementation
// -----------------------------------------------------------------------------
class PylonImpl
{
public:
        PylonImpl(const PylonId id, const P& p) :
                m_id(id),
                m_pos(p) {}

        virtual ~PylonImpl() = default;

        PylonId id() const
        {
                return m_id;
        }

        virtual std::string name(Article article) const = 0;

        virtual void on_new_turn() = 0;

protected:
        virtual std::string effect_descr() const = 0;

        void reveal() const;

        std::vector<actor::Actor*> living_actors_reached() const;

        actor::Actor* rnd_reached_living_actor() const;

        PylonId m_id {};
        P m_pos;
};

class PylonTerrify : public PylonImpl
{
public:
        PylonTerrify(const PylonId id, const P& p) :
                PylonImpl(id, p) {}

        void on_new_turn() override;

        std::string name(Article article) const override;

protected:
        std::string effect_descr() const override;
};

class PylonInvis : public PylonImpl
{
public:
        PylonInvis(const PylonId id, const P& p) :
                PylonImpl(id, p) {}

        void on_new_turn() override;

        std::string name(Article article) const override;

protected:
        std::string effect_descr() const override;
};

class PylonSlow : public PylonImpl
{
public:
        PylonSlow(const PylonId id, const P& p) :
                PylonImpl(id, p) {}

        void on_new_turn() override;

        std::string name(Article article) const override;

protected:
        std::string effect_descr() const override;
};

class PylonHaste : public PylonImpl
{
public:
        PylonHaste(const PylonId id, const P& p) :
                PylonImpl(id, p) {}

        void on_new_turn() override;

        std::string name(Article article) const override;

protected:
        std::string effect_descr() const override;
};

class PylonKnockback : public PylonImpl
{
public:
        PylonKnockback(const PylonId id, const P& p) :
                PylonImpl(id, p) {}

        void on_new_turn() override;

        std::string name(Article article) const override;

protected:
        std::string effect_descr() const override;
};

class PylonTeleport : public PylonImpl
{
public:
        PylonTeleport(const PylonId id, const P& p) :
                PylonImpl(id, p) {}

        void on_new_turn() override;

        std::string name(Article article) const override;

protected:
        std::string effect_descr() const override;
};

}  // namespace pylon

// -----------------------------------------------------------------------------
// Pylon
// -----------------------------------------------------------------------------
class Pylon : public Terrain
{
public:
        Pylon(const P& p, const TerrainData* data);

        Pylon() = delete;

        ~Pylon() = default;

        void on_new_turn_hook() override;

        void activate();

        gfx::TileId tile() const override;

        Color color_default() const override;

        std::string name(Article article) const override;

        void add_light_hook(Array2<bool>& light) const override;

        void hit(
                DmgType dmg_type,
                actor::Actor* actor,
                const P& from_pos,
                int dmg) override;

private:
        pylon::PylonImpl* make_pylon_impl_from_id(pylon::PylonId id);

        std::unique_ptr<pylon::PylonImpl> m_pylon_impl;
};

}  // namespace terrain

#endif  // TERRAIN_PYLON_HPP
