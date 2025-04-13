// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "actor_data.hpp"

#include <algorithm>
#include <iterator>
#include <optional>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "colors.hpp"
#include "debug.hpp"
#include "item_att_property.hpp"
#include "item_data.hpp"
#include "paths.hpp"
#include "property.hpp"
#include "property_data.hpp"
#include "property_factory.hpp"
#include "saving.hpp"
#include "xml.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
template <typename K, typename V>
static std::vector<std::string> get_keys_sorted(
        const std::unordered_map<K, V>& m)
{
        std::vector<std::string> keys;
        keys.reserve(m.size());

        for (const auto& it : m) {
                keys.push_back(it.first);
        }

        std::sort(std::begin(keys), std::end(keys));

        return keys;
}

static const std::unordered_map<std::string, MonShockLvl> str_to_shock_lvl_map = {
        {"MONSHOCK_NONE", MonShockLvl::none},
        {"MONSHOCK_UNSETTLING", MonShockLvl::unsettling},
        {"MONSHOCK_FRIGHTENING", MonShockLvl::frightening},
        {"MONSHOCK_TERRIFYING", MonShockLvl::terrifying},
        {"MONSHOCK_MIND_SHATTERING", MonShockLvl::mind_shattering}};

static const std::unordered_map<std::string, actor::Speed> str_to_speed_map = {
        {"MONSPEED_SLOW", actor::Speed::slow},
        {"MONSPEED_NORMAL", actor::Speed::normal},
        {"MONSPEED_FAST", actor::Speed::fast},
        {"MONSPEED_VERY_FAST", actor::Speed::very_fast}};

static const std::unordered_map<std::string, actor::MonGroupSize> s_str_to_group_size_map = {
        {"MONGROUPSIZE_ALONE", actor::MonGroupSize::alone},
        {"MONGROUPSIZE_FEW", actor::MonGroupSize::few},
        {"MONGROUPSIZE_PACK", actor::MonGroupSize::pack},
        {"MONGROUPSIZE_SWARM", actor::MonGroupSize::swarm}};

static const std::unordered_map<std::string, actor::Size> s_str_to_actor_size_map = {
        {"MONSIZE_FLOOR", actor::Size::floor},
        {"MONSIZE_HUMANOID", actor::Size::humanoid},
        {"MONSIZE_GIANT", actor::Size::giant}};

static const std::unordered_map<actor::AiId, std::string> s_ai_id_to_tag_name_map = {
        {actor::AiId::looks, "looks"},
        {actor::AiId::avoids_blocking_friend, "avoids_blocking_friend"},
        {actor::AiId::attacks, "attacks"},
        {actor::AiId::paths_to_target_when_aware, "paths_to_target_when_aware"},
        {actor::AiId::moves_to_target_when_los, "moves_to_target_when_los"},
        {actor::AiId::moves_to_lair, "moves_to_lair"},
        {actor::AiId::moves_to_leader, "moves_to_leader"},
        {actor::AiId::moves_randomly_when_unaware, "moves_randomly_when_unaware"}};

static void dump_text(xml::Element* text_e, actor::ActorData& data)
{
        data.name_a =
                xml::get_text_str(
                        xml::first_child(
                                text_e,
                                "name_a"));

        data.name_the =
                xml::get_text_str(
                        xml::first_child(
                                text_e,
                                "name_the"));

        data.corpse_name_a =
                xml::get_text_str(
                        xml::first_child(
                                text_e,
                                "corpse_name_a"));

        data.corpse_name_the =
                xml::get_text_str(
                        xml::first_child(
                                text_e,
                                "corpse_name_the"));

        data.descr =
                xml::get_text_str(
                        xml::first_child(
                                text_e,
                                "description"));

        data.allow_wielded_wpn_descr =
                xml::get_text_bool(
                        xml::first_child(
                                text_e,
                                "allow_wielded_weapon_description"));

        data.allow_speed_descr =
                xml::get_text_bool(
                        xml::first_child(
                                text_e,
                                "allow_speed_description"));

        data.smell_msg =
                xml::get_text_str(
                        xml::first_child(
                                text_e,
                                "smell_message"));

        data.wary_msg =
                xml::get_text_str(
                        xml::first_child(
                                text_e,
                                "wary_message"));

        auto* aware_msg_seen_e =
                xml::first_child(
                        text_e,
                        "aware_message_seen");

        data.aware_msg_mon_seen =
                xml::get_text_str(aware_msg_seen_e);

        xml::try_get_attribute_bool(
                aware_msg_seen_e,
                "use_cultist_messages",
                data.use_cultist_aware_msg_mon_seen);

        auto* aware_msg_hidden_e =
                xml::first_child(
                        text_e,
                        "aware_message_hidden");

        data.aware_msg_mon_hidden = xml::get_text_str(aware_msg_hidden_e);

        xml::try_get_attribute_bool(
                aware_msg_hidden_e,
                "use_cultist_messages",
                data.use_cultist_aware_msg_mon_hidden);

        data.spell_msg =
                xml::get_text_str(
                        xml::first_child(
                                text_e,
                                "spell_message"));

        auto* death_msg_e = xml::first_child(text_e, "death_message");

        if (death_msg_e) {
                data.death_msg_override = xml::get_text_str(death_msg_e);
        }
}

