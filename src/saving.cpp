// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "saving.hpp"

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <iterator>
#include <vector>

#include "actor_data.hpp"
#include "actor_player.hpp"
#include "debug.hpp"
#include "game.hpp"
#include "game_time.hpp"
#include "hints.hpp"
#include "insanity.hpp"
#include "inventory.hpp"
#include "item_curse.hpp"
#include "item_data.hpp"
#include "item_potion.hpp"
#include "item_rod.hpp"
#include "item_scroll.hpp"
#include "map.hpp"
#include "map_templates.hpp"
#include "map_travel.hpp"
#include "misc.hpp"
#include "paths.hpp"
#include "player_bon.hpp"
#include "player_spells.hpp"
#include "smell.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
// Only used to verify that the put/get methods are not called at the wrong time
enum class SaveLoadState
{
        saving,
        loading,
        stopped
};

static SaveLoadState s_state;

static std::vector<std::string> s_lines;

static void save_modules()
{
        TRACE_FUNC_BEGIN;

        ASSERT(s_lines.empty());

        saving::put_str(map::g_player->name_a());

        game::save();
        scroll::save();
        potion::save();
        rod::save();
        item::save();
        item_curse::save();
        map::g_player->m_inv.save();
        map::g_player->save();
        insanity::save();
        player_bon::save();
        map_travel::save();
        map::save();
        actor::save();
        game_time::save();
        player_spells::save();
        map_templates::save();
        hints::save();
        smell::save();

        TRACE_FUNC_END;
}

static void load_modules()
{
        TRACE_FUNC_BEGIN;

        ASSERT(!s_lines.empty());

        const std::string player_name = saving::get_str();

        ASSERT(!player_name.empty());

        map::g_player->m_data->name_a = player_name;

        map::g_player->m_data->name_the = player_name;

        game::load();
        scroll::load();
        potion::load();
        rod::load();
        item::load();
        item_curse::load();
        map::g_player->m_inv.load();
        map::g_player->load();
        insanity::load();
        player_bon::load();
        map_travel::load();
        map::load();
        actor::load();
        game_time::load();
        player_spells::load();
        map_templates::load();
        hints::load();
        smell::load();

        TRACE_FUNC_END;
}

static void write_file()
{
        std::ofstream file;

        // Current file content is discarded
        file.open(paths::save_file_path(), std::ios::trunc);

        if (file.is_open())
        {
                for (size_t i = 0; i < s_lines.size(); ++i)
                {
                        file << s_lines[i];

                        if (i != s_lines.size() - 1)
                        {
                                file << std::endl;
                        }
                }

                file.close();
        }
}

static void read_file()
{
        std::ifstream file(paths::save_file_path());

        if (file.is_open())
        {
                std::string current_line;

                while (getline(file, current_line))
                {
                        s_lines.push_back(current_line);
                }

                file.close();
        }
        else
        {
                // Could not open save file
                ASSERT(false && "Failed to open save file");
        }
}

// -----------------------------------------------------------------------------
// saving
// -----------------------------------------------------------------------------
namespace saving
{
void init()
{
        s_lines.clear();

        s_state = SaveLoadState::stopped;
}

void save_game()
{
        ASSERT(s_state == SaveLoadState::stopped);
        ASSERT(s_lines.empty());

        s_state = SaveLoadState::saving;

        // Tell all modules to append to the save lines (via this modules store
        // functions)
        save_modules();

        s_state = SaveLoadState::stopped;

        // Write the save lines to the save file
        write_file();

        s_lines.clear();
}

void load_game()
{
        ASSERT(s_state == SaveLoadState::stopped);
        ASSERT(s_lines.empty());

        s_state = SaveLoadState::loading;

        // Read the save file to the save lines
        read_file();

        ASSERT(!s_lines.empty());

        // Tell all modules to set up their state from the save lines (via the
        // read functions of this module)
        load_modules();

        s_state = SaveLoadState::stopped;

        ASSERT(s_lines.empty());
}

void erase_save()
{
        s_lines.clear();

        // Write empty save file
        write_file();
}

bool is_save_available()
{
        std::ifstream file(paths::save_file_path());

        if (file.good())
        {
                const bool is_empty =
                        file.peek() == std::ifstream::traits_type::eof();

                file.close();

                return !is_empty;
        }
        else
        {
                // Failed to open file
                file.close();

                return false;
        }
}

bool is_loading()
{
        return s_state == SaveLoadState::loading;
}

void put_str(const std::string& str)
{
        ASSERT(s_state == SaveLoadState::saving);

        s_lines.push_back(str);
}

void put_int(const int v)
{
        put_str(std::to_string(v));
}

void put_bool(const bool v)
{
        const std::string str = v ? "T" : "F";

        put_str(str);
}

std::string get_str()
{
        ASSERT(s_state == SaveLoadState::loading);
        ASSERT(!s_lines.empty());

        auto str = s_lines.front();

        s_lines.erase(std::begin(s_lines));

        return str;
}

int get_int()
{
        return to_int(get_str());
}

bool get_bool()
{
        return get_str() == "T";
}

}  // namespace saving
