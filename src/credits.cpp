// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "credits.hpp"

#include <string>
#include <vector>

#include "colors.hpp"
#include "io.hpp"
#include "panel.hpp"
#include "pos.hpp"
#include "version.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static const std::vector<ColoredString>& credits_lines()
{
        // NOTE: When the Android port moves to github, add its repository
        // link under the Werewolf Camp entry
        static const std::vector<ColoredString> lines = {
                {"Infra Arcana", colors::title()},
                // NOTE: The bitmap fonts are ASCII only (no "o umlaut")
                {"created by Martin Tornqvist", colors::text()},
                {"gitlab.com/martin-tornqvist/ia", colors::gray()},
                {"", colors::black()},
                {"Android port", colors::title()},
                {"by Werewolf Camp", colors::text()},
                {"werewolf.camp", colors::gray()},
        };

        return lines;
}

// -----------------------------------------------------------------------------
// Credits state
// -----------------------------------------------------------------------------
std::string CreditsState::title() const
{
        return "Credits";
}

InfoScreenType CreditsState::type() const
{
        return InfoScreenType::single_screen;
}

int CreditsState::get_lines_total() const
{
        return (int)credits_lines().size();
}

void CreditsState::draw()
{
        draw_interface();

        const auto& lines = credits_lines();

        const int screen_center_x = panels::center_x(Panel::screen);

        int y = panels::center_y(Panel::screen) - ((int)lines.size() / 2);

        for (const auto& line : lines) {
                if (!line.str.empty()) {
                        io::draw_text_center(
                                line.str,
                                Panel::screen,
                                {screen_center_x, y},
                                line.color);
                }

                ++y;
        }

        // Copyright and license footer, embedded in the bottom border
        // (moved here from the title screen)
        io::draw_text_center(
                std::string(
                        " " +
                        version_info::g_copyright_str +
                        ", " +
                        version_info::g_license_str +
                        " "),
                Panel::screen,
                {screen_center_x, panels::screen_box_area().p1.y},
                colors::gray());
}
