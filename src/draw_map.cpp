// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "draw_map.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <vector>

#include "actor.hpp"
#include "actor_see.hpp"
#include "array2.hpp"
#include "colors.hpp"
#include "config.hpp"
#include "debug.hpp"
#include "game_time.hpp"
#include "gfx.hpp"
#include "inventory.hpp"
#include "io.hpp"
#include "item.hpp"
#include "item_data.hpp"
#include "map.hpp"
#include "misc.hpp"
#include "player_bon.hpp"
#include "pos.hpp"
#include "property_data.hpp"
#include "property_handler.hpp"
#include "rect.hpp"
#include "terrain.hpp"
#include "terrain_data.hpp"
#include "viewport.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
// Background color to draw in cases where one "object" is obscuring another,
// such as when an item is on top of a trap.
static Array2<std::optional<Color>> s_bg_color_obscured(0, 0);

static gfx::TileId get_player_tile_for_wpn_id(const item::Id item_id)
{
        switch (item_id) {
        case item::Id::axe:
                return gfx::TileId::player_axe;

        case item::Id::club:
                return gfx::TileId::player_club;

        case item::Id::dagger:
        case item::Id::shadow_dagger:
                return gfx::TileId::player_dagger;

        case item::Id::electric_gun:
                return gfx::TileId::player_electric_gun;

        case item::Id::flagellant_whip:
                return gfx::TileId::player_flagellant_whip;

        case item::Id::hammer:
                return gfx::TileId::player_hammer;

        case item::Id::hatchet:
                return gfx::TileId::player_hatchet;

        case item::Id::machete:
                return gfx::TileId::player_machete;

        case item::Id::tommy_gun:
                return gfx::TileId::player_tommy_gun;

        case item::Id::morphic_blaster:
                return gfx::TileId::player_morphic_blaster;

        case item::Id::pharaoh_staff:
                return gfx::TileId::player_pharaoh_staff;

        case item::Id::pistol:
        case item::Id::revolver:
                return gfx::TileId::player_pistol;

        case item::Id::pitchfork:
                return gfx::TileId::player_pitchfork;

        case item::Id::pump_shotgun:
                return gfx::TileId::player_pump_shotgun;

        case item::Id::rifle:
                return gfx::TileId::player_rifle;

        case item::Id::sawed_off:
                return gfx::TileId::player_sawed_off;

        case item::Id::sledgehammer:
                return gfx::TileId::player_sledgehammer;

        case item::Id::spear:
                return gfx::TileId::player_spear;

        case item::Id::spike_gun:
                return gfx::TileId::player_spike_gun;

        case item::Id::spiked_mace:
                return gfx::TileId::player_spiked_mace;

        case item::Id::armor_asb_suit:
        case item::Id::armor_flak_jacket:
        case item::Id::armor_iron_suit:
        case item::Id::armor_leather_jacket:
        case item::Id::armor_heavy_coat:
        case item::Id::armor_mi_go:
        case item::Id::astral_opium:
        case item::Id::bone_charm:
        case item::Id::clockwork:
        case item::Id::device_blaster:
        case item::Id::device_force_field:
        case item::Id::device_rejuvenator:
        case item::Id::device_sentry_drone:
        case item::Id::device_translocator:
        case item::Id::drum_of_bullets:
        case item::Id::dynamite:
        case item::Id::flare:
        case item::Id::fluctuating_material:
        case item::Id::gas_mask:
        case item::Id::holy_symbol:
        case item::Id::horn_of_banishment:
        case item::Id::horn_of_malice:
        case item::Id::intr_putrid_spit:
        case item::Id::intr_strange_color_touch:
        case item::Id::intr_bite:
        case item::Id::intr_claw:
        case item::Id::intr_dust_engulf:
        case item::Id::intr_earth_breath:
        case item::Id::intr_energy_engulf:
        case item::Id::intr_fire_breath:
        case item::Id::intr_fire_engulf:
        case item::Id::intr_ghost_touch:
        case item::Id::intr_headbutt:
        case item::Id::intr_kick:
        case item::Id::intr_lightning_breath:
        case item::Id::intr_maul:
        case item::Id::intr_mind_leech_sting:
        case item::Id::intr_net_throw:
        case item::Id::intr_punch:
        case item::Id::intr_punch_knockback:
        case item::Id::intr_pus_spew:
        case item::Id::intr_raven_peck:
        case item::Id::intr_snake_venom_spit:
        case item::Id::intr_spear_thrust:
        case item::Id::intr_spores:
        case item::Id::intr_sting:
        case item::Id::intr_strangle:
        case item::Id::intr_strike:
        case item::Id::intr_vampiric_bite:
        case item::Id::intr_water_breath:
        case item::Id::intr_web_bola:
        case item::Id::iron_spike:
        case item::Id::lantern:
        case item::Id::medical_bag:
        case item::Id::molotov:
        case item::Id::necronomicon:
        case item::Id::onyx_drop:
        case item::Id::orb_of_life:
        case item::Id::pistol_mag:
        case item::Id::player_ghoul_claw:
        case item::Id::player_kick:
        case item::Id::player_punch:
        case item::Id::player_stomp:
        case item::Id::potion_blindness:
        case item::Id::potion_conf:
        case item::Id::potion_curing:
        case item::Id::potion_descent:
        case item::Id::potion_fortitude:
        case item::Id::potion_insight:
        case item::Id::potion_paralyze:
        case item::Id::potion_poison:
        case item::Id::potion_r_elec:
        case item::Id::potion_r_fire:
        case item::Id::potion_spirit:
        case item::Id::potion_vitality:
        case item::Id::refl_talisman:
        case item::Id::resurrect_talisman:
        case item::Id::revolver_bullet:
        case item::Id::rifle_bullet:
        case item::Id::rock:
        case item::Id::rod_displacement:
        case item::Id::rod_cloud_minds:
        case item::Id::rod_deafening:
        case item::Id::rod_door_creation:
        case item::Id::rod_mist:
        case item::Id::rod_opening:
        case item::Id::rod_shockwave:
        case item::Id::rod_unbinding:
        case item::Id::rod_mi_go_hypno:
        case item::Id::scroll_aura_of_decay:
        case item::Id::scroll_aza_gaze:
        case item::Id::scroll_bless:
        case item::Id::scroll_blood_temper:
        case item::Id::scroll_cataclysm:
        case item::Id::scroll_control_object:
        case item::Id::scroll_crimson_passage:
        case item::Id::scroll_darkbolt:
        case item::Id::scroll_enfeeble:
        case item::Id::scroll_erudition:
        case item::Id::scroll_haste:
        case item::Id::scroll_heal:
        case item::Id::scroll_invis:
        case item::Id::scroll_light:
        case item::Id::scroll_pestilence:
        case item::Id::scroll_premonition:
        case item::Id::scroll_resistance:
        case item::Id::scroll_sacrifice_life:
        case item::Id::scroll_see_invis:
        case item::Id::scroll_slow:
        case item::Id::scroll_spectral_wpns:
        case item::Id::scroll_spell_shield:
        case item::Id::scroll_telep:
        case item::Id::scroll_terrify:
        case item::Id::scroll_thorns:
        case item::Id::scroll_transmut:
        case item::Id::shotgun_shell:
        case item::Id::smoke_grenade:
        case item::Id::tele_ctrl_talisman:
        case item::Id::thr_knife:
        case item::Id::torture_collar:
        case item::Id::trap_dart:
        case item::Id::trap_dart_poison:
        case item::Id::trap_spear:
        case item::Id::trap_spear_poison:
        case item::Id::trapezohedron:
        case item::Id::witch_eye:
        case item::Id::zombie_dust:
        case item::Id::END:
                break;
        }

        ASSERT(false);

        return gfx::TileId::player_unarmed;
}

