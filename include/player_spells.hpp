// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef PLAYER_SPELLS_HPP
#define PLAYER_SPELLS_HPP

#include "browser.hpp"
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

class BrowseSpell : public State
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
        MenuBrowser m_browser {};

        bool m_allow_cast {true};
};

#endif  // PLAYER_SPELLS_HPP
