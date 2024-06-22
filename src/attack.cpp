// =============================================================================
// Copyright 2011-2024 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "attack.hpp"

#include <memory>
#include <string>

#include "actor.hpp"
#include "actor_see.hpp"
#include "attack_internal.hpp"
#include "common_text.hpp"
#include "game_time.hpp"
#include "item.hpp"
#include "item_att_property.hpp"
#include "item_weapon.hpp"
#include "map.hpp"
#include "msg_log.hpp"
#include "property.hpp"
#include "property_factory.hpp"
#include "property_handler.hpp"
#include "random.hpp"
#include "terrain_mob.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// attack
// -----------------------------------------------------------------------------
namespace attack
{
// TODO: It would be better to use absolute numbers, rather than relative to
// the weapon's max damage (if a weapon does 100 maximum damage, it would still
// be pretty catastrophic if it did 50 damage - but currently this would only
// count as a "small" hit). Perhaps use values relative to 'g_min_dmg_to_wound'
// (in global.hpp) as thresholds instead.
HitSize relative_hit_size(const int dmg, const int wpn_max_dmg)
{
        HitSize result = HitSize::small;

        if (wpn_max_dmg >= 4) {
                if (dmg > ((wpn_max_dmg * 5) / 6)) {
                        result = HitSize::hard;
                }
                else if (dmg > (wpn_max_dmg / 2)) {
                        result = HitSize::medium;
                }
        }

        return result;
}

std::string hit_size_punctuation_str(const HitSize hit_size)
{
        switch (hit_size) {
        case HitSize::small:
                return ".";

        case HitSize::medium:
                return "!";

        case HitSize::hard:
                return "!!!";
        }

        return "";
}

void try_apply_attack_property_on_actor(
        const ItemAttackProp& att_prop,
        actor::Actor& actor,
        const DmgType dmg_type)
{
        if (!rnd::percent(att_prop.pct_chance_to_apply)) {
                return;
        }

        const bool is_resisting_dmg = actor.m_properties.is_resisting_dmg(dmg_type, Verbose::no);

        if (!is_resisting_dmg) {
                prop::Prop* const prop_cpy = prop::make(att_prop.prop->id());

                const prop::PropDurationMode duration_mode = att_prop.prop->duration_mode();

                if (duration_mode == prop::PropDurationMode::specific) {
                        prop_cpy->set_duration(att_prop.prop->nr_turns_left());
                }
                else if (duration_mode == prop::PropDurationMode::indefinite) {
                        prop_cpy->set_indefinite();
                }

                actor.m_properties.apply(prop_cpy);
        }
}

BinaryAnswer query_player_attack_mon_with_ranged_wpn(
        const item::Wpn& wpn,
        const actor::Actor& mon)
{
        const std::string wpn_name = wpn.name(ItemNameType::a);

        const bool can_see_mon = can_player_see_actor(mon);

        const std::string mon_name = can_see_mon ? mon.name_the() : "it";

        const std::string msg =
                "Attack " +
                mon_name +
                " with " +
                wpn_name +
                "? " +
                common_text::g_yes_or_no_hint;

        msg_log::add(
                msg,
                colors::light_white(),
                MsgInterruptPlayer::no,
                MorePromptOnMsg::no,
                CopyToMsgHistory::no);

        const BinaryAnswer answer = query::yes_or_no();

        return answer;
}

}  // namespace attack
