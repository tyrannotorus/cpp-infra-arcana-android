// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef PLAYER_SPELLS_HPP
#define PLAYER_SPELLS_HPP

#include <vector>

#include "action_list_state.hpp"
#include "global.hpp"
#include "spells.hpp"
#include "state.hpp"

namespace player_spells
{
void init();
void cleanup();

void save();
void load();

bool is_spell_learned(SpellId id);
void learn_spell(SpellId id, Verbose verbose);

// Silently removes a learned spell (no messages).
void remove_learned_spell(SpellId id);

bool is_spell_forgotten(SpellId id);
void forget_spell(SpellId id);
bool recall_spell(SpellId id);
bool recall_all_spells();

void incr_spell_skill(SpellId id, Verbose verbose);
SpellSkill spell_skill(SpellId id);
void set_spell_skill(SpellId id, SpellSkill val);

bool is_getting_altar_bonus();

}  // namespace player_spells

// What can be done with the marked spell. NOTE: This is a set of one -
// the screen exists to cast - but it goes through the same action pin
// machinery as the inventory and throw screens, so that a spell is never
// cast by a stray tap on a row (see ActionListState).
enum class SpellActionId
{
        cast
};

// The spell list: pick a spell, read what it does, cast it. Built like the
// inventory and throw screens (see ActionListState) - the same list, the
// same scrollable description beside it, the same [ pins ].
class BrowseSpell : public ActionListState
{
public:
        BrowseSpell() = default;

        void on_start() override;

        void draw() override;

        void update() override;

        StateId id() const override;

        void disable_allow_cast()
        {
                m_allow_cast = false;
        }

private:
        bool has_action_pins() const override
        {
                return m_allow_cast;
        }

        std::vector<ActionPin> marked_entry_actions() const override;

        void run_action(int action_id) override;

        // The marked spell, or nullptr when the list is empty
        Spell* marked_spell() const;

        void draw_spell_descr();

        bool m_allow_cast {true};

        // The spell whose description is shown - so that the description
        // scroll can be reset when another one becomes marked
        const Spell* m_viewed_spell {nullptr};
};

#endif  // PLAYER_SPELLS_HPP
