// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "spells.hpp"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <iterator>
#include <ostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "actor.hpp"
#include "actor_data.hpp"
#include "actor_death.hpp"
#include "actor_factory.hpp"
#include "actor_hit.hpp"
#include "actor_see.hpp"
#include "array2.hpp"
#include "audio.hpp"
#include "audio_data.hpp"
#include "colors.hpp"
#include "config.hpp"
#include "debug.hpp"
#include "direction.hpp"
#include "draw_blast.hpp"
#include "drop.hpp"
#include "explosion.hpp"
#include "flood.hpp"
#include "game_time.hpp"
#include "gfx.hpp"
#include "global.hpp"
#include "inventory.hpp"
#include "inventory_handling.hpp"
#include "io.hpp"
#include "item.hpp"
#include "item_data.hpp"
#include "item_factory.hpp"
#include "knockback.hpp"
#include "map.hpp"
#include "map_parsing.hpp"
#include "marker.hpp"
#include "misc.hpp"
#include "msg_log.hpp"
#include "pathfind.hpp"
#include "player_bon.hpp"
#include "player_spells.hpp"
#include "pos.hpp"
#include "property.hpp"
#include "property_data.hpp"
#include "property_factory.hpp"
#include "property_handler.hpp"
#include "rect.hpp"
#include "sound.hpp"
#include "state.hpp"
#include "teleport.hpp"
#include "terrain.hpp"
#include "terrain_data.hpp"
#include "terrain_door.hpp"
#include "terrain_factory.hpp"
#include "text_format.hpp"
#include "viewport.hpp"
#include "wpn_dmg.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static const std::unordered_map<std::string, SpellId> s_str_to_spell_id_map = {
        {"SPELL_AURA_OF_DECAY", SpellId::aura_of_decay},
        {"SPELL_SPECTRAL_WEAPONS", SpellId::spectral_weapons},
        {"SPELL_AZA_GAZE", SpellId::aza_gaze},
        {"SPELL_BLESS", SpellId::bless},
        {"SPELL_BURN", SpellId::burn},
        {"SPELL_FORCE_BOLT", SpellId::force_bolt},
        {"SPELL_DARKBOLT", SpellId::darkbolt},
        {"SPELL_DEAFEN", SpellId::deafen},
        {"SPELL_DISEASE", SpellId::disease},
        {"SPELL_PREMONITION", SpellId::premonition},
        {"SPELL_ERUDITION", SpellId::erudition},
        {"SPELL_ENFEEBLE", SpellId::enfeeble},
        {"SPELL_CURSE", SpellId::curse},
        {"SPELL_CLEANSING_FIRE", SpellId::cleansing_fire},
        {"SPELL_SANCTUARY", SpellId::sanctuary},
        {"SPELL_PURGE", SpellId::purge},
        {"SPELL_FRENZY", SpellId::frenzy},
        {"SPELL_HEAL", SpellId::heal},
        {"SPELL_IDENTIFY", SpellId::identify},
        {"SPELL_KNOCKBACK", SpellId::knockback},
        {"SPELL_LIGHT", SpellId::light},
        {"SPELL_CATACLYSM", SpellId::cataclysm},
        {"SPELL_MI_GO_HYPNO", SpellId::mi_go_hypno},
        {"SPELL_CONTROL_OBJECT", SpellId::control_object},
        {"SPELL_PESTILENCE", SpellId::pestilence},
        {"SPELL_RES", SpellId::resistance},
        {"SPELL_SEE_INVIS", SpellId::see_invis},
        {"SPELL_SLOW", SpellId::slow},
        {"SPELL_HASTE", SpellId::haste},
        {"SPELL_SPELL_SHIELD", SpellId::spell_shield},
        {"SPELL_SUMMON", SpellId::summon},
        {"SPELL_SUMMON_TENTACLES", SpellId::summon_tentacles},
        {"SPELL_TELEPORT", SpellId::teleport},
        {"SPELL_TERRIFY", SpellId::terrify},
        {"SPELL_TRANSMUT", SpellId::transmut}};

static const std::unordered_map<std::string, SpellSkill> s_str_to_spell_skill_map = {
        {"SPELLSKILL_BASIC", SpellSkill::basic},
        {"SPELLSKILL_EXPERT", SpellSkill::expert},
        {"SPELLSKILL_MASTER", SpellSkill::master},
        {"SPELLSKILL_TRANSCENDENT", SpellSkill::transcendent}};

static const std::unordered_map<SpellDomain, ShockSrc> s_spell_domain_to_shock_type_map = {
        {SpellDomain::blood, ShockSrc::cast_intr_spell_blood},
        {SpellDomain::clairvoyance, ShockSrc::cast_intr_spell_clairvoyance},
        {SpellDomain::enchantment, ShockSrc::cast_intr_spell_enchantment},
        {SpellDomain::invocation, ShockSrc::cast_intr_spell_invocation},
        {SpellDomain::transmutation, ShockSrc::cast_intr_spell_transmutation},
        // NOTE: Not all spells belong to a domain:
        {SpellDomain::END, ShockSrc::cast_intr_spell_general}};

static const std::string s_spell_resist_msg = "The spell is resisted!";

static const std::string s_spell_reflect_msg = "The spell is reflected!";

static DmgType s_bolt_dmg_type = DmgType::blunt;

namespace spell_side_effects
{
struct Context
{
        Context(actor::Actor& spell_caster,
                const std::vector<P>& caster_nearby_positions) :
                caster(spell_caster),
                nearby_positions(caster_nearby_positions) {}

        actor::Actor& caster;
        const std::vector<P>& nearby_positions;
};

static void print_side_effect_trigger_message()
{
        msg_log::add("An unexpected effect was induced by the spell.");
}

static void spawn_monsters(const Context& context)
{
        TRACE_FUNC_BEGIN;

        const auto p = rnd::element(context.nearby_positions);

        const std::string id = "MON_TENTACLE_CLUSTER";

        auto spawned = actor::spawn(p, {id}, map::rect());

        bool printed_msg = false;

        for (auto* const actor : spawned.monsters) {
                if (!printed_msg) {
                        print_side_effect_trigger_message();
                        printed_msg = true;
                }

                prop::Prop* const conflicted = prop::make(prop::Id::conflict);

                conflicted->set_indefinite();

                actor->m_properties.apply(
                        conflicted,
                        prop::PropSrc::intr,
                        false,
                        Verbose::no);

                prop::Prop* const waiting = prop::make(prop::Id::waiting);

                waiting->set_duration(2);

                actor->m_properties.apply(waiting);

                prop::Prop* const summoned = prop::make(prop::Id::summoned);

                summoned->set_duration(rnd::range(3, 20));

                actor->m_properties.apply(summoned);
        }

        map::update_vision();
        actor::make_player_aware_seen_monsters();

        TRACE_FUNC_END;
}

static void swap_wall_floor(const Context& context)
{
        TRACE_FUNC_BEGIN;

        print_side_effect_trigger_message();

        Array2<bool> blocked(map::dims());

        map_parsers::BlocksWalking(ParseActors::no)
                .run(blocked, blocked.rect());

        const std::vector<terrain::Id> free_terrains = {
                terrain::Id::door,
        };

        for (const P& p : blocked.rect().positions()) {
                const bool is_free_terrain =
                        map_parsers::IsAnyOfTerrains(free_terrains)
                                .run(p);

                if (is_free_terrain) {
                        blocked.at(p) = false;
                }
        }

        Array2<bool> has_actor(map::dims());

        for (auto* actor : game_time::g_actors) {
                if (actor->m_state != ActorState::destroyed) {
                        has_actor.at(actor->m_pos) = true;
                }
        }

        for (const auto& p : context.nearby_positions) {
                if (!map::is_pos_inside_outer_walls(p) ||
                    has_actor.at(p) ||
                    map::g_items.at(p) ||
                    !rnd::one_in(14)) {
                        continue;
                }

                const auto terrain_id = map::g_terrain.at(p)->id();

                if (terrain_id == terrain::Id::wall) {
                        blocked.at(p) = false;

                        if (map_parsers::is_map_connected(blocked)) {
                                map::update_terrain(
                                        terrain::make(terrain::Id::floor, p));
                        }
                        else {
                                // Map would not be connected
                                blocked.at(p) = true;
                        }
                }
                else if (terrain_id == terrain::Id::floor) {
                        blocked.at(p) = true;

                        if (map_parsers::is_map_connected(blocked)) {
                                map::update_terrain(
                                        terrain::make(terrain::Id::wall, p));
                        }
                        else {
                                // Map would not be connected
                                blocked.at(p) = false;
                        }
                }
        }

        TRACE_FUNC_END;
}  // swap_wall_floor

static void ignite_terrain(const Context& context)
{
        TRACE_FUNC_BEGIN;

        Array2<bool> has_actor(map::dims());

        for (auto* actor : game_time::g_actors) {
                if (actor->m_state != ActorState::destroyed) {
                        has_actor.at(actor->m_pos) = true;
                }
        }

        bool printed_msg = false;
        for (const auto& p : context.nearby_positions) {
                if (has_actor.at(p)) {
                        continue;
                }

                if (!rnd::one_in(14)) {
                        continue;
                }

                if (!printed_msg) {
                        print_side_effect_trigger_message();
                        printed_msg = true;
                }

                terrain::Terrain* const terrain = map::g_terrain.at(p);

                terrain->hit(DmgType::fire, nullptr);
        }

        TRACE_FUNC_END;
}

static void open_close_doors(const Context& context)
{
        TRACE_FUNC_BEGIN;

        // Open or close doors
        const bool should_open = (bool)rnd::coin_toss();

        Array2<bool> has_actor(map::dims());

        for (auto* actor : game_time::g_actors) {
                if (actor->m_state != ActorState::destroyed) {
                        has_actor.at(actor->m_pos) = true;
                }
        }

        bool printed_msg = false;
        for (const auto& p : context.nearby_positions) {
                if (has_actor.at(p) || map::g_items.at(p)) {
                        continue;
                }

                auto* const terrain = map::g_terrain.at(p);

                if (terrain->id() != terrain::Id::door) {
                        continue;
                }

                if (static_cast<terrain::Door*>(terrain)->type() ==
                    terrain::DoorType::metal) {
                        continue;
                }

                if (!printed_msg) {
                        print_side_effect_trigger_message();
                        printed_msg = true;
                }

                // NOTE: Metal doors are skipped, so it's OK to just run
                // open/close here
                if (should_open) {
                        terrain->open(nullptr);
                }
                else {
                        terrain->close(nullptr);
                }
        }

        TRACE_FUNC_END;
}

static void flay_human(const Context& context)
{
        TRACE_FUNC_BEGIN;

        auto actors = actor::seen_actors(context.caster);

        actors.push_back(&context.caster);

        rnd::shuffle(actors);

        actor::Actor* target_actor = nullptr;

        for (auto* const actor : actors) {
                const auto* const actor_data = actor->m_data;

                const auto& properties = actor->m_properties;

                // NOTE: The target may be the caster, if caster is a monster
                if (!actor::is_player(actor) &&
                    actor->is_alive() &&
                    actor_data->is_humanoid &&
                    !actor_data->is_undead &&
                    !actor_data->is_unique &&
                    !properties.has(prop::Id::ethereal) &&
                    !properties.has(prop::Id::possessed_by_zuul) &&
                    !properties.has(prop::Id::spawns_zombie_parts_on_destroyed)) {
                        target_actor = actor;

                        break;
                }
        }

        if (!target_actor) {
                return;
        }

        print_side_effect_trigger_message();

        if (actor::can_player_see_actor(*target_actor)) {
                const auto name =
                        text_format::first_to_upper(
                                target_actor->name_the());

                msg_log::add(name + " is suddenly flayed alive!");
        }

        actor::kill(
                *target_actor,
                IsDestroyed::yes,
                AllowGore::yes,
                AllowDropItems::yes);

        actor::spawn(
                target_actor->m_pos,
                {"MON_CRAWLING_INTESTINES"},
                map::rect());

        TRACE_FUNC_END;
}

static void create_water(const Context& context)
{
        TRACE_FUNC_BEGIN;

        bool printed_msg = false;

        for (const auto& p : context.nearby_positions) {
                if ((map::g_terrain.at(p)->id() != terrain::Id::floor) ||
                    !rnd::one_in(8)) {
                        continue;
                }

                if (!printed_msg) {
                        print_side_effect_trigger_message();
                        printed_msg = true;
                }

                auto* const liquid =
                        static_cast<terrain::Liquid*>(
                                terrain::make(
                                        terrain::Id::liquid,
                                        p));

                liquid->m_type = LiquidType::water;

                map::update_terrain(liquid);
        }

        TRACE_FUNC_END;
}

static void alter_env(const Context& context)
{
        TRACE_FUNC_BEGIN;

        print_side_effect_trigger_message();

        prop::run_alter_env_effect(context.caster.m_pos);

        TRACE_FUNC_END;
}

static void create_doors(const Context& context)
{
        TRACE_FUNC_BEGIN;

        const auto adj_door_checker = map_parsers::AnyAdjIsAnyOfTerrains(terrain::Id::door);
        const auto adj_floor_checker = map_parsers::AnyAdjIsAnyOfTerrains(terrain::Id::floor);

        bool printed_msg = false;
        for (const auto& p : context.nearby_positions) {
                const auto id = map::g_terrain.at(p)->id();

                if (!rnd::one_in(2) ||
                    (id != terrain::Id::wall) ||
                    adj_door_checker.run(p) ||
                    !adj_floor_checker.run(p)) {
                        continue;
                }

                if (!printed_msg) {
                        print_side_effect_trigger_message();
                        printed_msg = true;
                }

                auto* const mimic = terrain::make(terrain::Id::wall, p);

                auto* const door =
                        static_cast<terrain::Door*>(
                                terrain::make(
                                        terrain::Id::door,
                                        p));

                door->set_mimic_terrain(mimic);

                door->init_type_and_state(
                        terrain::DoorType::wood,
                        terrain::DoorSpawnState::closed);

                map::update_terrain(door);
        }

        TRACE_FUNC_END;
}

static void create_dark_void(const Context& context)
{
        TRACE_FUNC_BEGIN;

        std::vector<P> sorted_positions = context.nearby_positions;

        std::sort(
                std::begin(sorted_positions),
                std::end(sorted_positions),
                [context](const auto& p1, const auto& p2) {
                        const auto caster_p = context.caster.m_pos;

                        const int d1 = king_dist(p1, caster_p);
                        const int d2 = king_dist(p2, caster_p);

                        return d1 < d2;
                });

        Array2<bool> blocked(map::dims());

        map_parsers::BlocksWalking(ParseActors::no)
                .run(blocked, blocked.rect());

        const std::vector<terrain::Id> free_terrains = {
                terrain::Id::door,
        };

        for (const P& p : blocked.rect().positions()) {
                const bool is_free_terrain =
                        map_parsers::IsAnyOfTerrains(free_terrains)
                                .run(p);

                if (is_free_terrain) {
                        blocked.at(p) = false;
                }
        }

        print_side_effect_trigger_message();

        for (const auto& p : sorted_positions) {
                if (!map::is_pos_inside_outer_walls(p)) {
                        continue;
                }

                map::g_dark.at(p) = true;
                map::g_light.at(p) = false;

                if (map::g_terrain.at(p)->id() == terrain::Id::wall) {
                        blocked.at(p) = false;

                        if (map_parsers::is_map_connected(blocked)) {
                                map::update_terrain(
                                        terrain::make(terrain::Id::floor, p));
                        }
                        else {
                                blocked.at(p) = true;
                        }
                }
        }

        TRACE_FUNC_END;
}

static void push_statue(const Context& context)
{
        TRACE_FUNC_BEGIN;

        for (const auto& p : context.nearby_positions) {
                auto* const terrain = map::g_terrain.at(p);

                if (terrain->id() != terrain::Id::statue) {
                        continue;
                }

                print_side_effect_trigger_message();

                auto* const statue = static_cast<terrain::Statue*>(terrain);

                const auto direction =
                        dir_utils::dir(
                                rnd::element(
                                        dir_utils::g_dir_list));

                statue->topple(direction);

                break;
        }

        TRACE_FUNC_END;
}

using SpellSideEffect = std::function<void(const Context&)>;

WeightedItems<SpellSideEffect> s_spell_side_effects {
        {
                create_dark_void,
                create_doors,
                alter_env,
                create_water,
                flay_human,
                ignite_terrain,
                open_close_doors,
                push_statue,
                spawn_monsters,
                swap_wall_floor,
        },
        {
                10,  // create_dark_void
                15,  // create_doors
                30,  // create_trees
                30,  // create_water
                50,  // flay_human
                50,  // ignite_terrain
                90,  // open_close_doors
                90,  // push_statue
                50,  // spawn_monsters
                70,  // swap_wall_floor
        }};

}  // namespace spell_side_effects

