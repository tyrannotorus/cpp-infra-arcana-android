// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "map_mode_gui.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <string>
#include <vector>

#include "actor.hpp"
#include "actor_player_state.hpp"
#include "actor_see.hpp"
#include "colors.hpp"
#include "config.hpp"
#include "draw_box.hpp"
#include "game.hpp"
#include "game_time.hpp"
#include "global.hpp"
#include "inventory.hpp"
#include "io.hpp"
#include "io_display.hpp"
#include "item.hpp"
#include "item_data.hpp"
#include "item_device.hpp"
#include "item_misc.hpp"
#include "item_weapon.hpp"
#include "map.hpp"
#include "panel.hpp"
#include "player_bon.hpp"
#include "property_handler.hpp"
#include "rect.hpp"
#include "room.hpp"
#include "scrollbar.hpp"
#include "text_format.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static Color label_color()
{
        return colors::dark_sepia();
}

static Color info_color()
{
        return colors::text();
}

static std::string make_wpn_stats_str(const item::Item& wpn)
{
        const ItemNameAttackInfo att_inf =
                (wpn.data().main_attack_mode == AttackMode::thrown)
                ? ItemNameAttackInfo::melee
                : ItemNameAttackInfo::main_attack_mode;

        const std::string plus_str = wpn.plus_str(att_inf);
        const std::string dmg_str = wpn.dmg_str(att_inf, ItemNameDmg::average);
        const std::string hit_mod_str = wpn.hit_mod_str(att_inf, AbbrevItemAttackInfo::yes);

        const std::string inf_str = wpn.name_info_str();

        std::string wpn_str;

        if (!plus_str.empty()) {
                text_format::append_with_space(wpn_str, "(" + plus_str + ")");
        }

        text_format::append_with_space(wpn_str, dmg_str);
        text_format::append_with_space(wpn_str, hit_mod_str);
        text_format::append_with_space(wpn_str, inf_str);

        return wpn_str;
}

static void draw_wielded_wpn(const int y, const Panel panel)
{
        const std::string label = "Wpn";

        io::draw_text(
                label,
                panel,
                {0, y},
                label_color(),
                io::DrawBg::no);

        const item::Item* item =
                map::g_player->m_inv.item_in_slot(SlotId::wpn);

        if (!item) {
                item = &map::g_player->unarmed_wpn();
        }

        const std::string wpn_str = make_wpn_stats_str(*item);

        Color color = info_color();

        if (item->data().ranged.is_ranged_wpn &&
            !item->data().ranged.has_infinite_ammo &&
            (item->data().ranged.max_ammo > 0)) {
                const auto* const wpn = static_cast<const item::Wpn*>(item);

                if (wpn->m_ammo_loaded == 0) {
                        color = colors::yellow();
                }
        }

        io::draw_text_right(
                wpn_str,
                panel,
                {panels::w(panel) - 1, y},
                color,
                io::DrawBg::no);
}

static void draw_alt_wpn(const int y, const Panel panel)
{
        const std::string label = "Alt";

        io::draw_text(
                label,
                panel,
                {0, y},
                label_color(),
                io::DrawBg::no);

        const item::Item* item =
                map::g_player->m_inv.item_in_slot(SlotId::wpn_alt);

        if (!item) {
                item = &map::g_player->unarmed_wpn();
        }

        const std::string wpn_str = make_wpn_stats_str(*item);

        Color color = info_color();

        if (item->data().ranged.is_ranged_wpn &&
            !item->data().ranged.has_infinite_ammo &&
            (item->data().ranged.max_ammo > 0)) {
                const auto* const wpn = static_cast<const item::Wpn*>(item);

                if (wpn->m_ammo_loaded == 0) {
                        color = colors::yellow();
                }
        }

        io::draw_text_right(
                wpn_str,
                panel,
                {panels::w(panel) - 1, y},
                color.shaded(50),
                io::DrawBg::no);
}

