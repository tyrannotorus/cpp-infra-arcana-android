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
#include "rect.hpp"
#include "state.hpp"

namespace popup
{
// Tappable confirm/cancel button areas of the most recently drawn popup
// (gui cells; negative coordinates when not present)
R ok_button_area();

R cancel_button_area();

enum class AddToMsgHistory
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

        // NOTE: A "(x)" style key prefix in a choice string is purely
        // stylistic - entries are engaged by tapping (or swipe + confirm),
        // never by letter keys, so duplicate letters are fine.
        Popup& setup_menu_mode(
                const std::vector<std::string>& choices,
                int* menu_choice_result);

        Popup& setup_number_query_mode(
                const query::QueryNumberConfig& config,
                int* number_result);

        Popup& set_sfx(audio::SfxId sfx);

        // The pointed-to flag is set to true if the popup is closed via
        // escape (the [ x ] control / back button) instead of confirmed
        Popup& set_cancelled_result(bool* cancelled_result);

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

        // If set, receives true when the popup is closed via escape (the
        // [ x ] control / back button) rather than confirmed
        bool* m_cancelled_result {nullptr};
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

        bool try_tap(const P& logical_px) override;

private:
        friend class Popup;

        void on_start_specific() override;

        std::vector<std::string> m_menu_choices {};
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

        std::string calc_input_str_number_only() const;

        // Updates the displayed input string, including blinking underscore.
        void update_input_str();

        int calc_max_nr_digits() const;

        query::QueryNumberConfig m_config {};

        int* m_number_result {nullptr};
        std::string m_input_str {};
        bool m_has_player_entered_value {false};
        bool m_is_empty_nr {false};
};

}  // namespace popup

#endif  // POPUP_HPP
