// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
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
#include "state.hpp"

namespace popup
{
enum class AddToMsgHistory
{
        no,
        yes
};

class PopupState;

// Frontend for the PopupState class. To display a popup, the client code shall
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

        Popup& set_menu(
                const std::vector<std::string>& choices,
                const std::vector<char>& menu_keys,
                int* menu_choice_result);

        Popup& set_sfx(audio::SfxId sfx);

private:
        std::unique_ptr<PopupState> m_popup_state {};
};

class PopupState : public State
{
public:
        PopupState(AddToMsgHistory add_to_msg_history);

        void on_start() override;

        void draw() override;

        bool draw_overlayed() const override
        {
                return false;
        }

        void on_window_resized() override;

        void update() override;

        StateId id() const override;

private:
        friend class Popup;

        void draw_msg_popup() const;

        void draw_menu_popup() const;

        std::string m_title {};
        std::string m_msg {};
        audio::SfxId m_sfx {audio::SfxId::END};
        std::vector<std::string> m_menu_choices {};
        std::vector<char> m_menu_keys {};
        int* m_menu_choice_result {nullptr};
        MenuBrowser m_browser {};

        const AddToMsgHistory m_add_to_msg_history;
};

}  // namespace popup

#endif  // POPUP_HPP