static void draw_hp(const int y, const Panel panel)
{
        const int hp = map::g_player->m_hp;
        const int max_hp = actor::max_hp(*map::g_player);
        const int hp_pct = (hp * 100) / max_hp;

        io::draw_text(
                "Health",
                panel,
                {0, y},
                label_color(),
                io::DrawBg::no);

        const std::string hp_str = std::to_string(hp);
        const std::string max_hp_str = std::to_string(max_hp);

        const int hp_str_w = (int)hp_str.length();
        const int max_hp_str_w = (int)max_hp_str.length();
        const int tot_w = hp_str_w + 1 + max_hp_str_w;
        const int hp_x = panels::w(panel) - tot_w;

        const int tint_pct = std::min(100 - hp_pct, 60);

        io::draw_text(
                hp_str,
                panel,
                {hp_x, y},
                colors::light_red().tinted(tint_pct),
                io::DrawBg::no);

        io::draw_text(
                "/" + max_hp_str,
                panel,
                {hp_x + hp_str_w, y},
                colors::light_red(),
                io::DrawBg::no);
}

static void draw_sp(const int y, const Panel panel)
{
        const int sp = map::g_player->m_sp;
        const int max_sp = actor::max_sp(*map::g_player);
        const int sp_pct = (sp * 100) / max_sp;

        io::draw_text(
                "Spirit",
                panel,
                {0, y},
                label_color(),
                io::DrawBg::no);

        const std::string sp_str = std::to_string(sp);
        const std::string max_sp_str = std::to_string(max_sp);

        const int sp_str_w = (int)sp_str.length();
        const int max_sp_str_w = (int)max_sp_str.length();
        const int tot_w = sp_str_w + 1 + max_sp_str_w;
        const int sp_x = panels::w(panel) - tot_w;

        const int tint_pct = std::min(100 - sp_pct, 60);

        io::draw_text(
                sp_str,
                panel,
                {sp_x, y},
                colors::light_blue().tinted(tint_pct),
                io::DrawBg::no);

        io::draw_text(
                "/" + max_sp_str,
                panel,
                {sp_x + sp_str_w, y},
                colors::light_blue(),
                io::DrawBg::no);
}

static void draw_exorcist_fervor(const int y, const Panel panel)
{
        const int fervor = actor::player_state::g_exorcist_fervor;
        const int max_fervor = actor::player_exorcist_max_fervor();
        const int fervor_pct_of_max_possible = (fervor * 100) / max_fervor;

        io::draw_text(
                "Fervor",
                panel,
                {0, y},
                label_color(),
                io::DrawBg::no);

        const std::string fervor_str = std::to_string(fervor);
        const std::string max_fervor_str = std::to_string(max_fervor);

        const int fervor_str_w = (int)fervor_str.length();
        const int max_fervor_str_w = (int)max_fervor_str.length();
        const int tot_w = fervor_str_w + 1 + max_fervor_str_w;
        const int fervor_x = panels::w(panel) - tot_w;

        const int tint_pct = std::min(100 - fervor_pct_of_max_possible, 60);

        io::draw_text(
                fervor_str,
                panel,
                {fervor_x, y},
                colors::red().tinted(tint_pct),
                io::DrawBg::no);

        io::draw_text(
                "/" + max_fervor_str,
                panel,
                {fervor_x + fervor_str_w, y},
                colors::red(),
                io::DrawBg::no);
}

static void draw_shock(const int y, const Panel panel)
{
        const int shock_pct = std::min(999, map::g_player->shock_tot());

        io::draw_text(
                "Shock",
                panel,
                {0, y},
                label_color(),
                io::DrawBg::no);

        const std::string shock_str = std::to_string(shock_pct) + "%";

        io::draw_text_right(
                shock_str,
                panel,
                {panels::w(panel) - 1, y},
                colors::magenta(),
                io::DrawBg::no);
}

static void draw_insanity(const int y, const Panel panel)
{
        const int ins_pct = map::g_player->insanity();

        io::draw_text(
                "Insanity",
                panel,
                {0, y},
                label_color(),
                io::DrawBg::no);

        const std::string ins_str = std::to_string(ins_pct) + "%";

        io::draw_text_right(
                ins_str,
                panel,
                {panels::w(panel) - 1, y},
                colors::magenta().shaded(40),
                io::DrawBg::no);
}

