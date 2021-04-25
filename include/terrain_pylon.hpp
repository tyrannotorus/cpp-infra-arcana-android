// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef TERRAIN_PYLON_HPP
#define TERRAIN_PYLON_HPP

#include <memory>

#include "terrain.hpp"

namespace terrain
{
class PylonImpl;

enum class PylonId
{
        slow,
        haste,
        terrify,
        invis,
        knockback,
        END,
};

// -----------------------------------------------------------------------------
// Pylon
// -----------------------------------------------------------------------------
class Pylon : public Terrain
{
public:
        Pylon(const P& p);

        Pylon() = delete;

        ~Pylon() = default;

        Id id() const override
        {
                return Id::pylon;
        }

        std::string name(Article article) const override;

        void add_light_hook(Array2<bool>& light) const override;

private:
        void on_hit(
                const DmgType dmg_type,
                actor::Actor* const actor,
                const P& from_pos,
                int dmg) override;

        PylonImpl* make_pylon_impl_from_id(PylonId id);

        void on_new_turn_hook() override;

        Color color_default() const override;

        std::unique_ptr<PylonImpl> m_pylon_impl;
};

// -----------------------------------------------------------------------------
// Pylon implementation
// -----------------------------------------------------------------------------
class PylonImpl
{
public:
        PylonImpl(P p, Pylon* pylon) :
                m_pos(p),
                m_pylon(pylon) {}

        virtual ~PylonImpl() = default;

        virtual void on_new_turn_activated() = 0;

protected:
        // void emit_trigger_snd() const;

        std::vector<actor::Actor*> living_actors_reached() const;

        actor::Actor* rnd_reached_living_actor() const;

        P m_pos;

        Pylon* const m_pylon;
};

class PylonTerrify : public PylonImpl
{
public:
        PylonTerrify(P p, Pylon* pylon) :
                PylonImpl(p, pylon) {}

        void on_new_turn_activated() override;
};

class PylonInvis : public PylonImpl
{
public:
        PylonInvis(P p, Pylon* pylon) :
                PylonImpl(p, pylon) {}

        void on_new_turn_activated() override;
};

class PylonSlow : public PylonImpl
{
public:
        PylonSlow(P p, Pylon* pylon) :
                PylonImpl(p, pylon) {}

        void on_new_turn_activated() override;
};

class PylonHaste : public PylonImpl
{
public:
        PylonHaste(P p, Pylon* pylon) :
                PylonImpl(p, pylon) {}

        void on_new_turn_activated() override;
};

class PylonKnockback : public PylonImpl
{
public:
        PylonKnockback(P p, Pylon* pylon) :
                PylonImpl(p, pylon) {}

        void on_new_turn_activated() override;
};

}  // namespace terrain

#endif  // TERRAIN_PYLON_HPP
