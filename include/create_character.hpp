// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef CREATE_CHARACTER_HPP
#define CREATE_CHARACTER_HPP

#include <string>
#include <utility>
#include <vector>

#include "browser.hpp"
#include "menu_descr_page.hpp"
#include "menu_page.hpp"
#include "player_bon.hpp"
#include "state.hpp"
#include "text_page.hpp"

enum class IsCharacterCreationTraitPick
{
        no,
        yes
};

class NewGameState : public State
{
public:
        void on_pushed() override;

        void on_resume() override;

        StateId id() const override
        {
                return StateId::new_game;
        }
};

// The character menus are standard two column description pages (list of
// picks, description of the marked pick), which also auto-replay recorded
// picks: stepping BACK during character creation ([ x ] / escape) resets
// the session and replays all but the last pick (picks are applied
// immediately when made, so "back" can only be implemented as a replay).
class CreateCharPageState : public MenuDescrPageState
{
public:
        void update() override;
};

class PickBgState : public CreateCharPageState
{
public:
        void on_start() override;

        void update() override;

        StateId id() const override
        {
                return StateId::pick_background;
        }

protected:
        std::string page_title() const override;

        std::vector<MenuPageEntry> page_entries() const override;

        int default_marked_idx() const override;

        void on_entry_selected(int idx) override;

        void on_cancelled() override;

        void draw_page_content() override;

private:
        std::vector<Bg> m_bgs {};
};

class PickOccultistState : public CreateCharPageState
{
public:
        void on_start() override;

        StateId id() const override
        {
                return StateId::pick_background_occultist;
        }

protected:
        std::string page_title() const override;

        std::vector<MenuPageEntry> page_entries() const override;

        void on_entry_selected(int idx) override;

        void on_cancelled() override;

        void draw_page_content() override;

private:
        std::vector<OccultistDomain> m_domains {};
};

class PickTraitState : public CreateCharPageState
{
public:
        PickTraitState(std::string title, IsCharacterCreationTraitPick is_char_creation) :
                m_title(std::move(title)),
                m_is_char_creation(is_char_creation)
        {
        }

        void on_start() override;

        void update() override;

        StateId id() const override
        {
                return StateId::pick_trait;
        }

protected:
        std::string page_title() const override;

        std::string page_hint() const override;

        std::vector<MenuPageEntry> page_entries() const override;

        void on_entry_selected(int idx) override;

        void on_cancelled() override;

        bool handle_custom_input(const io::InputData& input) override;

        Color entry_color(int idx, bool is_marked) const override;

        void draw_page_content() override;

private:
        // The pickable traits, followed by the currently unavailable ones
        // (prerequisites not met) - the unavailable traits are browsable
        // (shown in red, with their prerequisites in the description) but
        // cannot be selected
        Trait trait_at(int idx) const;

        bool is_idx_unavail(int idx) const;

        std::vector<Trait> m_traits_avail {};
        std::vector<Trait> m_traits_unavail {};

        std::string m_title;
        IsCharacterCreationTraitPick m_is_char_creation;
};

class RemoveTraitState : public CreateCharPageState
{
public:
        void on_start() override;

        void update() override;

        StateId id() const override
        {
                return StateId::remove_trait;
        }

protected:
        std::string page_title() const override;

        std::string page_hint() const override;

        std::vector<MenuPageEntry> page_entries() const override;

        void on_entry_selected(int idx) override;

        void on_cancelled() override;

        bool handle_custom_input(const io::InputData& input) override;

        void draw_page_content() override;

private:
        std::vector<Trait> m_traits_can_be_removed {};
};

class EnterNameState : public State
{
public:
        void on_start() override;

        void on_popped() override;

        void update() override;

        void draw() override;

        bool try_tap(const P& logical_px) override;

        StateId id() const override
        {
                return StateId::pick_name;
        }

private:
        std::string m_current_str {};
};

// The "The story so far..." intro page, as a character creation step of
// its own (after name entry, before the game starts) - so that its [ x ]
// control steps BACK to the name entry instead of dropping the player
// into the game.
class IntroStoryState : public TextPageState
{
public:
        void on_start() override;

        StateId id() const override
        {
                return StateId::intro_story;
        }

protected:
        std::string page_title() const override;

        std::string page_text() const override;

        void on_cancelled() override;
};

#endif  // CREATE_CHARACTER_HPP