static void draw_weight(const int y, const Panel panel)
{
        const int weight_pct = map::g_player->enc_percent();

        io::draw_text(
                "Weight",
                panel,
                {0, y},
                label_color(),
                io::DrawBg::no);

        const std::string enc_str = std::to_string(weight_pct) + "%";

        Color color = info_color();

        if (weight_pct >= g_enc_immobile_lvl) {
                color = colors::light_red();
        }
        else if (weight_pct >= 100) {
                color = colors::orange();
        }
        else if ((weight_pct + g_weakened_carry_weight_penalty) >= 100) {
                color = colors::yellow().tinted(25);
        }

        io::draw_text_right(
                enc_str,
                panel,
                {panels::w(panel) - 1, y},
                color,
                io::DrawBg::no);
}

static void draw_turn_number(const int y, const Panel panel)
{
        io::draw_text(
                "Turn",
                panel,
                {0, y},
                label_color(),
                io::DrawBg::no);

        const int turn_nr = game_time::turn_nr();

        const std::string turn_nr_str = std::to_string(turn_nr);

        io::draw_text_right(
                turn_nr_str,
                panel,
                {panels::w(panel) - 1, y},
                info_color(),
                io::DrawBg::no);
}

static void draw_armor(const int y, const Panel panel)
{
        io::draw_text(
                "Armor",
                panel,
                {0, y},
                label_color(),
                io::DrawBg::no);

        const std::string armor_str = std::to_string(actor::armor_points(*map::g_player));

        io::draw_text_right(
                armor_str,
                panel,
                {panels::w(panel) - 1, y},
                info_color(),
                io::DrawBg::no);
}

static void draw_name(const int y, const Panel panel)
{
        io::draw_text_center(
                actor::name_the(*map::g_player),
                panel,
                {panels::w(panel) / 2, y},
                colors::light_sepia(),
                io::DrawBg::no);
}

static void draw_class(const int y, const Panel panel)
{
        std::string bg_title;

        const Bg bg = player_bon::bg();

        if (bg == Bg::occultist) {
                const OccultistDomain domain = player_bon::occultist_domain();

                bg_title = player_bon::occultist_profession_title(domain);
        }
        else {
                bg_title = player_bon::bg_title(bg);
        }

        io::draw_text_center(
                bg_title,
                panel,
                {panels::w(panel) / 2, y},
                colors::light_sepia(),
                io::DrawBg::no);
}

static void draw_char_lvl_and_xp(const int y, const Panel panel)
{
        const int clvl = game::clvl();

        const bool is_max_lvl = clvl >= g_player_max_clvl;

        const int xp_pct = std::clamp(game::xp_pct(), 0, 100);

        io::draw_text(
                "Level",
                panel,
                {0, y},
                label_color(),
                io::DrawBg::no);

        const std::string clvl_str = std::to_string(clvl);

        std::string xp_str;

        if (!is_max_lvl) {
                xp_str = " (" + std::to_string(xp_pct) + "%)";
        }

        const int clvl_w = (int)clvl_str.length();
        const int xp_w = (int)xp_str.length();
        const int tot_w = clvl_w + xp_w;
        const int clvl_x = panels::w(panel) - tot_w;

        io::draw_text(
                clvl_str,
                panel,
                {clvl_x, y},
                info_color(),
                io::DrawBg::no);

        if (!is_max_lvl) {
                io::draw_text(
                        xp_str,
                        panel,
                        {clvl_x + clvl_w, y},
                        colors::green().tinted(100 - xp_pct),
                        io::DrawBg::no);
        }
}

static void draw_dlvl(const int y, const Panel panel)
{
        io::draw_text(
                "Depth",
                panel,
                {0, y},
                label_color(),
                io::DrawBg::no);

        const int dlvl = (int)map::g_dlvl;

        const std::string dlvl_str = std::to_string(dlvl);

        const int max_dlvl = g_dlvl_last;
        const int dlvl_pct = std::clamp((dlvl * 100) / max_dlvl, 0, 100);
        const int shade_pct = (dlvl_pct * 5) / 8;

        io::draw_text_right(
                dlvl_str,
                panel,
                {panels::w(panel) - 1, y},
                info_color().shaded(shade_pct),
                io::DrawBg::no);
}