static DidAction toggle_metal_door(const terrain::Terrain& door)
{
        // Find ANY lever for this door, and toggle it.
        //
        // NOTE: If there are more levers connected to this door, this is OK
        // since the lever will set its sibblings to the same position.
        //
        for (const auto& p : map::rect().positions()) {
                auto* const terrain = map::g_terrain.at(p);

                if (terrain->id() != terrain::Id::lever) {
                        continue;
                }

                auto* const lever = static_cast<terrain::Lever*>(terrain);

                if (!lever->is_linked_to(door)) {
                        continue;
                }

                lever->toggle();

                return DidAction::yes;
        }

        // Reaching this point means we didn't find a lever for the metal door,
        // which is not supposed to happen.
        ASSERT(false);

        return DidAction::no;
}

static std::string get_noise_descr(const bool is_noisy)
{
        std::string str =
                is_noisy
                ? "Casting this spell requires making sounds."
                : "This spell can be cast silently.";

        return str;
}

static std::string get_skill_descr(
        const SpellSkill skill,
        const SpellSrc source)
{
        std::string str =
                "The spell can be cast at " +
                spells::skill_to_str(skill) +
                " level";

        std::vector<std::string> bon_words;

        if (source == SpellSrc::manuscript) {
                bon_words.emplace_back("manuscript");
        }

        if (player_spells::is_getting_altar_bonus()) {
                bon_words.emplace_back("altar");
        }

        if (map::g_player->m_properties.has(prop::Id::erudition)) {
                bon_words.emplace_back("erudition");
        }

        if (map::g_player->m_inv.has_item_in_backpack(item::Id::necronomicon)) {
                bon_words.emplace_back("necronomicon");
        }

        for (size_t i = 0; i < bon_words.size(); ++i) {
                if (i == 0) {
                        str += " (";
                }

                str += bon_words[i];

                if (i < (bon_words.size() - 1)) {
                        str += ", ";
                }
                else {
                        str += ")";
                }
        }

        str += ".";

        return str;
}

static void end_properties_for_casting_spell(
        actor::Actor& caster,
        const SpellId spell_id)
{
        // End cloaking (unless invisibility was cast now).
        //
        // NOTE: This could be slightly weird, as the player could for example
        // have cloaking with a very long duration applied, and then cast
        // invisibility, applying actual invisibility instead of cloaking - then
        // the long cloaking effect will still be kept. But this seems like a
        // very minor issue or perhaps not an issue at all. It just means that
        // the invisibility spell is an exception from spells breaking cloaking.
        //
        if (spell_id != SpellId::invis) {
                caster.m_properties.end_prop(prop::Id::cloaked);
        }

        // End focused
        caster.m_properties.end_prop(prop::Id::meditative_focused);

        // End erudition (unless that was the spell that was cast now).
        if (spell_id != SpellId::erudition) {
                const auto* const prop =
                        caster.m_properties.prop(prop::Id::erudition);

                const bool should_end =
                        prop &&
                        static_cast<const prop::Erudition*>(prop)->should_end_on_spell_cast();

                if (should_end) {
                        caster.m_properties.end_prop(prop::Id::erudition);
                }
        }
}

// -----------------------------------------------------------------------------
// spells
// -----------------------------------------------------------------------------
namespace spells
{
Spell* make(const SpellId spell_id)
{
        switch (spell_id) {
        case SpellId::aura_of_decay:
                return new SpellAuraOfDecay();

        case SpellId::enfeeble:
                return new SpellEnfeeble();

        case SpellId::curse:
                return new SpellCurse();

        case SpellId::slow:
                return new SpellSlow();

        case SpellId::terrify:
                return new SpellTerrify();

        case SpellId::disease:
                return new SpellDisease();

        case SpellId::force_bolt:
                return new SpellBolt(new ForceBolt);

        case SpellId::darkbolt:
                return new SpellBolt(new Darkbolt);

        case SpellId::aza_gaze:
                return new SpellAzaGaze();

        case SpellId::summon:
                return new SpellSummonMon();

        case SpellId::summon_tentacles:
                return new SpellSummonTentacles();

        case SpellId::heal:
                return new SpellHeal();

        case SpellId::knockback:
                return new SpellKnockBack();

        case SpellId::teleport:
                return new SpellTeleport();

        case SpellId::cataclysm:
                return new SpellCataclysm();

        case SpellId::pestilence:
                return new SpellPestilence();

        case SpellId::spectral_weapons:
                return new SpellSpectralWeapons();

        case SpellId::control_object:
                return new SpellControlObject();

        case SpellId::cleansing_fire:
                return new SpellCleansingFire();

        case SpellId::sanctuary:
                return new SpellSanctuary();

        case SpellId::purge:
                return new SpellPurge();

        case SpellId::frenzy:
                return new SpellFrenzy();

        case SpellId::bless:
                return new SpellBless();

        case SpellId::mi_go_hypno:
                return new SpellMiGoHypno();

        case SpellId::burn:
                return new SpellBurn();

        case SpellId::deafen:
                return new SpellDeafen();

        case SpellId::resistance:
                return new SpellResistance();

        case SpellId::light:
                return new SpellLight();

        case SpellId::transmut:
                return new SpellTransmut();

        case SpellId::invis:
                return new SpellInvis();

        case SpellId::see_invis:
                return new SpellSeeInvis();

        case SpellId::spell_shield:
                return new SpellSpellShield();

        case SpellId::haste:
                return new SpellHaste();

        case SpellId::premonition:
                return new SpellPremonition();

        case SpellId::erudition:
                return new SpellErudition();

        case SpellId::identify:
                return new SpellIdentify();

        case SpellId::blood_tempering:
                return new SpellBloodTempering();

        case SpellId::sacrifice_life:
                return new SpellSacrificeLife();

        case SpellId::thorns:
                return new SpellThorns();

        case SpellId::crimson_passage:
                return new SpellCrimsonPassage();

        case SpellId::END:
                break;
        }

        ASSERT(false);

        return nullptr;
}

SpellId str_to_spell_id(const std::string& str)
{
        return s_str_to_spell_id_map.at(str);
}

SpellSkill str_to_spell_skill_id(const std::string& str)
{
        return s_str_to_spell_skill_map.at(str);
}

std::string spell_domain_title(const SpellDomain domain)
{
        switch (domain) {
        case SpellDomain::clairvoyance:
                return "Clairvoyance";

        case SpellDomain::enchantment:
                return "Enchantment";

        case SpellDomain::invocation:
                return "Invocation";

        case SpellDomain::transmutation:
                return "Transmutation";

        case SpellDomain::blood:
                return "Blood";

        case SpellDomain::END:
                break;
        }

        ASSERT(false);

        return "";
}

std::string skill_to_str(const SpellSkill skill)
{
        switch (skill) {
        case SpellSkill::basic:
                return "basic";

        case SpellSkill::expert:
                return "expert";

        case SpellSkill::master:
                return "master";

        case SpellSkill::transcendent:
                return "transcendent";
        }

        ASSERT(false);

        return "";
}

terrain::DidOpen run_opening_spell_effect_at(
        const P& pos,
        const SpellSkill skill)
{
        auto* const terrain = map::g_terrain.at(pos);

        terrain::Door* door = nullptr;

        if (terrain->id() == terrain::Id::door) {
                door = static_cast<terrain::Door*>(terrain);

                if (door->is_open()) {
                        return terrain::DidOpen::no;
                }

                if ((door->type() == terrain::DoorType::metal)) {
                        if (skill == SpellSkill::basic) {
                                return terrain::DidOpen::no;
                        }

                        const auto did_toggle = toggle_metal_door(*door);

                        const auto did_open =
                                (did_toggle == DidAction::yes)
                                ? terrain::DidOpen::yes
                                : terrain::DidOpen::no;

                        return did_open;
                }
        }

        // TODO: Shouldn't the actor parameter be the caster here?
        const auto did_open = terrain->open(nullptr);

        return did_open;
}

terrain::DidClose run_close_spell_effect_at(
        const P& pos,
        const SpellSkill skill)
{
        auto* const terrain = map::g_terrain.at(pos);

        if (terrain->id() == terrain::Id::door) {
                auto* const door = static_cast<terrain::Door*>(terrain);

                if (!door->is_open()) {
                        return terrain::DidClose::no;
                }

                if ((door->type() == terrain::DoorType::metal)) {
                        if (skill == SpellSkill::basic) {
                                return terrain::DidClose::no;
                        }

                        const auto did_toggle = toggle_metal_door(*door);

                        const auto did_close =
                                (did_toggle == DidAction::yes)
                                ? terrain::DidClose::yes
                                : terrain::DidClose::no;

                        return did_close;
                }
        }

        // TODO: Shouldn't the actor parameter be the caster here?
        const auto did_close = terrain->close(nullptr);

        return did_close;
}

}  // namespace spells

// -----------------------------------------------------------------------------
// Spell
// -----------------------------------------------------------------------------
Range Spell::cost_range(
        const SpellSkill skill,
        const actor::Actor* const caster) const
{
        const int cost_max = base_max_cost(skill);
        const int cost_min = (cost_max + 1) / 2;

        Range range(cost_min, cost_max);

        if (actor::is_player(caster) &&
            caster->m_properties.has(prop::Id::meditative_focused)) {
                --range.min;
                --range.max;
        }

        range.min = std::max(0, range.min);
        range.max = std::max(0, range.max);

        return range;
}

void Spell::cast(
        actor::Actor* const caster,
        const SpellSkill skill,
        const SpellSrc spell_src,
        const std::vector<actor::Actor*>& seen_targets) const
{
        TRACE_FUNC_BEGIN;

        ASSERT(caster);

        prop::PropHandler& properties = caster->m_properties;

        // If this is an intrinsic cast, check properties which NEVER allows
        // casting or speaking.
        //
        // NOTE: If this is a non-intrinsic cast (e.g. from a scroll), then we
        // assume that the caller has made all checks themselves.
        //
        if (spell_src == SpellSrc::learned) {
                if (!properties.allow_cast_intr_spell_absolute(Verbose::yes)) {
                        return;
                }

                if (!properties.allow_speak(Verbose::yes)) {
                        // TODO: Not all spells "require making noise", it seems
                        // insconsistent to outright prevent casting when the
                        // caster cannot speak.
                        return;
                }
        }

        // OK, we can try to cast

        if (actor::is_player(caster)) {
                TRACE << "Player casting spell" << std::endl;

                const ShockSrc shock_src =
                        (spell_src == SpellSrc::learned)
                        ? s_spell_domain_to_shock_type_map.at(domain())
                        : ShockSrc::use_strange_item;

                int shock = shock_value();

                if (map::g_player->m_inv.has_item_in_backpack(
                            item::Id::necronomicon)) {
                        shock *= 2;
                }

                if (shock > 0) {
                        map::g_player->incr_shock((double)shock, shock_src);
                }

                // Make sound if noisy - casting from scrolls is always noisy.
                if (is_noisy(skill) || (spell_src == SpellSrc::manuscript)) {
                        Snd snd(
                                "",
                                audio::SfxId::END,
                                // audio::SfxId::spell_generic,
                                IgnoreMsgIfOriginSeen::yes,
                                caster->m_pos,
                                caster,
                                SndVol::low,
                                AlertsMon::yes);

                        snd.run();
                }
        }
        else {
                // Caster is monster
                TRACE << "Monster casting spell" << std::endl;

                // Make sound if noisy - casting from scrolls is always noisy.
                if (is_noisy(skill) || (spell_src == SpellSrc::manuscript)) {
                        const bool is_mon_seen =
                                actor::can_player_see_actor(*caster);

                        std::string spell_msg = caster->m_data->spell_msg;

                        if (!spell_msg.empty()) {
                                std::string mon_name;

                                if (is_mon_seen) {
                                        mon_name =
                                                text_format::first_to_upper(
                                                        caster->name_the());
                                }
                                else {
                                        // Cannot see monster.
                                        mon_name =
                                                caster->m_data->is_humanoid
                                                ? "Someone"
                                                : "Something";
                                }

                                spell_msg = mon_name + " " + spell_msg;
                        }

                        Snd snd(
                                spell_msg,
                                audio::SfxId::END,
                                IgnoreMsgIfOriginSeen::no,
                                caster->m_pos,
                                caster,
                                SndVol::low,
                                AlertsMon::no);

                        snd.run();
                }
        }

        bool allow_cast = true;

        if (spell_src == SpellSrc::learned) {
                const Range cost = cost_range(skill, caster);

                if (cost.min > 0) {
                        if (cost_type() == SpellCostType::spirit) {
                                // Spell costs spirit to cast.
                                actor::hit_sp(
                                        *caster,
                                        cost.roll(),
                                        nullptr,
                                        Verbose::no);
                        }
                        else {
                                // Spell costs hit points to cast.
                                actor::hit(
                                        *caster,
                                        cost.roll(),
                                        DmgType::pure,
                                        nullptr,
                                        AllowWound::no);
                        }
                }

                // Check properties which MAY allow casting.
                allow_cast =
                        properties.allow_cast_intr_spell_chance(
                                Verbose::yes);
        }

        const bool is_focused_player =
                actor::is_player(caster) &&
                caster->m_properties.has(prop::Id::meditative_focused);

        if (allow_cast && caster->is_alive()) {
                TRACE
                        << "Running spell effect for spell "
                        << "'" << name() << "'"
                        << std::endl;

                // Here we run the actual casting of the spell itself:
                run_effect(caster, skill, seen_targets);

                end_properties_for_casting_spell(*caster, id());

                // Grant regeneration from Flagellant galvanization trait?
                if (actor::is_player(caster) &&
                    player_bon::has_trait(Trait::galvanization) &&
                    (domain() == SpellDomain::blood)) {
                        prop::Prop* const regen = prop::make(prop::Id::regenerating);

                        regen->set_duration(rnd::range(4, 6));

                        properties.apply(regen);
                }

                // Disable tenebrous spell for the player?
                if (actor::is_player(caster) &&
                    is_tenebrous() &&
                    (spell_src == SpellSrc::learned)) {
                        player_spells::forget_spell(id());
                }
        }

        if (actor::is_player(caster) &&
            caster->is_alive() &&
            allow_cast &&
            (base_max_cost(skill) > 0) &&
            rnd::one_in(7)) {
                // Run a random side effect.
                const int d = 3;

                const R rect(
                        {std::max(0, caster->m_pos.x - d),
                         std::max(0, caster->m_pos.y - d)},
                        {std::min(map::w() - 1, caster->m_pos.x + d),
                         std::min(map::h() - 1, caster->m_pos.y + d)});

                std::vector<P> nearby_positions = rect.positions();

                rnd::shuffle(nearby_positions);

                const auto& side_effect =
                        rnd::weighted_choice(
                                spell_side_effects::s_spell_side_effects);

                TRACE << "Running spell side effect" << std::endl;

                side_effect({*caster, nearby_positions});
        }

        const bool is_casting_from_item = (spell_src == SpellSrc::item);

        if (!is_casting_from_item && !is_focused_player) {
                game_time::tick();
        }

        TRACE_FUNC_END;
}

void Spell::on_resist(actor::Actor& target) const
{
        const bool is_player = actor::is_player(&target);

        const bool player_see_target = actor::can_player_see_actor(target);

        if (player_see_target) {
                msg_log::add(s_spell_resist_msg);

                if (is_player) {
                        audio::play(audio::SfxId::spell_shield_break);
                }

                draw_blast_at_cells({target.m_pos}, colors::white());
        }

        // End spell resistance if not a natural property.
        if (!target.m_data->natural_props[(size_t)prop::Id::r_spell]) {
                target.m_properties.end_prop(prop::Id::r_spell);
        }

        if (is_player && player_bon::has_trait(Trait::absorb)) {
                map::g_player->restore_sp(
                        rnd::range(1, 6),
                        false,  // Not allowed above max
                        Verbose::yes);
        }
}

