// =============================================================================
// Copyright 2011-2024 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef TERRAIN_TRAP_HPP
#define TERRAIN_TRAP_HPP

#include <string>

#include "colors.hpp"
#include "gfx.hpp"
#include "global.hpp"
#include "pos.hpp"
#include "random.hpp"
#include "terrain.hpp"

namespace terrain
{
struct TerrainData;
}  // namespace terrain

namespace actor
{
class Actor;
}  // namespace actor

namespace terrain
{
class TrapImpl;

enum class TrapId
{
        // Mechanical traps

        alarm,
        blinding,
        dart,
        deafening,
        fire,
        smoke,
        spear,
        web,

        END_MECHANICAL,

        // Magical traps

        // Negative
        curse,
        hp_sap,
        slow,
        spi_sap,
        summon,
        teleport,
        unlearn_spell,

        // Positive
        bless,
        haste,

        // Neutral
        alter_env,

        END,

        any
};

enum class TrapPlacementValid
{
        no,
        yes
};

class Trap : public Terrain
{
public:
        Trap(const P& pos, const TerrainData* const data) :
                Terrain(pos, data) {}

        Trap() = delete;

        ~Trap();

        bool try_init_type(TrapId id);

        void set_mimic_terrain(terrain::Terrain* const terrain)
        {
                m_mimic_terrain = terrain;
        }

        AllowAction pre_bump(actor::Actor& actor_bumping) override;

        void bump(actor::Actor& actor_bumping) override;

        gfx::TileId tile() const override;

        char character() const override;

        std::string name(Article article) const override;

        void hit(
                DmgType dmg_type,
                actor::Actor* actor,
                const P& from_pos,
                int dmg) override;

        bool disarm();

        // Quietly destroys the trap, and either places rubble, or replaces it
        // with the mimic terrain (depending on trap type)
        void destroy();

        void on_new_turn_hook() override;

        bool can_have_blood() const override
        {
                return m_is_hidden;
        }

        bool can_have_gore() const override
        {
                return m_is_hidden;
        }

        bool is_magical() const;

        void reveal(PrintRevealMsg print_reveal_msg) override;

        void on_revealed_from_searching() override;

        Material material() const override;

        TrapId type() const;

        const TrapImpl* trap_impl() const
        {
                return m_trap_impl;
        }

        void player_try_spot_hidden();

private:
        Color color_default() const override;

        DidTriggerTrap trigger_trap(actor::Actor* actor) override;

        void trigger_start(const actor::Actor* actor);

        Terrain* m_mimic_terrain {nullptr};
        int m_nr_turns_until_trigger {-1};

        // TODO: Should be a unique pointer
        TrapImpl* m_trap_impl {nullptr};
};

class TrapImpl
{
public:
        TrapImpl(P p, TrapId type, Trap* const base_trap) :
                m_pos(p),
                m_type(type),
                m_base_trap(base_trap) {}

        virtual ~TrapImpl() = default;

        TrapId type() const
        {
                return m_type;
        }

        // Called by the trap terrain after picking a random trap
        // implementation. This allows the specific implementation initialize
        // and to modify the map. The implementation may report that the
        // placement is impossible (e.g. no suitable wall to fire a dart from),
        // in which case another implementation will be picked at random.
        virtual TrapPlacementValid on_place()
        {
                return TrapPlacementValid::yes;
        }

        // NOTE: The trigger may happen several turns after the trap activates,
        // so it's pointless to provide actor triggering as a parameter here.
        virtual void trigger() = 0;

        virtual Range nr_turns_range_to_trigger() const = 0;

        virtual std::string name(Article article) const = 0;

        virtual Color color() const = 0;

        virtual gfx::TileId tile() const = 0;

        virtual char character() const
        {
                return '^';
        }

        virtual bool is_magical() const = 0;

        virtual bool is_disarmable() const
        {
                return true;
        }

        virtual std::string disarm_msg() const = 0;

protected:
        P m_pos;
        TrapId m_type;
        P m_dart_origin_pos {-1, -1};
        Trap* const m_base_trap;
};

class MechTrapImpl : public TrapImpl
{
public:
        MechTrapImpl(P pos, TrapId type, Trap* const base_trap) :
                TrapImpl(pos, type, base_trap) {}

