// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "spells.hpp"

#include <algorithm>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "actor_death.hpp"
#include "actor_factory.hpp"
#include "actor_hit.hpp"
#include "actor_mon.hpp"
#include "actor_player.hpp"
#include "actor_see.hpp"
#include "explosion.hpp"
#include "flood.hpp"
#include "game.hpp"
#include "game_time.hpp"
#include "init.hpp"
#include "inventory.hpp"
#include "io.hpp"
#include "item_factory.hpp"
#include "item_scroll.hpp"
#include "knockback.hpp"
#include "line_calc.hpp"
#include "map.hpp"
#include "map_parsing.hpp"
#include "misc.hpp"
#include "msg_log.hpp"
#include "pathfind.hpp"
#include "player_bon.hpp"
#include "player_spells.hpp"
#include "postmortem.hpp"
#include "property.hpp"
#include "property_data.hpp"
#include "property_factory.hpp"
#include "property_handler.hpp"
#include "teleport.hpp"
#include "terrain.hpp"
#include "terrain_door.hpp"
#include "text_format.hpp"
#include "viewport.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
typedef std::unordered_map<std::string, SpellId> StrToSpellIdMap;

static const StrToSpellIdMap s_str_to_spell_id_map = {
        {"aura_of_decay", SpellId::aura_of_decay},
        {"spectral_wpns", SpellId::spectral_wpns},
        {"aza_wrath", SpellId::aza_wrath},
        {"bless", SpellId::bless},
        {"burn", SpellId::burn},
        {"force_bolt", SpellId::force_bolt},
        {"darkbolt", SpellId::darkbolt},
        {"deafen", SpellId::deafen},
        {"disease", SpellId::disease},
        {"premonition", SpellId::premonition},
        {"enfeeble", SpellId::enfeeble},
        {"cleansing_fire", SpellId::cleansing_fire},
        {"sanctuary", SpellId::sanctuary},
        {"purge", SpellId::purge},
        {"frenzy", SpellId::frenzy},
        {"heal", SpellId::heal},
        {"identify", SpellId::identify},
        {"knockback", SpellId::knockback},
        {"light", SpellId::light},
        {"mayhem", SpellId::mayhem},
        {"mi_go_hypno", SpellId::mi_go_hypno},
        {"opening", SpellId::opening},
        {"pestilence", SpellId::pestilence},
        {"res", SpellId::res},
        {"see_invis", SpellId::see_invis},
        {"slow", SpellId::slow},
        {"haste", SpellId::haste},
        {"spell_shield", SpellId::spell_shield},
        {"summon", SpellId::summon},
        {"summon_tentacles", SpellId::summon_tentacles},
        {"teleport", SpellId::teleport},
        {"terrify", SpellId::terrify},
        {"transmut", SpellId::transmut}};

using StrToSpellSkillMap = std::unordered_map<std::string, SpellSkill>;

static const StrToSpellSkillMap s_str_to_spell_skill_map = {
        {"basic", SpellSkill::basic},
        {"expert", SpellSkill::expert},
        {"master", SpellSkill::master}};

static const std::string s_spell_resist_msg = "The spell is resisted!";

static const std::string s_spell_reflect_msg = "The spell is reflected!";

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

        const auto id = actor::Id::tentacles;

        auto spawned = actor::spawn(p, {id}, map::rect());

        bool printed_msg = false;

        for (auto* const actor : spawned.monsters)
        {
                if (!printed_msg)
                {
                        print_side_effect_trigger_message();
                        printed_msg = true;
                }

                auto* const conflicted =
                        property_factory::make(PropId::conflict);

                conflicted->set_indefinite();

                actor->m_properties.apply(
                        conflicted,
                        PropSrc::intr,
                        false,
                        Verbose::no);

                auto* const waiting =
                        property_factory::make(PropId::waiting);

                waiting->set_duration(2);

                actor->m_properties.apply(waiting);

                auto* const summoned =
                        property_factory::make(PropId::summoned);

                summoned->set_duration(rnd::range(3, 20));

                actor->m_properties.apply(summoned);
        }

        TRACE_FUNC_END;
}