static gfx::TileId get_ghoul_tile_for_wpn_id(const item::Id item_id)
{
        switch (item_id) {
        case item::Id::axe:
                return gfx::TileId::player_ghoul_axe;

        case item::Id::club:
                return gfx::TileId::player_ghoul_club;

        case item::Id::dagger:
        case item::Id::shadow_dagger:
                return gfx::TileId::player_ghoul_dagger;

        case item::Id::electric_gun:
                return gfx::TileId::player_ghoul_electric_gun;

        case item::Id::hammer:
                return gfx::TileId::player_ghoul_hammer;

        case item::Id::hatchet:
                return gfx::TileId::player_ghoul_hatchet;

        case item::Id::machete:
                return gfx::TileId::player_ghoul_machete;

        case item::Id::tommy_gun:
                return gfx::TileId::player_ghoul_tommy_gun;

        case item::Id::morphic_blaster:
                return gfx::TileId::player_ghoul_morphic_blaster;

        case item::Id::pharaoh_staff:
                return gfx::TileId::player_ghoul_pharaoh_staff;

        case item::Id::pistol:
        case item::Id::revolver:
                return gfx::TileId::player_ghoul_pistol;

        case item::Id::pitchfork:
                return gfx::TileId::player_ghoul_pitchfork;

        case item::Id::pump_shotgun:
                return gfx::TileId::player_ghoul_pump_shotgun;

        case item::Id::rifle:
                return gfx::TileId::player_ghoul_rifle;

        case item::Id::sawed_off:
                return gfx::TileId::player_ghoul_sawed_off;

        case item::Id::sledgehammer:
                return gfx::TileId::player_ghoul_sledgehammer;

        case item::Id::spear:
                return gfx::TileId::player_ghoul_spear;

        case item::Id::spike_gun:
                return gfx::TileId::player_ghoul_spike_gun;

        case item::Id::spiked_mace:
                return gfx::TileId::player_ghoul_spiked_mace;

        case item::Id::armor_asb_suit:
        case item::Id::armor_flak_jacket:
        case item::Id::armor_iron_suit:
        case item::Id::armor_leather_jacket:
        case item::Id::armor_heavy_coat:
        case item::Id::armor_mi_go:
        case item::Id::astral_opium:
        case item::Id::bone_charm:
        case item::Id::clockwork:
        case item::Id::device_blaster:
        case item::Id::device_force_field:
        case item::Id::device_rejuvenator:
        case item::Id::device_sentry_drone:
        case item::Id::device_translocator:
        case item::Id::drum_of_bullets:
        case item::Id::dynamite:
        case item::Id::flagellant_whip:
        case item::Id::flare:
        case item::Id::fluctuating_material:
        case item::Id::gas_mask:
        case item::Id::holy_symbol:
        case item::Id::horn_of_banishment:
        case item::Id::horn_of_malice:
        case item::Id::intr_putrid_spit:
        case item::Id::intr_strange_color_touch:
        case item::Id::intr_bite:
        case item::Id::intr_claw:
        case item::Id::intr_dust_engulf:
        case item::Id::intr_earth_breath:
        case item::Id::intr_energy_engulf:
        case item::Id::intr_fire_breath:
        case item::Id::intr_fire_engulf:
        case item::Id::intr_ghost_touch:
        case item::Id::intr_headbutt:
        case item::Id::intr_kick:
        case item::Id::intr_lightning_breath:
        case item::Id::intr_maul:
        case item::Id::intr_mind_leech_sting:
        case item::Id::intr_net_throw:
        case item::Id::intr_punch:
        case item::Id::intr_punch_knockback:
        case item::Id::intr_pus_spew:
        case item::Id::intr_raven_peck:
        case item::Id::intr_snake_venom_spit:
        case item::Id::intr_spear_thrust:
        case item::Id::intr_spores:
        case item::Id::intr_sting:
        case item::Id::intr_strangle:
        case item::Id::intr_strike:
        case item::Id::intr_vampiric_bite:
        case item::Id::intr_water_breath:
        case item::Id::intr_web_bola:
        case item::Id::iron_spike:
        case item::Id::lantern:
        case item::Id::medical_bag:
        case item::Id::molotov:
        case item::Id::necronomicon:
        case item::Id::onyx_drop:
        case item::Id::orb_of_life:
        case item::Id::pistol_mag:
        case item::Id::player_ghoul_claw:
        case item::Id::player_kick:
        case item::Id::player_punch:
        case item::Id::player_stomp:
        case item::Id::potion_blindness:
        case item::Id::potion_conf:
        case item::Id::potion_curing:
        case item::Id::potion_descent:
        case item::Id::potion_fortitude:
        case item::Id::potion_insight:
        case item::Id::potion_paralyze:
        case item::Id::potion_poison:
        case item::Id::potion_r_elec:
        case item::Id::potion_r_fire:
        case item::Id::potion_spirit:
        case item::Id::potion_vitality:
        case item::Id::refl_talisman:
        case item::Id::resurrect_talisman:
        case item::Id::revolver_bullet:
        case item::Id::rifle_bullet:
        case item::Id::rock:
        case item::Id::rod_displacement:
        case item::Id::rod_cloud_minds:
        case item::Id::rod_deafening:
        case item::Id::rod_door_creation:
        case item::Id::rod_mist:
        case item::Id::rod_opening:
        case item::Id::rod_shockwave:
        case item::Id::rod_unbinding:
        case item::Id::rod_mi_go_hypno:
        case item::Id::scroll_aura_of_decay:
        case item::Id::scroll_aza_gaze:
        case item::Id::scroll_bless:
        case item::Id::scroll_blood_temper:
        case item::Id::scroll_cataclysm:
        case item::Id::scroll_control_object:
        case item::Id::scroll_crimson_passage:
        case item::Id::scroll_darkbolt:
        case item::Id::scroll_enfeeble:
        case item::Id::scroll_erudition:
        case item::Id::scroll_haste:
        case item::Id::scroll_heal:
        case item::Id::scroll_invis:
        case item::Id::scroll_light:
        case item::Id::scroll_pestilence:
        case item::Id::scroll_premonition:
        case item::Id::scroll_resistance:
        case item::Id::scroll_sacrifice_life:
        case item::Id::scroll_see_invis:
        case item::Id::scroll_slow:
        case item::Id::scroll_spectral_wpns:
        case item::Id::scroll_spell_shield:
        case item::Id::scroll_telep:
        case item::Id::scroll_terrify:
        case item::Id::scroll_thorns:
        case item::Id::scroll_transmut:
        case item::Id::shotgun_shell:
        case item::Id::smoke_grenade:
        case item::Id::tele_ctrl_talisman:
        case item::Id::thr_knife:
        case item::Id::torture_collar:
        case item::Id::trap_dart:
        case item::Id::trap_dart_poison:
        case item::Id::trap_spear:
        case item::Id::trap_spear_poison:
        case item::Id::trapezohedron:
        case item::Id::witch_eye:
        case item::Id::zombie_dust:
        case item::Id::END:
                break;
        }

        ASSERT(false);

        return gfx::TileId::player_unarmed;
}