std::vector<std::string> Spell::descr(
        const SpellSkill skill,
        const SpellSrc spell_src) const
{
        std::vector<std::string> lines = descr_specific(skill);

        if (spell_src != SpellSrc::manuscript) {
                lines.push_back(get_noise_descr(is_noisy(skill)));
        }

        if (spell_src == SpellSrc::learned) {
                const std::string forgotten_hint_str =
                        "A forgotten spell is recalled again if "
                        "a monolith is activated, "
                        "or if the spell is cast from a manuscript.";

                if (player_spells::is_spell_forgotten(id())) {
                        lines.emplace_back(
                                "Forgotten - this spell can no longer be "
                                "cast from memory. " +
                                forgotten_hint_str);
                }
                else if (is_tenebrous()) {
                        lines.emplace_back(
                                "Tenebrous - this spell will be instantly "
                                "forgotten if cast from memory. " +
                                forgotten_hint_str);
                }
        }

        std::string str;

        if (can_be_improved_with_skill()) {
                str = get_skill_descr(skill, spell_src);
        }

        if (!player_bon::is_bg(Bg::exorcist)) {
                text_format::append_with_space(str, domain_descr());
        }

        if (!str.empty()) {
                lines.push_back(str);
        }

        return lines;
}

std::string Spell::domain_descr() const
{
        const SpellDomain my_domain = domain();

        if (my_domain == SpellDomain::END) {
                return "";
        }

        const std::string domain_title =
                text_format::first_to_upper(
                        spells::spell_domain_title(domain()));

        return "It belongs to the \"" + domain_title + "\" domain.";
}

int Spell::shock_value() const
{
        const SpellShock type = shock_type();

        int value = 0;

        switch (type) {
        case SpellShock::none:
                value = 0;
                break;

        case SpellShock::mild:
                value = 4;
                break;

        case SpellShock::disturbing:
                value = 16;
                break;

        case SpellShock::severe:
                value = 24;
                break;
        }

        return value;
}

// -----------------------------------------------------------------------------
// Aura of Decay
// -----------------------------------------------------------------------------
Range SpellAuraOfDecay::dmg_range(const SpellSkill skill) const
{
        switch (skill) {
        case SpellSkill::basic:
                return {1, 1};  // Avg 1.0

        case SpellSkill::expert:
                return {1, 2};  // Avg 1.5

        case SpellSkill::master:
        case SpellSkill::transcendent:
                return {1, 3};  // Avg 2.0
        }

        ASSERT(false);

        return {1, 1};
}

Range SpellAuraOfDecay::duration_range(const SpellSkill skill) const
{
        const int k = std::min(3, (int)skill + 1);

        Range duration_range;
        duration_range.min = 15 * k;
        duration_range.max = duration_range.min * 2;

        return duration_range;
}

int SpellAuraOfDecay::base_max_cost(const SpellSkill skill) const
{
        (void)skill;

        return 6;
}

void SpellAuraOfDecay::run_effect(
        actor::Actor* const caster,
        const SpellSkill skill,
        const std::vector<actor::Actor*>& seen_targets) const
{
        (void)seen_targets;

        auto* prop =
                static_cast<prop::AuraOfDecay*>(
                        prop::make(
                                prop::Id::aura_of_decay));

        prop->set_duration(duration_range(skill).roll());

        prop->set_dmg_range(dmg_range(skill));

        if (skill == SpellSkill::transcendent) {
                prop->set_allow_instant_kill();
        }

        caster->m_properties.apply(prop);
}

std::vector<std::string> SpellAuraOfDecay::descr_specific(
        const SpellSkill skill) const
{
        std::vector<std::string> descr;

        descr.emplace_back(
                "The caster exudes death and decay. Creatures within a "
                "distance of two moves take damage each standard turn.");

        descr.push_back(
                "The spell does " +
                dmg_range(skill).str() +
                " damage per creature.");

        if (skill == SpellSkill::transcendent) {
                descr.emplace_back(
                        "Any time a creature takes damage from the spell, "
                        "they may be destroyed immediately (2% chance).");
        }

        descr.push_back(
                "The spell lasts " +
                duration_range(skill).str() +
                " turns.");

        return descr;
}

int SpellAuraOfDecay::mon_cooldown() const
{
        return 30;
}

bool SpellAuraOfDecay::allow_mon_cast_now(
        const actor::Actor& mon,
        const std::vector<actor::Actor*>& seen_targets) const
{
        return (
                !seen_targets.empty() &&
                !mon.m_properties.has(prop::Id::aura_of_decay));
}

// -----------------------------------------------------------------------------
// Bolt spells
// -----------------------------------------------------------------------------
Range ForceBolt::damage(
        const SpellSkill skill,
        const actor::Actor& caster) const
{
        (void)caster;

        switch (skill) {
        case SpellSkill::basic:
                return {3, 4};  // Avg 3.5

        case SpellSkill::expert:
                return {5, 7};  // Avg 6.0

        case SpellSkill::master:
        case SpellSkill::transcendent:
                return {9, 12};  // Avg 10.5
        }

        ASSERT(false);

        return {1, 1};
}

std::vector<std::string> ForceBolt::descr_specific(const SpellSkill skill) const
{
        (void)skill;

        return {};
}

Range Darkbolt::damage(const SpellSkill skill, const actor::Actor& caster) const
{
        (void)caster;

        switch (skill) {
        case SpellSkill::basic:
                return {4, 9};  // Avg 6.5

        case SpellSkill::expert:
                return {5, 11};  // Avg 8.0

        case SpellSkill::master:
        case SpellSkill::transcendent:
                return {6, 13};  // Avg 9.5
        }

        ASSERT(false);

        return {1, 1};
}

std::vector<std::string> Darkbolt::descr_specific(const SpellSkill skill) const
{
        std::vector<std::string> descr;

        descr.emplace_back(
                "A bolt of siphoned energy is hurled towards a target "
                "with great force. "
                "The conjured bolt has some will on its own - "
                "once released it launches itself towards any creature "
                "sensed as a threat, "
                "precise control is therefore not possible.");

        const auto dmg_range = damage(skill, *map::g_player);

        std::string effect_str =
                "The impact does " +
                dmg_range.str() +
                " damage.";

        if (skill >= SpellSkill::master) {
                effect_str += " The target is paralyzed and set aflame.";

                if (skill == SpellSkill::transcendent) {
                        effect_str +=
                                " If the target is sufficiently far away from "
                                "the caster, the bolt explodes on impact.";
                }
        }
        else {
                // <= Expert
                effect_str += " The target is paralyzed.";
        }

        descr.push_back(effect_str);

        return descr;
}

void Darkbolt::on_hit(
        actor::Actor& actor_hit,
        actor::Actor& caster,
        const SpellSkill skill) const
{
        if (skill == SpellSkill::transcendent) {
                const int dist = king_dist(caster.m_pos, actor_hit.m_pos);

                if (dist > g_expl_std_radi) {
                        explosion::run(actor_hit.m_pos, ExplType::expl);
                }
        }

        if (!actor_hit.is_alive()) {
                return;
        }

        if (!actor_hit.m_properties.is_resisting_dmg(s_bolt_dmg_type, Verbose::no)) {
                prop::Prop* paralyzed = prop::make(prop::Id::paralyzed);

                paralyzed->set_duration(rnd::range(1, 2));

                actor_hit.m_properties.apply(paralyzed);

                if (skill >= SpellSkill::master) {
                        prop::Prop* burning = prop::make(prop::Id::burning);

                        burning->set_duration(rnd::range(2, 3));

                        actor_hit.m_properties.apply(burning);
                }
        }
}

void SpellBolt::run_effect(
        actor::Actor* const caster,
        const SpellSkill skill,
        const std::vector<actor::Actor*>& seen_targets) const
{
        if (seen_targets.empty()) {
                if (actor::is_player(caster)) {
                        msg_log::add(
                                "A dark sphere materializes, but quickly "
                                "fizzles out.");
                }

                return;
        }

        {
                Snd snd(
                        "I hear something rushing through the air.",
                        audio::SfxId::darkbolt_release,
                        IgnoreMsgIfOriginSeen::yes,
                        caster->m_pos,
                        caster,
                        SndVol::low,
                        AlertsMon::yes);

                snd.run();
        }

        actor::Actor* const target =
                map::random_closest_actor(
                        caster->m_pos,
                        seen_targets);

        // Spell resistance?
        if (target->m_properties.has(prop::Id::r_spell)) {
                on_resist(*target);

                // Spell reflection?
                if (target->m_properties.has(prop::Id::spell_reflect)) {
                        if (actor::can_player_see_actor(*target)) {
                                msg_log::add(
                                        s_spell_reflect_msg,
                                        colors::text(),
                                        MsgInterruptPlayer::no,
                                        MorePromptOnMsg::yes);
                        }

                        // Run effect with the target as caster, and the caster
                        // as seen target instead.
                        run_effect(target, skill, {caster});
                }

                return;
        }

        draw_projectile_travel(*caster, *target);

        {
                Snd snd(
                        "I hear an impact.",
                        audio::SfxId::darkbolt_impact,
                        IgnoreMsgIfOriginSeen::yes,
                        target->m_pos,
                        nullptr,
                        SndVol::low,
                        AlertsMon::yes);

                snd.run();
        }

        const P& target_p = target->m_pos;
        const bool player_see_pos = map::g_seen.at(target_p);
        const bool player_see_tgt = actor::can_player_see_actor(*target);

        if (player_see_tgt || player_see_pos) {
                draw_blast_at_cells({target->m_pos}, colors::magenta());

                Color msg_clr = colors::msg_good();

                std::string str_begin = "I am";

                if (actor::is_player(target)) {
                        msg_clr = colors::msg_bad();
                }
                else {
                        // Target is monster
                        const std::string name_the =
                                player_see_tgt
                                ? text_format::first_to_upper(target->name_the())
                                : "It";

                        str_begin = name_the + " is";

                        if (map::g_player->is_leader_of(target)) {
                                msg_clr = colors::white();
                        }
                }

                const std::string hit_msg =
                        str_begin +
                        " " +
                        m_impl->hit_msg_ending();

                msg_log::add(hit_msg, msg_clr);
        }

        const auto dmg_range = m_impl->damage(skill, *caster);

        actor::hit(
                *target,
                dmg_range.roll(),
                s_bolt_dmg_type,
                caster,
                AllowWound::no);

        m_impl->on_hit(*target, *caster, skill);

        if (!actor::is_player(target)) {
                target->become_aware_player(actor::AwareSource::spell_victim);
        }
}

void SpellBolt::draw_projectile_travel(
        const actor::Actor& caster,
        const actor::Actor& target) const
{
        Array2<bool> blocked(map::dims());

        map_parsers::BlocksProjectiles()
                .run(blocked, blocked.rect());

        const auto flood = floodfill(caster.m_pos, blocked);

        const auto path =
                pathfind_with_flood(
                        caster.m_pos,
                        target.m_pos,
                        flood);

        if (!path.empty()) {
                states::draw();

                const int idx_0 = (int)(path.size()) - 1;

                for (int i = idx_0; i > 0; --i) {
                        const auto& p = path[i];

                        if (!map::g_seen.at(p)) {
                                continue;
                        }

                        states::draw();

                        io::MapDrawObj draw_obj;
                        draw_obj.tile = gfx::TileId::blast1;
                        draw_obj.character = '*';
                        draw_obj.pos = viewport::to_view_pos(p);
                        draw_obj.color = colors::magenta();

                        draw_obj.draw();

                        io::update_screen();

                        io::sleep(config::delay_projectile_draw());
                }
        }
}

bool SpellBolt::allow_mon_cast_now(
        const actor::Actor& mon,
        const std::vector<actor::Actor*>& seen_targets) const
{
        (void)mon;

        return !seen_targets.empty();
}

// -----------------------------------------------------------------------------
// Azathoths wrath
// -----------------------------------------------------------------------------
Range SpellAzaGaze::dmg_range(const SpellSkill skill) const
{
        switch (skill) {
        case SpellSkill::basic:
                return {2, 5};  // Avg 3.5

        case SpellSkill::expert:
                return {4, 8};  // Avg 6.0

        case SpellSkill::master:
        case SpellSkill::transcendent:
                return {6, 11};  // Avg 8.5
        }

        ASSERT(false);

        return {1, 1};
}

Range SpellAzaGaze::faint_duration_range(SpellSkill skill) const
{
        switch (skill) {
        case SpellSkill::basic:
                return {2, 6};

        case SpellSkill::expert:
                return {3, 7};

        case SpellSkill::master:
        case SpellSkill::transcendent:
                return {4, 8};
        }

        ASSERT(false);

        return {1, 1};
}

Range SpellAzaGaze::conflict_duration_range(SpellSkill skill) const
{
        (void)skill;

        return {10, 12};
}

void SpellAzaGaze::do_damage_on_target(
        actor::Actor& target,
        SpellSkill const skill,
        actor::Actor* const caster) const
{
        const int dmg = dmg_range(skill).roll();

        actor::hit(
                target,
                dmg,
                DmgType::explosion,
                caster,
                AllowWound::no);
}

void SpellAzaGaze::apply_properties_on_target(
        actor::Actor& target,
        SpellSkill skill) const
{
        if (!target.is_alive()) {
                return;
        }

        {
                auto* prop = prop::make(prop::Id::fainted);

                const int duration = faint_duration_range(skill).roll();

                prop->set_duration(duration);

                target.m_properties.apply(prop);
        }

        if (skill >= SpellSkill::transcendent) {
                auto* prop = prop::make(prop::Id::conflict);

                const int duration = conflict_duration_range(skill).roll();

                prop->set_duration(duration);

                target.m_properties.apply(prop);
        }
}

void SpellAzaGaze::run_effect_on_target(
        actor::Actor* const caster,
        actor::Actor& target,
        const SpellSkill skill) const
{
        // Spell resistance?
        if (target.m_properties.has(prop::Id::r_spell)) {
                on_resist(target);

                // Spell reflection?
                if (target.m_properties.has(prop::Id::spell_reflect)) {
                        if (actor::can_player_see_actor(target)) {
                                msg_log::add(
                                        s_spell_reflect_msg,
                                        colors::white(),
                                        MsgInterruptPlayer::no,
                                        MorePromptOnMsg::yes);
                        }

                        // Run effect with the target as caster, and the caster
                        // as seen target instead.
                        run_effect(&target, skill, {caster});
                }

                return;
        }

        if (actor::is_player(&target)) {
                Snd snd(
                        "I am assailed by a torrent of chaos!",
                        audio::SfxId::aza_gaze,
                        IgnoreMsgIfOriginSeen::yes,
                        map::g_player->m_pos,
                        nullptr,
                        SndVol::high,
                        AlertsMon::no);

                snd.run();
        }

        do_damage_on_target(target, skill, caster);

        if (!actor::is_player(&target)) {
                target.become_aware_player(actor::AwareSource::spell_victim);
        }

        apply_properties_on_target(target, skill);

        Snd snd(
                "",
                audio::SfxId::END,
                IgnoreMsgIfOriginSeen::yes,
                target.m_pos,
                nullptr,
                SndVol::high,
                AlertsMon::yes);

        snd.run();
}

void SpellAzaGaze::run_effect(
        actor::Actor* const caster,
        const SpellSkill skill,
        const std::vector<actor::Actor*>& seen_targets) const
{
        if (actor::is_player(caster)) {
                std::string msg;

                if (seen_targets.empty()) {
                        msg = "An insane cacophony resounds through the air!";
                }

                Snd snd(
                        msg,
                        audio::SfxId::aza_gaze,
                        IgnoreMsgIfOriginSeen::no,
                        map::g_player->m_pos,
                        nullptr,
                        SndVol::high,
                        AlertsMon::no);

                snd.run();
        }

        draw_blast_at_seen_actors(seen_targets, colors::light_red());

        for (auto* const target : seen_targets) {
                run_effect_on_target(caster, *target, skill);
        }
}

std::vector<std::string> SpellAzaGaze::descr_specific(
        const SpellSkill skill) const
{
        std::vector<std::string> descr;

        descr.emplace_back(
                "Channels the chaos of Azathoth unto all visible enemies. "
                "The channel can only be opened for a fraction of a second, "
                "but even this is enough to cause great physical and mental "
                "devastation.");

        descr.push_back(
                "The spell does " +
                dmg_range(skill).str() +
                " damage per creature.");

        descr.push_back(
                "Causes the victims to faint for " +
                faint_duration_range(skill).str() +
                " turns, if they are susceptible.");

        if (skill == SpellSkill::transcendent) {
                descr.push_back(
                        "The victims become conflicted for " +
                        conflict_duration_range(skill).str() +
                        " turns, causing them to view any creature as " +
                        "their enemy.");
        }

        return descr;
}

bool SpellAzaGaze::allow_mon_cast_now(
        const actor::Actor& mon,
        const std::vector<actor::Actor*>& seen_targets) const
{
        (void)mon;

        return !seen_targets.empty();
}

// -----------------------------------------------------------------------------
// Cataclysm
// -----------------------------------------------------------------------------
int SpellCataclysm::destruction_radi(const SpellSkill skill) const
{
        return g_fov_radi_int + 1 + ((int)skill * 2);
}