static void dump_gfx(xml::Element* gfx_e, actor::ActorData& data)
{
        data.tile =
                gfx::str_to_tile_id(
                        xml::get_text_str(
                                xml::first_child(gfx_e, "tile")));

        const std::string char_str =
                xml::get_text_str(
                        xml::first_child(
                                gfx_e, "character"));

        if (char_str.empty()) {
                data.character = 0;
        }
        else {
                ASSERT(char_str.length() == 1);

                data.character = char_str[0];
        }

        data.color =
                colors::name_to_color(
                        xml::get_text_str(
                                xml::first_child(gfx_e, "color")));
}

static void dump_audio(xml::Element* audio_e, actor::ActorData& data)
{
        data.aware_sfx_mon_seen =
                audio::str_to_sfx_id(
                        xml::get_text_str(
                                xml::first_child(
                                        audio_e, "aware_sfx_seen")));

        data.aware_sfx_mon_hidden =
                audio::str_to_sfx_id(
                        xml::get_text_str(
                                xml::first_child(
                                        audio_e, "aware_sfx_hidden")));
}

static void dump_attributes(xml::Element* attrib_e, actor::ActorData& data)
{
        data.hp =
                xml::get_text_int(
                        xml::first_child(
                                attrib_e, "hit_points"));

        data.spi =
                xml::get_text_int(
                        xml::first_child(
                                attrib_e, "spirit"));

        data.speed =
                str_to_speed_map.at(
                        xml::get_text_str(
                                xml::first_child(
                                        attrib_e, "speed")));

        data.mon_shock_lvl =
                str_to_shock_lvl_map.at(
                        xml::get_text_str(
                                xml::first_child(
                                        attrib_e, "shock_level")));

        data.ability_values.set_val(
                AbilityId::melee,
                xml::get_text_int(
                        xml::first_child(
                                attrib_e, "melee")));

        data.ability_values.set_val(
                AbilityId::ranged,
                xml::get_text_int(
                        xml::first_child(
                                attrib_e, "ranged")));

        data.ability_values.set_val(
                AbilityId::dodging,
                xml::get_text_int(
                        xml::first_child(
                                attrib_e, "dodging")));

        data.ability_values.set_val(
                AbilityId::stealth,
                xml::get_text_int(
                        xml::first_child(
                                attrib_e, "stealth")));

        data.ability_values.set_val(
                AbilityId::searching,
                xml::get_text_int(
                        xml::first_child(
                                attrib_e, "searching")));

        data.can_open_doors =
                xml::get_text_bool(
                        xml::first_child(
                                attrib_e, "can_open_doors"));

        data.can_bash_doors =
                xml::get_text_bool(
                        xml::first_child(
                                attrib_e, "can_bash_doors"));

        data.actor_size =
                s_str_to_actor_size_map.at(
                        xml::get_text_str(xml::first_child(
                                attrib_e, "size")));

        data.prevent_knockback =
                xml::get_text_bool(
                        xml::first_child(
                                attrib_e, "always_prevent_knockback"));

        data.is_humanoid = xml::has_child(attrib_e, "humanoid");
        data.is_rat = xml::has_child(attrib_e, "rat");
        data.is_canine = xml::has_child(attrib_e, "canine");
        data.is_spider = xml::has_child(attrib_e, "spider");
        data.is_undead = xml::has_child(attrib_e, "undead");
        data.is_ghost = xml::has_child(attrib_e, "ghost");
        data.is_ghoul = xml::has_child(attrib_e, "ghoul");
        data.is_snake = xml::has_child(attrib_e, "snake");
        data.is_reptile = xml::has_child(attrib_e, "reptile");
        data.is_amphibian = xml::has_child(attrib_e, "amphibian");

        data.can_bleed =
                xml::get_text_bool(
                        xml::first_child(
                                attrib_e, "can_bleed"));

        data.can_leave_corpse =
                xml::get_text_bool(
                        xml::first_child(
                                attrib_e, "can_leave_corpse"));

        data.prio_corpse_bash =
                xml::get_text_bool(
                        xml::first_child(
                                attrib_e, "prioritize_destroying_corpse"));
}