static gfx::TileId get_player_tile()
{
        const item::Item* const wpn = map::g_player->m_inv.item_in_slot(SlotId::wpn);

        if (player_bon::is_bg(Bg::ghoul)) {
                if (wpn) {
                        return get_ghoul_tile_for_wpn_id(wpn->id());
                }
                else {
                        return gfx::TileId::ghoul;
                }
        }
        else {
                if (wpn) {
                        return get_player_tile_for_wpn_id(wpn->id());
                }
                else {
                        return gfx::TileId::player_unarmed;
                }
        }
}

static void set_bg_color_obscured_terrain(
        const terrain::Terrain* const terrain,
        const size_t pos_idx)
{
        std::optional<Color>& value = s_bg_color_obscured.at(pos_idx);

        switch (terrain->id()) {
        case terrain::Id::liquid: {
                value = terrain->color_default();
        } break;

        case terrain::Id::chains: {
                value = terrain->color_default();
        } break;

        case terrain::Id::trap: {
                if (!terrain->is_hidden()) {
                        if (config::use_trap_color_when_obscured()) {
                                value = terrain->color_default();
                        }
                        else {
                                value = colors::yellow();
                        }
                }
        } break;

        default:
        {
        } break;
        }
}

static void set_bg_color_when_obscured_dead_actor(const actor::Actor& actor)
{
        const Color& color_default = colors::gray_brown();
        const Color& color_corpse_rises = colors::dark_teal();

        std::optional<Color>& color_here = s_bg_color_obscured.at(actor.m_pos);

        if (color_here && (color_here.value() == color_corpse_rises)) {
                // This position is colored as containing a corpse that will
                // rise again, do not change the color.
                return;
        }

        const bool is_corpse_rises =
                actor.m_properties.has(prop::Id::corpse_rises);

        const Color& new_color =
                is_corpse_rises
                ? color_corpse_rises
                : color_default;

        s_bg_color_obscured.at(actor.m_pos) = new_color;
}