int SpellCataclysm::nr_destruction_sweeps(const SpellSkill skill) const
{
        switch (skill) {
        case SpellSkill::basic:
                return 2;

        case SpellSkill::expert:
        case SpellSkill::master:
        case SpellSkill::transcendent:
                return 3;
        }

        ASSERT(false);

        return 1;
}

int SpellCataclysm::nr_explosions(const SpellSkill skill) const
{
        switch (skill) {
        case SpellSkill::basic:
                return 6;

        case SpellSkill::expert:
                return 9;

        case SpellSkill::master:
                return 12;

        case SpellSkill::transcendent:
                return 20;
        }

        ASSERT(false);

        return 1;
}

int SpellCataclysm::base_max_cost(const SpellSkill skill) const
{
        (void)skill;

        return 7;
}

void SpellCataclysm::run_effect(
        actor::Actor* const caster,
        const SpellSkill skill,
        const std::vector<actor::Actor*>& seen_targets) const
{
        (void)seen_targets;

        const bool is_player = actor::is_player(caster);

        if (actor::can_player_see_actor(*caster)) {
                std::string caster_name =
                        is_player
                        ? "me"
                        : caster->name_the();

                msg_log::add("Destruction rages around " + caster_name + "!");
        }

        const auto& caster_pos = caster->m_pos;

        const int destr_radi = destruction_radi(skill);

        const R area(
                std::max(1, caster_pos.x - destr_radi),
                std::max(1, caster_pos.y - destr_radi),
                std::min(map::w() - 1, caster_pos.x + destr_radi) - 1,
                std::min(map::h() - 1, caster_pos.y + destr_radi) - 1);

        const auto positions = area.positions();

        // Run explosions
        std::vector<P> p_bucket;

        const int expl_radi_diff = -1;

        for (const auto& p : positions) {
                const auto* const terrain = map::g_terrain.at(p);

                if (!terrain->is_walkable()) {
                        continue;
                }

                const int dist = king_dist(caster_pos, p);

                const int min_dist = g_expl_std_radi + 1 + expl_radi_diff;

                if (dist >= min_dist) {
                        p_bucket.push_back(p);
                }
        }

        const int nr_expl = nr_explosions(skill);

        for (int i = 0; i < nr_expl; ++i) {
                if (p_bucket.empty()) {
                        return;
                }

                const auto idx = rnd::range(0, (int)p_bucket.size() - 1);

                const auto& p = rnd::element(p_bucket);

                explosion::run(
                        p,
                        ExplType::expl,
                        EmitExplSnd::yes,
                        expl_radi_diff);

                p_bucket.erase(std::begin(p_bucket) + idx);
        }

        // Explode braziers
        for (const auto& p : positions) {
                const auto terrain_id = map::g_terrain.at(p)->id();

                if (terrain_id == terrain::Id::brazier) {
                        Snd snd(
                                "I hear an explosion!",
                                audio::SfxId::explosion_molotov,
                                IgnoreMsgIfOriginSeen::yes,
                                p,
                                nullptr,
                                SndVol::high,
                                AlertsMon::yes);

                        snd.run();

                        map::update_terrain(terrain::make(terrain::Id::rubble_low, p));

                        prop::Prop* const burning = prop::make(prop::Id::burning);

                        explosion::run(
                                p,
                                ExplType::apply_prop,
                                EmitExplSnd::yes,
                                0,
                                ExplExclCenter::yes,
                                {burning});
                }
        }

        // Destroy the surrounding environment
        const int nr_sweeps = nr_destruction_sweeps(skill);

        for (int i = 0; i < nr_sweeps; ++i) {
                for (const auto& p : positions) {
                        if (!rnd::one_in(8)) {
                                continue;
                        }

                        bool is_adj_to_walkable_cell = false;

                        for (const P& d : dir_utils::g_dir_list) {
                                const auto p_adj = p + d;

                                if (map::g_terrain.at(p_adj)->is_walkable()) {
                                        is_adj_to_walkable_cell = true;
                                }
                        }

                        if (is_adj_to_walkable_cell) {
                                map::g_terrain.at(p)->hit(DmgType::explosion, nullptr);
                        }
                }
        }

        // Put blood, and set stuff on fire
        for (const auto& p : positions) {
                auto* const terrain = map::g_terrain.at(p);

                if (rnd::one_in(10)) {
                        terrain->try_make_bloody();

                        if (rnd::one_in(3)) {
                                terrain->try_put_gore();
                        }
                }

                if ((p != caster->m_pos) && rnd::one_in(6)) {
                        terrain->hit(DmgType::fire, nullptr);
                }
        }

        Snd snd(
                "",
                audio::SfxId::END,
                IgnoreMsgIfOriginSeen::yes,
                caster_pos,
                nullptr,
                SndVol::high,
                AlertsMon::yes);

        snd.run();
}

std::vector<std::string> SpellCataclysm::descr_specific(
        const SpellSkill skill) const
{
        (void)skill;

        std::vector<std::string> descr;

        descr.emplace_back(
                "Blasts the surrounding area with terrible force.");

        descr.emplace_back(
                "Higher skill levels increases the magnitude of the "
                "destruction.");

        return descr;
}

bool SpellCataclysm::allow_mon_cast_now(
        const actor::Actor& mon,
        const std::vector<actor::Actor*>& seen_targets) const
{
        // Always allow casting with a visible target.
        if (!seen_targets.empty()) {
                return true;
        }

        // Sometimes allow casting if monster has an unseen target.
        if (mon.m_ai_state.target && rnd::one_in(20)) {
                return true;
        }

        return false;
}

// -----------------------------------------------------------------------------
// Pestilence
// -----------------------------------------------------------------------------
int SpellPestilence::nr_rats_summoned(SpellSkill skill) const
{
        if (skill == SpellSkill::transcendent) {
                return 3;
        }
        else {
                return 6 + (int)skill * 3;
        }
}

void SpellPestilence::on_rat_summoned(
        actor::Actor* const mon,
        const SpellSkill skill) const
{
        mon->m_properties.apply(prop::make(prop::Id::summoned));

        {
                auto* prop = prop::make(prop::Id::waiting);

                prop->set_duration(2);

                mon->m_properties.apply(prop);
        }

        if (skill == SpellSkill::master) {
                auto* prop = prop::make(prop::Id::hasted);

                prop->set_indefinite();

                mon->m_properties.apply(
                        prop,
                        prop::PropSrc::intr,
                        true,
                        Verbose::no);
        }
}

void SpellPestilence::run_effect(
        actor::Actor* const caster,
        const SpellSkill skill,
        const std::vector<actor::Actor*>& seen_targets) const
{
        (void)seen_targets;

        const size_t nr_mon = nr_rats_summoned(skill);

        actor::Actor* leader = nullptr;

        if (actor::is_player(caster)) {
                leader = caster;
        }
        else {
                // Caster is monster
                actor::Actor* const caster_leader = caster->m_leader;

                leader =
                        caster_leader
                        ? caster_leader
                        : caster;
        }

        const std::string id =
                (skill == SpellSkill::transcendent)
                ? "MON_TRANSCENDENT_RAT"
                : "MON_RAT";

        const auto mon_summoned =
                actor::spawn(
                        caster->m_pos,
                        {nr_mon, id},
                        map::rect())
                        .make_aware_of_player()
                        .set_leader(leader);

        const bool is_any_seen_by_player =
                std::any_of(
                        std::begin(mon_summoned.monsters),
                        std::end(mon_summoned.monsters),
                        [](auto* const mon) {
                                return actor::can_player_see_actor(*mon);
                        });

        std::for_each(
                std::begin(mon_summoned.monsters),
                std::end(mon_summoned.monsters),
                [skill, this](auto& mon) { on_rat_summoned(mon, skill); });

        if (mon_summoned.monsters.empty()) {
                return;
        }

        if (actor::is_player(caster) || is_any_seen_by_player) {
                std::string caster_str = "me";

                if (!actor::is_player(caster)) {
                        if (actor::can_player_see_actor(*caster)) {
                                caster_str = caster->name_the();
                        }
                        else {
                                caster_str = "it";
                        }
                }

                msg_log::add("Rats appear around " + caster_str + "!");
        }
}

std::vector<std::string> SpellPestilence::descr_specific(
        const SpellSkill skill) const
{
        std::vector<std::string> descr;

        descr.emplace_back("A pack of rats appear around the caster.");

        const size_t nr_mon = nr_rats_summoned(skill);

        descr.push_back("Summons " + std::to_string(nr_mon) + " rats.");

        if (skill == SpellSkill::master) {
                descr.emplace_back("The rats are Hasted (moves faster).");
        }
        else if (skill == SpellSkill::transcendent) {
                descr.emplace_back(
                        "The rats are ethereal (much harder to hit "
                        "with attacks, can move through solid objects), "
                        "are immune to magic, can cast spells, and have "
                        "+4 hit points and +3 maximum damage.");
        }

        return descr;
}

bool SpellPestilence::allow_mon_cast_now(
        const actor::Actor& mon,
        const std::vector<actor::Actor*>& seen_targets) const
{
        // Always allow casting with a visible target.
        if (!seen_targets.empty()) {
                return true;
        }

        // Sometimes allow casting if monster has an unseen target.
        if (mon.m_ai_state.target && rnd::one_in(30)) {
                return true;
        }

        return false;
}

// -----------------------------------------------------------------------------
// Spectral Weapons
// -----------------------------------------------------------------------------
int SpellSpectralWeapons::max_nr_weapons(const SpellSkill skill) const
{
        return 2 + (int)skill;
}

Range SpellSpectralWeapons::duration_range(const SpellSkill skill) const
{
        switch (skill) {
        case SpellSkill::basic:
                return {5, 10};

        case SpellSkill::expert:
        case SpellSkill::transcendent:
                // NOTE: For balancing reasons, the transcendent level uses the
                // same duration as the expert level.
                return {10, 15};

        case SpellSkill::master:
                return {15, 20};
        }

        ASSERT(false);

        return {1, 1};
}

void SpellSpectralWeapons::on_mon_summoned(
        item::Item* const item,
        actor::Actor* const mon,
        const SpellSkill skill) const
{
        ASSERT(!mon->m_inv.item_in_slot(SlotId::wpn));

        mon->m_inv.put_in_slot(SlotId::wpn, item, Verbose::no);

        {
                auto* prop = prop::make(prop::Id::spectral_wpn);
                prop->set_indefinite();
                mon->m_properties.apply(prop);
        }

        {
                auto* prop = prop::make(prop::Id::summoned);
                const int duration = duration_range(skill).roll();
                prop->set_duration(duration);
                mon->m_properties.apply(prop);
        }

        {
                auto* prop = prop::make(prop::Id::waiting);
                prop->set_duration(1);
                mon->m_properties.apply(prop);
        }

        if (skill >= SpellSkill::master) {
                auto* prop = prop::make(prop::Id::see_invis);

                prop->set_indefinite();

                mon->m_properties.apply(
                        prop,
                        prop::PropSrc::intr,
                        true,
                        Verbose::no);
        }

        if (skill == SpellSkill::transcendent) {
                auto* prop = prop::make(prop::Id::r_phys);

                prop->set_indefinite();

                mon->m_properties.apply(
                        prop,
                        prop::PropSrc::intr,
                        true,
                        Verbose::no);
        }

        if (actor::can_player_see_actor(*mon)) {
                msg_log::add(mon->name_a() + " appears!");
        }
}

void SpellSpectralWeapons::run_effect(
        actor::Actor* const caster,
        const SpellSkill skill,
        const std::vector<actor::Actor*>& seen_targets) const
{
        TRACE_FUNC_BEGIN;

        (void)seen_targets;

        if (!actor::is_player(caster)) {
                TRACE_FUNC_END;

                return;
        }

        // Find available weapons
        auto is_melee_wpn = [](const auto* const item) {
                return item && (item->data().type == ItemType::melee_wpn);
        };

        std::vector<const item::Item*> weapons;

        for (const auto& slot : caster->m_inv.m_slots) {
                if (is_melee_wpn(slot.item)) {
                        weapons.push_back(slot.item);
                }
        }

        for (const auto& item : caster->m_inv.m_backpack) {
                if (is_melee_wpn(item)) {
                        weapons.push_back(item);
                }
        }

        // Cap the number of weapons spawned
        rnd::shuffle(weapons);

        const auto nr_max = (size_t)max_nr_weapons(skill);

        if (nr_max < weapons.size()) {
                weapons.resize(nr_max);
        }

        // Spawn weapon monsters
        for (const auto* const item : weapons) {
                auto* new_item = item::make(item->id());

                new_item->set_base_melee_dmg(item->base_melee_dmg());

                const auto summoned =
                        actor::spawn(
                                caster->m_pos,
                                {"MON_SPECTRAL_WPN"},
                                map::rect())
                                .set_leader(caster);

                std::for_each(
                        std::begin(summoned.monsters),
                        std::end(summoned.monsters),
                        [this, new_item, skill](auto* const mon) {
                                on_mon_summoned(new_item, mon, skill);
                        });
        }

        TRACE_FUNC_END;
}

std::vector<std::string> SpellSpectralWeapons::descr_specific(
        const SpellSkill skill) const
{
        std::vector<std::string> descr;

        descr.emplace_back(
                "Conjures ghostly copies of carried weapons, which will float "
                "through the air and protect their master. It is only "
                "possible to create copies of basic melee weapons - "
                "\"modern\" mechanisms such as pistols or machine guns are "
                "far too complex.");

        const auto nr_max = (size_t)max_nr_weapons(skill);

        std::string nr_and_duration_str =
                "A maximum of " +
                std::to_string(nr_max) +
                " ";

        if (nr_max == 1) {
                nr_and_duration_str += "weapon";
        }
        else {
                nr_and_duration_str += "weapons";
        }

        nr_and_duration_str += " may be spawned on casting the spell.";

        nr_and_duration_str +=
                " The weapons exist for " +
                duration_range(skill).str() +
                " turns.";

        descr.push_back(nr_and_duration_str);

        if (skill >= SpellSkill::master) {
                descr.emplace_back(
                        "The weapons can see invisible creatures.");
        }

        if (skill == SpellSkill::transcendent) {
                descr.emplace_back(
                        "The weapons cannot be harmed by physical damage.");
        }

        return descr;
}

// -----------------------------------------------------------------------------
// Control Object
// -----------------------------------------------------------------------------
int SpellControlObject::base_max_cost(const SpellSkill skill) const
{
        if (skill == SpellSkill::transcendent) {
                return 1;
        }
        else {
                return 4;
        }
}

int SpellControlObject::max_dist(const SpellSkill skill) const
{
        const int k = (int)skill + 1;

        return std::min(g_fov_radi_int, k * 2);
}

void SpellControlObject::run_effect(
        actor::Actor* const caster,
        const SpellSkill skill,
        const std::vector<actor::Actor*>& seen_targets) const
{
        (void)seen_targets;

        const auto origin = caster->m_pos;

        auto ctrl_obj_state =
                std::make_unique<CtrlObj>(
                        origin,
                        max_dist(skill),
                        skill);

        // Run the state immediately, so that spell side effects happen AFTER
        // the player has finished casting the spell.
        states::run_until_state_done(std::move(ctrl_obj_state));
}

std::vector<std::string> SpellControlObject::descr_specific(
        const SpellSkill skill) const
{
        std::vector<std::string> descr;

        std::string control_descr =
                "Opens doors, chests, tombs, or cabinets. "
                "Closes or jams doors. "
                "Strikes doors, braziers, or statues.";

        if (skill == SpellSkill::basic) {
                control_descr +=
                        " (Heavy doors operated externally by a switch cannot "
                        "be opened or closed.)";
        }
        else {
                control_descr +=
                        " Heavy doors operated externally by a switch can also "
                        "be opened or closed.";
        }

        if (skill == SpellSkill::transcendent) {
                control_descr +=
                        " Walls can be destroyed.";
        }

        descr.emplace_back(control_descr);

        descr.emplace_back(
                "Maximum control distance is " +
                std::to_string(max_dist(skill)) +
                ".");

        descr.emplace_back(
                "When casting the spell, select a seen object to control "
                "within the maximum distance.");

        return descr;
}

bool SpellControlObject::is_noisy(const SpellSkill skill) const
{
        return (skill == SpellSkill::basic);
}

// -----------------------------------------------------------------------------
// Exorcist Cleansing Fire
// -----------------------------------------------------------------------------
Range SpellCleansingFire::burn_duration_range() const
{
        return {3, 5};
}

