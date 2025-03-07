// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef ATTACK_DATA_HPP
#define ATTACK_DATA_HPP

#include <optional>

#include "actor_data.hpp"
#include "item.hpp"
#include "item_weapon.hpp"
#include "pos.hpp"

namespace actor
{
class Actor;
}  // namespace actor

struct AttData
{
public:
        virtual ~AttData() = default;

        int hit_chance_tot {0};
        int dmg {0};
        actor::Actor* attacker {nullptr};
        actor::Actor* defender {nullptr};
        const item::Item* att_item {nullptr};
        bool is_intrinsic_att {false};

protected:
        AttData(
                actor::Actor* attacker,
                actor::Actor* defender,
                const item::Item& att_item);
};

struct MeleeAttData : public AttData
{
public:
        MeleeAttData(
                actor::Actor* attacker,
                actor::Actor& defender,
                const item::Wpn& wpn);

        ~MeleeAttData() = default;

        bool is_backstab {false};
        bool is_weak_attack {false};
};

// TODO: This class should not have anything to do with aim level.
struct RangedAttData : public AttData
{
public:
        RangedAttData(
                actor::Actor* attacker,
                const P& attacker_origin,
                const P& aim_pos,
                const P& current_pos,
                const item::Wpn& wpn,
                std::optional<actor::Size> aim_lvl_override = std::nullopt);

        ~RangedAttData() = default;

        P aim_pos {0, 0};
        actor::Size aim_lvl {(actor::Size)0};
        actor::Size defender_size {(actor::Size)0};
};

// TODO: This class should not have anything to do with aim level.
struct ThrowAttData : public AttData
{
public:
        ThrowAttData(
                actor::Actor* attacker,
                const P& attacker_origin,
                const P& aim_pos,
                const P& current_pos,
                const item::Item& item);

        actor::Size aim_lvl {(actor::Size)0};
        actor::Size defender_size {(actor::Size)0};
};

#endif  // ATTACK_DATA_HPP