static void use_bg_color_obscuring(Color& color, const P& p)
{
        color = s_bg_color_obscured.at(p).value_or(color);
}

static void adapt_color_for_lit_pos(Color& color)
{
        color.set_rgb(
                std::min(255, color.r() + 80),
                std::min(255, color.g() + 80),
                color.b());
}

static void adapt_color_for_dark_pos(Color& color)
{
        color = color.shaded(40);

        color.set_rgb(
                color.r(),
                color.g(),
                std::min(255, color.b() + 20));
}

static void adapt_color_for_light_level(Color& color, const size_t pos_idx)
{
        const terrain::Terrain* const t = map::g_terrain.at(pos_idx);

        if (!map::g_seen.at(pos_idx) ||
            !t->is_los_passable() ||
            (t->id() == terrain::Id::chasm)) {
                return;
        }

        if (map::g_light.at(pos_idx)) {
                adapt_color_for_lit_pos(color);
        }
        else if (map::g_dark.at(pos_idx)) {
                adapt_color_for_dark_pos(color);
        }
}

static void adapt_color_for_light_level(Color& color, const P& pos)
{
        adapt_color_for_light_level(color, map::g_terrain.pos_to_idx(pos));
}

static void adapt_color_for_distance_to_player(Color& color, const P& pos)
{
        if (map::g_light.at(pos)) {
                return;
        }

        const int dist = king_dist(pos, map::g_player->m_pos);

        const int k = std::clamp(dist - 1, 0, 4);

        if (k > 0) {
                color = color.shaded(k * 15);
        }
}

