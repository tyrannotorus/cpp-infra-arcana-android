// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef TEXT_PAGE_HPP
#define TEXT_PAGE_HPP

#include <string>

#include "info_screen_state.hpp"
#include "rect.hpp"
#include "text.hpp"

// The standard fullscreen text page: a fullscreen border with the page title
// embedded top center and a hint line embedded bottom center, and a centered
// blurb of text framed by two horizontal divider rules (one above and one
// below the text). This is the page for text the player reads and then taps
// through - story beats (the intro story), descriptive pages (a monster
// description), and tutorial style pages. All such pages extend this class,
// so that every one of them renders and behaves through the same code.
//
// A blurb too long for the page is not centered: it starts at the top of the
// area between the rules and scrolls (the scrolling of any fullscreen text
// content, see InfoScreenState), with the rules staying at the edges of the
// area.
//
// Interaction: tapping anywhere continues, dragging the scrollbar scrolls,
// and the [ x ] border control (or the device back button) cancels. What
// continuing and cancelling do is up to the page - by default both simply
// leave it.
class TextPageState : public InfoScreenState
{
public:
        void on_start() override;

        void draw() final;

        void update() override;

        void on_window_resized() override;

protected:
        // --- Page content hooks ---
        virtual std::string page_title() const = 0;

        // The blurb. Formatting is supported (colors, "{_}" non-breaking
        // spaces, "\n" newlines - see the Text class). Called when the page
        // is started (and on a window resize), not every frame.
        virtual std::string page_text() const = 0;

        // Embedded in the bottom border row (empty = no footer). Defaults
        // to the standard "tap to continue" guidance.
        virtual std::string page_hint() const;

        // Rebuilds the drawn text from page_text(). Call when the content
        // has changed (it is built when the page starts, and on a resize).
        void rebuild_text();

        // Adds the title and the text to the message history, as the lines
        // they are drawn as. Story beats belong in the history the player
        // can scroll back through - call this from on_start (pages that are
        // just looked up, e.g. a monster description, do not).
        void add_text_to_msg_history();

private:
        // The gui cell rows available to the text (between the two divider
        // rules, one blank row clear of each)
        R content_gui_area() const;

        // --- InfoScreenState ---
        std::string title() const final;

        InfoScreenType type() const final
        {
                return InfoScreenType::scrolling;
        }

        int get_lines_total() const final
        {
                return m_nr_lines;
        }

        R content_px_rect() const final;

        // The text as it is drawn, compiled once (the inline color markup
        // must survive - one color per line would not be enough)
        Text m_text {};

        int m_nr_lines {0};

        // Width of the longest drawn line - the text block is centered on
        // this, and the divider rules are sized from it
        int m_block_w {0};
};

#endif  // TEXT_PAGE_HPP
