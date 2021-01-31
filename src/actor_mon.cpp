// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "actor_mon.hpp"

#include <algorithm>
#include <cstring>
#include <vector>

#include "actor_factory.hpp"
#include "actor_player.hpp"
#include "actor_see.hpp"
#include "ai.hpp"
#include "attack.hpp"
#include "drop.hpp"
#include "flood.hpp"
#include "fov.hpp"
#include "game_time.hpp"
#include "gods.hpp"
#include "init.hpp"
#include "inventory.hpp"
#include "io.hpp"
#include "item.hpp"
#include "item_factory.hpp"
#include "knockback.hpp"
#include "line_calc.hpp"
#include "map.hpp"
#include "map_parsing.hpp"
#include "misc.hpp"
#include "msg_log.hpp"
#include "popup.hpp"
#include "property.hpp"
#include "property_data.hpp"
#include "property_factory.hpp"
#include "property_handler.hpp"
#include "reload.hpp"
#include "sound.hpp"
#include "terrain_door.hpp"
#include "terrain_mob.hpp"
#include "terrain_trap.hpp"
#include "text_format.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static void unblock_passable_doors(
        const actor::ActorData& actor_data,
        Array2<bool>& blocked)
{
        for (size_t i = 0; i < blocked.length(); ++i)
        {
                const auto* const t = map::g_terrain.at(i);

                if (t->id() != terrain::Id::door)
                {
                        continue;
                }

                const auto* const door = static_cast<const terrain::Door*>(t);

                if (door->type() == terrain::DoorType::metal)
                {
                        continue;
                }

                if (actor_data.can_open_doors ||
                    actor_data.can_bash_doors)
                {
                        blocked.at(i) = false;
                }
        }
}

