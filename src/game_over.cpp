// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "game_over.hpp"

#include <memory>
#include <string>
#include <utility>

#include "actor.hpp"
#include "game.hpp"
#include "game_over_summary.hpp"
#include "game_summary_data.hpp"
#include "highscore.hpp"
#include "init.hpp"
#include "map.hpp"
#include "paths.hpp"
#include "state.hpp"
#include "time.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static std::string calc_game_summary_file_path()
{
        const std::string game_summary_time_stamp =
                game::start_time().time_str(TimeType::second, false);

        const std::string game_summary_filename =
                map::g_player->name_a() +
                "_" +
                game_summary_time_stamp +
                ".txt";

        return paths::user_dir() + game_summary_filename;
}

// -----------------------------------------------------------------------------
// Public
// -----------------------------------------------------------------------------
void on_game_over()
{
        const std::string game_summary_file_path = calc_game_summary_file_path();

        HighscoreEntry highscore_entry =
                highscore::make_entry_from_current_session(
                        game_summary_file_path);

        highscore::append_entry_to_highscores_file(highscore_entry);

        // Collect data from the game session.
        const game_summary_data::GameSummaryData game_data =
                game_summary_data::collect();

        // Create the object that can present the data.
        auto game_over_summary = std::make_unique<GameOverSummary>();
        game_over_summary->setup(game_data);

        // Dump the lines to a file.
        game_over_summary->dump_to_file(game_summary_file_path);

        // From now on the session data is not needed anymore.
        init::cleanup_session();

        // Show game summary first, then highscores.
        states::push(std::make_unique<BrowseHighscore>());

        states::push(std::move(game_over_summary));
}