static void draw_lantern(const int y, const Panel panel)
{
        io::draw_text(
                "Lantern",
                panel,
                {0, y},
                label_color(),
                io::DrawBg::no);

        const item::Item* const item = map::g_player->m_inv.item_in_backpack(item::Id::lantern);

        Color color = info_color();

        std::string lantern_str = "None";

        if (item) {
                const auto* const lantern = static_cast<const item::Lantern*>(item);

                if (lantern->is_activated()) {
                        color = colors::yellow();
                }

                lantern_str = std::to_string(lantern->nr_turns_left());
        }

        io::draw_text_right(
                lantern_str,
                panel,
                {panels::w(panel) - 1, y},
                color,
                io::DrawBg::no);
}

static void draw_med_suppl(const int y, const Panel panel)
{
        io::draw_text(
                "Med Suppl",
                panel,
                {0, y},
                label_color(),
                io::DrawBg::no);

        std::string suppl_str = "-";

        const item::Item* const item =
                map::g_player->m_inv
                        .item_in_backpack(item::Id::medical_bag);

        if (item) {
                const auto* const medical_bag = static_cast<const item::MedicalBag*>(item);

                suppl_str = std::to_string(medical_bag->nr_supplies());
        }

        io::draw_text_right(
                suppl_str,
                panel,
                {panels::w(panel) - 1, y},
                info_color(),
                io::DrawBg::no);
}

static std::vector<ColoredString> property_lines()
{
        const prop::PropHandler& properties = map::g_player->m_properties;

        auto lines = properties.property_names_short();

        if (map::g_dark.at(map::g_player->m_pos)) {
                const ColoredString dark = {
                        "DARK AREA",
                        colors::gray()};

                lines.push_back(dark);
        }

        return lines;
}

#ifndef NDEBUG
static void draw_debug_pos(const int y, const Panel panel)
{
        std::string debug_str =
                std::to_string(map::g_player->m_pos.x) +
                "," +
                std::to_string(map::g_player->m_pos.y);

        const room::Room* const room = map::g_room_map.at(map::g_player->m_pos);

        if (room) {
                text_format::append_with_space(
                        debug_str,
                        room::room_type_to_str(room->m_type));
        }

        io::draw_text(
                debug_str,
                panel,
                {0, y},
                colors::yellow(),
                io::DrawBg::no);
}
#endif  // NDEBUG

// Draws one content row; empty for a blank row
using DrawRow = std::function<void(int y, Panel panel)>;

// Scroll over the content (see io::Display::side_content), its limit set
// when drawing
static float s_scroll_px = 0.0f;
static int s_max_scroll_px = 0;

static int scroll_px()
{
        return (int)std::lround(s_scroll_px);
}

// Whether the content continues above / below the window
static bool is_cut_at_top()
{
        return scroll_px() > 0;
}

static bool is_cut_at_bottom()
{
        return scroll_px() < s_max_scroll_px;
}

// Fills the frame around the window - the content shows through from below
static void cover_frame(const R& frame_px, const R& window_px)
{
        const Color& bg = colors::extra_dark_gray();

        io::set_display_for_panel(Panel::map_gui_stats_border);

        io::draw_rectangle_filled(
                {frame_px.p0, {frame_px.p1.x, window_px.p0.y - 1}},
                bg);

        io::draw_rectangle_filled(
                {{frame_px.p0.x, window_px.p1.y + 1}, frame_px.p1},
                bg);

        io::draw_rectangle_filled(
                {{frame_px.p0.x, window_px.p0.y},
                 {window_px.p0.x - 1, window_px.p1.y}},
                bg);

        io::draw_rectangle_filled(
                {{window_px.p1.x + 1, window_px.p0.y},
                 {frame_px.p1.x, window_px.p1.y}},
                bg);
}