static void swap_wall_floor(const Context& context)
{
        TRACE_FUNC_BEGIN;

        // TODO: This is pretty much copy/pasted from the "Alter Environment"
        // property, consider refactoring.
        // Alternatively, maybe just apply that property on the caster instead.

        print_side_effect_trigger_message();

        Array2<bool> blocked(map::dims());

        map_parsers::BlocksWalking(ParseActors::no)
                .run(blocked, blocked.rect());

        const std::vector<terrain::Id> free_terrains = {
                terrain::Id::door,
                terrain::Id::liquid_deep,
        };

        for (const P& p : blocked.rect().positions())
        {
                const bool is_free_terrain =
                        map_parsers::IsAnyOfTerrains(free_terrains)
                                .run(p);

                if (is_free_terrain)
                {
                        blocked.at(p) = false;
                }
        }

        Array2<bool> has_actor(map::dims());

        for (auto* actor : game_time::g_actors)
        {
                if (actor->m_state != ActorState::destroyed)
                {
                        has_actor.at(actor->m_pos) = true;
                }
        }

        for (const auto& p : context.nearby_positions)
        {
                if (!map::is_pos_inside_outer_walls(p) ||
                    has_actor.at(p) ||
                    map::g_items.at(p) ||
                    !rnd::one_in(14))
                {
                        continue;
                }

                const auto terrain_id = map::g_terrain.at(p)->id();

                if (terrain_id == terrain::Id::wall)
                {
                        blocked.at(p) = false;

                        if (map_parsers::is_map_connected(blocked))
                        {
                                map::put(new terrain::Floor(p));
                        }
                        else
                        {
                                // Map would not be connected
                                blocked.at(p) = true;
                        }
                }
                else if (terrain_id == terrain::Id::floor)
                {
                        blocked.at(p) = true;

                        if (map_parsers::is_map_connected(blocked))
                        {
                                map::put(new terrain::RubbleHigh(p));
                        }
                        else
                        {
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

        for (auto* actor : game_time::g_actors)
        {
                if (actor->m_state != ActorState::destroyed)
                {
                        has_actor.at(actor->m_pos) = true;
                }
        }

        bool printed_msg = false;
        for (const auto& p : context.nearby_positions)
        {
                if (has_actor.at(p))
                {
                        continue;
                }

                if (!rnd::one_in(14))
                {
                        continue;
                }

                if (!printed_msg)
                {
                        print_side_effect_trigger_message();
                        printed_msg = true;
                }

                auto* const terrain = map::g_terrain.at(p);

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

        for (auto* actor : game_time::g_actors)
        {
                if (actor->m_state != ActorState::destroyed)
                {
                        has_actor.at(actor->m_pos) = true;
                }
        }

        bool printed_msg = false;
        for (const auto& p : context.nearby_positions)
        {
                if (has_actor.at(p) || map::g_items.at(p))
                {
                        continue;
                }

                auto* const terrain = map::g_terrain.at(p);

                if (terrain->id() != terrain::Id::door)
                {
                        continue;
                }

                if (static_cast<terrain::Door*>(terrain)->type() ==
                    terrain::DoorType::metal)
                {
                        continue;
                }

                if (!printed_msg)
                {
                        print_side_effect_trigger_message();
                        printed_msg = true;
                }

                // NOTE: Metal doors are skipped, so it's OK to just run
                // open/close here
                if (should_open)
                {
                        terrain->open(nullptr);
                }
                else
                {
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

        for (auto* const actor : actors)
        {
                const auto* const actor_data = actor->m_data;

                const auto& properties = actor->m_properties;

                // NOTE: The target may be the caster, if caster is a monster
                if (!actor->is_player() &&
                    actor->is_alive() &&
                    actor_data->is_humanoid &&
                    !actor_data->is_undead &&
                    !actor_data->is_unique &&
                    !properties.has(PropId::ethereal) &&
                    !properties.has(PropId::possessed_by_zuul) &&
                    !properties.has(PropId::spawns_zombie_parts_on_destroyed))
                {
                        target_actor = actor;

                        break;
                }
        }

        if (!target_actor)
        {
                return;
        }

        print_side_effect_trigger_message();

        if (actor::can_player_see_actor(*target_actor))
        {
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
                {actor::Id::crawling_intestines},
                map::rect());

        TRACE_FUNC_END;
}

static void create_water(const Context& context)
{
        TRACE_FUNC_BEGIN;

        bool printed_msg = false;

        for (const auto& p : context.nearby_positions)
        {
                if ((map::g_terrain.at(p)->id() != terrain::Id::floor) ||
                    !rnd::one_in(8))
                {
                        continue;
                }

                if (!printed_msg)
                {
                        print_side_effect_trigger_message();
                        printed_msg = true;
                }

                auto* const liquid = new terrain::LiquidShallow(p);

                liquid->m_type = LiquidType::water;

                map::put(liquid);
        }

        TRACE_FUNC_END;
}

static void create_trees(const Context& context)
{
        TRACE_FUNC_BEGIN;

        print_side_effect_trigger_message();

        Array2<bool> blocked(map::dims());

        map_parsers::BlocksWalking(ParseActors::no)
                .run(blocked, blocked.rect());

        const std::vector<terrain::Id> free_terrains = {
                terrain::Id::door,
                terrain::Id::liquid_deep,
        };

        for (const P& p : blocked.rect().positions())
        {
                const bool is_free_terrain =
                        map_parsers::IsAnyOfTerrains(free_terrains)
                                .run(p);

                if (is_free_terrain)
                {
                        blocked.at(p) = false;
                }
        }

        std::vector<P> tree_pos_bucket;

        for (const auto& p : context.nearby_positions)
        {
                const bool is_floor_like =
                        map::g_terrain.at(p)->data().is_floor_like;

                const bool is_adj_to_lever =
                        map_parsers::AnyAdjIsAnyOfTerrains(terrain::Id::lever)
                                .run(p);

                if (blocked.at(p) || !is_floor_like || is_adj_to_lever)
                {
                        continue;
                }

                tree_pos_bucket.push_back(p);

                map::put(new terrain::Grass(p));
        }

        Array2<bool> has_actor(map::dims());

        for (auto* actor : game_time::g_actors)
        {
                if (actor->m_state != ActorState::destroyed)
                {
                        has_actor.at(actor->m_pos) = true;
                }
        }

        int nr_trees_placed = 0;

        const int tree_one_in_n = rnd::range(1, 20);

        TRACE << "tree_one_in_n: " << tree_one_in_n << std::endl;

        while (!tree_pos_bucket.empty())
        {
                const auto p = tree_pos_bucket.back();

                tree_pos_bucket.pop_back();

                if (has_actor.at(p) || !rnd::one_in(tree_one_in_n))
                {
                        continue;
                }

                blocked.at(p) = true;

                if (map_parsers::is_map_connected(blocked))
                {
                        map::put(new terrain::Tree(p));

                        ++nr_trees_placed;
                }
                else
                {
                        blocked.at(p) = false;
                }
        }

        TRACE_FUNC_END;
}

static void create_doors(const Context& context)
{
        TRACE_FUNC_BEGIN;

        const auto adj_door_checker =
                map_parsers::AnyAdjIsAnyOfTerrains(terrain::Id::door);

        const auto adj_floor_checker =
                map_parsers::AnyAdjIsAnyOfTerrains(terrain::Id::floor);

        bool printed_msg = false;
        for (const auto& p : context.nearby_positions)
        {
                const auto id = map::g_terrain.at(p)->id();

                if (!rnd::one_in(2) ||
                    (id != terrain::Id::wall) ||
                    adj_door_checker.run(p) ||
                    !adj_floor_checker.run(p))
                {
                        continue;
                }

                if (!printed_msg)
                {
                        print_side_effect_trigger_message();
                        printed_msg = true;
                }

                const auto* const mimic = new terrain::Wall(p);

                auto* const door =
                        new terrain::Door(
                                p,
                                mimic,
                                terrain::DoorType::wood,
                                terrain::DoorSpawnState::closed);

                map::put(door);
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
                terrain::Id::liquid_deep,
        };

        for (const P& p : blocked.rect().positions())
        {
                const bool is_free_terrain =
                        map_parsers::IsAnyOfTerrains(free_terrains)
                                .run(p);

                if (is_free_terrain)
                {
                        blocked.at(p) = false;
                }
        }

        print_side_effect_trigger_message();

        for (const auto& p : sorted_positions)
        {
                if (!map::is_pos_inside_outer_walls(p))
                {
                        continue;
                }

                map::g_dark.at(p) = true;
                map::g_light.at(p) = false;

                if (map::g_terrain.at(p)->id() == terrain::Id::wall)
                {
                        blocked.at(p) = false;

                        if (map_parsers::is_map_connected(blocked))
                        {
                                map::put(new terrain::Floor(p));
                        }
                        else
                        {
                                blocked.at(p) = true;
                        }
                }
        }

        TRACE_FUNC_END;
}

static void push_statue(const Context& context)
{
        TRACE_FUNC_BEGIN;

        bool printed_msg = false;

        for (const auto& p : context.nearby_positions)
        {
                auto* const terrain = map::g_terrain.at(p);

                if (terrain->id() != terrain::Id::statue)
                {
                        continue;
                }

                if (!printed_msg)
                {
                        print_side_effect_trigger_message();
                        printed_msg = true;
                }

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

static const std::vector<SpellSideEffect> s_spell_side_effects {
        create_dark_void,
        create_doors,
        create_trees,
        create_water,
        flay_human,
        ignite_terrain,
        open_close_doors,
        push_statue,
        spawn_monsters,
        swap_wall_floor,
};

}  // namespace spell_side_effects

static terrain::DidOpen run_opening_spell_for_metal_door(
        const terrain::Terrain& door)
{
        // Find ANY lever for this door, and toggle it.
        //
        // NOTE: If there are more levers connected to this door, this is OK
        // since the lever will set its sibblings to the same position.
        //
        for (const auto& p : map::rect().positions())
        {
                auto* const terrain = map::g_terrain.at(p);

                if (terrain->id() != terrain::Id::lever)
                {
                        continue;
                }

                auto* const lever = static_cast<terrain::Lever*>(terrain);

                if (!lever->is_linked_to(door))
                {
                        continue;
                }

                lever->toggle();

                return terrain::DidOpen::yes;
        }

        // Reaching this point means we didn't find a lever for the metal door,
        // which is not supposed to happen.
        ASSERT(false);

        return terrain::DidOpen::no;
}

// -----------------------------------------------------------------------------
// spells
// -----------------------------------------------------------------------------
namespace spells
{
Spell* make(const SpellId spell_id)
{
        switch (spell_id)
        {
        case SpellId::aura_of_decay:
                return new SpellAuraOfDecay();

        case SpellId::enfeeble:
                return new SpellEnfeeble();

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

        case SpellId::aza_wrath:
                return new SpellAzaWrath();

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

        case SpellId::mayhem:
                return new SpellMayhem();

        case SpellId::pestilence:
                return new SpellPestilence();

        case SpellId::spectral_wpns:
                return new SpellSpectralWpns();

        case SpellId::opening:
                return new SpellOpening();

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

        case SpellId::res:
                return new SpellRes();

        case SpellId::light:
                return new SpellLight();

        case SpellId::transmut:
                return new SpellTransmut();

        case SpellId::see_invis:
                return new SpellSeeInvis();

        case SpellId::spell_shield:
                return new SpellSpellShield();

        case SpellId::haste:
                return new SpellHaste();

        case SpellId::premonition:
                return new SpellPremonition();

        case SpellId::identify:
                return new SpellIdentify();

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

std::string skill_to_str(const SpellSkill skill)
{
        switch (skill)
        {
        case SpellSkill::basic:
                return "basic";

        case SpellSkill::expert:
                return "expert";

        case SpellSkill::master:
                return "master";
        }

        ASSERT(false);

        return "";
}

terrain::DidOpen run_opening_spell_effect_at(
        const P& pos,
        const int chance,
        const SpellSkill skill)
{
        if (!rnd::percent(chance))
        {
                return terrain::DidOpen::no;
        }

        auto* const terrain = map::g_terrain.at(pos);

        if (terrain->id() == terrain::Id::door)
        {
                auto* const door = static_cast<terrain::Door*>(terrain);

                if ((door->type() == terrain::DoorType::metal))
                {
                        const bool is_skill_enough =
                                ((int)skill >= (int)SpellSkill::expert);

                        if (is_skill_enough && !door->is_open())
                        {
                                const auto did_open =
                                        run_opening_spell_for_metal_door(*door);

                                return did_open;
                        }
                        else
                        {
                                return terrain::DidOpen::no;
                        }
                }
        }

        const auto did_open = terrain->open(nullptr);

        return did_open;
}

}  // namespace spells

// -----------------------------------------------------------------------------
// BrowseSpell
// -----------------------------------------------------------------------------
StateId BrowseSpell::id() const
{
        return StateId::browse_spells;
}

// -----------------------------------------------------------------------------
// Spell
// -----------------------------------------------------------------------------
Range Spell::spi_cost_range(const SpellSkill skill) const
{
        const int cost_max = base_max_spi_cost(skill);
        const int cost_min = (cost_max + 1) / 2;

        return {cost_min, cost_max};
}

void Spell::cast(
        actor::Actor* const caster,
        const SpellSkill skill,
        const SpellSrc spell_src) const
{
        TRACE_FUNC_BEGIN;

        ASSERT(caster);

        auto& properties = caster->m_properties;

        // If this is an intrinsic cast, check properties which NEVER allows
        // casting or speaking

        // NOTE: If this is a non-intrinsic cast (e.g. from a scroll), then we
        // assume that the caller has made all checks themselves
        if ((spell_src == SpellSrc::learned) &&
            (!properties.allow_cast_intr_spell_absolute(Verbose::yes) ||
             !properties.allow_speak(Verbose::yes)))
        {
                return;
        }

        // OK, we can try to cast

        if (caster->is_player())
        {
                TRACE << "Player casting spell" << std::endl;

                const ShockSrc shock_src =
                        (spell_src == SpellSrc::learned)
                        ? ShockSrc::cast_intr_spell
                        : ShockSrc::use_strange_item;

                const int value = shock_value();

                map::g_player->incr_shock((double)value, shock_src);

                // Make sound if noisy - casting from scrolls is always noisy
                if (is_noisy(skill) || (spell_src == SpellSrc::manuscript))
                {
                        Snd snd(
                                "",
                                audio::SfxId::spell_generic,
                                IgnoreMsgIfOriginSeen::yes,
                                caster->m_pos,
                                caster,
                                SndVol::low,
                                AlertsMon::yes);

                        snd_emit::run(snd);
                }
        }
        else
        {
                // Caster is monster
                TRACE << "Monster casting spell" << std::endl;

                // Make sound if noisy - casting from scrolls is always noisy
                if (is_noisy(skill) ||
                    (spell_src == SpellSrc::manuscript))
                {
                        auto* const mon = static_cast<actor::Mon*>(caster);

                        const bool is_mon_seen =
                                actor::can_player_see_actor(*mon);

                        std::string spell_msg = mon->m_data->spell_msg;

                        if (!spell_msg.empty())
                        {
                                std::string mon_name;

                                if (is_mon_seen)
                                {
                                        mon_name =
                                                text_format::first_to_upper(
                                                        mon->name_the());
                                }
                                else
                                {
                                        // Cannot see monster
                                        mon_name =
                                                mon->m_data->is_humanoid
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

                        snd_emit::run(snd);
                }
        }

        bool allow_cast = true;

        if (spell_src == SpellSrc::learned)
        {
                const auto cost_range = spi_cost_range(skill);

                if (cost_range.min > 0)
                {
                        actor::hit_sp(*caster, cost_range.roll(), Verbose::no);
                }

                // Check properties which MAY allow casting with a random chance
                allow_cast =
                        properties.allow_cast_intr_spell_chance(
                                Verbose::yes);
        }

        if (allow_cast && caster->is_alive())
        {
                run_effect(caster, skill);

                // Casting spells ends cloaking
                caster->m_properties.end_prop(PropId::cloaked);
        }

        if (caster->is_player() &&
            caster->is_alive() &&
            allow_cast &&
            (base_max_spi_cost(skill) > 0) &&
            rnd::one_in(10))
        {
                // Run a random side effect
                const int d = 3;

                const R rect(
                        {std::max(0, caster->m_pos.x - d),
                         std::max(0, caster->m_pos.y - d)},
                        {std::min(map::w() - 1, caster->m_pos.x + d),
                         std::min(map::h() - 1, caster->m_pos.y + d)});

                auto nearby_positions = rect.positions();

                rnd::shuffle(nearby_positions);

                const auto& side_effect =
                        rnd::element(spell_side_effects::s_spell_side_effects);

                TRACE << "Running spell side effect" << std::endl;

                side_effect({*caster, nearby_positions});
        }

        game_time::tick();

        TRACE_FUNC_END;
}

void Spell::on_resist(actor::Actor& target) const
{
        const bool is_player = target.is_player();

        const bool player_see_target = actor::can_player_see_actor(target);

        if (player_see_target)
        {
                msg_log::add(s_spell_resist_msg);

                if (is_player)
                {
                        audio::play(audio::SfxId::spell_shield_break);
                }

                io::draw_blast_at_cells({target.m_pos}, colors::white());
        }

        // TODO: Only end r_spell if this is not a natural property
        target.m_properties.end_prop(PropId::r_spell);

        if (is_player && player_bon::has_trait(Trait::absorb))
        {
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
        auto lines = descr_specific(skill);

        if (spell_src != SpellSrc::manuscript)
        {
                if (is_noisy(skill))
                {
                        lines.emplace_back(
                                "Casting this spell requires making sounds.");
                }
                else
                {
                        // The spell is silent
                        lines.emplace_back(
                                "This spell can be cast silently.");
                }
        }

        if (!player_bon::is_bg(Bg::exorcist))
        {
                const std::string domain_str = domain_descr();

                if (!domain_str.empty())
                {
                        lines.push_back(domain_str);
                }
        }

        if (can_be_improved_with_skill())
        {
                std::string skill_str =
                        text_format::first_to_upper(
                                spells::skill_to_str(skill));

                const bool is_scroll =
                        (spell_src == SpellSrc::manuscript);

                const bool is_altar_bonus =
                        player_spells::is_getting_altar_bonus();

                if (is_scroll || is_altar_bonus)
                {
                        skill_str += " (";
                }

                if (is_scroll)
                {
                        skill_str += "casting from manuscript";
                }

                if (is_altar_bonus)
                {
                        if (is_scroll)
                        {
                                skill_str += ", ";
                        }

                        skill_str += "standing at altar";
                }

                if (is_scroll || is_altar_bonus)
                {
                        skill_str += ")";
                }

                lines.push_back("Skill level: " + skill_str);
        }

        return lines;
}

std::string Spell::domain_descr() const
{
        const auto my_domain = domain();

        if (my_domain == OccultistDomain::END)
        {
                return "";
        }
        else
        {
                const std::string domain_title =
                        player_bon::spell_domain_title(domain());

                return "Spell domain: " + domain_title;
        }
}

int Spell::shock_value() const
{
        const SpellShock type = shock_type();

        int value = 0;

        switch (type)
        {
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
int SpellAuraOfDecay::base_max_spi_cost(const SpellSkill skill) const
{
        (void)skill;

        return 6;
}

void SpellAuraOfDecay::run_effect(
        actor::Actor* const caster,
        const SpellSkill skill) const
{
        const int max_dmg = 1 + (int)skill;

        Range duration_range;
        duration_range.min = 20 * ((int)skill + 1);
        duration_range.max = duration_range.min * 2;

        auto* prop = new PropAuraOfDecay();

        prop->set_duration(duration_range.roll());

        prop->set_max_dmg(max_dmg);

        caster->m_properties.apply(prop);
}

std::vector<std::string> SpellAuraOfDecay::descr_specific(
        const SpellSkill skill) const
{
        std::vector<std::string> descr;

        descr.emplace_back(
                "The caster exudes death and decay. Creatures within a "
                "distance of two moves take damage each standard turn.");

        const auto dmg_range = Range(1, 1 + (int)skill);

        descr.push_back(
                "The spell does " +
                dmg_range.str() +
                " damage per creature.");

        Range duration_range;
        duration_range.min = 20 * ((int)skill + 1);
        duration_range.max = duration_range.min * 2;

        descr.push_back("The spell lasts " + duration_range.str() + " turns.");

        return descr;
}

int SpellAuraOfDecay::mon_cooldown() const
{
        return 30;
}

bool SpellAuraOfDecay::allow_mon_cast_now(actor::Mon& mon) const
{
        return mon.m_ai_state.target &&
                mon.m_ai_state.is_target_seen &&
                !mon.m_properties.has(PropId::aura_of_decay);
}

// -----------------------------------------------------------------------------
// Bolt spells
// -----------------------------------------------------------------------------
Range ForceBolt::damage(
        const SpellSkill skill,
        const actor::Actor& caster) const
{
        (void)caster;

        switch (skill)
        {
        case SpellSkill::basic:
                return {3, 4};  // Avg 3.5

        case SpellSkill::expert:
                return {5, 7};  // Avg 6.0

        case SpellSkill::master:
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

        switch (skill)
        {
        case SpellSkill::basic:
                return {4, 9};  // Avg 6.5

        case SpellSkill::expert:
                return {5, 11};  // Avg 8.0

        case SpellSkill::master:
                return {6, 13};  // Avg 9.5
        }

        ASSERT(false);

        return {1, 1};
}

std::vector<std::string> Darkbolt::descr_specific(const SpellSkill skill) const
{
        std::vector<std::string> descr;

        descr.emplace_back(
                "Siphons power from some infernal dimension, which is focused "
                "into a bolt hurled towards a target with great force. The "
                "conjured bolt has some will on its own - the caster cannot "
                "determine exactly which creature will be struck.");

        const auto dmg_range = damage(skill, *map::g_player);

        descr.push_back("The impact does " + dmg_range.str() + " damage.");

        if (skill == SpellSkill::master)
        {
                descr.emplace_back("The target is paralyzed, and set aflame.");
        }
        else
        {
                // Not master
                descr.emplace_back("The target is paralyzed.");
        }

        return descr;
}

void Darkbolt::on_hit(actor::Actor& actor_hit, const SpellSkill skill) const
{
        if (!actor_hit.is_alive())
        {
                return;
        }

        auto* paralyzed = new PropParalyzed();

        paralyzed->set_duration(rnd::range(1, 2));

        actor_hit.m_properties.apply(paralyzed);

        if (skill == SpellSkill::master)
        {
                auto* burning = new PropBurning();

                burning->set_duration(rnd::range(2, 3));

                actor_hit.m_properties.apply(burning);
        }
}

void SpellBolt::run_effect(
        actor::Actor* const caster,
        const SpellSkill skill) const
{
        // if (caster == map::g_player &&
        //     player_bon::traits[(size_t)Trait::warlock] &&
        //     rnd::percent(warlock_multi_cast_chance_pct))
        // {
        //     run_effect(caster, skill);

        //     if (!caster->is_alive())
        //     {
        //         return;
        //     }
        // }

        std::vector<actor::Actor*> target_bucket;

        target_bucket = actor::seen_foes(*caster);

        if (target_bucket.empty())
        {
                if (caster->is_player())
                {
                        msg_log::add(
                                "A dark sphere materializes, but quickly "
                                "fizzles out.");
                }

                return;
        }

        auto* const target =
                map::random_closest_actor(
                        caster->m_pos,
                        target_bucket);

        // Spell resistance?
        if (target->m_properties.has(PropId::r_spell))
        {
                on_resist(*target);

                // Spell reflection?
                if (target->m_properties.has(PropId::spell_reflect))
                {
                        if (actor::can_player_see_actor(*target))
                        {
                                msg_log::add(
                                        s_spell_reflect_msg,
                                        colors::text(),
                                        MsgInterruptPlayer::no,
                                        MorePromptOnMsg::yes);
                        }

                        // Run effect with the target as caster instead
                        run_effect(target, skill);
                }

                return;
        }

        // Draw projectile traveling
        {
                Array2<bool> blocked(map::dims());

                map_parsers::BlocksProjectiles()
                        .run(blocked, blocked.rect());

                const auto flood = floodfill(caster->m_pos, blocked);

                const auto path =
                        pathfind_with_flood(
                                caster->m_pos,
                                target->m_pos,
                                flood);

                if (!path.empty())
                {
                        states::draw();

                        const int idx_0 = (int)(path.size()) - 1;

                        for (int i = idx_0; i > 0; --i)
                        {
                                const auto& p = path[i];

                                if (!map::g_seen.at(p) ||
                                    !viewport::is_in_view(p))
                                {
                                        continue;
                                }

                                states::draw();

                                io::draw_symbol(
                                        gfx::TileId::blast1,
                                        '*',
                                        Panel::map,
                                        viewport::to_view_pos(p),
                                        colors::magenta());

                                io::update_screen();

                                io::sleep(
                                        config::delay_projectile_draw());
                        }
                }
        }

        const auto& target_p = target->m_pos;

        const bool player_see_cell = map::g_seen.at(target_p);

        const bool player_see_tgt = actor::can_player_see_actor(*target);

        if (player_see_tgt || player_see_cell)
        {
                io::draw_blast_at_cells({target->m_pos}, colors::magenta());

                Color msg_clr = colors::msg_good();

                std::string str_begin = "I am";

                if (target->is_player())
                {
                        msg_clr = colors::msg_bad();
                }
                else
                {
                        // Target is monster
                        const std::string name_the =
                                player_see_tgt
                                ? text_format::first_to_upper(
                                          target->name_the())
                                : "It";

                        str_begin = name_the + " is";

                        if (map::g_player->is_leader_of(target))
                        {
                                msg_clr = colors::white();
                        }
                }

                msg_log::add(
                        str_begin +
                                " " +
                                m_impl->hit_msg_ending(),
                        msg_clr);
        }

        const auto dmg_range = m_impl->damage(skill, *caster);

        actor::hit(
                *target,
                dmg_range.roll(),
                DmgType::blunt,
                AllowWound::no);

        m_impl->on_hit(*target, skill);

        if (!target->is_player())
        {
                static_cast<actor::Mon*>(target)
                        ->become_aware_player(
                                actor::AwareSource::spell_victim);
        }

        Snd snd(
                "",
                audio::SfxId::END,
                IgnoreMsgIfOriginSeen::yes,
                target->m_pos,
                nullptr,
                SndVol::low,
                AlertsMon::yes);

        snd_emit::run(snd);
}

bool SpellBolt::allow_mon_cast_now(actor::Mon& mon) const
{
        return mon.m_ai_state.target && mon.m_ai_state.is_target_seen;
}

// -----------------------------------------------------------------------------
// Azathoths wrath
// -----------------------------------------------------------------------------
void SpellAzaWrath::run_effect(
        actor::Actor* const caster,
        const SpellSkill skill) const
{
        // if (caster == map::g_player &&
        //     player_bon::traits[(size_t)Trait::warlock] &&
        //     rnd::percent(warlock_multi_cast_chance_pct))
        // {
        //     run_effect(caster, skill);

        //     if (!caster->is_alive())
        //     {
        //         return;
        //     }
        // }

        const auto targets = actor::seen_foes(*caster);

        if (targets.empty())
        {
                if (caster->is_player())
                {
                        msg_log::add(
                                "There is a faint rumbling sound, like "
                                "distant thunder.");
                }
        }

        io::draw_blast_at_seen_actors(targets, colors::light_red());

        for (auto* const target : targets)
        {
                // Spell resistance?
                if (target->m_properties.has(PropId::r_spell))
                {
                        on_resist(*target);

                        // Spell reflection?
                        if (target->m_properties.has(PropId::spell_reflect))
                        {
                                if (actor::can_player_see_actor(*target))
                                {
                                        msg_log::add(
                                                s_spell_reflect_msg,
                                                colors::white(),
                                                MsgInterruptPlayer::no,
                                                MorePromptOnMsg::yes);
                                }

                                // Run effect with the target as caster
                                run_effect(target, skill);
                        }

                        continue;
                }

                std::string str_begin = "I am";

                Color msg_clr = colors::msg_good();

                if (target->is_player())
                {
                        msg_clr = colors::msg_bad();
                }
                else
                {
                        // Target is monster
                        str_begin =
                                text_format::first_to_upper(
                                        target->name_the()) +
                                " is";

                        if (map::g_player->is_leader_of(target))
                        {
                                msg_clr = colors::white();
                        }
                }

                if (actor::can_player_see_actor(*target))
                {
                        msg_log::add(
                                str_begin + " struck by a roaring blast!",
                                msg_clr);
                }

                // Skill  damage     avg
                // -----------------------
                // 0      2  - 5     3.5
                // 1      4  - 8     6.0
                // 2      6  - 11    8.5
                Range dmg_range(
                        2 + (int)skill * 2,
                        5 + (int)skill * 3);

                actor::hit(
                        *target,
                        dmg_range.roll(),
                        DmgType::explosion,
                        AllowWound::no);

                if (!target->is_player())
                {
                        static_cast<actor::Mon*>(target)
                                ->become_aware_player(
                                        actor::AwareSource::spell_victim);
                }

                if (target->is_alive())
                {
                        auto* prop = new PropParalyzed();

                        prop->set_duration(1);

                        target->m_properties.apply(prop);
                }

                if ((skill == SpellSkill::master) &&
                    target->is_alive())
                {
                        auto* prop = new PropBurning();

                        prop->set_duration(2);

                        target->m_properties.apply(prop);
                }

                Snd snd(
                        "",
                        audio::SfxId::END,
                        IgnoreMsgIfOriginSeen::yes,
                        target->m_pos,
                        nullptr,
                        SndVol::high,
                        AlertsMon::yes);

                snd_emit::run(snd);
        }
}

std::vector<std::string> SpellAzaWrath::descr_specific(
        const SpellSkill skill) const
{
        std::vector<std::string> descr;

        descr.emplace_back(
                "Channels the destructive force of Azathoth unto all "
                "visible enemies.");

        Range dmg_range(
                2 + (int)skill * 2,
                5 + (int)skill * 3);

        descr.push_back(
                "The spell does " +
                dmg_range.str() +
                " damage per creature.");

        if (skill == SpellSkill::master)
        {
                descr.emplace_back("The target is paralyzed, and set aflame.");
        }
        else
        {
                // Not master
                descr.emplace_back("The target is paralyzed.");
        }

        return descr;
}

bool SpellAzaWrath::allow_mon_cast_now(actor::Mon& mon) const
{
        return mon.m_ai_state.target && mon.m_ai_state.is_target_seen;
}

// -----------------------------------------------------------------------------
// Mayhem
// -----------------------------------------------------------------------------
int SpellMayhem::base_max_spi_cost(const SpellSkill skill) const
{
        (void)skill;

        return 6;
}

void SpellMayhem::run_effect(
        actor::Actor* const caster,
        const SpellSkill skill) const
{
        // if (caster == map::g_player &&
        //     player_bon::traits[(size_t)Trait::warlock] &&
        //     rnd::percent(warlock_multi_cast_chance_pct))
        // {
        //     run_effect(caster, skill);

        //     if (!caster->is_alive())
        //     {
        //         return;
        //     }
        // }

        const bool is_player = caster->is_player();

        if (actor::can_player_see_actor(*caster))
        {
                std::string caster_name =
                        is_player ? "me" : caster->name_the();

                msg_log::add("Destruction rages around " + caster_name + "!");
        }

        const P& caster_pos = caster->m_pos;

        const int destr_radi = g_fov_radi_int + (int)skill * 2;

        const R area(
                std::max(1, caster_pos.x - destr_radi),
                std::max(1, caster_pos.y - destr_radi),
                std::min(map::w() - 1, caster_pos.x + destr_radi) - 1,
                std::min(map::h() - 1, caster_pos.y + destr_radi) - 1);

        const auto positions = area.positions();

        // Run explosions
        std::vector<P> p_bucket;

        const int expl_radi_diff = -1;

        for (const auto& p : positions)
        {
                const auto* const terrain = map::g_terrain.at(p);

                if (!terrain->is_walkable())
                {
                        continue;
                }

                const int dist = king_dist(caster_pos, p);

                const int min_dist = g_expl_std_radi + 1 + expl_radi_diff;

                if (dist >= min_dist)
                {
                        p_bucket.push_back(p);
                }
        }

        int nr_expl = 2 + (int)skill;

        for (int i = 0; i < nr_expl; ++i)
        {
                if (p_bucket.empty())
                {
                        return;
                }

                const size_t idx = rnd::range(0, (int)p_bucket.size() - 1);

                const P& p = rnd::element(p_bucket);

                explosion::run(
                        p,
                        ExplType::expl,
                        EmitExplSnd::yes,
                        expl_radi_diff);

                p_bucket.erase(p_bucket.begin() + idx);
        }

        // Explode braziers
        for (const auto& p : positions)
        {
                const auto terrain_id = map::g_terrain.at(p)->id();

                if (terrain_id == terrain::Id::brazier)
                {
                        Snd snd(
                                "I hear an explosion!",
                                audio::SfxId::explosion_molotov,
                                IgnoreMsgIfOriginSeen::yes,
                                p,
                                nullptr,
                                SndVol::high,
                                AlertsMon::yes);

                        snd.run();

                        map::put(new terrain::RubbleLow(p));

                        explosion::run(
                                p,
                                ExplType::apply_prop,
                                EmitExplSnd::yes,
                                0,
                                ExplExclCenter::yes,
                                {new PropBurning()});
                }
        }

        // Destroy the surrounding environment
        const int nr_sweeps = 2 + (int)skill;

        for (int i = 0; i < nr_sweeps; ++i)
        {
                for (const auto& p : positions)
                {
                        if (!rnd::one_in(8))
                        {
                                continue;
                        }

                        bool is_adj_to_walkable_cell = false;

                        for (const P& d : dir_utils::g_dir_list)
                        {
                                const auto p_adj = p + d;

                                if (map::g_terrain.at(p_adj)->is_walkable())
                                {
                                        is_adj_to_walkable_cell = true;
                                }
                        }

                        if (is_adj_to_walkable_cell)
                        {
                                map::g_terrain.at(p)->hit(
                                        DmgType::explosion,
                                        nullptr);
                        }
                }
        }

        // Put blood, and set stuff on fire
        for (const auto& p : positions)
        {
                auto* const terrain = map::g_terrain.at(p);

                if (rnd::one_in(10))
                {
                        terrain->try_make_bloody();

                        if (rnd::one_in(3))
                        {
                                terrain->try_put_gore();
                        }
                }

                if ((p != caster->m_pos) && rnd::one_in(6))
                {
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

        snd_emit::run(snd);
}

std::vector<std::string> SpellMayhem::descr_specific(
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

bool SpellMayhem::allow_mon_cast_now(actor::Mon& mon) const
{
        return mon.m_ai_state.target &&
                (mon.m_ai_state.is_target_seen || rnd::one_in(20));
}

// -----------------------------------------------------------------------------
// Pestilence
// -----------------------------------------------------------------------------
void SpellPestilence::run_effect(
        actor::Actor* const caster,
        const SpellSkill skill) const
{
        const size_t nr_mon = 6 + (int)skill * 6;

        actor::Actor* leader = nullptr;

        if (caster->is_player())
        {
                leader = caster;
        }
        else
        {
                // Caster is monster
                auto* const caster_leader =
                        static_cast<actor::Mon*>(caster)->m_leader;

                leader =
                        caster_leader
                        ? caster_leader
                        : caster;
        }

        bool is_any_seen_by_player = false;

        const auto mon_summoned =
                actor::spawn(
                        caster->m_pos,
                        {nr_mon, actor::Id::rat},
                        map::rect())
                        .make_aware_of_player()
                        .set_leader(leader);

        std::for_each(
                std::begin(mon_summoned.monsters),
                std::end(mon_summoned.monsters),
                [skill, &is_any_seen_by_player](auto* const mon) {
                        mon->m_properties.apply(
                                property_factory::make(PropId::summoned));

                        auto* prop_waiting = new PropWaiting();

                        prop_waiting->set_duration(2);

                        mon->m_properties.apply(prop_waiting);

                        if (actor::can_player_see_actor(*mon))
                        {
                                is_any_seen_by_player = true;
                        }

                        // Haste the rats if master
                        if (skill == SpellSkill::master)
                        {
                                auto* prop_hasted = new PropHasted();

                                prop_hasted->set_indefinite();

                                mon->m_properties.apply(
                                        prop_hasted,
                                        PropSrc::intr,
                                        true,
                                        Verbose::no);
                        }
                });

        if (mon_summoned.monsters.empty())
        {
                return;
        }

        if (caster->is_player() ||
            is_any_seen_by_player)
        {
                std::string caster_str = "me";

                if (!caster->is_player())
                {
                        if (actor::can_player_see_actor(*caster))
                        {
                                caster_str = caster->name_the();
                        }
                        else
                        {
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

        const size_t nr_mon = 6 + (int)skill * 6;

        descr.push_back("Summons " + std::to_string(nr_mon) + " rats.");

        if (skill == SpellSkill::master)
        {
                descr.emplace_back("The rats are Hasted (moves faster).");
        }

        return descr;
}

bool SpellPestilence::allow_mon_cast_now(actor::Mon& mon) const
{
        const bool is_deep_liquid =
                map::g_terrain.at(mon.m_pos)->id() ==
                terrain::Id::liquid_deep;

        return mon.m_ai_state.target &&
                (mon.m_ai_state.is_target_seen || rnd::one_in(30)) &&
                !is_deep_liquid;
}

// -----------------------------------------------------------------------------
// Animate weapons
// -----------------------------------------------------------------------------
void SpellSpectralWpns::run_effect(
        actor::Actor* const caster,
        const SpellSkill skill) const
{
        TRACE_FUNC_BEGIN;

        if (!caster->is_player())
        {
                TRACE_FUNC_END;

                return;
        }

        // Find available weapons
        auto is_melee_wpn = [](const auto* const item) {
                return item && item->data().type == ItemType::melee_wpn;
        };

        std::vector<const item::Item*> weapons;

        for (const auto& slot : caster->m_inv.m_slots)
        {
                if (is_melee_wpn(slot.item))
                {
                        weapons.push_back(slot.item);
                }
        }

        for (const auto& item : caster->m_inv.m_backpack)
        {
                if (is_melee_wpn(item))
                {
                        weapons.push_back(item);
                }
        }

        // Cap the number of weapons spawned
        rnd::shuffle(weapons);

        const size_t nr_weapons_max = 1 + (int)skill;

        if (nr_weapons_max < weapons.size())
        {
                weapons.resize(nr_weapons_max);
        }

        // Spawn weapon monsters
        for (const auto* const item : weapons)
        {
                auto* new_item = item::make(item->id());

                new_item->set_melee_base_dmg(new_item->melee_base_dmg());

                auto spectral_wpn_init = [new_item, skill](auto* const mon) {
                        ASSERT(!mon->m_inv.item_in_slot(SlotId::wpn));

                        mon->m_inv.put_in_slot(
                                SlotId::wpn,
                                new_item,
                                Verbose::no);

                        mon->m_properties.apply(new PropSummoned());

                        auto* prop_waiting = new PropWaiting();

                        prop_waiting->set_duration(1);

                        mon->m_properties.apply(prop_waiting);

                        if (skill >= SpellSkill::expert)
                        {
                                auto* prop = new PropSeeInvis();

                                prop->set_indefinite();

                                mon->m_properties.apply(
                                        prop,
                                        PropSrc::intr,
                                        true,
                                        Verbose::no);
                        }

                        if (skill == SpellSkill::master)
                        {
                                auto* prop = new PropHasted();

                                prop->set_indefinite();

                                mon->m_properties.apply(
                                        prop,
                                        PropSrc::intr,
                                        true,
                                        Verbose::no);
                        }

                        if (actor::can_player_see_actor(*mon))
                        {
                                msg_log::add(mon->name_a() + " appears!");
                        }
                };

                const auto summoned =
                        actor::spawn(
                                caster->m_pos,
                                {actor::Id::spectral_wpn},
                                map::rect())
                                .set_leader(caster);

                std::for_each(
                        std::begin(summoned.monsters),
                        std::end(summoned.monsters),
                        spectral_wpn_init);
        }

        TRACE_FUNC_END;
}

std::vector<std::string> SpellSpectralWpns::descr_specific(
        const SpellSkill skill) const
{
        std::vector<std::string> descr;

        descr.emplace_back(
                "Conjures ghostly copies of carried weapons, which will float "
                "through the air and protect their master. The weapons almost "
                "always hit their target, but they quickly dematerialize when "
                "damaged. It is only possible to create copies of basic "
                "melee weapons - \"modern\" mechanisms such as pistols or "
                "machine guns are far too complex.");

        const size_t nr_weapons_max = 1 + (int)skill;

        std::string nr_summoned_descr =
                "A maximum of " +
                std::to_string(nr_weapons_max) +
                " ";

        if (nr_weapons_max == 1)
        {
                nr_summoned_descr += "weapon is";
        }
        else
        {
                nr_summoned_descr += "weapons are";
        }

        nr_summoned_descr += " spawned on casting the spell.";

        descr.push_back(nr_summoned_descr);

        if (skill >= SpellSkill::expert)
        {
                descr.emplace_back("The weapons can see invisible creatures.");
        }

        if (skill == SpellSkill::master)
        {
                descr.emplace_back("The weapons are hasted.");
        }

        return descr;
}

// -----------------------------------------------------------------------------
// Opening
// -----------------------------------------------------------------------------
void SpellOpening::run_effect(
        actor::Actor* const caster,
        const SpellSkill skill) const
{
        (void)caster;

        const int range = 1 + (int)skill * 3;

        const int orig_x = map::g_player->m_pos.x;
        const int orig_y = map::g_player->m_pos.y;

        const R area(
                std::max(0, orig_x - range),
                std::max(0, orig_y - range),
                std::min(map::w() - 1, orig_x + range),
                std::min(map::h() - 1, orig_y + range));

        bool is_any_opened = false;

        const int chance = 50 + (int)skill * 25;

        for (const auto& p : area.positions())
        {
                const auto did_open =
                        spells::run_opening_spell_effect_at(
                                p,
                                chance,
                                skill);

                if (did_open == terrain::DidOpen::yes)
                {
                        is_any_opened = true;
                }
        }

        if (is_any_opened)
        {
                map::update_vision();
        }
        else
        {
                // Nothing was opened
                msg_log::add("I hear faint rattling and knocking.");
        }
}

std::vector<std::string> SpellOpening::descr_specific(
        const SpellSkill skill) const
{
        std::vector<std::string> descr;

        std::string str = "Opens all locks, lids and doors";

        if (skill == SpellSkill::basic)
        {
                str += " (except heavy doors operated externally by a switch)";
        }

        str += ".";

        descr.push_back(str);

        if ((int)skill >= (int)SpellSkill::expert)
        {
                descr.emplace_back(
                        "If cast within range of a door operated by a lever, "
                        "then the lever is toggled.");
        }

        const int range = 1 + (int)skill * 3;

        if (range == 1)
        {
                descr.emplace_back("Only adjacent objects are opened.");
        }
        else
        {
                // Range > 1
                descr.push_back(
                        "Opens objects within a distance of " +
                        std::to_string(range) +
                        " cells.");
        }

        const int chance_to_open = 50 + (int)skill * 25;

        descr.push_back(std::to_string(chance_to_open) + "% chance to open.");

        return descr;
}

bool SpellOpening::is_noisy(const SpellSkill skill) const
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
        SpellSkill skill) const
{
        if (!caster)
        {
                return;
        }

        std::vector<actor::Actor*> targets;

        const auto seen_foes = actor::seen_foes(*caster);

        if (seen_foes.empty())
        {
                return;
        }

        if (skill == SpellSkill::basic)
        {
                targets.push_back(rnd::element(seen_foes));
        }
        else
        {
                // Skill greater than basic - target all seen foes
                targets = seen_foes;
        }

        for (auto* const actor : targets)
        {
                // Spell resistance?
                if (actor->m_properties.has(PropId::r_spell))
                {
                        on_resist(*actor);

                        // Spell reflection?
                        if (actor->m_properties.has(PropId::spell_reflect))
                        {
                                if (actor::can_player_see_actor(*actor))
                                {
                                        msg_log::add(
                                                s_spell_reflect_msg,
                                                colors::text(),
                                                MsgInterruptPlayer::no,
                                                MorePromptOnMsg::yes);
                                }

                                // Run effect with the target as caster instead
                                run_effect(actor, skill);
                        }

                        continue;
                }

                for (const auto& d : dir_utils::g_dir_list)
                {
                        const auto p(actor->m_pos + d);

                        // Hit the terrain with burning several times, to
                        // increase the chance of it catching fire
                        for (int i = 0; i < 6; ++i)
                        {
                                map::g_terrain.at(p)->hit(
                                        DmgType::fire,
                                        nullptr);
                        }
                }

                auto* const burning = property_factory::make(PropId::burning);

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

        if (skill == SpellSkill::basic)
        {
                descr.emplace_back(
                        "Affects one random visible hostile creature.");
        }
        else
        {
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
        if (skill == SpellSkill::basic)
        {
                return {3, 5};
        }
        else
        {
                return {5, 10};
        }
}

void SpellSanctuary::run_effect(
        actor::Actor* caster,
        SpellSkill skill) const
{
        if (!caster)
        {
                return;
        }

        const auto prop_duration = duration(skill);

        auto* const sanctuary = property_factory::make(PropId::sanctuary);

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
        return {3, 6};
}

Range SpellPurge::fear_duration_range() const
{
        return {2, 4};
}

void SpellPurge::run_effect(
        actor::Actor* caster,
        SpellSkill skill) const
{
        (void)skill;

        if (!caster)
        {
                return;
        }

        for (const auto& d : dir_utils::g_dir_list)
        {
                const auto p(caster->m_pos + d);

                auto* const terrain = map::g_terrain.at(p);

                switch (terrain->id())
                {
                case terrain::Id::altar:
                case terrain::Id::monolith:
                case terrain::Id::gong:
                {
                        if (map::is_pos_seen_by_player(p))
                        {
                                io::draw_blast_at_cells(
                                        {p},
                                        colors::light_white());
                        }

                        terrain->hit(DmgType::pure, caster);
                }
                break;

                default:
                {
                }
                break;
                }
        }

        for (auto* const actor : game_time::g_actors)
        {
                if ((actor != caster) &&
                    actor->m_pos.is_adjacent(caster->m_pos) &&
                    actor->m_data->is_undead)
                {
                        // Is adjacent undead creature

                        if (actor::can_player_see_actor(*actor))
                        {
                                const auto name =
                                        text_format::first_to_upper(
                                                actor->name_the());

                                msg_log::add(
                                        name + " is struck.",
                                        colors::msg_good());

                                io::draw_blast_at_cells(
                                        {actor->m_pos},
                                        colors::light_white());
                        }

                        actor::hit(*actor, dmg_range().roll(), DmgType::pure);

                        if (actor->is_alive())
                        {
                                auto* const fear =
                                        property_factory::make(
                                                PropId::terrified);

                                fear->set_duration(
                                        fear_duration_range().roll());

                                actor->m_properties.apply(fear);
                        }
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
        const SpellSkill skill) const
{
        (void)skill;

        auto* prop = new PropFrenzied();

        prop->set_duration(rnd::range(30, 40));

        caster->m_properties.apply(prop);
}

std::vector<std::string> SpellFrenzy::descr_specific(
        const SpellSkill skill) const
{
        (void)skill;

        return {
                "Incites a great rage in the caster, which will charge their "
                "enemies with a terrible, uncontrollable fury."};
}

// -----------------------------------------------------------------------------
// Bless
// -----------------------------------------------------------------------------
void SpellBless::run_effect(
        actor::Actor* const caster,
        const SpellSkill skill) const
{
        auto* prop = new PropBlessed();

        prop->set_duration(20 + (int)skill * 60);

        caster->m_properties.apply(prop);
}

std::vector<std::string> SpellBless::descr_specific(
        const SpellSkill skill) const
{
        std::vector<std::string> descr;

        descr.emplace_back(
                "The caster becomes more lucky (+10% to hit chance, "
                "evasion, stealth, and searching).");

        const int nr_turns = 20 + (int)skill * 60;

        descr.push_back(
                "The spell lasts " +
                std::to_string(nr_turns) +
                " turns.");

        return descr;
}

// -----------------------------------------------------------------------------
// Light
// -----------------------------------------------------------------------------
void SpellLight::run_effect(
        actor::Actor* const caster,
        const SpellSkill skill) const
{
        auto* prop = property_factory::make(PropId::radiant_fov);

        prop->set_duration(20 + (int)skill * 20);

        caster->m_properties.apply(prop);

        if (skill == SpellSkill::master)
        {
                auto* const blind = new PropBlind();

                blind->set_duration(std::max(1, blind->nr_turns_left() / 4));

                explosion::run(
                        caster->m_pos,
                        ExplType::apply_prop,
                        EmitExplSnd::no,
                        -1,
                        ExplExclCenter::yes,
                        {blind},
                        colors::yellow());
        }
}

std::vector<std::string> SpellLight::descr_specific(
        const SpellSkill skill) const
{
        std::vector<std::string> descr;

        descr.emplace_back("Illuminates the area around the caster.");

        const int nr_turns = 20 + (int)skill * 20;

        descr.push_back(
                "The spell lasts " +
                std::to_string(nr_turns) +
                " turns.");

        if (skill == SpellSkill::master)
        {
                descr.emplace_back(
                        "On casting, causes a blinding flash centered on the "
                        "caster (but not affecting the caster itself).");
        }

        return descr;
}

// -----------------------------------------------------------------------------
// See Invisible
// -----------------------------------------------------------------------------
void SpellSeeInvis::run_effect(
        actor::Actor* const caster,
        const SpellSkill skill) const
{
        const Range duration_range =
                (skill == SpellSkill::basic)
                ? Range(5, 8)
                : (skill == SpellSkill::expert)
                ? Range(40, 80)
                : Range(400, 600);

        auto* prop = new PropSeeInvis();

        prop->set_duration(duration_range.roll());

        caster->m_properties.apply(prop);
}

std::vector<std::string> SpellSeeInvis::descr_specific(
        const SpellSkill skill) const
{
        std::vector<std::string> descr;

        descr.emplace_back(
                "Grants the caster the ability to see the invisible.");

        const Range duration_range =
                (skill == SpellSkill::basic)
                ? Range(5, 8)
                : (skill == SpellSkill::expert)
                ? Range(40, 80)
                : Range(400, 600);

        descr.push_back("The spell lasts " + duration_range.str() + " turns.");

        return descr;
}

bool SpellSeeInvis::allow_mon_cast_now(actor::Mon& mon) const
{
        return (
                !mon.m_properties.has(PropId::see_invis) &&
                mon.is_aware_of_player() &&
                rnd::one_in(8));
}

// -----------------------------------------------------------------------------
// Spell Shield
// -----------------------------------------------------------------------------
int SpellSpellShield::base_max_spi_cost(const SpellSkill skill) const
{
        return 5 - (int)skill;
}

void SpellSpellShield::run_effect(
        actor::Actor* const caster,
        const SpellSkill skill) const
{
        (void)skill;

        auto* prop = property_factory::make(PropId::r_spell);

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

        descr.emplace_back(
                "Skill level affects the amount of Spirit one needs to spend "
                "to cast the spell.");

        return descr;
}

bool SpellSpellShield::allow_mon_cast_now(actor::Mon& mon) const
{
        return !mon.m_properties.has(PropId::r_spell);
}

// -----------------------------------------------------------------------------
// Haste
// -----------------------------------------------------------------------------
int SpellHaste::base_max_spi_cost(const SpellSkill skill) const
{
        (void)skill;

        return 5;
}

void SpellHaste::run_effect(
        actor::Actor* const caster,
        const SpellSkill skill) const
{
        Range duration_range;
        duration_range.min = 5 * ((int)skill + 1);
        duration_range.max = duration_range.min * 2;

        const int duration = duration_range.roll();

        auto* prop = new PropHasted();

        prop->set_duration(duration);

        caster->m_properties.apply(prop);
}

std::vector<std::string> SpellHaste::descr_specific(
        const SpellSkill skill) const
{
        std::vector<std::string> descr;

        descr.emplace_back(
                "Time slows down relative to the caster's perspective.");

        Range duration_range;
        duration_range.min = 5 * ((int)skill + 1);
        duration_range.max = duration_range.min * 2;

        descr.push_back("The spell lasts " + duration_range.str() + " turns.");

        return descr;
}

int SpellHaste::mon_cooldown() const
{
        return 20;
}

bool SpellHaste::allow_mon_cast_now(actor::Mon& mon) const
{
        return mon.m_ai_state.target &&
                mon.m_ai_state.is_target_seen &&
                !mon.m_properties.has(PropId::hasted);
}

// -----------------------------------------------------------------------------
// Premonition
// -----------------------------------------------------------------------------
int SpellPremonition::base_max_spi_cost(const SpellSkill skill) const
{
        (void)skill;

        return 7;
}

void SpellPremonition::run_effect(
        actor::Actor* const caster,
        const SpellSkill skill) const
{
        Range duration_range;
        duration_range.min = 4 + (int)skill * 4;
        duration_range.max = duration_range.min * 2;

        auto* prop = new PropPremonition();

        prop->set_duration(duration_range.roll());

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

        Range duration_range;
        duration_range.min = 4 + (int)skill * 4;
        duration_range.max = duration_range.min * 2;

        descr.push_back(
                "The spell lasts " +
                duration_range.str() +
                " turns.");

        return descr;
}

// -----------------------------------------------------------------------------
// Identify
// -----------------------------------------------------------------------------
int SpellIdentify::base_max_spi_cost(const SpellSkill skill) const
{
        (void)skill;

        return 8;
}

void SpellIdentify::run_effect(
        actor::Actor* const caster,
        const SpellSkill skill) const
{
        (void)caster;
        (void)skill;

        std::vector<ItemType> item_types_allowed;

        if (skill != SpellSkill::master)
        {
                item_types_allowed.push_back(ItemType::scroll);

                if (skill == SpellSkill::expert)
                {
                        item_types_allowed.push_back(ItemType::potion);
                }
        }

        // Run identify selection menu
        states::push(std::make_unique<SelectIdentify>(item_types_allowed));

        msg_log::more_prompt();
}

std::vector<std::string> SpellIdentify::descr_specific(
        const SpellSkill skill) const
{
        std::string descr = "Identifies ";

        switch (skill)
        {
        case SpellSkill::basic:
                descr += "Manuscripts";
                break;

        case SpellSkill::expert:
                descr += "Manuscripts and Potions";
                break;

        case SpellSkill::master:
                descr += "all items";
                break;
        }

        descr += ".";

        return {descr};
}

// -----------------------------------------------------------------------------
// Teleport
// -----------------------------------------------------------------------------
void SpellTeleport::run_effect(
        actor::Actor* const caster,
        const SpellSkill skill) const
{
        if ((int)skill >= (int)SpellSkill::expert)
        {
                auto* const invis = property_factory::make(PropId::invis);

                invis->set_duration(3);

                caster->m_properties.apply(invis);
        }

        const int max_d = max_dist(skill);

        teleport(*caster, ShouldCtrlTele::if_tele_ctrl_prop, max_d);
}

int SpellTeleport::max_dist(const SpellSkill skill) const
{
        switch (skill)
        {
        case SpellSkill::basic:
                return 5;

        case SpellSkill::expert:
                return 10;

        case SpellSkill::master:
                return 15;
        }

        ASSERT(false);
        return -1;
}

bool SpellTeleport::allow_mon_cast_now(actor::Mon& mon) const
{
        const bool is_low_hp = mon.m_hp <= (actor::max_hp(mon) / 2);

        return (
                mon.is_aware_of_player() &&
                is_low_hp &&
                rnd::fraction(3, 4));
}

std::vector<std::string> SpellTeleport::descr_specific(
        const SpellSkill skill) const
{
        std::vector<std::string> descr;

        descr.emplace_back(
                "Instantly moves the caster to a different position.");

        const int dist = max_dist(skill);

        descr.emplace_back(
                "Maximum teleport distance is " +
                std::to_string(dist) +
                " cells.");

        if ((int)skill >= (int)SpellSkill::expert)
        {
                const int nr_turns = 3;

                descr.push_back(
                        "On teleporting, the caster is invisible for " +
                        std::to_string(nr_turns) +
                        " turns.");
        }

        return descr;
}

// -----------------------------------------------------------------------------
// Resistance
// -----------------------------------------------------------------------------
void SpellRes::run_effect(
        actor::Actor* const caster,
        const SpellSkill skill) const
{
        int nr_turns = 15 + (int)skill * 35;

        auto* prop_r_fire = new PropRFire;
        auto* prop_r_elec = new PropRElec;

        prop_r_fire->set_duration(nr_turns);
        prop_r_elec->set_duration(nr_turns);

        caster->m_properties.apply(prop_r_fire);
        caster->m_properties.apply(prop_r_elec);
}

std::vector<std::string> SpellRes::descr_specific(
        const SpellSkill skill) const
{
        std::vector<std::string> descr;

        descr.emplace_back(
                "The caster is completely shielded from fire and electricity.");

        const int nr_turns = 15 + (int)skill * 35;

        descr.push_back(
                "The spell lasts " +
                std::to_string(nr_turns) +
                " turns.");

        return descr;
}

bool SpellRes::allow_mon_cast_now(actor::Mon& mon) const
{
        const bool has_rfire = mon.m_properties.has(PropId::r_fire);
        const bool has_relec = mon.m_properties.has(PropId::r_elec);

        return (!has_rfire || !has_relec) &&
                mon.m_ai_state.target;
}

// -----------------------------------------------------------------------------
// Knockback
// -----------------------------------------------------------------------------
void SpellKnockBack::run_effect(
        actor::Actor* const caster,
        const SpellSkill skill) const
{
        (void)skill;

        ASSERT(!caster->is_player());

        auto msg_clr = colors::msg_good();
        std::string target_str = "me";
        auto* caster_used = caster;
        auto* const mon = static_cast<actor::Mon*>(caster_used);
        auto* target = mon->m_ai_state.target;

        ASSERT(target);
        ASSERT(mon->m_ai_state.is_target_seen);

        // Spell resistance?
        if (target->m_properties.has(PropId::r_spell))
        {
                on_resist(*target);

                // Spell reflection?
                if (target->m_properties.has(PropId::spell_reflect))
                {
                        if (actor::can_player_see_actor(*target))
                        {
                                msg_log::add(
                                        s_spell_reflect_msg,
                                        colors::text(),
                                        MsgInterruptPlayer::no,
                                        MorePromptOnMsg::yes);
                        }

                        std::swap(caster_used, target);
                }
                else
                {
                        // No spell reflection, just abort
                        return;
                }
        }

        if (target->is_player())
        {
                msg_clr = colors::msg_bad();
        }
        else
        {
                // Target is monster
                target_str = target->name_the();

                if (map::g_player->is_leader_of(target))
                {
                        msg_clr = colors::white();
                }
        }

        if (actor::can_player_see_actor(*target))
        {
                msg_log::add("A force pushes " + target_str + "!", msg_clr);
        }

        knockback::run(*target, caster->m_pos, false);

        if (!target->is_player())
        {
                static_cast<actor::Mon*>(target)
                        ->become_aware_player(
                                actor::AwareSource::spell_victim);
        }
}

bool SpellKnockBack::allow_mon_cast_now(actor::Mon& mon) const
{
        return mon.m_ai_state.target && mon.m_ai_state.is_target_seen;
}

// -----------------------------------------------------------------------------
// Enfeeble
// -----------------------------------------------------------------------------
int SpellEnfeeble::base_max_spi_cost(const SpellSkill skill) const
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
        const SpellSkill skill) const
{
        Range duration_range;
        duration_range.min = 15 * ((int)skill + 1);
        duration_range.max = duration_range.min * 2;

        const int duration = duration_range.roll();

        auto targets = actor::seen_foes(*caster);

        if (targets.empty())
        {
                msg_log::add(
                        "The bugs on the ground suddenly move very feebly.");

                return;
        }

        // There are targets available

        if (skill == SpellSkill::basic)
        {
                auto* const target = rnd::element(targets);

                targets.clear();

                targets.push_back(target);
        }

        io::draw_blast_at_seen_actors(targets, colors::magenta());

        for (auto* const target : targets)
        {
                // Spell resistance?
                if (target->m_properties.has(PropId::r_spell))
                {
                        on_resist(*target);

                        // Spell reflection?
                        if (target->m_properties.has(PropId::spell_reflect))
                        {
                                if (actor::can_player_see_actor(*target))
                                {
                                        msg_log::add(
                                                s_spell_reflect_msg,
                                                colors::text(),
                                                MsgInterruptPlayer::no,
                                                MorePromptOnMsg::yes);
                                }

                                // Run effect with target as caster
                                run_effect(target, skill);
                        }

                        continue;
                }

                auto* const prop = property_factory::make(PropId::weakened);

                prop->set_duration(duration);

                target->m_properties.apply(prop);

                if (!target->is_player())
                {
                        static_cast<actor::Mon*>(target)
                                ->become_aware_player(
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

        if (skill == SpellSkill::basic)
        {
                descr.emplace_back(
                        "Affects one random visible hostile creature.");
        }
        else
        {
                descr.emplace_back(
                        "Affects all visible hostile creatures.");
        }

        Range duration_range;
        duration_range.min = 15 * ((int)skill + 1);
        duration_range.max = duration_range.min * 2;

        descr.push_back(
                "The spell lasts " +
                duration_range.str() +
                " turns.");

        return descr;
}

bool SpellEnfeeble::allow_mon_cast_now(actor::Mon& mon) const
{
        return mon.m_ai_state.target && mon.m_ai_state.is_target_seen;
}

// -----------------------------------------------------------------------------
// Slow
// -----------------------------------------------------------------------------
int SpellSlow::base_max_spi_cost(const SpellSkill skill) const
{
        (void)skill;

        return 4;
}

void SpellSlow::run_effect(
        actor::Actor* const caster,
        const SpellSkill skill) const
{
        Range duration_range;
        duration_range.min = 10 * ((int)skill + 1);
        duration_range.max = duration_range.min * 2;

        const int duration = duration_range.roll();

        auto targets = actor::seen_foes(*caster);

        if (targets.empty())
        {
                msg_log::add(
                        "The bugs on the ground suddenly move very slowly.");

                return;
        }

        // There are targets available

        if (skill == SpellSkill::basic)
        {
                auto* const target = rnd::element(targets);

                targets.clear();

                targets.push_back(target);
        }

        io::draw_blast_at_seen_actors(targets, colors::magenta());

        for (auto* const target : targets)
        {
                // Spell resistance?
                if (target->m_properties.has(PropId::r_spell))
                {
                        on_resist(*target);

                        // Spell reflection?
                        if (target->m_properties.has(PropId::spell_reflect))
                        {
                                if (actor::can_player_see_actor(*target))
                                {
                                        msg_log::add(
                                                s_spell_reflect_msg,
                                                colors::text(),
                                                MsgInterruptPlayer::no,
                                                MorePromptOnMsg::yes);
                                }

                                // Run effect with target as caster
                                run_effect(target, skill);
                        }

                        continue;
                }

                auto* const prop = new PropSlowed();

                prop->set_duration(duration);

                target->m_properties.apply(prop);

                if (!target->is_player())
                {
                        static_cast<actor::Mon*>(target)
                                ->become_aware_player(
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

        if (skill == SpellSkill::basic)
        {
                descr.emplace_back(
                        "Affects one random visible hostile creature.");
        }
        else
        {
                descr.emplace_back(
                        "Affects all visible hostile creatures.");
        }

        Range duration_range;
        duration_range.min = 10 * ((int)skill + 1);
        duration_range.max = duration_range.min * 2;

        descr.push_back(
                "The spell lasts " +
                duration_range.str() +
                " turns.");

        return descr;
}

int SpellSlow::mon_cooldown() const
{
        return 20;
}

bool SpellSlow::allow_mon_cast_now(actor::Mon& mon) const
{
        return mon.m_ai_state.target && mon.m_ai_state.is_target_seen;
}

// -----------------------------------------------------------------------------
// Terrify
// -----------------------------------------------------------------------------
Range SpellTerrify::duration_range(SpellSkill skill) const
{
        Range range;
        range.min = 6 * ((int)skill + 1);
        range.max = range.min * 2;

        return range;
}

int SpellTerrify::base_max_spi_cost(const SpellSkill skill) const
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
        const SpellSkill skill) const
{
        auto targets = actor::seen_foes(*caster);

        if (targets.empty())
        {
                msg_log::add("The bugs on the ground suddenly scatter away.");

                return;
        }

        // There are targets available

        if (skill == SpellSkill::basic)
        {
                auto* const target = rnd::element(targets);

                targets.clear();

                targets.push_back(target);
        }

        io::draw_blast_at_seen_actors(targets, colors::magenta());

        for (auto* const target : targets)
        {
                // Spell resistance?
                if (target->m_properties.has(PropId::r_spell))
                {
                        on_resist(*target);

                        // Spell reflection?
                        if (target->m_properties.has(PropId::spell_reflect))
                        {
                                if (actor::can_player_see_actor(*target))
                                {
                                        msg_log::add(
                                                s_spell_reflect_msg,
                                                colors::text(),
                                                MsgInterruptPlayer::no,
                                                MorePromptOnMsg::yes);
                                }

                                // Run effect with target as caster
                                run_effect(target, skill);
                        }

                        continue;
                }

                auto* const prop = new PropTerrified();

                prop->set_duration(
                        duration_range(skill).roll());

                target->m_properties.apply(prop);

                if (!target->is_player())
                {
                        static_cast<actor::Mon*>(target)
                                ->become_aware_player(
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

        if (skill == SpellSkill::basic)
        {
                descr.emplace_back(
                        "Affects one random visible hostile creature.");
        }
        else
        {
                descr.emplace_back(
                        "Affects all visible hostile creatures.");
        }

        descr.push_back(
                "The spell lasts " +
                duration_range(skill).str() +
                " turns.");

        return descr;
}

bool SpellTerrify::allow_mon_cast_now(actor::Mon& mon) const
{
        return mon.m_ai_state.target && mon.m_ai_state.is_target_seen;
}

// -----------------------------------------------------------------------------
// Disease
// -----------------------------------------------------------------------------
void SpellDisease::run_effect(
        actor::Actor* const caster,
        const SpellSkill skill) const
{
        (void)skill;

        ASSERT(!caster->is_player());

        auto* caster_used = caster;

        auto* const mon = static_cast<actor::Mon*>(caster_used);

        auto* target = mon->m_ai_state.target;

        ASSERT(target);

        ASSERT(mon->m_ai_state.is_target_seen);

        // Spell resistance?
        if (target->m_properties.has(PropId::r_spell))
        {
                on_resist(*target);

                // Spell reflection?
                if (target->m_properties.has(PropId::spell_reflect))
                {
                        if (actor::can_player_see_actor(*target))
                        {
                                msg_log::add(
                                        s_spell_reflect_msg,
                                        colors::text(),
                                        MsgInterruptPlayer::no,
                                        MorePromptOnMsg::yes);
                        }

                        std::swap(caster_used, target);
                }
                else
                {
                        // No spell reflection, just abort
                        return;
                }
        }

        std::string actor_name = "me";

        if (!target->is_player())
        {
                actor_name = target->name_the();
        }

        if (actor::can_player_see_actor(*target))
        {
                msg_log::add(
                        "A horrible disease is starting to afflict " +
                        actor_name +
                        "!");
        }

        target->m_properties.apply(new PropDiseased());

        if (!target->is_player())
        {
                static_cast<actor::Mon*>(target)
                        ->become_aware_player(
                                actor::AwareSource::spell_victim);
        }
}

bool SpellDisease::allow_mon_cast_now(actor::Mon& mon) const
{
        return mon.m_ai_state.target && mon.m_ai_state.is_target_seen;
}

// -----------------------------------------------------------------------------
// Summon monster
// -----------------------------------------------------------------------------
void SpellSummonMon::run_effect(
        actor::Actor* const caster,
        const SpellSkill skill) const
{
        std::vector<actor::Id> summon_bucket;

        if (caster->is_player())
        {
                // Player summong, pick from predefined lists
                if ((skill == SpellSkill::basic) ||
                    (skill == SpellSkill::expert))
                {
                        summon_bucket.push_back(actor::Id::raven);
                        summon_bucket.push_back(actor::Id::wolf);
                }

                if ((skill == SpellSkill::expert) ||
                    (skill == SpellSkill::master))
                {
                        summon_bucket.push_back(actor::Id::green_spider);
                        summon_bucket.push_back(actor::Id::white_spider);
                }

                if (skill == SpellSkill::master)
                {
                        summon_bucket.push_back(actor::Id::leng_spider);
                        summon_bucket.push_back(actor::Id::energy_hound);
                        summon_bucket.push_back(actor::Id::fire_hound);
                }
        }
        else
        {
                // Caster is monster
                int max_dlvl_spawned =
                        (skill == SpellSkill::basic)
                        ? g_dlvl_last_early_game
                        : ((skill == SpellSkill::expert)
                                   ? g_dlvl_last_mid_game
                                   : g_dlvl_last);

                const int nr_dlvls_ood_allowed = 3;

                max_dlvl_spawned =
                        std::min(
                                max_dlvl_spawned,
                                map::g_dlvl + nr_dlvls_ood_allowed);

                const int min_dlvl_spawned = max_dlvl_spawned - 10;

                for (size_t i = 0; i < (size_t)actor::Id::END; ++i)
                {
                        const auto& data = actor::g_data[i];

                        if (!data.can_be_summoned_by_mon)
                        {
                                continue;
                        }

                        if ((data.spawn_min_dlvl <= max_dlvl_spawned) &&
                            (data.spawn_min_dlvl >= min_dlvl_spawned))
                        {
                                summon_bucket.push_back(actor::Id(i));
                        }
                }

                // If no monsters could be found which matched the level
                // criteria, try again, but without the min level criterium
                if (summon_bucket.empty())
                {
                        for (size_t i = 0; i < (size_t)actor::Id::END; ++i)
                        {
                                const auto& data = actor::g_data[i];

                                if (!data.can_be_summoned_by_mon)
                                {
                                        continue;
                                }

                                if (data.spawn_min_dlvl <= max_dlvl_spawned)
                                {
                                        summon_bucket.push_back(actor::Id(i));
                                }
                        }
                }
        }

        if (summon_bucket.empty())
        {
                TRACE << "No elligible monsters found for spawning"
                      << std::endl;

                ASSERT(false);

                return;
        }

#ifndef NDEBUG
        if (!caster->is_player())
        {
                for (const auto id : summon_bucket)
                {
                        ASSERT(actor::g_data[(size_t)id]
                                       .can_be_summoned_by_mon);
                }
        }
#endif  // NDEBUG

        const actor::Id mon_id = rnd::element(summon_bucket);

        actor::Actor* leader = nullptr;

        if (caster->is_player())
        {
                leader = caster;
        }
        else
        {
                // Caster is monster
                auto* const caster_leader =
                        static_cast<actor::Mon*>(caster)->m_leader;

                leader =
                        caster_leader
                        ? caster_leader
                        : caster;
        }

        const auto summoned =
                actor::spawn(
                        caster->m_pos,
                        {mon_id},
                        map::rect())
                        .make_aware_of_player()
                        .set_leader(leader);

        std::for_each(
                std::begin(summoned.monsters),
                std::end(summoned.monsters),
                [](auto* const mon) {
                        mon->m_properties.apply(new PropSummoned());

                        auto* prop_waiting = new PropWaiting();

                        prop_waiting->set_duration(2);

                        mon->m_properties.apply(prop_waiting);
                });

        if (summoned.monsters.empty())
        {
                return;
        }

        auto* const mon = summoned.monsters[0];

        if (actor::can_player_see_actor(*mon))
        {
                msg_log::add(
                        text_format::first_to_upper(
                                mon->name_a()) +
                        " appears!");
        }

        // Player cannot see monster
        else if (caster->is_player())
        {
                msg_log::add("I sense a new presence.");
        }
}

std::vector<std::string> SpellSummonMon::descr_specific(
        const SpellSkill skill) const
{
        (void)skill;

        return {
                "Summons a creature to do the caster's bidding. "
                "A more skilled sorcerer summons beings of greater might and "
                "rarity."};
}

bool SpellSummonMon::allow_mon_cast_now(actor::Mon& mon) const
{
        const bool is_deep_liquid =
                map::g_terrain.at(mon.m_pos)->id() ==
                terrain::Id::liquid_deep;

        return mon.m_ai_state.target &&
                (mon.m_ai_state.is_target_seen || rnd::one_in(30)) &&
                !is_deep_liquid;
}

// -----------------------------------------------------------------------------
// Summon tentacles
// -----------------------------------------------------------------------------
void SpellSummonTentacles::run_effect(
        actor::Actor* const caster,
        const SpellSkill skill) const
{
        (void)skill;

        actor::Actor* leader = nullptr;

        if (caster->is_player())
        {
                leader = caster;
        }
        else
        {
                // Caster is monster
                auto* const caster_leader =
                        static_cast<actor::Mon*>(caster)->m_leader;

                leader =
                        caster_leader
                        ? caster_leader
                        : caster;
        }

        const auto summoned =
                actor::spawn(
                        caster->m_pos,
                        {actor::Id::tentacles},
                        map::rect())
                        .make_aware_of_player()
                        .set_leader(leader);

        std::for_each(
                std::begin(summoned.monsters),
                std::end(summoned.monsters),
                [](auto* const mon) {
                        mon->m_properties.apply(new PropSummoned());

                        auto* prop_waiting = new PropWaiting();

                        prop_waiting->set_duration(2);

                        mon->m_properties.apply(prop_waiting);
                });

        if (summoned.monsters.empty())
        {
                return;
        }

        auto* const mon = summoned.monsters[0];

        if (actor::can_player_see_actor(*mon))
        {
                msg_log::add("Monstrous tentacles rises up from the ground!");
        }
}

bool SpellSummonTentacles::allow_mon_cast_now(actor::Mon& mon) const
{
        const bool is_deep_liquid =
                map::g_terrain.at(mon.m_pos)->id() ==
                terrain::Id::liquid_deep;

        return mon.m_ai_state.target &&
                mon.m_ai_state.is_target_seen &&
                !is_deep_liquid;
}

// -----------------------------------------------------------------------------
// Healing
// -----------------------------------------------------------------------------
void SpellHeal::run_effect(
        actor::Actor* const caster,
        const SpellSkill skill) const
{
        if ((int)skill >= (int)SpellSkill::expert)
        {
                caster->m_properties.end_prop(PropId::infected);
                caster->m_properties.end_prop(PropId::diseased);
                caster->m_properties.end_prop(PropId::weakened);
                caster->m_properties.end_prop(PropId::hp_sap);
        }

        if (skill == SpellSkill::master)
        {
                caster->m_properties.end_prop(PropId::blind);
                caster->m_properties.end_prop(PropId::deaf);
                caster->m_properties.end_prop(PropId::poisoned);

                if (caster->is_player())
                {
                        Prop* const wound_prop =
                                map::g_player->m_properties.prop(PropId::wound);

                        if (wound_prop)
                        {
                                auto* const wound =
                                        static_cast<PropWound*>(wound_prop);

                                wound->heal_one_wound();
                        }
                }
        }

        const int nr_hp_restored = 8 + (int)skill * 8;

        caster->restore_hp(nr_hp_restored);
}

bool SpellHeal::allow_mon_cast_now(actor::Mon& mon) const
{
        return mon.m_hp < actor::max_hp(mon);
}

std::vector<std::string> SpellHeal::descr_specific(
        const SpellSkill skill) const
{
        std::vector<std::string> descr;

        const int nr_hp_restored = 8 + (int)skill * 8;

        descr.push_back(
                "Restores " +
                std::to_string(nr_hp_restored) +
                " Hit Points.");

        if (skill == SpellSkill::expert)
        {
                descr.emplace_back(
                        "Cures infections, disease, weakening, and life "
                        "sapping.");
        }
        else if (skill == SpellSkill::master)
        {
                descr.emplace_back(
                        "Cures infections, disease, weakening, life sapping, "
                        "blindness, deafness, and poisoning.");

                descr.emplace_back("Heals one wound.");
        }

        return descr;
}

// -----------------------------------------------------------------------------
// Mi-go hypnosis
// -----------------------------------------------------------------------------
void SpellMiGoHypno::run_effect(
        actor::Actor* const caster,
        const SpellSkill skill) const
{
        (void)skill;

        ASSERT(!caster->is_player());

        auto* caster_used = caster;

        auto* const mon = static_cast<actor::Mon*>(caster_used);

        auto* target = mon->m_ai_state.target;

        ASSERT(target);

        ASSERT(mon->m_ai_state.is_target_seen);

        // Spell resistance?
        if (target->m_properties.has(PropId::r_spell))
        {
                on_resist(*target);

                // Spell reflection?
                if (target->m_properties.has(PropId::spell_reflect))
                {
                        if (actor::can_player_see_actor(*target))
                        {
                                msg_log::add(
                                        s_spell_reflect_msg,
                                        colors::text(),
                                        MsgInterruptPlayer::no,
                                        MorePromptOnMsg::yes);
                        }

                        std::swap(caster_used, target);
                }
                else
                {
                        // No spell reflection, just abort
                        return;
                }
        }

        if (target->is_player())
        {
                msg_log::add("There is a sharp droning in my head!");
        }

        if (rnd::coin_toss())
        {
                auto* prop_fainted = new PropFainted();

                prop_fainted->set_duration(rnd::range(2, 10));

                target->m_properties.apply(prop_fainted);
        }
        else
        {
                msg_log::add("I feel dizzy.");
        }

        if (!target->is_player())
        {
                static_cast<actor::Mon*>(target)
                        ->become_aware_player(
                                actor::AwareSource::spell_victim);
        }
}

bool SpellMiGoHypno::allow_mon_cast_now(actor::Mon& mon) const
{
        return mon.m_ai_state.target &&
                mon.m_ai_state.is_target_seen &&
                mon.m_ai_state.target->is_player();
}

// -----------------------------------------------------------------------------
// Immolation
// -----------------------------------------------------------------------------
void SpellBurn::run_effect(
        actor::Actor* const caster,
        const SpellSkill skill) const
{
        ASSERT(!caster->is_player());

        auto* caster_used = caster;

        auto* const mon = static_cast<actor::Mon*>(caster_used);

        auto* target = mon->m_ai_state.target;

        ASSERT(target);

        ASSERT(mon->m_ai_state.is_target_seen);

        // Spell resistance?
        if (target->m_properties.has(PropId::r_spell))
        {
                on_resist(*target);

                // Spell reflection?
                if (target->m_properties.has(PropId::spell_reflect))
                {
                        if (actor::can_player_see_actor(*target))
                        {
                                msg_log::add(
                                        s_spell_reflect_msg,
                                        colors::text(),
                                        MsgInterruptPlayer::no,
                                        MorePromptOnMsg::yes);
                        }

                        std::swap(caster_used, target);
                }
                else
                {
                        // No spell reflection, just abort
                        return;
                }
        }

        std::string target_str = "me";

        if (!target->is_player())
        {
                target_str = target->name_the();
        }

        if (actor::can_player_see_actor(*target))
        {
                msg_log::add("Flames are rising around " + target_str + "!");
        }

        auto* prop = new PropBurning();

        prop->set_duration(2 + (int)skill);

        target->m_properties.apply(prop);

        if (!target->is_player())
        {
                static_cast<actor::Mon*>(target)
                        ->become_aware_player(
                                actor::AwareSource::spell_victim);
        }
}

bool SpellBurn::allow_mon_cast_now(actor::Mon& mon) const
{
        return mon.m_ai_state.target && mon.m_ai_state.is_target_seen;
}

// -----------------------------------------------------------------------------
// Deafening
// -----------------------------------------------------------------------------
void SpellDeafen::run_effect(
        actor::Actor* const caster,
        const SpellSkill skill) const
{
        ASSERT(!caster->is_player());

        auto* caster_used = caster;

        auto* const mon = static_cast<actor::Mon*>(caster_used);

        auto* target = mon->m_ai_state.target;

        ASSERT(target);

        ASSERT(mon->m_ai_state.is_target_seen);

        // Spell resistance?
        if (target->m_properties.has(PropId::r_spell))
        {
                on_resist(*target);

                // Spell reflection?
                if (target->m_properties.has(PropId::spell_reflect))
                {
                        if (actor::can_player_see_actor(*target))
                        {
                                msg_log::add(
                                        s_spell_reflect_msg,
                                        colors::text(),
                                        MsgInterruptPlayer::no,
                                        MorePromptOnMsg::yes);
                        }

                        std::swap(caster_used, target);
                }
                else
                {
                        // No spell reflection, just abort
                        return;
                }
        }

        auto* prop = property_factory::make(PropId::deaf);

        prop->set_duration(75 + (int)skill * 75);

        target->m_properties.apply(prop);

        if (!target->is_player())
        {
                static_cast<actor::Mon*>(target)
                        ->become_aware_player(
                                actor::AwareSource::spell_victim);
        }
}

bool SpellDeafen::allow_mon_cast_now(actor::Mon& mon) const
{
        return mon.m_ai_state.target && mon.m_ai_state.is_target_seen;
}

// -----------------------------------------------------------------------------
// Transmutation
// -----------------------------------------------------------------------------
void SpellTransmut::run_effect(
        actor::Actor* const caster,
        const SpellSkill skill) const
{
        (void)caster;

        const P& p = map::g_player->m_pos;

        auto* item_before = map::g_items.at(p);

        if (!item_before)
        {
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

        const int melee_plus = item_before->melee_base_dmg().plus();

        const auto id_before = item_before->id();

        std::string item_name_before = "The ";

        if (nr_items_before > 1)
        {
                item_name_before += item_before->name(ItemRefType::plural);
        }
        else
        {
                // Single item
                item_name_before += item_before->name(ItemRefType::plain);
        }

        // Remove the existing item(s)
        delete map::g_items.at(p);
        map::g_items.at(p) = nullptr;

        if (map::g_seen.at(p))
        {
                msg_log::add(
                        item_name_before + " disappears.",
                        colors::text(),
                        MsgInterruptPlayer::no,
                        MorePromptOnMsg::yes);
        }

        // Determine which item(s) to spawn, if any
        int pct_chance_per_item = 10 * (int)skill;

        std::vector<item::Id> id_bucket;

        // Converting a potion?
        if (item_type_before == ItemType::potion)
        {
                pct_chance_per_item += 40;

                for (size_t item_id = 0;
                     (item::Id)item_id != item::Id::END;
                     ++item_id)
                {
                        if ((item::Id)item_id == id_before)
                        {
                                continue;
                        }

                        const auto& d = item::g_data[item_id];

                        if (d.type == ItemType::potion)
                        {
                                id_bucket.push_back((item::Id)item_id);
                        }
                }
        }
        // Converting a scroll?
        else if (item_type_before == ItemType::scroll)
        {
                pct_chance_per_item += 40;

                for (size_t item_id = 0;
                     (item::Id)item_id != item::Id::END;
                     ++item_id)
                {
                        if ((item::Id)item_id == id_before)
                        {
                                continue;
                        }

                        const auto& d = item::g_data[item_id];

                        if (d.type == ItemType::scroll)
                        {
                                id_bucket.push_back((item::Id)item_id);
                        }
                }
        }
        // Converting a melee weapon (with at least one "plus")?
        else if ((item_type_before == ItemType::melee_wpn) && (melee_plus >= 1))
        {
                pct_chance_per_item += (melee_plus * 10);

                for (size_t item_id = 0;
                     (item::Id)item_id != item::Id::END;
                     ++item_id)
                {
                        const auto& d = item::g_data[item_id];

                        if ((d.type == ItemType::potion) ||
                            (d.type == ItemType::scroll))
                        {
                                id_bucket.push_back((item::Id)item_id);
                        }
                }
        }

        // Never spawn Transmute scrolls, this is just dumb
        for (auto it = std::begin(id_bucket); it != std::end(id_bucket);)
        {
                if (*it == item::Id::scroll_transmut)
                {
                        it = id_bucket.erase(it);
                }
                else
                {
                        // Not transmute
                        ++it;
                }
        }

        auto id_new = item::Id::END;

        if (!id_bucket.empty())
        {
                id_new = rnd::element(id_bucket);
        }

        int nr_items_new = 0;

        // How many items?
        for (int i = 0; i < nr_items_before; ++i)
        {
                if (rnd::percent(pct_chance_per_item))
                {
                        ++nr_items_new;
                }
        }

        if ((id_new == item::Id::END) || (nr_items_new < 1))
        {
                msg_log::add("Nothing appears.");

                return;
        }

        // OK, items are good, and player succeeded the rolls etc

        // Spawn new item(s)
        auto* const item_new =
                item::make_item_on_floor(
                        id_new,
                        map::g_player->m_pos);

        if (item_new->data().is_stackable)
        {
                item_new->m_nr_items = nr_items_new;
        }

        if (map::g_seen.at(p))
        {
                const std::string item_name_new =
                        text_format::first_to_upper(
                                item_new->name(ItemRefType::plural));

                msg_log::add(item_name_new + " appears.");
        }
}

std::vector<std::string> SpellTransmut::descr_specific(
        const SpellSkill skill) const
{
        std::vector<std::string> descr;

        descr.emplace_back(
                "Attempts to convert items (stand over an item when casting). "
                "On failure, the item is destroyed.");

        const int skill_bon = 10 * (int)skill;

        const int chance_per_pot = 40 + skill_bon;

        const int chance_per_scroll = 40 + skill_bon;

        const int chance_per_wpn_plus = 10;

        const int chance_wpn_plus_1 = skill_bon + chance_per_wpn_plus;
        const int chance_wpn_plus_2 = skill_bon + chance_per_wpn_plus * 2;
        const int chance_wpn_plus_3 = skill_bon + chance_per_wpn_plus * 3;

        descr.push_back(
                "Converts Potions with " +
                std::to_string(chance_per_pot) +
                "% chance.");

        descr.push_back(
                "Converts Manuscripts with " +
                std::to_string(chance_per_scroll) +
                "% chance.");

        descr.push_back(
                "Melee weapons with at least +1 damage (not counting any "
                "damage bonus from skills) are converted to a Potion or "
                "Manuscript, with " +
                std::to_string(chance_wpn_plus_1) +
                "% chance for a +1 weapon, " +
                std::to_string(chance_wpn_plus_2) +
                "% chance for a +2 weapon, " +
                std::to_string(chance_wpn_plus_3) +
                "% chance for a +3 weapon, etc.");

        return descr;
}