static void draw_terrains()
{
        const size_t nr_positions = map::nr_positions();

        for (size_t i = 0; i < nr_positions; ++i) {
                if (!map::g_seen.at(i)) {
                        continue;
                }

                const terrain::Terrain* const t = map::g_terrain.at(i);

                io::MapDrawObj draw_obj;

                draw_obj.pos = viewport::to_view_pos(t->pos());

                auto gore_tile = gfx::TileId::END;
                char gore_character = 0;

                if (t->can_have_gore()) {
                        gore_tile = t->gore_tile();
                        gore_character = t->gore_character();
                }

                if (gore_tile == gfx::TileId::END) {
                        draw_obj.tile = t->tile();
                        draw_obj.character = t->character();
                        draw_obj.color = t->color();
                }
                else {
                        draw_obj.tile = gore_tile;
                        draw_obj.character = gore_character;
                        draw_obj.color = colors::red();
                }

                const Color terrain_color_bg = t->color_bg();

                if (terrain_color_bg == colors::black()) {
                        // Set background color to use if this terrain is
                        // obscured by another object (e.g. an item on a trap).
                        set_bg_color_obscured_terrain(t, i);
                }
                else {
                        draw_obj.color_bg = terrain_color_bg;

                        s_bg_color_obscured.at(i) = terrain_color_bg;
                }

                if (config::text_mode_filled_walls()) {
                        if (draw_obj.character == '#') {
                                // Any terrain with the '#' symbol is converted
                                // to a filled rectangle instead.
                                //
                                // NOTE: No other (static) terrain except WALLS
                                // (or terrain imitating walls, such as hidden
                                // doors) must use the '#' character!
                                //
                                draw_obj.character = io::g_filled_rect_char;
                        }
                        else if (t->id() == terrain::Id::grate) {
                                // Since we are using filled rectangle as wall
                                // symbol, then we can use the '#' character for
                                // grates (looks good for this terrain, but
                                // obviously not if walls are also using this).
                                draw_obj.character = '#';
                        }
                }

                adapt_color_for_light_level(draw_obj.color, i);

                adapt_color_for_distance_to_player(draw_obj.color, t->pos());

                draw_obj.draw();
        }
}