void SpellCleansingFire::run_effect(
        actor::Actor* caster,
        SpellSkill skill,
        const std::vector<actor::Actor*>& seen_targets) const
{
        if (!caster) {
                return;
        }

        std::vector<actor::Actor*> targets;

        if (seen_targets.empty()) {
                return;
        }

        if (skill == SpellSkill::basic) {
                targets.push_back(rnd::element(seen_targets));
        }
        else {
                // Skill greater than basic - target all seen foes
                targets = seen_targets;
        }

        for (auto* const actor : targets) {
                // Spell resistance?
                if (actor->m_properties.has(prop::Id::r_spell)) {
                        on_resist(*actor);

                        // Spell reflection?
                        if (actor->m_properties.has(prop::Id::spell_reflect)) {
                                if (actor::can_player_see_actor(*actor)) {
                                        msg_log::add(
                                                s_spell_reflect_msg,
                                                colors::text(),
                                                MsgInterruptPlayer::no,
                                                MorePromptOnMsg::yes);
                                }

                                // Run effect with the target as caster, and the
                                // caster as seen target instead.
                                run_effect(actor, skill, {caster});
                        }

                        continue;
                }

                for (const auto& d : dir_utils::g_dir_list) {
                        const auto p(actor->m_pos + d);

                        // Hit the terrain with burning several times, to
                        // increase the chance of it catching fire
                        for (int i = 0; i < 6; ++i) {
                                map::g_terrain.at(p)->hit(DmgType::fire, nullptr);
                        }
                }

                prop::Prop* const burning = prop::make(prop::Id::burning);

                burning->set_duration(burn_duration_range().roll());

                actor->m_properties.apply(burning);
        }
}

std::vector<std::string> SpellCleansingFire::descr_specific(
        SpellSkill skill) const
{
        std::vector<std::string> descr;

        descr.emplace_back(
                "Causes the spell's victims to burn for " +
                burn_duration_range().str() +
                " turns, and scorches the ground around them with fire "
                "(be careful with hitting adjacent creatures).");

        if (skill == SpellSkill::basic) {
                descr.emplace_back(
                        "Affects one random visible hostile creature.");
        }
        else {
                descr.emplace_back(
                        "Affects all visible hostile creatures.");
        }

        return descr;
}

// -----------------------------------------------------------------------------
// Exorcist Sanctuary
// -----------------------------------------------------------------------------
Range SpellSanctuary::duration(const SpellSkill skill) const
{
        if (skill == SpellSkill::basic) {
                return {3, 5};
        }
        else {
                return {5, 10};
        }
}

void SpellSanctuary::run_effect(
        actor::Actor* caster,
        SpellSkill skill,
        const std::vector<actor::Actor*>& seen_targets) const
{
        (void)seen_targets;

        if (!caster) {
                return;
        }

        const auto prop_duration = duration(skill);

        auto* const sanctuary = prop::make(prop::Id::sanctuary);

        sanctuary->set_duration(prop_duration.roll());

        caster->m_properties.apply(sanctuary);
}

std::vector<std::string> SpellSanctuary::descr_specific(
        SpellSkill skill) const
{
        std::vector<std::string> descr;

        descr.emplace_back(
                "The caster is ignored by all hostile creatures for the "
                "duration of the spell. The effect is interrupted if the "
                "caster moves or performs a melee or ranged attack.");

        descr.emplace_back(
                "The spell lasts for " +
                duration(skill).str() +
                " turns.");

        return descr;
}

// -----------------------------------------------------------------------------
// Exorcist Purge
// -----------------------------------------------------------------------------
Range SpellPurge::dmg_range() const
{
        return {5, 10};
}

Range SpellPurge::fear_duration_range() const
{
        return {3, 6};
}

void SpellPurge::run_effect(
        actor::Actor* caster,
        SpellSkill skill,
        const std::vector<actor::Actor*>& seen_targets) const
{
        (void)skill;
        (void)seen_targets;

        if (!caster) {
                return;
        }

        for (const auto& d : dir_utils::g_dir_list) {
                const auto p(caster->m_pos + d);

                auto* const terrain = map::g_terrain.at(p);

                switch (terrain->id()) {
                case terrain::Id::altar:
                case terrain::Id::monolith:
                case terrain::Id::gong: {
                        if (map::g_seen.at(p)) {
                                draw_blast_at_cells({p}, colors::light_white());
                        }

                        terrain->hit(DmgType::pure, caster);
                } break;

                default:
                {
                } break;
                }
        }

        for (auto* const actor : game_time::g_actors) {
                if ((actor == caster) ||
                    !actor->m_pos.is_adjacent(caster->m_pos) ||
                    !actor->m_data->is_undead) {
                        continue;
                }

                // Is adjacent undead creature

                if (actor::can_player_see_actor(*actor)) {
                        const auto name = text_format::first_to_upper(actor->name_the());

                        msg_log::add(name + " is struck.", colors::msg_good());

                        draw_blast_at_cells({actor->m_pos}, colors::light_white());
                }

                actor::hit(
                        *actor,
                        dmg_range().roll(),
                        DmgType::pure,
                        caster);

                if (actor->is_alive()) {
                        prop::Prop* const fear = prop::make(prop::Id::terrified);

                        fear->set_duration(fear_duration_range().roll());

                        actor->m_properties.apply(fear);
                }
        }
}

std::vector<std::string> SpellPurge::descr_specific(
        SpellSkill skill) const
{
        (void)skill;

        std::vector<std::string> descr;

        descr.emplace_back(
                "Destroys any altars, monoliths, or gongs adjacent to the "
                "caster.");

        descr.emplace_back(
                "All undead creatures adjacent to the caster (seen or not) are "
                "struck with " +
                dmg_range().str() +
                " damage, and become terrified for " +
                fear_duration_range().str() +
                " turns (unless they resist fear).");

        return descr;
}

// -----------------------------------------------------------------------------
// Ghoul frenzy
// -----------------------------------------------------------------------------
void SpellFrenzy::run_effect(
        actor::Actor* const caster,
        const SpellSkill skill,
        const std::vector<actor::Actor*>& seen_targets) const
{
        (void)skill;
        (void)seen_targets;

        auto* prop = prop::make(prop::Id::frenzied);

        prop->set_duration(rnd::range(30, 40));

        caster->m_properties.apply(prop);
}

std::vector<std::string> SpellFrenzy::descr_specific(
        const SpellSkill skill) const
{
        (void)skill;

        return {
                "Incites a great rage in the caster, who will charge their "
                "enemies with a terrible, uncontrollable fury."};
}

// -----------------------------------------------------------------------------
// Bless
// -----------------------------------------------------------------------------
Range SpellBless::duration_range(SpellSkill skill) const
{
        switch (skill) {
        case SpellSkill::basic:
                return {15, 30};

        case SpellSkill::expert:
                return {60, 120};

        case SpellSkill::master:
                return {150, 300};

        case SpellSkill::transcendent:
                // Unexpected, the spell should be indefinite
                break;
        }

        ASSERT(false);

        return {1, 1};
}

void SpellBless::run_effect(
        actor::Actor* const caster,
        const SpellSkill skill,
        const std::vector<actor::Actor*>& seen_targets) const
{
        (void)seen_targets;

        auto* prop = prop::make(prop::Id::blessed);

        if (skill == SpellSkill::transcendent) {
                prop->set_indefinite();
        }
        else {
                prop->set_duration(duration_range(skill).roll());
        }

        caster->m_properties.apply(prop);
}

std::vector<std::string> SpellBless::descr_specific(
        const SpellSkill skill) const
{
        std::vector<std::string> descr;

        descr.emplace_back(
                "The caster becomes more lucky (+10% to hit chance, "
                "evasion, stealth, and searching).");

        if (skill == SpellSkill::transcendent) {
                descr.emplace_back("The spell lasts indefinitely.");
        }
        else {
                descr.push_back(
                        "The spell lasts " +
                        duration_range(skill).str() +
                        " turns.");
        }

        return descr;
}

// -----------------------------------------------------------------------------
// Light
// -----------------------------------------------------------------------------
Range SpellLight::light_duration_range(const SpellSkill skill) const
{
        switch (skill) {
        case SpellSkill::basic:
                return {10, 20};

        case SpellSkill::expert:
                return {15, 30};

        case SpellSkill::master:
        case SpellSkill::transcendent:
                return {20, 40};
        }

        ASSERT(false);

        return {1, 1};
}

Range SpellLight::blind_duration_range(const SpellSkill skill) const
{
        switch (skill) {
        case SpellSkill::basic:
        case SpellSkill::expert:
                // Not expected, should not cause blinding at these levels.
                break;

        case SpellSkill::master:
                return {2, 4};

        case SpellSkill::transcendent:
                return {5, 7};
        }

        ASSERT(false);

        return {1, 1};
}

Range SpellLight::burning_duration_range() const
{
        return {3, 6};
}

void SpellLight::run_effect(
        actor::Actor* const caster,
        const SpellSkill skill,
        const std::vector<actor::Actor*>& seen_targets) const
{
        (void)seen_targets;

        auto* radiant = prop::make(prop::Id::radiant_fov);

        radiant->set_duration(light_duration_range(skill).roll());

        caster->m_properties.apply(radiant);

        std::vector<prop::Prop*> properties;

        if (skill >= SpellSkill::master) {
                auto* const prop = prop::make(prop::Id::blind);

                prop->set_duration(blind_duration_range(skill).roll());

                properties.push_back(prop);
        }

        if (skill == SpellSkill::transcendent) {
                auto* const prop = prop::make(prop::Id::burning);

                prop->set_duration(burning_duration_range().roll());

                properties.push_back(prop);
        }

        if (!properties.empty()) {
                explosion::run(
                        caster->m_pos,
                        ExplType::apply_prop,
                        EmitExplSnd::no,
                        -1,
                        ExplExclCenter::yes,
                        properties,
                        colors::yellow());
        }
}

std::vector<std::string> SpellLight::descr_specific(
        const SpellSkill skill) const
{
        std::vector<std::string> descr;

        descr.emplace_back("Illuminates the area around the caster.");

        descr.push_back(
                "The spell lasts " +
                light_duration_range(skill).str() +
                " turns.");

        if (skill >= SpellSkill::master) {
                descr.push_back(
                        "On casting, causes a blinding flash centered on the "
                        "caster (but not affecting the caster itself). "
                        "The blinding effect lasts for " +
                        blind_duration_range(skill).str() +
                        " turns.");
        }

        if (skill == SpellSkill::transcendent) {
                descr.push_back(
                        "The flash is so intense that any victim caught in it "
                        "will also burn for " +
                        burning_duration_range().str() +
                        " turns.");
        }

        return descr;
}

// -----------------------------------------------------------------------------
// Invisibility
// -----------------------------------------------------------------------------
int SpellInvis::base_max_cost(const SpellSkill skill) const
{
        (void)skill;

        return 8;
}

Range SpellInvis::duration_range(SpellSkill skill) const
{
        switch (skill) {
        case SpellSkill::basic:
                return {4, 6};

        case SpellSkill::expert:
                return {5, 7};

        case SpellSkill::master:
                return {6, 8};

        case SpellSkill::transcendent:
                return {8, 10};
        }

        ASSERT(false);

        return {1, 1};
}

bool SpellInvis::is_noisy(const SpellSkill skill) const
{
        (void)skill;

        return false;
}

void SpellInvis::run_effect(
        actor::Actor* const caster,
        const SpellSkill skill,
        const std::vector<actor::Actor*>& seen_targets) const
{
        (void)seen_targets;

        const prop::Id prop_id =
                (skill == SpellSkill::basic)
                ? prop::Id::cloaked
                : prop::Id::invis;

        prop::Prop* const prop = prop::make(prop_id);

        prop->set_duration(duration_range(skill).roll());

        caster->m_properties.apply(prop);
}

std::vector<std::string> SpellInvis::descr_specific(
        const SpellSkill skill) const
{
        std::vector<std::string> descr;

        descr.emplace_back(
                "Makes the caster invisible to normal vision for a "
                "brief time.");

        if (skill == SpellSkill::basic) {
                descr.emplace_back(
                        "Attacking or casting spells reveals the caster.");
        }
        else {
                descr.emplace_back(
                        "The caster is truly invisible for the duration of "
                        "the the spell, and can freely attack or cast "
                        "spells without breaking the invisibility.");
        }

        descr.push_back(
                "The spell lasts " +
                duration_range(skill).str() +
                " turns.");

        return descr;
}

// -----------------------------------------------------------------------------
// See Invisible
// -----------------------------------------------------------------------------
int SpellSeeInvis::base_max_cost(const SpellSkill skill) const
{
        (void)skill;

        return 8;
}

Range SpellSeeInvis::duration_range(SpellSkill skill) const
{
        switch (skill) {
        case SpellSkill::basic:
                return {5, 10};

        case SpellSkill::expert:
                return {40, 80};

        case SpellSkill::master:
                return {250, 500};

        case SpellSkill::transcendent:
                return {600, 1200};
        }

        ASSERT(false);

        return {1, 1};
}

void SpellSeeInvis::run_effect(
        actor::Actor* const caster,
        const SpellSkill skill,
        const std::vector<actor::Actor*>& seen_targets) const
{
        (void)seen_targets;

        prop::Prop* prop = prop::make(prop::Id::see_invis);

        prop->set_duration(duration_range(skill).roll());

        caster->m_properties.apply(prop);
}

std::vector<std::string> SpellSeeInvis::descr_specific(
        const SpellSkill skill) const
{
        std::vector<std::string> descr;

        descr.emplace_back(
                "Grants the caster the ability to see the invisible.");

        descr.push_back(
                "The spell lasts " +
                duration_range(skill).str() +
                " turns.");

        return descr;
}

bool SpellSeeInvis::allow_mon_cast_now(
        const actor::Actor& mon,
        const std::vector<actor::Actor*>& seen_targets) const
{
        (void)seen_targets;

        return (
                !mon.m_properties.has(prop::Id::see_invis) &&
                mon.is_aware_of_player() &&
                rnd::one_in(8));
}

// -----------------------------------------------------------------------------
// Spell Shield
// -----------------------------------------------------------------------------
int SpellSpellShield::base_max_cost(const SpellSkill skill) const
{
        return 5 - (int)skill;
}

void SpellSpellShield::run_effect(
        actor::Actor* const caster,
        const SpellSkill skill,
        const std::vector<actor::Actor*>& seen_targets) const
{
        (void)skill;
        (void)seen_targets;

        auto* prop = prop::make(prop::Id::r_spell);

        prop->set_indefinite();

        caster->m_properties.apply(prop);
}

std::vector<std::string> SpellSpellShield::descr_specific(
        const SpellSkill skill) const
{
        (void)skill;

        std::vector<std::string> descr;

        descr.emplace_back(
                "Grants protection against harmful spells. The effect lasts "
                "until a spell is blocked.");

        return descr;
}

bool SpellSpellShield::allow_mon_cast_now(
        const actor::Actor& mon,
        const std::vector<actor::Actor*>& seen_targets) const
{
        (void)seen_targets;

        return !mon.m_properties.has(prop::Id::r_spell);
}

// -----------------------------------------------------------------------------
// Haste
// -----------------------------------------------------------------------------
Range SpellHaste::duration_range(const SpellSkill skill) const
{
        switch (skill) {
        case SpellSkill::basic:
                return {5, 10};

        case SpellSkill::expert:
                return {10, 20};

        case SpellSkill::master:
                return {15, 30};

        case SpellSkill::transcendent:
                return {300, 600};
        }

        ASSERT(false);

        return {1, 1};
}

int SpellHaste::base_max_cost(const SpellSkill skill) const
{
        (void)skill;

        return 5;
}

void SpellHaste::run_effect(
        actor::Actor* const caster,
        const SpellSkill skill,
        const std::vector<actor::Actor*>& seen_targets) const
{
        (void)seen_targets;

        auto* prop = prop::make(prop::Id::hasted);

        prop->set_duration(duration_range(skill).roll());

        caster->m_properties.apply(prop);
}

std::vector<std::string> SpellHaste::descr_specific(
        const SpellSkill skill) const
{
        std::vector<std::string> descr;

        descr.emplace_back(
                "The caster moves faster relative to the world around them.");

        descr.push_back(
                "The spell lasts " +
                duration_range(skill).str() +
                " turns.");

        return descr;
}

int SpellHaste::mon_cooldown() const
{
        return 20;
}

bool SpellHaste::allow_mon_cast_now(
        const actor::Actor& mon,
        const std::vector<actor::Actor*>& seen_targets) const
{
        return (
                !seen_targets.empty() &&
                !mon.m_properties.has(prop::Id::hasted));
}

