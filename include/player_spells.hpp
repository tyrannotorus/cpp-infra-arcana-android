// =============================================================================
// Copyright 2011-2021 Martin Törnqvist <m.tornq@gmail.com>
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

void learn_spell(SpellId id, Verbose verbose);

void unlearn_spell(SpellId id, Verbose verbose);

void incr_spell_skill(SpellId id, Verbose verbose);

void reduce_spell_skill(SpellId id, Verbose verbose);

SpellSkill spell_skill(SpellId id);

void set_spell_skill(SpellId id, SpellSkill val);

bool is_spell_learned(SpellId id);

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

private:
        MenuBrowser m_browser {};
};

#endif  // PLAYER_SPELLS_HPP