static void draw_dead_actors()
{
        for (actor::Actor* actor : game_time::g_actors) {
                const P& p = actor->m_pos;

                if (!map::g_seen.at(p) || !actor::is_corpse(*actor)) {
                        continue;
                }

                io::MapDrawObj draw_obj;

                draw_obj.pos = viewport::to_view_pos(p);
                draw_obj.color = actor::color(*actor);
                draw_obj.tile = actor::tile(*actor);
                draw_obj.character = actor::character(*actor);

                adapt_color_for_light_level(draw_obj.color, p);

                use_bg_color_obscuring(draw_obj.color_bg, p);

                set_bg_color_when_obscured_dead_actor(*actor);

                draw_obj.draw();
        }
}

static void draw_items()
{
        const P map_dims = map::dims();

        for (int x = 0; x < map_dims.x; ++x) {
                for (int y = 0; y < map_dims.y; ++y) {
                        const P p(x, y);

                        if (!map::g_seen.at(p)) {
                                continue;
                        }

                        const item::Item* const item = map::g_items.at(p);

                        if (!item) {
                                continue;
                        }

                        io::MapDrawObj draw_obj;

                        draw_obj.pos = viewport::to_view_pos(p);
                        draw_obj.color = item->color();
                        draw_obj.tile = item->tile();
                        draw_obj.character = item->character();

                        adapt_color_for_light_level(draw_obj.color, p);

                        use_bg_color_obscuring(draw_obj.color_bg, p);

                        draw_obj.draw();
                }
        }
}

static void draw_mobiles()
{
        for (terrain::Terrain* mob : game_time::g_mobs) {
                const P& p = mob->pos();
                const gfx::TileId mob_tile = mob->tile();
                const char mob_character = mob->character();

                if (!map::g_seen.at(p) ||
                    (mob_tile == gfx::TileId::END) ||
                    (mob_character == 0) ||
                    (mob_character == ' ')) {
                        continue;
                }

                io::MapDrawObj draw_obj;

                draw_obj.pos = viewport::to_view_pos(p);
                draw_obj.color = mob->color();
                draw_obj.tile = mob_tile;
                draw_obj.character = mob_character;

                adapt_color_for_light_level(draw_obj.color, p);

                draw_obj.draw();
        }
}

static void draw_living_seen_monster(const actor::Actor& mon)
{
        const gfx::TileId mon_tile = actor::tile(mon);
        const char mon_char = actor::character(mon);

        if ((mon_tile == gfx::TileId::END) ||
            (mon_char == 0) ||
            (mon_char == ' ')) {
                return;
        }

        io::MapDrawObj draw_obj;

        draw_obj.pos = viewport::to_view_pos(mon.m_pos);
        draw_obj.color = actor::color(mon);
        draw_obj.tile = actor::tile(mon);
        draw_obj.character = actor::character(mon);

        if (map::g_player->is_leader_of(&mon)) {
                // The monster is player-friendly
                draw_obj.color_bg = colors::mon_allied();
        }
        else {
                // The monster is hostile
                if (actor::is_aware_of_player(mon)) {
                        // Monster is aware of player
                        if (mon.m_properties.has_temporary_negative_prop_mon()) {
                                draw_obj.color_bg = colors::mon_temp_property();
                        }
                        else if (mon.m_properties.has(prop::Id::frenzied)) {
                                draw_obj.color_bg = colors::red();
                        }
                }
                else {
                        // Monster is not aware of the player
                        draw_obj.color_bg = colors::mon_unaware();
                }
        }

        adapt_color_for_light_level(draw_obj.color, mon.m_pos);

        draw_obj.draw();
}

