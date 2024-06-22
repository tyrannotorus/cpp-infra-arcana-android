// =============================================================================
// Copyright 2011-2023 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "study_inscription.hpp"

#include <cstddef>
#include <ostream>
#include <string>
#include <vector>

#include "actor.hpp"
#include "audio.hpp"
#include "audio_data.hpp"
#include "debug.hpp"
#include "game.hpp"
#include "global.hpp"
#include "inventory.hpp"
#include "item.hpp"
#include "item_data.hpp"
#include "item_potion.hpp"
#include "item_scroll.hpp"
#include "map.hpp"
#include "messages.hpp"
#include "msg_log.hpp"
#include "player_spells.hpp"
#include "random.hpp"
#include "spells.hpp"
#include "state.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static std::vector<SpellId> get_all_forgotten_spells()
{
        std::vector<SpellId> result;

        result.reserve((size_t)SpellId::END);

        for (int i = 0; i < (int)SpellId::END; ++i) {
                const auto id = (SpellId)i;

                if (player_spells::is_spell_forgotten(id)) {
                        result.push_back(id);
                }
        }

        return result;
}

static void recall_random_spell(const std::vector<SpellId>& spell_ids)
{
        TRACE_FUNC_BEGIN;

        if (spell_ids.empty()) {
                ASSERT(false);

                return;
        }

        msg_log::more_prompt();

        player_spells::recall_spell(rnd::element(spell_ids));

        TRACE_FUNC_END;
}

static std::vector<const item::Item*> get_all_unknown_items()
{
        std::vector<const item::Item*> result;

        for (const item::Item* const item : map::g_player->m_inv.m_backpack) {
                const item::ItemData& d = item->data();

                if (d.is_identified) {
                        continue;
                }

                if ((d.type == ItemType::potion) && (!d.is_alignment_known)) {
                        result.push_back(item);
                }
                else if ((d.type == ItemType::scroll) && (!d.is_spell_domain_known)) {
                        result.push_back(item);
                }
        }

        return result;
}

static void reveal_random_item(const std::vector<const item::Item*>& items)
{
        TRACE_FUNC_BEGIN;

        if (items.empty()) {
                ASSERT(false);

                return;
        }

        const item::Item* const item = rnd::element(items);

        ASSERT(!item->data().is_identified);

        TRACE << "Revealing info about '" << item->name(ItemNameType::plain) << "'" << std::endl;

        msg_log::more_prompt();

        switch (item->data().type) {
        case ItemType::scroll: {
                TRACE << "Item type is 'scroll'" << std::endl;

                ASSERT(!item->data().is_spell_domain_known);

                static_cast<const scroll::Scroll*>(item)->reveal_domain();
        } break;

        case ItemType::potion: {
                TRACE << "Item type is 'potion'" << std::endl;

                ASSERT(!item->data().is_alignment_known);

                static_cast<const potion::Potion*>(item)->reveal_alignment();
        } break;

        default: {
                TRACE << "Bad item type: " << item->name(ItemNameType::plain) << std::endl;
                ASSERT(false);
        } break;
        }

        TRACE_FUNC_END;
}

// -----------------------------------------------------------------------------
// study_inscription
// -----------------------------------------------------------------------------
namespace study_inscription
{
void run()
{
        TRACE_FUNC_BEGIN;

        // Recall a forgotten spell and/or gain insight on a manuscript/potion.
        // Either none of the above happens, or one, or both. The chances for
        // recalling a spell or gaining insight are independent of each other.
        //
        // The player also always gains some XP.
        //

        // The chance to recall a spell is the same regardless of the number of
        // forgotten spells.
        const std::vector<SpellId> forgotten_spells = get_all_forgotten_spells();

        TRACE << "Found '" << forgotten_spells.size() << "' forgotten spells" << std::endl;

        const bool should_recall_spell = (!forgotten_spells.empty() && rnd::coin_toss());

        TRACE << "Should recall spell: '" << should_recall_spell << "'" << std::endl;

        // The chance to gain insight on an item increases the more items that
        // are carried.
        //
        // This ensures that there is no benefit to for example drop every item
        // except the one that you want to reveal (which would be very
        // tedious). Doing so would not increase the chance to reveal that
        // particular item at all. Carrying item A and B gives the exact same
        // chance to reveal A, as when only carrying A.
        //
        const std::vector<const item::Item*> unknown_items = get_all_unknown_items();

        TRACE << "Found '" << unknown_items.size() << "' unknown items" << std::endl;

        const int pct_chance_to_reveal = (int)unknown_items.size() * 10;

        const bool should_reveal_item = rnd::percent(pct_chance_to_reveal);

        TRACE << "Should reveal item: '" << should_reveal_item << "'" << std::endl;

        audio::play(audio::SfxId::study_inscription);

        const std::string msg =
                (should_recall_spell || should_reveal_item)
                ? messages::get_random_terrain_inscription_msg_reveal_knowledge()
                : messages::get_random_terrain_inscription_msg_generic();

        msg_log::add(msg);

        game::incr_player_xp(4);

        msg_log::more_prompt();

        if (should_recall_spell) {
                recall_random_spell(forgotten_spells);
        }

        if (should_reveal_item) {
                reveal_random_item(unknown_items);
        }

        if (states::current_state()->id() == StateId::pick_trait) {
                msg_log::more_prompt();
        }

        map::g_player->incr_shock(6.0, ShockSrc::misc);

        TRACE_FUNC_END;
}

}  // namespace study_inscription