// -----------------------------------------------------------------------------
// Premonition
// -----------------------------------------------------------------------------
Range SpellPremonition::duration_range(const SpellSkill skill) const
{
        switch (skill) {
        case SpellSkill::basic:
                return {4, 8};

        case SpellSkill::expert:
                return {8, 16};

        case SpellSkill::master:
                return {12, 24};

        case SpellSkill::transcendent:
                return {20, 40};
        }

        ASSERT(false);

        return {1, 1};
}

int SpellPremonition::base_max_cost(const SpellSkill skill) const
{
        (void)skill;

        return 7;
}

void SpellPremonition::run_effect(
        actor::Actor* const caster,
        const SpellSkill skill,
        const std::vector<actor::Actor*>& seen_targets) const
{
        (void)seen_targets;

        auto* prop = prop::make(prop::Id::premonition);

        prop->set_duration(duration_range(skill).roll());

        caster->m_properties.apply(prop);
}

std::vector<std::string> SpellPremonition::descr_specific(
        const SpellSkill skill) const
{
        std::vector<std::string> descr;

        descr.emplace_back(
                "Grants foresight of attacks against the caster, "
                "making it extremely difficult for assailants to achieve a "
                "succesful hit.");

        descr.emplace_back(
                "The spell lasts " +
                duration_range(skill).str() +
                " turns.");

        return descr;
}

// -----------------------------------------------------------------------------
// Erudition
// -----------------------------------------------------------------------------
int SpellErudition::base_max_cost(const SpellSkill skill) const
{
        return 7 - (int)skill;
}

Range SpellErudition::get_duration_range(SpellSkill skill) const
{
        switch (skill) {
        case SpellSkill::basic:
                return {2, 4};

        case SpellSkill::expert:
                return {4, 8};

        case SpellSkill::master:
        case SpellSkill::transcendent:
                return {6, 12};
        }

        ASSERT(false);

        return {1, 1};
}

void SpellErudition::run_effect(
        actor::Actor* const caster,
        const SpellSkill skill,
        const std::vector<actor::Actor*>& seen_targets) const
{
        (void)seen_targets;

        {
                auto* prop = prop::make(prop::Id::erudition);

                prop->set_duration(get_duration_range(skill).roll());

                caster->m_properties.apply(prop);
        }

        if (skill == SpellSkill::transcendent) {
                auto* const prop =
                        caster->m_properties.prop(prop::Id::erudition);

                if (!prop) {
                        ASSERT(false);

                        return;
                }

                auto* const erudition = static_cast<prop::Erudition*>(prop);

                erudition->disable_end_on_spell_cast();
        }
}

std::vector<std::string> SpellErudition::descr_specific(
        const SpellSkill skill) const
{
        std::vector<std::string> descr;

        descr.emplace_back(
                "Temporarily bestows the caster with an expanded understanding "
                "of the esoteric mechanisms behind magical practice. "
                "The caster's skill is improved by one level for all spells.");

        std::string duration_descr =
                "The spell lasts " +
                get_duration_range(skill).str() +
                " turns";

        if (skill == SpellSkill::transcendent) {
                duration_descr +=
                        ". The effect does not end when casting spells, "
                        "only when the duration expires.";
        }
        else {
                duration_descr +=
                        ", or until a spell is cast (either from a Manuscript "
                        "or from memory).";
        }

        descr.push_back(duration_descr);

        return descr;
}

// -----------------------------------------------------------------------------
// Identify
// -----------------------------------------------------------------------------
int SpellIdentify::base_max_cost(const SpellSkill skill) const
{
        return ((skill == SpellSkill::transcendent) ? 4 : 8);
}

void SpellIdentify::run_effect(
        actor::Actor* const caster,
        const SpellSkill skill,
        const std::vector<actor::Actor*>& seen_targets) const
{
        (void)seen_targets;

        std::vector<ItemType> item_types_allowed;

        if (skill != SpellSkill::master) {
                item_types_allowed.push_back(ItemType::scroll);

                if (skill == SpellSkill::expert) {
                        item_types_allowed.push_back(ItemType::potion);
                }
        }

        if (skill == SpellSkill::transcendent) {
                // Immediately identify all items
                for (auto* const item : caster->m_inv.all_items()) {
                        item->identify(Verbose::yes);
                }
        }
        else {
                // Run identify selection menu
                auto state =
                        std::make_unique<SelectIdentify>(
                                item_types_allowed);

                states::push(std::move(state));
        }

        msg_log::more_prompt();
}

std::vector<std::string> SpellIdentify::descr_specific(
        const SpellSkill skill) const
{
        if (skill == SpellSkill::transcendent) {
                return {"Immediately identifies all carried items."};
        }

        std::vector<std::string> descr;

        descr.emplace_back("Identifies one carried item.");

        std::string identifies_str = "The spell can identify ";

        switch (skill) {
        case SpellSkill::basic:
                identifies_str += "Manuscripts";
                break;

        case SpellSkill::expert:
                identifies_str += "Manuscripts and Potions";
                break;

        case SpellSkill::master:
                identifies_str += "all items";
                break;

        case SpellSkill::transcendent:
                ASSERT(false);
                break;
        }

        identifies_str += ".";

        descr.push_back(identifies_str);

        return descr;
}

// -----------------------------------------------------------------------------
// Teleport
// -----------------------------------------------------------------------------
int SpellTeleport::max_dist(const SpellSkill skill) const
{
        switch (skill) {
        case SpellSkill::basic:
                return 5;

        case SpellSkill::expert:
                return 10;

        case SpellSkill::master:
        case SpellSkill::transcendent:
                return 15;
        }

        ASSERT(false);
        return -1;
}

int SpellTeleport::invis_duration(const SpellSkill skill) const
{
        return ((skill == SpellSkill::transcendent) ? 6 : 3);
}

void SpellTeleport::run_effect(
        actor::Actor* const caster,
        const SpellSkill skill,
        const std::vector<actor::Actor*>& seen_targets) const
{
        (void)seen_targets;

        if (skill >= SpellSkill::master) {
                auto* const invis = prop::make(prop::Id::invis);

                invis->set_duration(invis_duration(skill));

                caster->m_properties.apply(invis);
        }

        const int max_d = max_dist(skill);

        teleport(*caster, ShouldCtrlTele::if_tele_ctrl_prop, max_d);
}

bool SpellTeleport::allow_mon_cast_now(
        const actor::Actor& mon,
        const std::vector<actor::Actor*>& seen_targets) const
{
        const bool is_low_hp = (mon.m_hp <= (actor::max_hp(mon) / 2));

        return (
                !seen_targets.empty() &&
                is_low_hp &&
                rnd::fraction(3, 4));
}

std::vector<std::string> SpellTeleport::descr_specific(
        const SpellSkill skill) const
{
        std::vector<std::string> descr;

        descr.emplace_back(
                "Instantly moves the caster to a different position.");

        descr.emplace_back(
                "Maximum teleport distance is " +
                std::to_string(max_dist(skill)) +
                ".");

        if (skill >= SpellSkill::master) {
                descr.push_back(
                        "On teleporting, the caster is invisible for " +
                        std::to_string(invis_duration(skill)) +
                        " turns.");
        }

        return descr;
}

// -----------------------------------------------------------------------------
// Resistance
// -----------------------------------------------------------------------------
Range SpellResistance::duration_range(SpellSkill skill) const
{
        switch (skill) {
        case SpellSkill::basic:
                return {10, 20};

        case SpellSkill::expert:
                return {30, 60};

        case SpellSkill::master:
                return {80, 160};

        case SpellSkill::transcendent:
                return {300, 600};
        }

        ASSERT(false);

        return {1, 1};
}

void SpellResistance::run_effect(
        actor::Actor* const caster,
        const SpellSkill skill,
        const std::vector<actor::Actor*>& seen_targets) const
{
        (void)seen_targets;

        int nr_turns = duration_range(skill).roll();

        auto* prop_r_fire = prop::make(prop::Id::r_fire);
        auto* prop_r_elec = prop::make(prop::Id::r_elec);

        prop_r_fire->set_duration(nr_turns);
        prop_r_elec->set_duration(nr_turns);

        caster->m_properties.apply(prop_r_fire);
        caster->m_properties.apply(prop_r_elec);
}

std::vector<std::string> SpellResistance::descr_specific(
        const SpellSkill skill) const
{
        std::vector<std::string> descr;

        descr.emplace_back(
                "The caster is completely shielded from fire and electricity.");

        descr.push_back(
                "The spell lasts " +
                duration_range(skill).str() +
                " turns.");

        return descr;
}

bool SpellResistance::allow_mon_cast_now(
        const actor::Actor& mon,
        const std::vector<actor::Actor*>& seen_targets) const
{
        (void)seen_targets;

        const bool has_r_fire = mon.m_properties.has(prop::Id::r_fire);
        const bool has_r_elec = mon.m_properties.has(prop::Id::r_elec);

        return ((!has_r_fire || !has_r_elec) && mon.m_ai_state.target);
}

// -----------------------------------------------------------------------------
// Knockback
// -----------------------------------------------------------------------------
void SpellKnockBack::run_effect(
        actor::Actor* const caster,
        const SpellSkill skill,
        const std::vector<actor::Actor*>& seen_targets) const
{
        (void)skill;

        auto* target = map::random_closest_actor(caster->m_pos, seen_targets);

        ASSERT(target);

        // Spell resistance?
        if (target->m_properties.has(prop::Id::r_spell)) {
                on_resist(*target);

                // Spell reflection?
                if (target->m_properties.has(prop::Id::spell_reflect)) {
                        if (actor::can_player_see_actor(*target)) {
                                msg_log::add(
                                        s_spell_reflect_msg,
                                        colors::text(),
                                        MsgInterruptPlayer::no,
                                        MorePromptOnMsg::yes);
                        }

                        // Run effect with the target as caster, and the caster
                        // as seen target instead.
                        run_effect(target, skill, {caster});
                }

                return;
        }

        std::string target_str;
        Color msg_clr;

        if (actor::is_player(target)) {
                target_str = "me";

                msg_clr = colors::msg_bad();
        }
        else {
                // Target is monster
                target_str = target->name_the();

                msg_clr =
                        map::g_player->is_leader_of(target)
                        ? colors::white()
                        : colors::msg_good();
        }

        if (actor::can_player_see_actor(*target)) {
                msg_log::add("A force pushes " + target_str + "!", msg_clr);
        }

        knockback::run(
                *target,
                caster->m_pos,
                knockback::KnockbackSource::other);

        if (!actor::is_player(target)) {
                target->become_aware_player(actor::AwareSource::spell_victim);
        }
}

bool SpellKnockBack::allow_mon_cast_now(
        const actor::Actor& mon,
        const std::vector<actor::Actor*>& seen_targets) const
{
        (void)mon;

        return !seen_targets.empty();
}

// -----------------------------------------------------------------------------
// Curse
// -----------------------------------------------------------------------------
int SpellCurse::base_max_cost(const SpellSkill skill) const
{
        (void)skill;

        return 3;
}

int SpellCurse::mon_cooldown() const
{
        return 10;
}

void SpellCurse::run_effect(
        actor::Actor* const caster,
        const SpellSkill skill,
        const std::vector<actor::Actor*>& seen_targets) const
{
        Range duration_range;
        duration_range.min = 15 * ((int)skill + 1);
        duration_range.max = duration_range.min * 2;

        const int duration = duration_range.roll();

        if (seen_targets.empty()) {
                return;
        }

        // There are targets available

        std::vector<actor::Actor*> targets;

        if (skill == SpellSkill::basic) {
                targets = {rnd::element(seen_targets)};
        }
        else {
                targets = seen_targets;
        }

        draw_blast_at_seen_actors(targets, colors::magenta());

        for (auto* const target : targets) {
                // Spell resistance?
                if (target->m_properties.has(prop::Id::r_spell)) {
                        on_resist(*target);

                        // Spell reflection?
                        if (target->m_properties.has(prop::Id::spell_reflect)) {
                                if (actor::can_player_see_actor(*target)) {
                                        msg_log::add(
                                                s_spell_reflect_msg,
                                                colors::text(),
                                                MsgInterruptPlayer::no,
                                                MorePromptOnMsg::yes);
                                }

                                // Run effect with the target as caster, and the
                                // caster as seen target instead.
                                run_effect(target, skill, {caster});
                        }

                        continue;
                }

                auto id = prop::Id::cursed;

                if (skill == SpellSkill::master) {
                        id = prop::Id::doomed;
                }

                auto* const prop = prop::make(id);

                prop->set_duration(duration);

                target->m_properties.apply(prop);

                if (!actor::is_player(target)) {
                        target->become_aware_player(
                                actor::AwareSource::spell_victim);
                }
        }
}

bool SpellCurse::allow_mon_cast_now(
        const actor::Actor& mon,
        const std::vector<actor::Actor*>& seen_targets) const
{
        (void)mon;

        return !seen_targets.empty();
}

// -----------------------------------------------------------------------------
// Enfeeble
// -----------------------------------------------------------------------------
Range SpellEnfeeble::duration_range(const SpellSkill skill) const
{
        switch (skill) {
        case SpellSkill::basic:
                return {15, 30};

        case SpellSkill::expert:
                return {30, 60};

        case SpellSkill::master:
                return {45, 90};

        case SpellSkill::transcendent:
                return {60, 120};
        }

        ASSERT(false);

        return {1, 1};
}

int SpellEnfeeble::base_max_cost(const SpellSkill skill) const
{
        (void)skill;

        return 3;
}

int SpellEnfeeble::mon_cooldown() const
{
        return 5;
}

void SpellEnfeeble::run_effect(
        actor::Actor* const caster,
        const SpellSkill skill,
        const std::vector<actor::Actor*>& seen_targets) const
{
        const int duration = duration_range(skill).roll();

        if (seen_targets.empty()) {
                msg_log::add(
                        "The bugs on the ground suddenly move very feebly.");

                return;
        }

        // There are targets available

        std::vector<actor::Actor*> targets;

        if (skill == SpellSkill::basic) {
                targets = {rnd::element(seen_targets)};
        }
        else {
                targets = seen_targets;
        }

        draw_blast_at_seen_actors(targets, colors::magenta());

        for (auto* const target : targets) {
                // Spell resistance?
                if (target->m_properties.has(prop::Id::r_spell)) {
                        on_resist(*target);

                        // Spell reflection?
                        if (target->m_properties.has(prop::Id::spell_reflect)) {
                                if (actor::can_player_see_actor(*target)) {
                                        msg_log::add(
                                                s_spell_reflect_msg,
                                                colors::text(),
                                                MsgInterruptPlayer::no,
                                                MorePromptOnMsg::yes);
                                }

                                // Run effect with the target as caster, and the
                                // caster as seen target instead.
                                run_effect(target, skill, {caster});
                        }

                        continue;
                }

                auto* const prop = prop::make(prop::Id::weakened);

                prop->set_duration(duration);

                target->m_properties.apply(prop);

                if (!actor::is_player(target)) {
                        target->become_aware_player(
                                actor::AwareSource::spell_victim);
                }
        }
}

std::vector<std::string> SpellEnfeeble::descr_specific(
        const SpellSkill skill) const
{
        (void)skill;

        std::vector<std::string> descr;

        descr.emplace_back(
                "Physically enfeebles the spell's victims, causing them to "
                "only do half damage in melee combat.");

        if (skill == SpellSkill::basic) {
                descr.emplace_back(
                        "Affects one random visible hostile creature.");
        }
        else {
                descr.emplace_back(
                        "Affects all visible hostile creatures.");
        }

        descr.push_back(
                "The spell lasts " +
                duration_range(skill).str() +
                " turns.");

        return descr;
}

bool SpellEnfeeble::allow_mon_cast_now(
        const actor::Actor& mon,
        const std::vector<actor::Actor*>& seen_targets) const
{
        (void)mon;

        return !seen_targets.empty();
}

// -----------------------------------------------------------------------------
// Slow
// -----------------------------------------------------------------------------
Range SpellSlow::duration_range(const SpellSkill skill) const
{
        switch (skill) {
        case SpellSkill::basic:
                return {10, 20};

        case SpellSkill::expert:
                return {20, 40};

        case SpellSkill::master:
                return {30, 60};

        case SpellSkill::transcendent:
                return {40, 80};
        }

        ASSERT(false);

        return {1, 1};
}

int SpellSlow::base_max_cost(const SpellSkill skill) const
{
        (void)skill;

        return 4;
}