static void dump_intr_attack_property(
        xml::Element* property_e,
        actor::IntrAttData& attack_data)
{
        const auto prop_id =
                prop::str_to_prop_id(
                        xml::get_text_str(property_e));

        attack_data.prop_applied.prop.reset(prop::make(prop_id));

        xml::try_get_attribute_int(
                property_e,
                "percent_chance",
                attack_data.prop_applied.pct_chance_to_apply);

        if ((attack_data.prop_applied.pct_chance_to_apply <= 0) ||
            (attack_data.prop_applied.pct_chance_to_apply > 100)) {
                TRACE_ERROR_RELEASE
                        << "Invalid attack property chance: "
                        << attack_data.prop_applied.pct_chance_to_apply
                        << std::endl;

                PANIC;
        }

        int duration = 0;

        if (xml::try_get_attribute_int(property_e, "duration", duration)) {
                attack_data.prop_applied.prop->set_duration(duration);
        }
        else {
                // Duration not specified as integer

                // Check if duration is specified as string ("indefinite")

                std::string duration_str;

                if (xml::try_get_attribute_str(
                            property_e,
                            "duration",
                            duration_str)) {
                        if (duration_str == "indefinite") {
                                attack_data.prop_applied.prop
                                        ->set_indefinite();
                        }
                }
        }
}

static void dump_items(xml::Element* items_e, actor::ActorData& data)
{
        for (auto* item_set_e = xml::first_child(items_e);
             item_set_e;
             item_set_e = xml::next_sibling(item_set_e)) {
                actor::ActorItemSetData item_set;

                const std::string id_str = xml::get_text_str(item_set_e);

                item_set.item_set_id = item::str_to_item_set_id(id_str);

                xml::try_get_attribute_int(
                        item_set_e,
                        "percent_chance",
                        item_set.pct_chance_to_spawn);

                xml::try_get_attribute_int(
                        item_set_e,
                        "min",
                        item_set.nr_spawned_range.min);

                xml::try_get_attribute_int(
                        item_set_e,
                        "max",
                        item_set.nr_spawned_range.max);

                data.item_sets.push_back(item_set);
        }
}

static void dump_intr_attacks(xml::Element* attacks_e, actor::ActorData& data)
{
        for (auto* attack_e = xml::first_child(attacks_e);
             attack_e;
             attack_e = xml::next_sibling(attack_e)) {
                auto attack_data = std::make_unique<actor::IntrAttData>();

                const std::string id_str = xml::get_attribute_str(attack_e, "id");

                attack_data->item_id = item::str_to_intr_item_id(id_str);

                auto* e = xml::first_child(attack_e);

                attack_data->dmg = xml::get_text_int(e);

                // Properties applied
                for (e = xml::next_sibling(e);
                     e;
                     e = xml::next_sibling(e)) {
                        dump_intr_attack_property(e, *attack_data);
                }

                data.intr_attacks.push_back(std::move(attack_data));
        }
}

static void dump_spells(xml::Element* spells_e, actor::ActorData& data)
{
        for (auto* spell_e = xml::first_child(spells_e);
             spell_e;
             spell_e = xml::next_sibling(spell_e)) {
                actor::ActorSpellData spell_data;

                const std::string id_str = xml::get_text_str(spell_e);

                spell_data.spell_id = spells::str_to_spell_id(id_str);

                const std::string skill_str =
                        xml::get_attribute_str(spell_e, "skill");

                spell_data.spell_skill =
                        spells::str_to_spell_skill_id(
                                skill_str);

                xml::try_get_attribute_int(
                        spell_e,
                        "percent_chance",
                        spell_data.pct_chance_to_know);

                data.spells.push_back(spell_data);
        }
}

static void dump_properties(xml::Element* properties_e, actor::ActorData& data)
{
        for (auto* e = xml::first_child(properties_e);
             e;
             e = xml::next_sibling(e)) {
                const auto prop_id =
                        prop::str_to_prop_id(
                                xml::get_text_str(e));

                data.natural_props[(size_t)prop_id] = true;
        }
}