        virtual ~MechTrapImpl() = default;

        gfx::TileId tile() const override
        {
                return gfx::TileId::trap_general;
        }

        bool is_magical() const override
        {
                return false;
        }

        std::string disarm_msg() const override
        {
                return "I disarm a trap.";
        }
};

class TrapDart : public MechTrapImpl
{
public:
        TrapDart(P pos, Trap* base_trap);

        std::string name(const Article article) const override
        {
                std::string name = (article == Article::a) ? "a" : "the";

                name += " dart trap";

                return name;
        }

        Color color() const override
        {
                return colors::white();
        }

        void trigger() override;

        TrapPlacementValid on_place() override;

        Range nr_turns_range_to_trigger() const override
        {
                return {0, 0};
        }

private:
        bool m_is_poisoned;

        P m_dart_origin;

        bool m_is_dart_origin_destroyed;
};

class TrapSpear : public MechTrapImpl
{
public:
        TrapSpear(P pos, Trap* base_trap);

        std::string name(const Article article) const override
        {
                std::string name = (article == Article::a) ? "a" : "the";

                name += " spear trap";

                return name;
        }

        Color color() const override
        {
                return colors::light_white();
        }

        void trigger() override;

        TrapPlacementValid on_place() override;

        Range nr_turns_range_to_trigger() const override
        {
                return {0, 0};
        }

private:
        bool m_is_poisoned;

        P m_spear_origin;

        bool m_is_spear_origin_destroyed;
};

class TrapBlindingFlash : public MechTrapImpl
{
public:
        TrapBlindingFlash(P pos, Trap* const base_trap) :
                MechTrapImpl(pos, TrapId::blinding, base_trap) {}

        std::string name(const Article article) const override
        {
                std::string name = (article == Article::a) ? "a" : "the";

                name += " blinding trap";

                return name;
        }

        Color color() const override
        {
                return colors::yellow();
        }

        void trigger() override;

        Range nr_turns_range_to_trigger() const override
        {
                return {0, 3};
        }
};

class TrapDeafening : public MechTrapImpl
{
public:
        TrapDeafening(P pos, Trap* const base_trap) :
                MechTrapImpl(pos, TrapId::deafening, base_trap) {}

        std::string name(const Article article) const override
        {
                std::string name = (article == Article::a) ? "a" : "the";

                name += " deafening trap";

                return name;
        }

        Color color() const override
        {
                return colors::violet();
        }

        void trigger() override;

        Range nr_turns_range_to_trigger() const override
        {
                return {0, 3};
        }
};

class TrapSmoke : public MechTrapImpl
{
public:
        TrapSmoke(P pos, Trap* const base_trap) :
                MechTrapImpl(pos, TrapId::smoke, base_trap) {}

        std::string name(const Article article) const override
        {
                std::string name = (article == Article::a) ? "a" : "the";

                name += " smoke trap";

                return name;
        }

        Color color() const override
        {
                return colors::gray();
        }

        void trigger() override;

        Range nr_turns_range_to_trigger() const override
        {
                return {0, 3};
        }
};

class TrapFire : public MechTrapImpl
{
public:
        TrapFire(P pos, Trap* const base_trap) :
                MechTrapImpl(pos, TrapId::fire, base_trap) {}

        std::string name(const Article article) const override
        {
                std::string name = (article == Article::a) ? "a" : "the";

                name += " fire trap";

                return name;
        }

        Color color() const override
        {
                return colors::light_red();
        }

        void trigger() override;

        Range nr_turns_range_to_trigger() const override
        {
                return {5, 6};
        }
};

class TrapAlarm : public MechTrapImpl
{
public:
        TrapAlarm(P pos, Trap* const base_trap) :
                MechTrapImpl(pos, TrapId::alarm, base_trap) {}

        std::string name(const Article article) const override
        {
                std::string name = (article == Article::a) ? "an" : "the";

                name += " alarm trap";

                return name;
        }

        Color color() const override
        {
                return colors::orange();
        }

        void trigger() override;