// Over the content, at the window edges where it continues
static void draw_scroll_fades(const R& window_px)
{
        io::set_display_for_panel(Panel::map_gui_stats_border);

        scrollbar::draw_content_fades(
                window_px,
                scroll_px(),
                s_max_scroll_px,
                0,
                colors::extra_dark_gray());
}

// -----------------------------------------------------------------------------
// map_mode_gui
// -----------------------------------------------------------------------------
namespace map_mode_gui
{
void draw()
{
        const Panel panel = Panel::map_gui_stats;

        std::vector<DrawRow> rows;

        rows.emplace_back(draw_name);
        rows.emplace_back(draw_class);
        rows.emplace_back(draw_char_lvl_and_xp);
        rows.emplace_back(draw_dlvl);
        rows.emplace_back(draw_hp);
        rows.emplace_back(draw_sp);

        if (player_bon::is_bg(Bg::exorcist)) {
                rows.emplace_back(draw_exorcist_fervor);
        }

        rows.emplace_back(draw_shock);
        rows.emplace_back(draw_insanity);
        rows.emplace_back();
        rows.emplace_back(draw_wielded_wpn);
        rows.emplace_back(draw_alt_wpn);
        rows.emplace_back();
        rows.emplace_back(draw_lantern);
        rows.emplace_back(draw_med_suppl);
        rows.emplace_back(draw_armor);
        rows.emplace_back(draw_weight);
        rows.emplace_back();
        rows.emplace_back(draw_turn_number);
        rows.emplace_back();

#ifndef NDEBUG
        rows.emplace_back(draw_debug_pos);
        rows.emplace_back();
#endif  // NDEBUG

        for (const ColoredString& line : property_lines()) {
                rows.emplace_back(
                        [line](const int y, const Panel row_panel) {
                                io::draw_text(
                                        line.str,
                                        row_panel,
                                        {0, y},
                                        line.color,
                                        io::DrawBg::no,
                                        colors::black());
                        });
        }

        const R window_px = io::panel_logical_px_rect(panel);

        const int content_px_h = (int)rows.size() * config::gui_cell_px_h();

        s_max_scroll_px = std::max(0, content_px_h - window_px.h());

        s_scroll_px = std::clamp(s_scroll_px, 0.0f, (float)s_max_scroll_px);

        io::set_side_content_scroll_px(scroll_px());

        // The content, whole
        io::reserve_side_content_px_h(content_px_h);

        io::set_display_for_panel(panel);

        io::draw_rectangle_filled(
                {window_px.p0,
                 {window_px.p1.x,
                  window_px.p0.y +
                          std::max(content_px_h, window_px.h()) -
                          1}},
                colors::extra_dark_gray());

        for (size_t idx = 0; idx < rows.size(); ++idx) {
                if (rows[idx]) {
                        rows[idx]((int)idx, panel);
                }
        }

        // The frame over it
        cover_frame(
                io::panel_logical_px_rect(Panel::map_gui_stats_border),
                window_px);

        draw_box(
                panels::area(Panel::map_gui_stats_border),
                colors::dark_gray(),
                Panel::map_gui_stats_border);

        draw_scroll_fades(window_px);

        if (config::is_gj_mode()) {
                draw_text_right(
                        "GJ MODE ENABLED",
                        Panel::screen,
                        {panels::x1(Panel::screen), 0},
                        colors::yellow(),
                        io::DrawBg::yes);
        }
}

bool scroll_by_px(const float finger_dy_px)
{
        const bool was_cut_at_top = is_cut_at_top();
        const bool was_cut_at_bottom = is_cut_at_bottom();

        // The content follows the finger
        s_scroll_px =
                std::clamp(
                        s_scroll_px - finger_dy_px,
                        0.0f,
                        (float)s_max_scroll_px);

        io::set_side_content_scroll_px(scroll_px());

        return (
                (was_cut_at_top != is_cut_at_top()) ||
                (was_cut_at_bottom != is_cut_at_bottom()));
}

}  // namespace map_mode_gui