void SpellSlow::run_effect(
        actor::Actor* const caster,
        const SpellSkill skill,
        const std::vector<actor::Actor*>& seen_targets) const
{
        const int duration = duration_range(skill).roll();

        if (seen_targets.empty()) {
                msg_log::add(
                        "The bugs on the ground suddenly move very slowly.");

                return;
        }

        // There are targets available

        std::vector<actor::Actor*> targets;

        if (skill == SpellSkill::basic) {
                targets = {rnd::element(seen_targets)};
        }
        else {
                targets = seen_targets;
        }

        draw_blast_at_seen_actors(targets, colors::magenta());

        for (auto* const target : targets) {
                // Spell resistance?
                if (target->m_properties.has(prop::Id::r_spell)) {
                        on_resist(*target);

                        // Spell reflection?
                        if (target->m_properties.has(prop::Id::spell_reflect)) {
                                if (actor::can_player_see_actor(*target)) {
                                        msg_log::add(
                                                s_spell_reflect_msg,
                                                colors::text(),
                                                MsgInterruptPlayer::no,
                                                MorePromptOnMsg::yes);
                                }

                                // Run effect with the target as caster, and the
                                // caster as seen target instead.
                                run_effect(target, skill, {caster});
                        }

                        continue;
                }

                auto* const prop = prop::make(prop::Id::slowed);

                prop->set_duration(duration);

                target->m_properties.apply(prop);

                if (!actor::is_player(target)) {
                        target->become_aware_player(
                                actor::AwareSource::spell_victim);
                }
        }
}

std::vector<std::string> SpellSlow::descr_specific(
        const SpellSkill skill) const
{
        (void)skill;

        std::vector<std::string> descr;

        descr.emplace_back(
                "Causes the spell's victims to move more slowly.");

        if (skill == SpellSkill::basic) {
                descr.emplace_back(
                        "Affects one random visible hostile creature.");
        }
        else {
                descr.emplace_back(
                        "Affects all visible hostile creatures.");
        }

        descr.push_back(
                "The spell lasts " +
                duration_range(skill).str() +
                " turns.");

        return descr;
}

int SpellSlow::mon_cooldown() const
{
        return 20;
}

bool SpellSlow::allow_mon_cast_now(
        const actor::Actor& mon,
        const std::vector<actor::Actor*>& seen_targets) const
{
        (void)mon;

        return !seen_targets.empty();
}

// -----------------------------------------------------------------------------
// Terrify
// -----------------------------------------------------------------------------
Range SpellTerrify::duration_range(SpellSkill skill) const
{
        switch (skill) {
        case SpellSkill::basic:
                return {6, 12};

        case SpellSkill::expert:
                return {12, 24};

        case SpellSkill::master:
                return {18, 36};

        case SpellSkill::transcendent:
                return {24, 48};
        }

        ASSERT(false);

        return {1, 1};
}

int SpellTerrify::base_max_cost(const SpellSkill skill) const
{
        (void)skill;

        return 5;
}

int SpellTerrify::mon_cooldown() const
{
        return 5;
}

void SpellTerrify::run_effect(
        actor::Actor* const caster,
        const SpellSkill skill,
        const std::vector<actor::Actor*>& seen_targets) const
{
        if (seen_targets.empty()) {
                msg_log::add("The bugs on the ground suddenly scatter away.");

                return;
        }

        // There are targets available

        std::vector<actor::Actor*> targets;

        if (skill == SpellSkill::basic) {
                targets = {rnd::element(seen_targets)};
        }
        else {
                targets = seen_targets;
        }

        draw_blast_at_seen_actors(targets, colors::magenta());

        for (auto* const target : targets) {
                // Spell resistance?
                if (target->m_properties.has(prop::Id::r_spell)) {
                        on_resist(*target);

                        // Spell reflection?
                        if (target->m_properties.has(prop::Id::spell_reflect)) {
                                if (actor::can_player_see_actor(*target)) {
                                        msg_log::add(
                                                s_spell_reflect_msg,
                                                colors::text(),
                                                MsgInterruptPlayer::no,
                                                MorePromptOnMsg::yes);
                                }

                                // Run effect with the target as caster, and the
                                // caster as seen target instead.
                                run_effect(target, skill, {caster});
                        }

                        continue;
                }

                auto* const prop = prop::make(prop::Id::terrified);

                prop->set_duration(duration_range(skill).roll());

                target->m_properties.apply(prop);

                if (!actor::is_player(target)) {
                        target->become_aware_player(
                                actor::AwareSource::spell_victim);
                }
        }
}

std::vector<std::string> SpellTerrify::descr_specific(
        const SpellSkill skill) const
{
        (void)skill;

        std::vector<std::string> descr;

        descr.emplace_back(
                "Manifests an overpowering feeling of dread in the spell's "
                "victims.");

        if (skill == SpellSkill::basic) {
                descr.emplace_back(
                        "Affects one random visible hostile creature.");
        }
        else {
                descr.emplace_back(
                        "Affects all visible hostile creatures.");
        }

        descr.push_back(
                "The spell lasts " +
                duration_range(skill).str() +
                " turns.");

        return descr;
}

bool SpellTerrify::allow_mon_cast_now(
        const actor::Actor& mon,
        const std::vector<actor::Actor*>& seen_targets) const
{
        (void)mon;

        return !seen_targets.empty();
}

// -----------------------------------------------------------------------------
// Disease
// -----------------------------------------------------------------------------
void SpellDisease::run_effect(
        actor::Actor* const caster,
        const SpellSkill skill,
        const std::vector<actor::Actor*>& seen_targets) const
{
        (void)skill;

        auto* target = map::random_closest_actor(caster->m_pos, seen_targets);

        ASSERT(target);

        // Spell resistance?
        if (target->m_properties.has(prop::Id::r_spell)) {
                on_resist(*target);

                // Spell reflection?
                if (target->m_properties.has(prop::Id::spell_reflect)) {
                        if (actor::can_player_see_actor(*target)) {
                                msg_log::add(
                                        s_spell_reflect_msg,
                                        colors::text(),
                                        MsgInterruptPlayer::no,
                                        MorePromptOnMsg::yes);
                        }

                        // Run effect with the target as caster, and the caster
                        // as seen target instead.
                        run_effect(target, skill, {caster});
                }

                return;
        }

        if (actor::can_player_see_actor(*target)) {
                const std::string actor_name =
                        actor::is_player(target)
                        ? "me"
                        : target->name_the();

                msg_log::add(
                        "A horrible disease is starting to afflict " +
                        actor_name +
                        "!");
        }

        target->m_properties.apply(
                prop::make(
                        prop::Id::diseased));

        if (!actor::is_player(target)) {
                target->become_aware_player(actor::AwareSource::spell_victim);
        }
}

bool SpellDisease::allow_mon_cast_now(
        const actor::Actor& mon,
        const std::vector<actor::Actor*>& seen_targets) const
{
        (void)mon;

        return !seen_targets.empty();
}

// -----------------------------------------------------------------------------
// Summon monster
// -----------------------------------------------------------------------------
void SpellSummonMon::run_effect(
        actor::Actor* const caster,
        const SpellSkill skill,
        const std::vector<actor::Actor*>& seen_targets) const
{
        (void)seen_targets;

        Range mon_dlvl_range = get_allowed_mon_dlvl_range(skill);

        std::vector<std::string> summon_bucket =
                make_summon_bucket(mon_dlvl_range);

        if (summon_bucket.empty()) {
                // No eligible monsters found, try again but allow monsters from
                // as early depth as possible.

                mon_dlvl_range.min = 0;

                summon_bucket = make_summon_bucket(mon_dlvl_range);
        }

        if (summon_bucket.empty()) {
                TRACE
                        << "No elligible monsters found for spawning"
                        << std::endl;

                ASSERT(false);

                return;
        }

#ifndef NDEBUG
        for (const std::string& id : summon_bucket) {
                ASSERT(actor::g_data[id].can_be_summoned_by_mon);
        }
#endif  // NDEBUG

        const auto id = rnd::element(summon_bucket);

        summon(id, caster);
}

Range SpellSummonMon::get_allowed_mon_dlvl_range(const SpellSkill skill) const
{
        Range dlvl_range;

        switch (skill) {
        case SpellSkill::basic:
                dlvl_range.min = 0;
                dlvl_range.max = g_dlvl_last_early_game;
                break;

        case SpellSkill::expert:
                dlvl_range.min = 0;
                dlvl_range.max = g_dlvl_last_mid_game;
                break;

        case SpellSkill::master:
        case SpellSkill::transcendent:
                dlvl_range.min = g_dlvl_first_mid_game;
                dlvl_range.max = g_dlvl_last;
                break;
        }

        const int dlvl = map::g_dlvl + 2;

        dlvl_range.min = std::min(dlvl_range.min, dlvl);
        dlvl_range.max = std::min(dlvl_range.max, dlvl);

        return dlvl_range;
}

std::vector<std::string> SpellSummonMon::make_summon_bucket(
        const Range& dlvl_range) const
{
        std::vector<std::string> summon_bucket;

        for (auto& it : actor::g_data) {
                const actor::ActorData& data = it.second;

                if (!data.can_be_summoned_by_mon) {
                        continue;
                }

                // NOTE: The "min" dungeon level in the monster data is used
                // here as a general "strength" of the monster. The "max"
                // dungeon level is not considered.
                const int mon_dlvl = data.spawn_min_dlvl;

                if (!dlvl_range.is_in_range(mon_dlvl)) {
                        continue;
                }

                summon_bucket.push_back(data.id);
        }

        return summon_bucket;
}

void SpellSummonMon::summon(const std::string& id, actor::Actor* caster) const
{
        auto* const caster_leader = caster->m_leader;

        auto* const leader =
                caster_leader
                ? caster_leader
                : caster;

        const auto summoned =
                actor::spawn(
                        caster->m_pos,
                        {id},
                        map::rect())
                        .make_aware_of_player()
                        .set_leader(leader);

        std::for_each(
                std::begin(summoned.monsters),
                std::end(summoned.monsters),
                [](auto* const mon) {
                        mon->m_properties.apply(
                                prop::make(prop::Id::summoned));

                        auto* prop_waiting =
                                prop::make(prop::Id::waiting);

                        prop_waiting->set_duration(2);

                        mon->m_properties.apply(prop_waiting);
                });

        if (summoned.monsters.empty()) {
                return;
        }

        auto* const mon = summoned.monsters[0];

        if (actor::can_player_see_actor(*mon)) {
                msg_log::add(
                        text_format::first_to_upper(mon->name_a()) +
                        " appears!");

                actor::make_player_aware_mon(*mon);
        }
}

bool SpellSummonMon::allow_mon_cast_now(
        const actor::Actor& mon,
        const std::vector<actor::Actor*>& seen_targets) const
{
        // Always allow casting with a visible target.
        if (!seen_targets.empty()) {
                return true;
        }

        // Sometimes allow casting if monster has an unseen target.
        if (mon.m_ai_state.target && rnd::one_in(30)) {
                return true;
        }

        return false;
}

// -----------------------------------------------------------------------------
// Summon tentacles
// -----------------------------------------------------------------------------
void SpellSummonTentacles::run_effect(
        actor::Actor* const caster,
        const SpellSkill skill,
        const std::vector<actor::Actor*>& seen_targets) const
{
        (void)skill;
        (void)seen_targets;

        actor::Actor* leader = nullptr;

        if (actor::is_player(caster)) {
                leader = caster;
        }
        else {
                // Caster is monster
                actor::Actor* const caster_leader = caster->m_leader;

                leader =
                        caster_leader
                        ? caster_leader
                        : caster;
        }

        const auto summoned =
                actor::spawn(
                        caster->m_pos,
                        {"MON_TENTACLE_CLUSTER"},
                        map::rect())
                        .make_aware_of_player()
                        .set_leader(leader);

        std::for_each(
                std::begin(summoned.monsters),
                std::end(summoned.monsters),
                [](auto* const mon) {
                        mon->m_properties.apply(
                                prop::make(prop::Id::summoned));

                        auto* prop_waiting =
                                prop::make(prop::Id::waiting);

                        prop_waiting->set_duration(2);

                        mon->m_properties.apply(prop_waiting);
                });

        if (summoned.monsters.empty()) {
                return;
        }

        auto* const mon = summoned.monsters[0];

        if (actor::can_player_see_actor(*mon)) {
                msg_log::add("Monstrous tentacles rise up from the ground!");

                actor::make_player_aware_mon(*mon);
        }
}

bool SpellSummonTentacles::allow_mon_cast_now(
        const actor::Actor& mon,
        const std::vector<actor::Actor*>& seen_targets) const
{
        // Always allow casting with a visible target.
        if (!seen_targets.empty()) {
                return true;
        }

        // Sometimes allow casting if monster has an unseen target.
        if (mon.m_ai_state.target && rnd::one_in(30)) {
                return true;
        }

        return false;
}

// -----------------------------------------------------------------------------
// Healing
// -----------------------------------------------------------------------------
int SpellHeal::nr_hp_restored(SpellSkill skill) const
{
        return 8 + (int)skill * 8;
}

Range SpellHeal::regen_duration() const
{
        return {50, 100};
}

void SpellHeal::run_effect(
        actor::Actor* const caster,
        const SpellSkill skill,
        const std::vector<actor::Actor*>& seen_targets) const
{
        (void)seen_targets;

        if ((int)skill >= (int)SpellSkill::expert) {
                caster->m_properties.end_prop(prop::Id::infected);
                caster->m_properties.end_prop(prop::Id::diseased);
                caster->m_properties.end_prop(prop::Id::weakened);
                caster->m_properties.end_prop(prop::Id::hp_sap);
                caster->m_properties.end_prop(prop::Id::poisoned);
        }

        if (skill >= SpellSkill::master) {
                caster->m_properties.end_prop(prop::Id::blind);
                caster->m_properties.end_prop(prop::Id::deaf);

                if (actor::is_player(caster)) {
                        auto* const wound_prop =
                                map::g_player->m_properties.prop(prop::Id::wound);

                        if (wound_prop) {
                                auto* const wound = static_cast<prop::Wound*>(wound_prop);

                                wound->heal_one_wound();
                        }
                }
        }

        if (skill == SpellSkill::transcendent) {
                auto* const prop =
                        prop::make(
                                prop::Id::regenerating);

                prop->set_duration(regen_duration().roll());

                caster->m_properties.apply(prop);
        }

        caster->restore_hp(nr_hp_restored(skill));
}

bool SpellHeal::allow_mon_cast_now(
        const actor::Actor& mon,
        const std::vector<actor::Actor*>& seen_targets) const
{
        (void)seen_targets;

        return mon.m_hp < actor::max_hp(mon);
}

std::vector<std::string> SpellHeal::descr_specific(
        const SpellSkill skill) const
{
        std::vector<std::string> descr;

        descr.push_back(
                "Restores " +
                std::to_string(nr_hp_restored(skill)) +
                " hit points.");

        if (skill == SpellSkill::expert) {
                descr.emplace_back(
                        "Cures infections, disease, weakening, life sapping, "
                        "and poisoning.");
        }
        else if (skill >= SpellSkill::master) {
                descr.emplace_back(
                        "Cures infections, disease, weakening, life sapping, "
                        "poisoning, blindness, and deafness.");

                descr.emplace_back("Heals one wound.");
        }

        if (skill == SpellSkill::transcendent) {
                descr.emplace_back(
                        "+1 hit point regenerated per turn, for " +
                        regen_duration().str() +
                        " turns.");
        }

        return descr;
}

// -----------------------------------------------------------------------------
// Mi-go hypnosis
// -----------------------------------------------------------------------------
void SpellMiGoHypno::run_effect(
        actor::Actor* const caster,
        const SpellSkill skill,
        const std::vector<actor::Actor*>& seen_targets) const
{
        (void)skill;

        actor::Actor* target = map::random_closest_actor(caster->m_pos, seen_targets);

        if (!target) {
                ASSERT(false);

                return;
        }

        // Spell resistance?
        if (target->m_properties.has(prop::Id::r_spell)) {
                on_resist(*target);

                // Spell reflection?
                if (target->m_properties.has(prop::Id::spell_reflect)) {
                        if (actor::can_player_see_actor(*target)) {
                                msg_log::add(
                                        s_spell_reflect_msg,
                                        colors::text(),
                                        MsgInterruptPlayer::no,
                                        MorePromptOnMsg::yes);
                        }

                        // Run effect with the target as caster, and the caster
                        // as seen target instead.
                        run_effect(target, skill, {caster});
                }

                return;
        }

        if (actor::is_player(target)) {
                msg_log::add("There is a sharp droning in my head!");
        }

        if (rnd::coin_toss()) {
                prop::Prop* prop_fainted = prop::make(prop::Id::fainted);

                prop_fainted->set_duration(rnd::range(2, 10));

                target->m_properties.apply(prop_fainted);
        }
        else {
                if (actor::is_player(target)) {
                        msg_log::add("I feel dizzy.");
                }
        }

        if (!actor::is_player(target)) {
                target->become_aware_player(actor::AwareSource::spell_victim);
        }
}