        Range nr_turns_range_to_trigger() const override
        {
                return {0, 0};
        }
};

class TrapWeb : public MechTrapImpl
{
public:
        TrapWeb(P pos, Trap* const base_trap) :
                MechTrapImpl(pos, TrapId::web, base_trap) {}

        void trigger() override;

        Color color() const override
        {
                return colors::light_white();
        }

        std::string name(const Article article) const override
        {
                std::string name = (article == Article::a) ? "a" : "the";

                name += " spider web";

                return name;
        }

        gfx::TileId tile() const override
        {
                return gfx::TileId::web;
        }

        char character() const override
        {
                return '*';
        }

        Range nr_turns_range_to_trigger() const override
        {
                return {0, 0};
        }

        bool is_magical() const override
        {
                return false;
        }

        std::string disarm_msg() const override
        {
                return "I tear down a spider web.";
        }
};

class MagicTrapImpl : public TrapImpl
{
public:
        MagicTrapImpl(P pos, TrapId type, Trap* const base_trap) :
                TrapImpl(pos, type, base_trap) {}

        virtual ~MagicTrapImpl() = default;

        std::string name(const Article article) const override
        {
                std::string name = (article == Article::a) ? "a" : "the";

                name += " strange shape on the floor";

                return name;
        }

        gfx::TileId tile() const override
        {
                return gfx::TileId::elder_sign;
        }

        char character() const override
        {
                return '*';
        }

        Color color() const override
        {
                return colors::light_red();
        }

        bool is_magical() const override
        {
                return true;
        }

        std::string disarm_msg() const override
        {
                return "I dispel a magic symbol.";
        }

        Range nr_turns_range_to_trigger() const override
        {
                return {0, 0};
        }
};

class TrapTeleport : public MagicTrapImpl
{
public:
        TrapTeleport(P pos, Trap* const base_trap) :
                MagicTrapImpl(pos, TrapId::teleport, base_trap) {}

        void trigger() override;
};

class TrapSummonMon : public MagicTrapImpl
{
public:
        TrapSummonMon(P pos, Trap* const base_trap) :
                MagicTrapImpl(pos, TrapId::summon, base_trap) {}

        void trigger() override;
};

class TrapHpSap : public MagicTrapImpl
{
public:
        TrapHpSap(P pos, Trap* const base_trap) :
                MagicTrapImpl(pos, TrapId::hp_sap, base_trap) {}

        void trigger() override;
};

class TrapSpiSap : public MagicTrapImpl
{
public:
        TrapSpiSap(P pos, Trap* const base_trap) :
                MagicTrapImpl(pos, TrapId::spi_sap, base_trap) {}

        void trigger() override;
};

class TrapSlow : public MagicTrapImpl
{
public:
        TrapSlow(P pos, Trap* const base_trap) :
                MagicTrapImpl(pos, TrapId::slow, base_trap) {}

        void trigger() override;
};

class TrapHaste : public MagicTrapImpl
{
public:
        TrapHaste(P pos, Trap* const base_trap) :
                MagicTrapImpl(pos, TrapId::slow, base_trap) {}

        void trigger() override;
};

class TrapAlterEnv : public MagicTrapImpl
{
public:
        TrapAlterEnv(P pos, Trap* const base_trap) :
                MagicTrapImpl(pos, TrapId::slow, base_trap) {}

        void trigger() override;
};

class TrapCurse : public MagicTrapImpl
{
public:
        TrapCurse(P pos, Trap* const base_trap) :
                MagicTrapImpl(pos, TrapId::curse, base_trap) {}

        void trigger() override;
};

class TrapBless : public MagicTrapImpl
{
public:
        TrapBless(P pos, Trap* const base_trap) :
                MagicTrapImpl(pos, TrapId::bless, base_trap) {}

        void trigger() override;
};

class TrapUnlearnSpell : public MagicTrapImpl
{
public:
        TrapUnlearnSpell(P pos, Trap* const base_trap) :
                MagicTrapImpl(pos, TrapId::unlearn_spell, base_trap) {}

        void trigger() override;

private:
        void try_unlearn_for_player() const;

        void try_unlearn_for_monster(actor::Actor& actor) const;
};

}  // namespace terrain

#endif  // TERRAIN_TRAP_HPP