// -----------------------------------------------------------------------------
// actor
// -----------------------------------------------------------------------------
namespace actor
{
std::string get_cultist_phrase()
{
        std::vector<std::string> phrase_bucket = {
                "Apigami!",
                "Bhuudesco invisuu!",
                "Bhuuesco marana!",
                "Crudux cruo!",
                "Cruento paashaeximus!",
                "Cruento pestis shatruex!",
                "Cruo crunatus durbe!",
                "Cruo lokemundux!",
                "Cruo stragara-na!",
                "Gero shay cruo!",
                "In marana domus-bhaava crunatus!",
                "Caecux infirmux!",
                "Malax sayti!",
                "Marana pallex!",
                "Marana malax!",
                "Pallex ti!",
                "Peroshay bibox malax!",
                "Pestis Cruento!",
                "Pestis cruento vilomaxus pretiacruento!",
                "Pretaanluxis cruonit!",
                "Pretiacruento!",
                "Stragar-Naya!",
                "Vorox esco marana!",
                "Vilomaxus!",
                "Prostragaranar malachtose!",
                "Apigami!"};

        if (rnd::one_in(4))
        {
                const God& god = gods::current_god();

                const std::vector<std::string> god_phrases = {
                        god.name + " save us!",
                        god.descr + " will save us!",
                        god.name + " watches over us!",
                        god.descr + " watches over us!",
                        god.name + ", guide us!",
                        god.descr + " guides us!",
                        "For " + god.name + "!",
                        "For " + god.descr + "!",
                        "Blood for " + god.name + "!",
                        "Blood for " + god.descr + "!",
                        "Perish for " + god.name + "!",
                        "Perish for " + god.descr + "!",
                        "In the name of " + god.name + "!",
                };

                phrase_bucket.insert(
                        std::end(phrase_bucket),
                        std::begin(god_phrases),
                        std::end(god_phrases));
        }

        return rnd::element(phrase_bucket);
}

std::string get_cultist_aware_msg_seen(const Actor& actor)
{
        const std::string name_the =
                text_format::first_to_upper(
                        actor.name_the());

        return name_the + ": " + get_cultist_phrase();
}

std::string get_cultist_aware_msg_hidden()
{
        return "Voice: " + get_cultist_phrase();
}

// -----------------------------------------------------------------------------
// Monster
// -----------------------------------------------------------------------------
Mon::~Mon()
{
        for (auto& spell : m_mon_spells)
        {
                delete spell.spell;
        }
}

std::vector<Actor*> Mon::foes_aware_of() const
{
        std::vector<Actor*> result;

        if (is_actor_my_leader(map::g_player))
        {
                // TODO: This prevents player-allied monsters from casting
                // spells on unreachable hostile monsters - but it probably
                // doesn't matter for now

                Array2<bool> blocked(map::dims());

                map_parsers::BlocksActor(*this, ParseActors::no)
                        .run(blocked, blocked.rect());

                unblock_passable_doors(*m_data, blocked);

                const auto flood = floodfill(m_pos, blocked);

                // Add all player-hostile monsters which the player is aware of
                for (Actor* const actor : game_time::g_actors)
                {
                        if (!actor->is_player() &&
                            !actor->is_actor_my_leader(map::g_player) &&
                            (flood.at(actor->m_pos) > 0) &&
                            actor->is_aware_of_player())
                        {
                                result.push_back(actor);
                        }
                }
        }
        // Player is not my leader
        else if (is_aware_of_player())
        {
                result.push_back(map::g_player);

                for (Actor* const actor : game_time::g_actors)
                {
                        if (!actor->is_player() &&
                            actor->is_actor_my_leader(map::g_player))
                        {
                                result.push_back(actor);
                        }
                }
        }

        return result;
}

bool Mon::is_sneaking() const
{
        // NOTE: We require a stealth ability greater than zero, both for the
        // basic skill value, AND when including properties properties -
        // This prevents monsters who should never be able to sneak from
        // suddenly gaining this ability due to some bonus

        const int stealth_base = ability(AbilityId::stealth, false);

        const int stealth_current = ability(AbilityId::stealth, true);

        return (
                (!is_player_aware_of_me()) &&
                (stealth_base > 0) &&
                (stealth_current > 0) &&
                !is_actor_my_leader(map::g_player));
}

Color Mon::color() const
{
        if (!is_alive())
        {
                return m_data->color;
        }

        Color tmp_color;

        if (m_properties.affect_actor_color(tmp_color))
        {
                return tmp_color;
        }

        auto* const data =
                m_mimic_data
                ? m_mimic_data
                : m_data;

        return data->color;
}

SpellSkill Mon::spell_skill(const SpellId id) const
{
        (void)id;

        for (const auto& spell : m_mon_spells)
        {
                if (spell.spell->id() == id)
                {
                        return spell.skill;
                }
        }

        ASSERT(false);

        return SpellSkill::basic;
}

void Mon::hear_sound(const Snd& snd)
{
        if (m_properties.has(PropId::deaf))
        {
                return;
        }

        snd.on_heard(*this);

        // NOTE: The monster may have become deaf through the sound callback
        // above (e.g.  from the Horn of Deafening artifact)
        if (m_properties.has(PropId::deaf))
        {
                return;
        }

        if (is_alive() && snd.is_alerting_mon())
        {
                become_aware_player(AwareSource::other);
        }
}

void Mon::speak_phrase(const AlertsMon alerts_others)
{
        const bool is_seen_by_player = can_player_see_actor(*this);

        std::string msg =
                is_seen_by_player
                ? aware_msg_mon_seen()
                : aware_msg_mon_hidden();

        msg = text_format::first_to_upper(msg);

        const audio::SfxId sfx =
                is_seen_by_player
                ? aware_sfx_mon_seen()
                : aware_sfx_mon_hidden();

        Snd snd(
                msg,
                sfx,
                IgnoreMsgIfOriginSeen::no,
                m_pos,
                this,
                SndVol::low,
                alerts_others);

        snd_emit::run(snd);
}

std::string Mon::aware_msg_mon_seen() const
{
        if (m_data->use_cultist_aware_msg_mon_seen)
        {
                return get_cultist_aware_msg_seen(*this);
        }

        std::string msg_end = m_data->aware_msg_mon_seen;

        if (msg_end.empty())
        {
                return "";
        }

        const std::string name = text_format::first_to_upper(name_the());

        return name + " " + msg_end;
}

std::string Mon::aware_msg_mon_hidden() const
{
        if (m_data->use_cultist_aware_msg_mon_hidden)
        {
                return get_cultist_aware_msg_hidden();
        }

        return m_data->aware_msg_mon_hidden;
}

void Mon::become_aware_player(const AwareSource source, const int factor)
{
        if (!is_alive() || is_actor_my_leader(map::g_player))
        {
                TRACE_FUNC_END;
                return;
        }

        const int nr_turns = m_data->nr_turns_aware * factor;

        const int aware_counter_before = m_mon_aware_state.aware_counter;

        const bool was_aware_before = is_aware_of_player();

        m_mon_aware_state.aware_counter =
                std::max(nr_turns, aware_counter_before);

        m_mon_aware_state.wary_counter = m_mon_aware_state.aware_counter;

        bool do_reaction_time = false;

        switch (source)
        {
        case AwareSource::attacked:
                do_reaction_time = false;
                break;

        case AwareSource::spell_victim:
                do_reaction_time = false;
                break;

        case AwareSource::seeing:
                do_reaction_time = true;
                break;

        case AwareSource::other:
                do_reaction_time = true;
                break;
        }

        if (!do_reaction_time)
        {
                // Becoming aware without reaction time also ends waiting
                // (prevents leaving the monster in a waiting state if for
                // example an attack made a sound which alerted the monster
                // before the attack itself alerted it).
                m_properties.end_prop(PropId::waiting);
        }

        if (!was_aware_before)
        {
                // Monster became aware now

                if ((source == AwareSource::seeing) &&
                    can_player_see_actor(*this))
                {
                        print_player_see_mon_become_aware_msg();
                }

                if (rnd::coin_toss())
                {
                        speak_phrase(AlertsMon::yes);
                }

                if (do_reaction_time)
                {
                        // Give the monster some reaction time
                        if (!is_actor_my_leader(map::g_player))
                        {
                                auto* const prop =
                                        property_factory::make(PropId::waiting);

                                prop->set_duration(1);

                                m_properties.apply(prop);
                        }
                }
        }
}

void Mon::become_wary_player()
{
        if (!is_alive() || is_actor_my_leader(map::g_player))
        {
                return;
        }

        // NOTE: Reusing aware duration to determine number of wary turns
        const int nr_turns = m_data->nr_turns_aware;

        const int wary_counter_before = m_mon_aware_state.wary_counter;

        m_mon_aware_state.wary_counter =
                std::max(nr_turns, wary_counter_before);

        if (wary_counter_before <= 0)
        {
                if (can_player_see_actor(*this))
                {
                        print_player_see_mon_become_wary_msg();
                }

                if (rnd::one_in(4))
                {
                        speak_phrase(AlertsMon::no);
                }
        }
}

void Mon::print_player_see_mon_become_aware_msg() const
{
        std::string msg = text_format::first_to_upper(name_the()) + " sees me!";

        const std::string dir_str =
                dir_utils::compass_dir_name(map::g_player->m_pos, m_pos);

        msg += "(" + dir_str + ")";

        msg_log::add(msg);
}

void Mon::print_player_see_mon_become_wary_msg() const
{
        if (m_data->wary_msg.empty())
        {
                return;
        }

        std::string msg = text_format::first_to_upper(name_the());

        msg += " " + m_data->wary_msg;

        msg += "(";
        msg += dir_utils::compass_dir_name(map::g_player->m_pos, m_pos);
        msg += ")";

        msg_log::add(msg);
}

void Mon::set_player_aware_of_me(int duration_factor)
{
        int nr_turns = 2 * duration_factor;

        if (player_bon::bg() == Bg::rogue)
        {
                nr_turns *= 8;
        }

        m_mon_aware_state.player_aware_of_me_counter =
                std::max(
                        nr_turns,
                        m_mon_aware_state.player_aware_of_me_counter);
}

DidAction Mon::try_attack(Actor& defender)
{
        if (!is_alive())
        {
                return DidAction::no;
        }

        if (!is_aware_of_player() && (m_leader != map::g_player))
        {
                return DidAction::no;
        }

        map::update_vision();

        const auto my_avail_attacks = avail_attacks(defender);

        const auto att = choose_attack(my_avail_attacks);

        if (!att.wpn)
        {
                return DidAction::no;
        }

        if (att.is_melee)
        {
                if (att.wpn->data().melee.is_melee_wpn)
                {
                        attack::melee(this, m_pos, defender, *att.wpn);

                        return DidAction::yes;
                }

                return DidAction::no;
        }

        if (att.wpn->data().ranged.is_ranged_wpn)
        {
                if (my_avail_attacks.should_reload)
                {
                        reload::try_reload(*this, att.wpn);

                        return DidAction::yes;
                }

                const bool ignore_blocking_friend = rnd::one_in(20);

                if (!ignore_blocking_friend &&
                    is_friend_blocking_ranged_attack(defender.m_pos))
                {
                        return DidAction::no;
                }

                if (m_data->ranged_cooldown_turns > 0)
                {
                        auto* prop = new PropDisabledRanged();

                        prop->set_duration(m_data->ranged_cooldown_turns);

                        m_properties.apply(prop);
                }

                const auto did_attack =
                        attack::ranged(
                                this,
                                m_pos,
                                defender.m_pos,
                                *att.wpn);

                return did_attack;
        }

        return DidAction::no;
}

bool Mon::is_friend_blocking_ranged_attack(const P& target_pos) const
{
        const auto line =
                line_calc::calc_new_line(
                        m_pos,
                        target_pos,
                        true,
                        9999,
                        false);

        for (const P& line_pos : line)
        {
                if ((line_pos != m_pos) && (line_pos != target_pos))
                {
                        auto* const actor_here =
                                map::first_actor_at_pos(line_pos);

                        // TODO: This does not consider who is allied/hostile!
                        if (actor_here)
                        {
                                return true;
                        }
                }
        }

        return false;
}

AiAvailAttacksData Mon::avail_attacks(Actor& defender) const
{
        AiAvailAttacksData result;

        if (!m_properties.allow_attack(Verbose::no))
        {
                return result;
        }

        result.is_melee = is_pos_adj(m_pos, defender.m_pos, false);

        if (result.is_melee)
        {
                if (!m_properties.allow_attack_melee(Verbose::no))
                {
                        return result;
                }

                result.weapons = avail_intr_melee();

                auto* wielded_wpn = avail_wielded_melee();

                if (wielded_wpn)
                {
                        result.weapons.push_back(wielded_wpn);
                }
        }
        else
        {
                // Ranged attack
                if (!m_properties.allow_attack_ranged(Verbose::no))
                {
                        return result;
                }

                result.weapons = avail_intr_ranged();

                auto* const wielded_wpn = avail_wielded_ranged();

                if (wielded_wpn)
                {
                        result.weapons.push_back(wielded_wpn);

                        result.should_reload = should_reload(*wielded_wpn);
                }
        }

        return result;
}

item::Wpn* Mon::avail_wielded_melee() const
{
        auto* const item = m_inv.item_in_slot(SlotId::wpn);

        if (item)
        {
                auto* const wpn = static_cast<item::Wpn*>(item);

                if (wpn->data().melee.is_melee_wpn)
                {
                        return wpn;
                }
        }

        return nullptr;
}

item::Wpn* Mon::avail_wielded_ranged() const
{
        auto* const item = m_inv.item_in_slot(SlotId::wpn);

        if (item)
        {
                auto* const wpn = static_cast<item::Wpn*>(item);

                if (wpn->data().ranged.is_ranged_wpn)
                {
                        return wpn;
                }
        }

        return nullptr;
}

std::vector<item::Wpn*> Mon::avail_intr_melee() const
{
        std::vector<item::Wpn*> result;

        for (auto* const item : m_inv.m_intrinsics)
        {
                auto* wpn = static_cast<item::Wpn*>(item);

                if (wpn->data().melee.is_melee_wpn)
                {
                        result.push_back(wpn);
                }
        }

        return result;
}

std::vector<item::Wpn*> Mon::avail_intr_ranged() const
{
        std::vector<item::Wpn*> result;

        for (auto* const item : m_inv.m_intrinsics)
        {
                auto* const wpn = static_cast<item::Wpn*>(item);

                if (wpn->data().ranged.is_ranged_wpn)
                {
                        result.push_back(wpn);
                }
        }

        return result;
}

bool Mon::should_reload(const item::Wpn& wpn) const
{
        // TODO: If the monster does not see any enemies it should reload even
        // if the weapon is not completely empty
        // TODO: Reloading should not be handled here, it should be done as a
        // separate action
        return (
                (wpn.m_ammo_loaded == 0) &&
                !wpn.data().ranged.has_infinite_ammo &&
                m_inv.has_ammo_for_firearm_in_inventory());
}

AiAttData Mon::choose_attack(const AiAvailAttacksData& avail_attacks) const
{
        AiAttData result;

        result.is_melee = avail_attacks.is_melee;

        if (avail_attacks.weapons.empty())
        {
                return result;
        }

        result.wpn = rnd::element(avail_attacks.weapons);

        return result;
}

bool Mon::is_leader_of(const Actor* const actor) const
{
        if (actor && !actor->is_player())
        {
                const auto* const mon = static_cast<const Mon*>(actor);

                return (mon->m_leader == this);
        }

        return false;
}

bool Mon::is_actor_my_leader(const Actor* const actor) const
{
        return m_leader == actor;
}

int Mon::nr_mon_in_group() const
{
        const Actor* const group_leader = m_leader ? m_leader : this;

        int ret = 1;  // Starting at one to include leader

        for (const Actor* const actor : game_time::g_actors)
        {
                if (actor->is_actor_my_leader(group_leader))
                {
                        ++ret;
                }
        }

        return ret;
}

void Mon::add_spell(SpellSkill skill, Spell* const spell)
{
        const auto search = std::find_if(
                std::begin(m_mon_spells),
                std::end(m_mon_spells),
                [spell](MonSpell& spell_entry) {
                        return spell_entry.spell->id() == spell->id();
                });

        if (search != std::end(m_mon_spells))
        {
                delete spell;

                return;
        }

        MonSpell spell_entry;

        spell_entry.spell = spell;

        spell_entry.skill = skill;

        m_mon_spells.push_back(spell_entry);
}

// -----------------------------------------------------------------------------
// Specific monsters
// -----------------------------------------------------------------------------
// TODO: This should either be a property or be controlled by the map
DidAction Khephren::on_act()
{
        // Try summoning locusts

        if (!is_alive() || !is_aware_of_player() || m_has_summoned_locusts)
        {
                return DidAction::no;
        }

        Array2<bool> blocked(map::dims());

        const R fov_rect = fov::fov_rect(m_pos, blocked.dims());

        map_parsers::BlocksLos()
                .run(blocked,
                     fov_rect,
                     MapParseMode::overwrite);

        if (!can_mon_see_actor(*this, *map::g_player, blocked))
        {
                return DidAction::no;
        }

        msg_log::add("Khephren calls a plague of Locusts!");

        map::g_player->incr_shock(ShockLvl::terrifying, ShockSrc::misc);

        Actor* const leader_of_spawned_mon = m_leader ? m_leader : this;

        const size_t nr_of_spawns = 15;

        auto summoned =
                actor::spawn(
                        m_pos,
                        {nr_of_spawns, actor::Id::locust},
                        map::rect());

        summoned.set_leader(leader_of_spawned_mon);
        summoned.make_aware_of_player();

        std::for_each(
                std::begin(summoned.monsters),
                std::end(summoned.monsters),
                [](Mon* const mon) {
                        auto* prop = new PropSummoned();

                        prop->set_indefinite();

                        mon->m_properties.apply(prop);
                });

        m_has_summoned_locusts = true;

        game_time::tick();

        return DidAction::yes;
}

DidAction Ape::on_act()
{
        if (m_frenzy_cooldown > 0)
        {
                --m_frenzy_cooldown;
        }

        if ((m_frenzy_cooldown <= 0) &&
            m_ai_state.target &&
            (m_hp <= (actor::max_hp(*this) / 2)))
        {
                m_frenzy_cooldown = 30;

                auto* prop = new PropFrenzied();

                prop->set_duration(rnd::range(4, 6));

                m_properties.apply(prop);
        }

        return DidAction::no;
}

// TODO: This should be a property (should probably be merged with
// 'corrupts_env_color')
Color StrangeColor::color() const
{
        Color color = colors::light_magenta();

        const Range range(40, 255);

        color.set_rgb(
                range.roll(),
                range.roll(),
                range.roll());

        return color;
}

void SpectralWpn::on_death()
{
        // Remove the item from the inventory to avoid dropping it on the floor
        // (but do not yet delete the item, in case it's still being used in the
        // the call stack)
        auto* const item =
                m_inv.remove_item_in_slot(
                        SlotId::wpn,
                        false);  // Do not delete the item

        m_discarded_item.reset(item);
}

std::string SpectralWpn::name_the() const
{
        auto* item = m_inv.item_in_slot(SlotId::wpn);

        ASSERT(item);

        const std::string name =
                item->name(
                        ItemRefType::plain,
                        ItemRefInf::yes,
                        ItemRefAttInf::none);

        return "The Spectral " + name;
}

std::string SpectralWpn::name_a() const
{
        auto* item = m_inv.item_in_slot(SlotId::wpn);

        ASSERT(item);

        const std::string name =
                item->name(
                        ItemRefType::plain,
                        ItemRefInf::yes,
                        ItemRefAttInf::none);

        return "A Spectral " + name;
}

char SpectralWpn::character() const
{
        auto* item = m_inv.item_in_slot(SlotId::wpn);

        ASSERT(item);

        return item->character();
}

gfx::TileId SpectralWpn::tile() const
{
        auto* item = m_inv.item_in_slot(SlotId::wpn);

        ASSERT(item);

        return item->tile();
}

std::string SpectralWpn::descr() const
{
        auto* item = m_inv.item_in_slot(SlotId::wpn);

        ASSERT(item);

        std::string str =
                item->name(
                        ItemRefType::a,
                        ItemRefInf::yes,
                        ItemRefAttInf::none);

        str = text_format::first_to_upper(str);

        str += ", floating through the air as if wielded by an invisible hand.";

        return str;
}

}  // namespace actor
