// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef POPUP_HPP
#define POPUP_HPP

#include <memory>
#include <string>
#include <vector>

#include "audio_data.hpp"
#include "browser.hpp"
#include "query.hpp"
#include "state.hpp"

namespace popup
{
enum class AddToMsgHistory
{
        no,
        yes
};

enum class MenuModeShowCancelHint
{
        no,
        yes
};

class PopupState;

// Frontend for the PopupState class. To display a popup, the calling code shall
// create an instance of the Popup class, configure it, then finally call the
// run function. This causes the Popup class to immediately (when run is called)
// execute the PopupState in the state handler until the popup is closed.
//
// NOTE: The above method is the *only* way to configure the PopupState class
// (since the PopupState data is private with no setters, only the Popup friend
// class can configure them), and therefore the only way to display popups is
// via the Popup class. So this should make things pretty fail safe.
class Popup
{
public:
        Popup(AddToMsgHistory add_to_msg_history);

        ~Popup();

        void run();

        Popup& set_title(const std::string& title);

        Popup& set_msg(const std::string& msg);

        Popup& setup_menu_mode(
                const std::vector<std::string>& choices,
                const std::vector<char>& menu_keys,
                MenuModeShowCancelHint show_cancel_hint,
                int* menu_choice_result);

        Popup& setup_number_query_mode(
                const query::QueryNumberConfig& config,
                int* number_result);

        Popup& set_sfx(audio::SfxId sfx);

private:
        void copy_common_config(const PopupState* from, PopupState* to) const;

        std::unique_ptr<PopupState> m_popup_state {};

        const AddToMsgHistory m_add_to_msg_history;
};

class PopupState : public State
{
public:
        PopupState();

        void on_start() final;

        bool draw_overlayed() const override
        {
                return false;
        }

        void on_window_resized() override;

        StateId id() const final;

protected:
        friend class Popup;

        virtual void on_start_specific() = 0;

        void draw_msg_popup() const;

        void draw_menu_popup() const;

        std::string m_title {};
        std::string m_msg {};
        audio::SfxId m_sfx {audio::SfxId::END};
        AddToMsgHistory m_add_to_msg_history;
};

class MsgPopupState : public PopupState
{
public:
        MsgPopupState();

        void draw() override;

        void update() override;

private:
        void on_start_specific() override;
};

class MenuPopupState : public PopupState
{
public:
        MenuPopupState();

        void draw() override;

        void update() override;

private:
        friend class Popup;

        void on_start_specific() override;

        std::vector<std::string> m_menu_choices {};
        std::vector<char> m_menu_keys {};
        MenuModeShowCancelHint m_show_cancel_hint {MenuModeShowCancelHint::no};
        int* m_menu_choice_result {nullptr};
        MenuBrowser m_browser {};
};

class NumberQueryPopupState : public PopupState
{
public:
        NumberQueryPopupState();

        void draw() override;

        void update() override;

private:
        friend class Popup;

        void on_start_specific() override;

        void update_input_str();

        int calc_max_nr_digits() const;

        query::QueryNumberConfig m_config {};

        int* m_number_result {nullptr};
        std::string m_input_str {};
        bool m_has_player_entered_value {false};
};

}  // namespace popup

#endif  // POPUP_HPP