bool SpellMiGoHypno::allow_mon_cast_now(
        const actor::Actor& mon,
        const std::vector<actor::Actor*>& seen_targets) const
{
        (void)mon;

        return !seen_targets.empty();
}

// -----------------------------------------------------------------------------
// Immolation
// -----------------------------------------------------------------------------
void SpellBurn::run_effect(
        actor::Actor* const caster,
        const SpellSkill skill,
        const std::vector<actor::Actor*>& seen_targets) const
{
        auto* target = map::random_closest_actor(caster->m_pos, seen_targets);

        ASSERT(target);

        // Spell resistance?
        if (target->m_properties.has(prop::Id::r_spell)) {
                on_resist(*target);

                // Spell reflection?
                if (target->m_properties.has(prop::Id::spell_reflect)) {
                        if (actor::can_player_see_actor(*target)) {
                                msg_log::add(
                                        s_spell_reflect_msg,
                                        colors::text(),
                                        MsgInterruptPlayer::no,
                                        MorePromptOnMsg::yes);
                        }

                        // Run effect with the target as caster, and the caster
                        // as seen target instead.
                        run_effect(target, skill, {caster});
                }

                return;
        }

        if (actor::can_player_see_actor(*target)) {
                const std::string actor_name =
                        actor::is_player(target)
                        ? "me"
                        : target->name_the();

                msg_log::add("Flames are rising around " + actor_name + "!");
        }

        auto* prop = prop::make(prop::Id::burning);

        prop->set_duration(2 + (int)skill);

        target->m_properties.apply(prop);

        if (!actor::is_player(target)) {
                target->become_aware_player(actor::AwareSource::spell_victim);
        }
}

bool SpellBurn::allow_mon_cast_now(
        const actor::Actor& mon,
        const std::vector<actor::Actor*>& seen_targets) const
{
        (void)mon;

        return !seen_targets.empty();
}

// -----------------------------------------------------------------------------
// Deafening
// -----------------------------------------------------------------------------
void SpellDeafen::run_effect(
        actor::Actor* const caster,
        const SpellSkill skill,
        const std::vector<actor::Actor*>& seen_targets) const
{
        auto* target = map::random_closest_actor(caster->m_pos, seen_targets);

        ASSERT(target);

        // Spell resistance?
        if (target->m_properties.has(prop::Id::r_spell)) {
                on_resist(*target);

                // Spell reflection?
                if (target->m_properties.has(prop::Id::spell_reflect)) {
                        if (actor::can_player_see_actor(*target)) {
                                msg_log::add(
                                        s_spell_reflect_msg,
                                        colors::text(),
                                        MsgInterruptPlayer::no,
                                        MorePromptOnMsg::yes);
                        }

                        // Run effect with the target as caster, and the caster
                        // as seen target instead.
                        run_effect(target, skill, {caster});
                }

                return;
        }

        auto* prop = prop::make(prop::Id::deaf);

        prop->set_duration(75 + (int)skill * 75);

        target->m_properties.apply(prop);

        if (!actor::is_player(target)) {
                target->become_aware_player(actor::AwareSource::spell_victim);
        }
}

bool SpellDeafen::allow_mon_cast_now(
        const actor::Actor& mon,
        const std::vector<actor::Actor*>& seen_targets) const
{
        (void)mon;

        return !seen_targets.empty();
}

// -----------------------------------------------------------------------------
// Transmutation
// -----------------------------------------------------------------------------
int SpellTransmut::skill_bon(const SpellSkill skill) const
{
        return 10 * (int)skill;
}

int SpellTransmut::chance_scroll(const SpellSkill skill) const
{
        return skill_bon(skill) + 40;
}

int SpellTransmut::chance_potion(const SpellSkill skill) const
{
        return skill_bon(skill) + 40;
}

int SpellTransmut::chance_weapon(
        const SpellSkill skill,
        int plus) const
{
        plus = std::min(5, plus);

        return skill_bon(skill) + (plus * 10);
}

void SpellTransmut::run_effect(
        actor::Actor* const caster,
        const SpellSkill skill,
        const std::vector<actor::Actor*>& seen_targets) const
{
        (void)caster;
        (void)seen_targets;

        const auto& p = map::g_player->m_pos;

        auto* item_before = map::g_items.at(p);

        if (!item_before) {
                msg_log::add("There is a vague change in the air.");

                return;
        }

        // Player is standing on an item

        // Get information on the existing item(s)
        const bool is_stackable_before = item_before->data().is_stackable;

        const int nr_items_before =
                is_stackable_before
                ? item_before->m_nr_items
                : 1;

        const auto item_type_before = item_before->data().type;

        const int melee_plus = item_before->base_melee_dmg().plus();

        const auto id_before = item_before->id();

        std::string item_name_before = "The ";

        if (nr_items_before > 1) {
                item_name_before += item_before->name(ItemNameType::plural);
        }
        else {
                // Single item
                item_name_before += item_before->name(ItemNameType::plain);
        }

        // Remove the existing item(s)
        delete map::g_items.at(p);
        map::g_items.at(p) = nullptr;

        if (map::g_seen.at(p)) {
                std::string disappear_str =
                        (nr_items_before == 1)
                        ? "disappears"
                        : "disappear";

                msg_log::add(
                        item_name_before + " " + disappear_str + ".",
                        colors::text(),
                        MsgInterruptPlayer::no,
                        MorePromptOnMsg::yes);
        }

        // Determine which item(s) to spawn, if any

        int pct_chance_per_item = 0;

        std::vector<item::Id> id_bucket;

        // Converting a potion?
        if (item_type_before == ItemType::potion) {
                pct_chance_per_item = chance_potion(skill);

                for (size_t item_id = 0;
                     (item::Id)item_id != item::Id::END;
                     ++item_id) {
                        if ((item::Id)item_id == id_before) {
                                continue;
                        }

                        const auto& d = item::g_data[item_id];

                        if (d.type == ItemType::potion) {
                                id_bucket.push_back((item::Id)item_id);
                        }
                }
        }
        // Converting a scroll?
        else if (item_type_before == ItemType::scroll) {
                pct_chance_per_item = chance_scroll(skill);

                for (size_t item_id = 0;
                     (item::Id)item_id != item::Id::END;
                     ++item_id) {
                        if ((item::Id)item_id == id_before) {
                                continue;
                        }

                        const auto& d = item::g_data[item_id];

                        if (d.type == ItemType::scroll) {
                                id_bucket.push_back((item::Id)item_id);
                        }
                }
        }
        // Converting a melee weapon (with at least one "plus")?
        else if ((item_type_before == ItemType::melee_wpn) && (melee_plus >= 1)) {
                pct_chance_per_item = chance_weapon(skill, melee_plus);

                for (size_t item_id = 0;
                     (item::Id)item_id != item::Id::END;
                     ++item_id) {
                        const auto& d = item::g_data[item_id];

                        if ((d.type == ItemType::potion) ||
                            (d.type == ItemType::scroll)) {
                                id_bucket.push_back((item::Id)item_id);
                        }
                }
        }

        // Never spawn Transmute scrolls, this is just dumb
        for (auto it = std::begin(id_bucket); it != std::end(id_bucket);) {
                if (*it == item::Id::scroll_transmut) {
                        it = id_bucket.erase(it);
                }
                else {
                        // Not transmute
                        ++it;
                }
        }

        auto id_new = item::Id::END;

        if (!id_bucket.empty()) {
                id_new = rnd::element(id_bucket);
        }

        int nr_items_new = 0;

        // How many items?
        for (int i = 0; i < nr_items_before; ++i) {
                if (rnd::percent(pct_chance_per_item)) {
                        ++nr_items_new;
                }
        }

        if ((id_new == item::Id::END) || (nr_items_new < 1)) {
                msg_log::add("Nothing appears.");

                return;
        }

        // OK, items are good, and player succeeded the rolls etc

        auto* item_new = item::make(id_new, nr_items_new);

        item::randomize_item_properties(*item_new);

        if (item_new->data().is_stackable) {
                item_new->m_nr_items = nr_items_new;
        }

        const std::string item_name_new =
                text_format::first_to_upper(
                        item_new->name(ItemNameType::plural));

        if (map::g_seen.at(p)) {
                std::string appear_str =
                        (nr_items_new == 1)
                        ? "appears"
                        : "appear";

                msg_log::add(item_name_new + " " + appear_str + ".");
        }

        // NOTE: This will possibly make the player "discover" the item, so it
        // should occur last, after the "appear" message.
        item_drop::drop_item_on_map(map::g_player->m_pos, *item_new);
}

std::vector<std::string> SpellTransmut::descr_specific(
        const SpellSkill skill) const
{
        std::vector<std::string> descr;

        descr.emplace_back(
                "Attempts to convert items (stand over an item when casting). "
                "On failure, the item is destroyed.");

        descr.push_back(
                "Converts Potions with " +
                std::to_string(chance_potion(skill)) +
                "% chance.");

        descr.push_back(
                "Converts Manuscripts with " +
                std::to_string(chance_scroll(skill)) +
                "% chance.");

        descr.push_back(
                "Melee weapons with at least +1 damage (not counting any "
                "damage bonus from skills) are converted to a Potion or "
                "Manuscript, with " +
                std::to_string(chance_weapon(skill, 1)) +
                "% chance for a +1 weapon, " +
                std::to_string(chance_weapon(skill, 2)) +
                "% chance for a +2 weapon, " +
                std::to_string(chance_weapon(skill, 3)) +
                "% chance for a +3 weapon, etc.");

        return descr;
}

// -----------------------------------------------------------------------------
// Blood Tempering
// -----------------------------------------------------------------------------
Range SpellBloodTempering::duration_range(SpellSkill skill) const
{
        Range duration_range;

        duration_range.min = 4 + ((int)skill * 2);
        duration_range.max = duration_range.min + 4;

        return duration_range;
}

int SpellBloodTempering::base_max_cost(const SpellSkill skill) const
{
        (void)skill;

        return 8;
}

void SpellBloodTempering::run_effect(
        actor::Actor* caster,
        SpellSkill skill,
        const std::vector<actor::Actor*>& seen_targets) const
{
        (void)seen_targets;

        int nr_turns = duration_range(skill).roll();

        prop::Prop* r_phys = prop::make(prop::Id::r_phys);

        r_phys->set_duration(nr_turns);

        caster->m_properties.apply(r_phys);
}

std::vector<std::string> SpellBloodTempering::descr_specific(
        SpellSkill skill) const
{
        std::vector<std::string> descr;

        descr.emplace_back(
                "Through ardous suffering, the caster tempers their body to "
                "resist physical force (cannot be harmed by normal attacks, "
                "however other forms of damage such as fire is still "
                "harmful).");

        descr.emplace_back(
                "The spell lasts for " +
                duration_range(skill).str() +
                " turns.");

        return descr;
}

// -----------------------------------------------------------------------------
// Thorns
// -----------------------------------------------------------------------------
Range SpellThorns::duration_range(const SpellSkill skill) const
{
        Range duration_range;

        duration_range.min = ((int)skill + 1) * 5;
        duration_range.max = duration_range.min * 2;

        return duration_range;
}

Range SpellThorns::dmg_range(const SpellSkill skill) const
{
        switch (skill) {
        case SpellSkill::basic:
                return {2, 4};  // Avg 3.0

        case SpellSkill::expert:
                return {3, 6};  // Avg 4.5

        case SpellSkill::master:
                return {4, 8};  // Avg 6.0

        case SpellSkill::transcendent:
                return {5, 10};  // Avg 7.5
        }

        ASSERT(false);

        return {1, 1};
}

void SpellThorns::run_effect(
        actor::Actor* caster,
        SpellSkill skill,
        const std::vector<actor::Actor*>& seen_targets) const
{
        (void)seen_targets;

        auto* const prop = static_cast<prop::Thorns*>(prop::make(prop::Id::thorns));

        prop->set_duration(duration_range(skill).roll());

        prop->set_dmg(dmg_range(skill).roll());

        caster->m_properties.apply(prop);
}

std::vector<std::string> SpellThorns::descr_specific(
        SpellSkill skill) const
{
        std::vector<std::string> descr;

        // Re-using the property description as spell description.
        descr.push_back(prop::g_data[(size_t)prop::Id::thorns].descr);

        descr.push_back(
                "The spell returns " +
                dmg_range(skill).str() +
                " damage to the attacker.");

        descr.emplace_back(
                "The spell lasts for " +
                duration_range(skill).str() +
                " turns before it is wasted.");

        return descr;
}

// -----------------------------------------------------------------------------
// Crimson Passage
// -----------------------------------------------------------------------------
bool SpellCrimsonPassage::is_noisy(SpellSkill skill) const
{
        return (skill == SpellSkill::basic) ? true : false;
}

SpellShock SpellCrimsonPassage::shock_type() const
{
        // If the effect is already active, the spell does not cause shock.
        //
        // HACK: Assuming only the player can cast this spell.
        return (
                map::g_player->m_properties.has(prop::Id::crimson_passage)
                        ? SpellShock::none
                        : SpellShock::disturbing);
}

int SpellCrimsonPassage::base_max_cost(const SpellSkill skill) const
{
        (void)skill;

        // If the effect is already active, the spell is free to cast.

        // HACK: Assuming only the player can cast this spell.
        return map::g_player->m_properties.has(prop::Id::crimson_passage) ? 0 : 3;
}

int SpellCrimsonPassage::nr_steps_allowed(const SpellSkill skill) const
{
        if (skill == SpellSkill::transcendent) {
                return -1;
        }
        else {
                return ((int)skill + 1) * 4;
        }
}

void SpellCrimsonPassage::run_effect(
        actor::Actor* caster,
        SpellSkill skill,
        const std::vector<actor::Actor*>& seen_targets) const
{
        (void)seen_targets;

        if (caster->m_properties.has(prop::Id::crimson_passage)) {
                // Effect already active, cancel it instead.
                caster->m_properties.end_prop(prop::Id::crimson_passage);

                return;
        }

        auto* const prop =
                static_cast<prop::CrimsonPassage*>(
                        prop::make(prop::Id::crimson_passage));

        prop->set_indefinite();

        prop->set_nr_steps_allowed(nr_steps_allowed(skill));

        caster->m_properties.apply(prop);
}

std::vector<std::string> SpellCrimsonPassage::descr_specific(
        SpellSkill skill) const
{
        std::vector<std::string> descr;

        // Re-using the property description as spell description.
        descr.push_back(prop::g_data[(size_t)prop::Id::crimson_passage].descr);

        const int nr_steps = nr_steps_allowed(skill);

        if (nr_steps == -1) {
                descr.emplace_back(
                        "An infinite number of steps may be taken, the spell "
                        "is only limited by the number of hit points.");
        }
        else {
                descr.emplace_back(
                        std::to_string(nr_steps_allowed(skill)) +
                        " steps may be taken before the effect ends.");
        }

        descr.emplace_back(
                "Casting the spell again while it is already active cancels "
                "the effect (this does not drain hit points or cause shock).");

        return descr;
}

// -----------------------------------------------------------------------------
// Sacrifice Life
// -----------------------------------------------------------------------------
int SpellSacrificeLife::nr_sp_per_hp(const SpellSkill skill) const
{
        return 1 + (int)skill;
}

void SpellSacrificeLife::run_effect(
        actor::Actor* caster,
        SpellSkill skill,
        const std::vector<actor::Actor*>& seen_targets) const
{
        (void)skill;
        (void)seen_targets;

        const int hp = caster->m_hp;

        if (hp <= 2) {
                // Not enough HP.
                msg_log::add("I feel like I have very little to offer.");

                return;
        }

        int hp_drained = ((hp - 1) / 2) * 2;

        hp_drained = std::min(8, hp_drained);

        actor::hit(
                *caster,
                hp_drained,
                DmgType::pure,
                nullptr,
                AllowWound::no);

        const int sp_gained = hp_drained * nr_sp_per_hp(skill);

        caster->restore_sp(sp_gained, true);
}

std::vector<std::string> SpellSacrificeLife::descr_specific(
        SpellSkill skill) const
{
        std::vector<std::string> descr;

        descr.emplace_back(
                "Sacrifices the life force of the caster in order to restore "
                "the spirit. The amount restored is proportional to the life "
                "lost. A maximum of 8 hit points may be sacrificed.");

        const int k = nr_sp_per_hp(skill);

        if (k == 1) {
                descr.emplace_back(
                        "For each hit point sacrificed, " +
                        std::to_string(k) +
                        " spirit point is gained.");
        }
        else {
                descr.emplace_back(
                        "For each hit point sacrificed, " +
                        std::to_string(k) +
                        " spirit points are gained.");
        }

        return descr;
}