static void dump_ai(xml::Element* ai_e, actor::ActorData& data)
{
        data.erratic_move_pct =
                xml::get_text_int(
                        xml::first_child(
                                ai_e, "erratic_move_percent"));

        data.nr_turns_aware =
                xml::get_text_int(
                        xml::first_child(
                                ai_e, "turns_aware"));

        data.ranged_cooldown_turns =
                xml::get_text_int(
                        xml::first_child(
                                ai_e, "ranged_cooldown_turns"));

        data.is_pausing_on_player_seen =
                xml::get_text_bool(
                        xml::first_child(
                                ai_e, "is_pausing_on_player_seen"));

        for (size_t i = 0; i < (size_t)actor::AiId::END; ++i) {
                const std::string ai_id_str =
                        s_ai_id_to_tag_name_map.at((actor::AiId)i);

                data.ai[i] =
                        xml::get_text_bool(
                                xml::first_child(
                                        ai_e, ai_id_str));
        }
}

static void dump_group_size(xml::Element* group_e, actor::ActorData& data)
{
        actor::MonGroupSpawnRule rule;

        rule.group_size =
                s_str_to_group_size_map.at(
                        xml::get_text_str(group_e));

        xml::try_get_attribute_int(
                group_e,
                "weight",
                rule.weight);

        xml::try_get_attribute_int(
                group_e,
                "required_dungeon_level",
                rule.required_dlvl);

        data.group_sizes.push_back(rule);
}

static void dump_native_room(
        xml::Element* native_room_e,
        actor::ActorData& data)
{
        const auto room_type =
                room::str_to_room_type(
                        xml::get_text_str(native_room_e));

        data.native_rooms.push_back(room_type);
}

static void dump_spawning(xml::Element* spawn_e, actor::ActorData& data)
{
        data.spawn_min_dlvl =
                xml::get_text_int(
                        xml::first_child(
                                spawn_e, "min_dungeon_level"));

        data.spawn_max_dlvl =
                xml::get_text_int(
                        xml::first_child(
                                spawn_e, "max_dungeon_level"));

        data.spawn_weight =
                xml::get_text_int(
                        xml::first_child(
                                spawn_e, "spawn_weight"));

        data.is_auto_spawn_allowed =
                xml::get_text_bool(
                        xml::first_child(
                                spawn_e, "auto_spawn"));

        data.can_be_summoned_by_mon =
                xml::get_text_bool(
                        xml::first_child(
                                spawn_e, "can_be_summoned_by_monster"));

        data.can_spawn_from_tomb =
                xml::get_text_bool(
                        xml::first_child(
                                spawn_e, "can_spawn_from_tomb"));

        data.can_be_shapeshifted_into =
                xml::get_text_bool(
                        xml::first_child(
                                spawn_e, "can_be_shapeshifted_into"));

        data.is_unique = xml::has_child(spawn_e, "unique");

        data.nr_left_allowed_to_spawn =
                xml::get_text_int(
                        xml::first_child(
                                spawn_e, "nr_allowed_to_spawn"));

        const std::string group_size_element_str = "group_size";

        for (auto* e = xml::first_child(spawn_e, group_size_element_str);
             e;
             e = xml::next_sibling(e, group_size_element_str)) {
                dump_group_size(e, data);
        }

        const std::string native_room_element_str = "native_room";

        for (auto* e = xml::first_child(spawn_e, native_room_element_str);
             e;
             e = xml::next_sibling(e, native_room_element_str)) {
                dump_native_room(e, data);
        }
}

static void dump_starting_allies(xml::Element* allies_e, actor::ActorData& data)
{
        for (auto* e = xml::first_child(allies_e);
             e;
             e = xml::next_sibling(e)) {
                const std::string id = xml::get_attribute_str(e, "id");

                actor::StartingAllyEntry starting_ally;

                starting_ally.id = id;

                xml::try_get_attribute_int(e, "min", starting_ally.nr.min);
                xml::try_get_attribute_int(e, "max", starting_ally.nr.max);

                data.starting_allies.push_back(starting_ally);
        }
}