static void draw_living_hidden_monster(const actor::Actor& mon)
{
        if (!actor::is_player_aware_of_me(mon)) {
                return;
        }

        io::MapDrawObj draw_obj;

        const Color color_bg =
                map::g_player->is_leader_of(&mon)
                ? colors::mon_allied()
                : colors::dark_gray();

        draw_obj.pos = viewport::to_view_pos(mon.m_pos);
        draw_obj.tile = gfx::TileId::excl_mark;
        draw_obj.character = '!';
        draw_obj.color = colors::white();
        draw_obj.color_bg = color_bg;

        adapt_color_for_light_level(draw_obj.color, mon.m_pos);

        draw_obj.draw();
}

static void draw_living_monsters()
{
        for (actor::Actor* actor : game_time::g_actors) {
                if (actor::is_player(actor) || !actor::is_alive(*actor)) {
                        continue;
                }

                if (can_player_see_actor(*actor)) {
                        draw_living_seen_monster(*actor);
                }
                else {
                        draw_living_hidden_monster(*actor);
                }
        }
}

static io::MapDrawObj player_memory_to_draw_obj(
        const map::PlayerMemoryAppearance& d)
{
        io::MapDrawObj draw_obj;

        draw_obj.tile = d.tile;
        draw_obj.color = d.color;
        draw_obj.color_bg = colors::black();
        draw_obj.character = d.character;

        return draw_obj;
}

static void draw_unseen_cells_from_player_memory()
{
        R view = viewport::get_map_view_area();

        // Also draw a little bit outside the viewport - we allow showing a
        // fraction of tiles if the map panel size is not aligned with a whole
        // number of map tiles (for example 15.6 map tiles can be shown on the Y
        // axis). The drawing is clipped to the map panel, so pixels outside the
        // map panel will not be drawn.
        view.p1 = view.p1.with_offsets(2, 2);

        for (int x = view.p0.x; x < view.p1.x; ++x) {
                for (int y = view.p0.y; y < view.p1.y; ++y) {
                        const P p(x, y);

                        if (!map::is_pos_inside_map(p)) {
                                continue;
                        }

                        if (map::g_seen.at(p)) {
                                continue;
                        }

                        io::MapDrawObj draw_obj;

                        const map::PlayerMemoryTerrain& terrain_memory =
                                map::g_terrain_memory.at(p);

                        const map::PlayerMemoryItem& item_memory =
                                map::g_item_memory.at(p);

                        if (terrain_memory.appearance.is_defined()) {
                                draw_obj =
                                        player_memory_to_draw_obj(
                                                terrain_memory.appearance);
                        }

                        if (item_memory.appearance.is_defined()) {
                                draw_obj =
                                        player_memory_to_draw_obj(
                                                item_memory.appearance);
                        }

                        draw_obj.pos = viewport::to_view_pos(p);

                        draw_obj.color = draw_obj.color.shaded(80);

                        draw_obj.draw();
                }
        }
}

static void draw_player_character()
{
        const actor::Actor& player = *map::g_player;

        if (!viewport::is_in_view(player.m_pos)) {
                return;
        }

        const Color color = actor::color(player);
        const Color color_bg = colors::black();

        gfx::TileId tile = get_player_tile();

        io::MapDrawObj draw_obj;

        const char character = '@';

        draw_obj.pos = viewport::to_view_pos(player.m_pos);
        draw_obj.tile = tile;
        draw_obj.character = character;
        draw_obj.color = color;
        draw_obj.color_bg = color_bg;

        draw_obj.draw();
}

// -----------------------------------------------------------------------------
// draw_map
// -----------------------------------------------------------------------------
namespace draw_map
{
void run()
{
        // NOTE: This will also setup the whole array with default values.
        s_bg_color_obscured.resize(map::dims());

        draw_unseen_cells_from_player_memory();
        draw_terrains();
        draw_dead_actors();
        draw_items();
        draw_mobiles();
        draw_living_monsters();

        draw_player_character();

#ifndef NDEBUG
        io::g_allow_render = true;
#endif  // NDEBUG
}

}  // namespace draw_map
