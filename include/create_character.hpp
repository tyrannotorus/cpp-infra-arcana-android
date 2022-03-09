// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef CREATE_CHARACTER_HPP
#define CREATE_CHARACTER_HPP

#include <string>
#include <utility>
#include <vector>

#include "browser.hpp"
#include "player_bon.hpp"
#include "state.hpp"

enum class TraitScreenMode
{
        pick_new,
        view_unavail
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

class PickBgState : public State
{
public:
        void on_start() override;

        void update() override;

        void draw() override;

        StateId id() const override
        {
                return StateId::pick_background;
        }

private:
        MenuBrowser m_browser {};

        std::vector<Bg> m_bgs {};
};

class PickOccultistState : public State
{
public:
        void on_start() override;

        void update() override;

        void draw() override;

        StateId id() const override
        {
                return StateId::pick_background_occultist;
        }

private:
        MenuBrowser m_browser {};

        std::vector<OccultistDomain> m_domains {};
};

class PickTraitState : public State
{
public:
        PickTraitState(std::string title) :
                m_title(std::move(title))
        {
        }

        void on_start() override;

        void update() override;

        void draw() override;

        void on_window_resized() override;

        StateId id() const override
        {
                return StateId::pick_trait;
        }

private:
        void init_browsers();

        void draw_trait_menu_item(
                Trait trait,
                int y,
                bool is_marked,
                const MenuBrowser& browser) const;

        void draw_trait_prereq_info(
                const player_bon::TraitPrereqData& prereq_data,
                int x,
                int y) const;

        MenuBrowser m_browser_traits_avail {};
        MenuBrowser m_browser_traits_unavail {};

        std::vector<Trait> m_traits_avail {};
        std::vector<Trait> m_traits_unavail {};

        TraitScreenMode m_screen_mode {TraitScreenMode::pick_new};

        std::string m_title;
};

class RemoveTraitState : public State
{
public:
        void on_start() override;

        void update() override;

        void draw() override;

        void on_window_resized() override;

        StateId id() const override
        {
                return StateId::remove_trait;
        }

private:
        void init_browser();

        void draw_trait_menu_item(
                Trait trait,
                int y,
                bool is_marked,
                const MenuBrowser& browser) const;

        MenuBrowser m_browser {};

        std::vector<Trait> m_traits_can_be_removed {};
};

class EnterNameState : public State
{
public:
        void on_start() override;

        void update() override;

        void draw() override;

        StateId id() const override
        {
                return StateId::pick_name;
        }

private:
        std::string m_current_str {};
};

#endif  // CREATE_CHARACTER_HPP