static void read_actor_definitions_xml()
{
        TRACE_FUNC_BEGIN;

        xml::Doc doc;

        const std::string file_path = paths::data_dir() + "monsters.xml";

        TRACE << "Loading " << file_path << std::endl;

        xml::load_file(file_path, doc);

        auto* top_e = xml::first_child(doc);

        auto* mon_e = xml::first_child(top_e);

        TRACE << "Reading monster data" << std::endl;

        for (; mon_e; mon_e = xml::next_sibling(mon_e, "monster")) {
                const std::string id = xml::get_attribute_str(mon_e, "id");

                TRACE << "Reading monster data for ID: " << id << std::endl;

                actor::ActorData& data = actor::g_data[id];

                data.reset();

                data.id = id;

                dump_text(xml::first_child(mon_e, "text"), data);
                dump_gfx(xml::first_child(mon_e, "graphics"), data);
                dump_audio(xml::first_child(mon_e, "audio"), data);
                dump_attributes(xml::first_child(mon_e, "attributes"), data);

                auto* items_e = xml::first_child(mon_e, "items");

                if (items_e) {
                        dump_items(items_e, data);
                }

                auto* attacks_e = xml::first_child(mon_e, "attacks");

                if (attacks_e) {
                        dump_intr_attacks(attacks_e, data);
                }

                auto* spells_e = xml::first_child(mon_e, "spells");

                if (spells_e) {
                        dump_spells(spells_e, data);
                }

                auto* props_e = xml::first_child(mon_e, "properties");

                if (props_e) {
                        dump_properties(props_e, data);
                }

                auto* ai_e = xml::first_child(mon_e, "ai");

                if (ai_e) {
                        dump_ai(ai_e, data);
                }

                dump_spawning(xml::first_child(mon_e, "spawning"), data);

                auto* allies_e = xml::first_child(mon_e, "starting_allies");

                if (allies_e) {
                        dump_starting_allies(allies_e, data);
                }
        }

        TRACE_FUNC_END;

}  // read_actor_definitions_xml

// -----------------------------------------------------------------------------
// actor
// -----------------------------------------------------------------------------
namespace actor
{
// -----------------------------------------------------------------------------
// ActorData
// -----------------------------------------------------------------------------
void ActorData::reset()
{
        id = "";
        name_a = "";
        name_the = "";
        corpse_name_a = "";
        corpse_name_the = "";
        tile = gfx::TileId::END;
        character = 'X';
        color = colors::yellow();

        group_sizes.assign({});

        hp = 0;
        item_sets.clear();
        intr_attacks.clear();
        spells.clear();
        spi = 0;
        speed = Speed::normal;

        for (size_t i = 0; i < (size_t)prop::Id::END; ++i) {
                natural_props[i] = false;
        }

        ability_values.reset();

        for (size_t i = 0; i < (size_t)AiId::END; ++i) {
                ai[i] = false;
        }

        ai[(size_t)AiId::moves_randomly_when_unaware] = true;

        nr_turns_aware = 0;
        ranged_cooldown_turns = 0;
        spawn_min_dlvl = -1;
        spawn_max_dlvl = -1;
        spawn_weight = 100;
        actor_size = Size::humanoid;
        nr_kills = 0;
        has_player_seen = false;
        can_open_doors = can_bash_doors = false;
        prevent_knockback = false;
        nr_left_allowed_to_spawn = -1;
        is_unique = false;
        is_auto_spawn_allowed = true;
        wary_msg = "";
        aware_msg_mon_seen = "";
        aware_msg_mon_hidden = "";
        use_cultist_aware_msg_mon_seen = false;
        use_cultist_aware_msg_mon_hidden = false;
        smell_msg = "";
        aware_sfx_mon_seen = audio::SfxId::END;
        aware_sfx_mon_hidden = audio::SfxId::END;
        spell_msg = "";
        erratic_move_pct = 0;
        mon_shock_lvl = MonShockLvl::none;
        is_humanoid = false;
        is_rat = false;
        is_canine = false;
        is_spider = false;
        is_undead = false;
        is_ghost = false;
        is_ghoul = false;
        is_snake = false;
        is_reptile = false;
        is_amphibian = false;
        can_be_summoned_by_mon = false;
        can_spawn_from_tomb = false;
        can_be_shapeshifted_into = false;
        can_bleed = true;
        can_leave_corpse = true;
        prio_corpse_bash = false;
        native_rooms.clear();
        starting_allies.clear();
        descr = "";
}

std::unordered_map<std::string, ActorData> g_data;

void init()
{
        TRACE_FUNC_BEGIN;

        read_actor_definitions_xml();

        TRACE_FUNC_END;
}

void save()
{
        const std::vector<std::string> keys = get_keys_sorted(g_data);

        for (const std::string& key : keys) {
                ActorData& d = g_data[key];

                saving::put_int(d.nr_left_allowed_to_spawn);
                saving::put_int(d.nr_kills);
                saving::put_bool(d.has_player_seen);
        }
}

void load()
{
        const std::vector<std::string> keys = get_keys_sorted(g_data);

        for (const std::string& key : keys) {
                ActorData& d = g_data[key];

                d.nr_left_allowed_to_spawn = saving::get_int();
                d.nr_kills = saving::get_int();
                d.has_player_seen = saving::get_bool();
        }
}

}  // namespace actor
