// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "player_bon.hpp"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <iterator>
#include <memory>

#include "actor.hpp"
#include "actor_data.hpp"
#include "colors.hpp"
#include "create_character.hpp"
#include "debug.hpp"
#include "game.hpp"
#include "global.hpp"
#include "item_data.hpp"
#include "map.hpp"
#include "player_spells.hpp"
#include "property.hpp"
#include "property_data.hpp"
#include "property_factory.hpp"
#include "property_handler.hpp"
#include "random.hpp"
#include "saving.hpp"
#include "spells.hpp"
#include "state.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
struct TraitData
{
        Trait id {Trait::END};
        std::string title {};
        std::string descr {};
        std::function<void()> on_picked {};
        std::function<void()> on_removed {};
        std::vector<Trait> trait_prereqs {};
        Bg bg_prereq {Bg::END};
        std::vector<Bg> blocked_for_bgs {};
        std::vector<OccultistDomain> blocked_for_occultist_domains {};
};

static TraitData s_trait_data[(size_t)Trait::END];

// NOTE: This is stored separately from the trait data since we sometimes need
// to update the trait data (e.g. to update trait descriptions containing
// information on the player's current spirit for spell traits). Bundling the
// picked state with the other trait data would be confusing and inconvenient.
static bool s_traits_picked[(size_t)Trait::END];

static std::vector<player_bon::TraitLogEntry> s_trait_log;

static auto s_player_bg = Bg::END;
static auto s_player_occultist_domain = OccultistDomain::END;

static const int s_exorcist_bon_trait_lvl_1 = 2;
static const int s_exorcist_bon_trait_lvl_2 = 4;
static const int s_exorcist_bon_trait_lvl_3 = 6;

static const int s_spell_upgrade_lvl_1 = 4;
static const int s_spell_upgrade_lvl_2 = 8;

static std::string trait_descr_for_spell(
        const SpellId spell_id,
        const SpellSkill skill)
{
        std::unique_ptr<Spell> spell(spells::make(spell_id));

        std::string str =
                "Gain the ability to cast \"" +
                spell->name() +
                "\"";

        if (spell->can_be_improved_with_skill()) {
                str +=
                        " at " +
                        spells::skill_to_str(skill) +
                        " level";
        }

        str += " -";

        const auto descr = spell->descr_specific(skill);

        for (const auto& line : descr) {
                str += " " + line;
        }

        const auto cost_str = spell->cost_range(skill).str();
        const auto sp_str = std::to_string(map::g_player->m_sp);
        const auto max_sp_str = std::to_string(actor::max_sp(*map::g_player));

        str +=
                " This spell costs " +
                cost_str +
                " spirit to cast, you currently have " +
                sp_str +
                "/" +
                max_sp_str +
                " spirit.";

        return str;
}

static TraitData& trait_data(const Trait id)
{
        ASSERT(id != Trait::END);

        return s_trait_data[(size_t)id];
}

static void set_trait_data(TraitData& d)
{
        ASSERT(d.id != Trait::END);

        s_trait_data[(size_t)d.id] = d;

        d = {};
}

static void update_trait_data()
{
        for (auto& d : s_trait_data) {
                d = {};
        }

        TraitData d;

        // --- Adept Melee Fighter ---
        d.id = Trait::adept_melee;
        d.title = "Adept Melee Fighter";
        d.descr = "+10% hit chance and +1 damage with melee attacks";
        set_trait_data(d);

        // --- Expert Melee Fighter ---
        d = trait_data(Trait::adept_melee);
        d.id = Trait::expert_melee;
        d.title = "Expert Melee Fighter";
        d.trait_prereqs = {Trait::adept_melee};
        d.blocked_for_bgs = {Bg::exorcist};
        set_trait_data(d);

        // --- Master Melee Fighter ---
        d = trait_data(Trait::adept_melee);
        d.id = Trait::master_melee;
        d.title = "Master Melee Fighter";
        d.trait_prereqs = {Trait::expert_melee};
        d.blocked_for_bgs = {Bg::exorcist, Bg::occultist};
        set_trait_data(d);

        // --- Adept Marksman ---
        d.id = Trait::adept_marksman;
        d.title = "Adept Marksman";
        d.descr = "+10% hit chance with firearms and thrown weapons";
        d.blocked_for_bgs = {Bg::ghoul};
        set_trait_data(d);

        // --- Expert Marksman ---
        d = trait_data(Trait::adept_marksman);
        d.id = Trait::expert_marksman;
        d.title = "Expert Marksman";
        d.trait_prereqs = {Trait::adept_marksman};
        d.blocked_for_bgs = {Bg::ghoul, Bg::exorcist};
        set_trait_data(d);

        // --- Master Marksman ---
        d = trait_data(Trait::adept_marksman);
        d.id = Trait::master_marksman;
        d.title = "Master Marksman";
        d.trait_prereqs = {Trait::expert_marksman};
        d.blocked_for_bgs = {
                Bg::ghoul,
                Bg::exorcist,
                Bg::occultist,
                Bg::flagellant};
        set_trait_data(d);

        // --- Cool-headed ---
        d.id = Trait::cool_headed;
        d.title = "Cool-headed";
        d.descr = "+20% shock resistance";
        set_trait_data(d);

        // --- Courageous ---
        d = trait_data(Trait::cool_headed);
        d.id = Trait::courageous;
        d.title = "Courageous";
        d.trait_prereqs = {Trait::cool_headed};
        set_trait_data(d);

        // --- Dexterous ---
        d.id = Trait::dexterous;
        d.title = "Dexterous";
        d.descr = "+25% chance to evade attacks";
        set_trait_data(d);

        // --- Lithe ---
        d = trait_data(Trait::dexterous);
        d.id = Trait::lithe;
        d.title = "Lithe";
        d.trait_prereqs = {Trait::dexterous};
        set_trait_data(d);

        // --- Crippling Strikes ---
        d.id = Trait::crippling_strikes;
        d.title = "Crippling Strikes";
        d.descr =
                "Your melee attacks have 60% chance to weaken the target "
                "creature for 2-3 turns (reducing their melee damage by half)";
        d.trait_prereqs = {Trait::dexterous, Trait::adept_melee};
        d.bg_prereq = Bg::rogue;
        set_trait_data(d);

        // --- Fearless ---
        d.id = Trait::fearless;
        d.title = "Fearless";
        d.descr = "You cannot become terrified, +10% shock resistance";
        d.on_picked = []() {
                auto* prop = property_factory::make(PropId::r_fear);

                prop->set_indefinite();

                map::g_player->m_properties.apply(
                        prop,
                        PropSrc::intr,
                        true,
                        Verbose::no);
        };
        d.on_removed = []() {
                map::g_player->m_properties.end_prop(PropId::r_fear);
        };
        d.trait_prereqs = {Trait::cool_headed};
        set_trait_data(d);

        // --- Stealthy ---
        d.id = Trait::stealthy;
        d.title = "Stealthy";
        d.descr = "+45% chance to avoid detection by sight";
        set_trait_data(d);

        // --- Imperceptible ---
        d = trait_data(Trait::stealthy);
        d.id = Trait::imperceptible;
        d.title = "Imperceptible";
        d.trait_prereqs = {Trait::stealthy};
        d.bg_prereq = Bg::rogue;
        set_trait_data(d);

        // --- Silent ---
        d.id = Trait::silent;
        d.title = "Silent";
        d.descr =
                "All your melee attacks are silent (regardless of the weapon), "
                "and creatures are not alerted when you open or close doors, "
                "or wade through water";
        d.trait_prereqs = {Trait::stealthy};
        set_trait_data(d);

        // --- Vigilant ---
        d.id = Trait::vigilant;
        d.title = "Vigilant";
        d.descr = "You are always aware of nearby creatures";
        d.blocked_for_occultist_domains = {OccultistDomain::clairvoyant};
        set_trait_data(d);

        // --- Treasure Hunter ---
        d.id = Trait::treasure_hunter;
        d.title = "Treasure Hunter";
        d.descr = "You tend to find more items";
        d.blocked_for_bgs = {
                Bg::exorcist,
                Bg::ghoul,
                Bg::war_vet,
                Bg::flagellant};
        d.blocked_for_occultist_domains = {
                OccultistDomain::enchanter,
                OccultistDomain::invoker,
                OccultistDomain::transmuter};
        set_trait_data(d);

        // --- Self-aware ---
        d.id = Trait::self_aware;
        d.title = "Self-aware";
        d.descr =
                "You cannot become confused, the number of remaining turns "
                "for status effects are displayed";
        d.on_picked = []() {
                auto* prop = property_factory::make(PropId::r_conf);

                prop->set_indefinite();

                map::g_player->m_properties.apply(
                        prop,
                        PropSrc::intr,
                        true,
                        Verbose::no);
        };
        d.on_removed = []() {
                map::g_player->m_properties.end_prop(PropId::r_conf);
        };
        d.trait_prereqs = {Trait::stout_spirit, Trait::cool_headed};
        set_trait_data(d);

        // --- Healer ---
        d.id = Trait::healer;
        d.title = "Healer";
        d.descr =
                "Using medical equipment requires only half the normal time "
                "and resources";
        d.blocked_for_bgs = {Bg::ghoul};
        set_trait_data(d);

        // --- Rapid Recoverer ---
        d.id = Trait::rapid_recoverer;
        d.title = "Rapid Recoverer";
        d.descr = "You regenerate 1 hit point every second turn";
        d.trait_prereqs = {Trait::tough, Trait::healer};
        d.blocked_for_bgs = {Bg::ghoul};
        set_trait_data(d);

        // --- Survivalist ---
        d.id = Trait::survivalist;
        d.title = "Survivalist";
        d.descr =
                "You cannot become diseased, wounds do not affect your combat "
                "abilities, and their negative effect on hit points and "
                "regeneration is halved";
        d.on_picked = []() {
                auto* prop = property_factory::make(PropId::r_disease);

                prop->set_indefinite();

                map::g_player->m_properties.apply(
                        prop,
                        PropSrc::intr,
                        true,
                        Verbose::no);
        };
        d.on_removed = []() {
                map::g_player->m_properties.end_prop(PropId::r_disease);
        };
        d.trait_prereqs = {Trait::healer};
        d.blocked_for_bgs = {Bg::ghoul, Bg::flagellant};
        set_trait_data(d);

        // --- Stout Spirit ---
        d.id = Trait::stout_spirit;
        d.title = "Stout Spirit";
        d.descr =
                "+2 spirit points, increased spirit regeneration rate, you "
                "can defy harmful spells (it takes 125-150 turns to regain "
                "spell resistance after a spell is blocked)";
        d.on_picked = []() {
                auto* prop = property_factory::make(PropId::r_spell);

                prop->set_indefinite();

                map::g_player->m_properties.apply(
                        prop,
                        PropSrc::intr,
                        true,
                        Verbose::no);

                const int spi_incr = 2;

                map::g_player->change_max_sp(
                        spi_incr,
                        Verbose::no);

                map::g_player->restore_sp(
                        spi_incr,
                        false,  // Not allowed above max
                        Verbose::no);
        };
        d.on_removed = []() {
                map::g_player->change_max_sp(-2, Verbose::no);
        };
        set_trait_data(d);

        // --- Strong Spirit ---
        d = trait_data(Trait::stout_spirit);
        d.id = Trait::strong_spirit;
        d.title = "Strong Spirit";
        d.descr =
                "+2 spirit points, increased spirit regeneration rate, it "
                "takes 75-100 turns to regain spell resistance after a spell "
                "is blocked";
        d.trait_prereqs = {Trait::stout_spirit};
        set_trait_data(d);

        // --- Mighty Spirit ---
        d = trait_data(Trait::stout_spirit);
        d.id = Trait::mighty_spirit;
        d.title = "Mighty Spirit";
        d.descr =
                "+2 spirit points, increased spirit regeneration rate, it "
                "takes 25-50 turns to regain spell resistance after a spell "
                "is blocked";
        d.trait_prereqs = {Trait::strong_spirit};
        set_trait_data(d);

        // --- Meditative ---
        d.id = Trait::meditative;
        d.title = "Meditative";
        d.descr =
                "Applies a focused state which allows the next spell to be "
                "cast without spending a turn, and with the cost reduced by "
                "1 spirit point - it takes 125-150 turns to regain this "
                "state after a spell is cast";
        d.on_picked = []() {
                auto* prop = property_factory::make(PropId::meditative_focused);

                prop->set_indefinite();

                map::g_player->m_properties.apply(
                        prop,
                        PropSrc::intr,
                        true,
                        Verbose::no);
        };
        d.trait_prereqs = {Trait::stout_spirit, Trait::cool_headed};
        d.blocked_for_bgs = {Bg::ghoul, Bg::war_vet, Bg::rogue};
        set_trait_data(d);

        // --- Absorption ---
        d.id = Trait::absorb;
        d.title = "Absorption";
        d.descr =
                "1-6 spirit points are restored each time a spell is resisted "
                "by spell resistance (granted by spirit traits, or the Spell "
                "Shield spell)";
        d.trait_prereqs = {Trait::strong_spirit};
        set_trait_data(d);

        // --- Tough ---
        d.id = Trait::tough;
        d.title = "Tough";
        d.descr =
                "+6 hit points, +10% chance to resist paralysis, burning, and "
                "poison, less likely to sprain when kicking, more likely to "
                "succeed with object interactions requiring strength (e.g. "
                "bashing things open)";
        d.on_picked = []() {
                const int hp_incr = 6;

                map::g_player->change_max_hp(hp_incr, Verbose::no);

                map::g_player->restore_hp(
                        hp_incr,
                        false,  // Not allowed above max
                        Verbose::no);
        };
        d.on_removed = []() {
                map::g_player->change_max_hp(-6, Verbose::no);
        };
        set_trait_data(d);

        // --- Rugged ---
        d = trait_data(Trait::tough);
        d.id = Trait::rugged;
        d.title = "Rugged";
        d.trait_prereqs = {Trait::tough};
        set_trait_data(d);

        // --- Unbreakable ---
        d = trait_data(Trait::rugged);
        d.id = Trait::unbreakable;
        d.title = "Unbreakable";
        d.bg_prereq = Bg::flagellant;
        d.trait_prereqs = {Trait::rugged};
        set_trait_data(d);

        // --- Thick Skinned ---
        d.id = Trait::thick_skinned;
        d.title = "Thick Skinned";
        d.descr = "+1 armor point (physical damage reduced by 1 point)";
        d.trait_prereqs = {Trait::tough};
        set_trait_data(d);

        // --- Callous ---
        d = trait_data(Trait::thick_skinned);
        d.id = Trait::callous;
        d.title = "Callous";
        d.bg_prereq = Bg::flagellant;
        d.trait_prereqs = {Trait::thick_skinned};
        set_trait_data(d);

        // --- Resistant ---
        d.id = Trait::resistant;
        d.title = "Resistant";
        d.descr =
                "+25% chance to resist paralysis, burning, and poison - "
                "and the duration of those effects is halved";
        d.trait_prereqs = {Trait::tough};
        set_trait_data(d);

        // --- Strong-backed ---
        d.id = Trait::strong_backed;
        d.title = "Strong-backed";
        d.descr = "+50% carry weight limit";
        d.trait_prereqs = {Trait::tough};
        set_trait_data(d);

        // --- Bane of the Undead ---
        d.id = Trait::undead_bane;
        d.title = "Bane of the Undead";
        d.descr =
                "+2 melee and ranged attack damage against all undead "
                "monsters, +50% hit chance against ethereal undead monsters";
        d.trait_prereqs =
                {Trait::tough, Trait::fearless, Trait::stout_spirit};
        set_trait_data(d);

        // --- Electrically Inclined ---
        d.id = Trait::elec_incl;
        d.title = "Electrically Inclined";
        d.descr =
                "Rods recharge twice as fast, strange devices are less likely "
                "to malfunction or break, electric lanterns last twice as "
                "long, +1 damage with electricity weapons";
        d.blocked_for_bgs = {Bg::ghoul};
        set_trait_data(d);

        // --- Cast Bless ---
        d.id = Trait::cast_bless_i;
        d.title = "Cast Bless";
        d.descr =
                trait_descr_for_spell(
                        SpellId::bless,
                        SpellSkill::basic);
        d.on_picked = []() {
                player_spells::learn_spell(
                        SpellId::bless,
                        Verbose::no);
        };
        d.on_removed = []() {
                player_spells::remove_learned_spell(SpellId::bless);
        };
        d.bg_prereq = Bg::exorcist;
        set_trait_data(d);

        // --- Cast Bless II ---
        d.id = Trait::cast_bless_ii;
        d.title = "Cast Bless II";
        d.descr =
                trait_descr_for_spell(
                        SpellId::bless,
                        SpellSkill::expert);
        d.on_picked = []() {
                player_spells::incr_spell_skill(
                        SpellId::bless,
                        Verbose::no);
        };
        d.on_removed = []() {
                player_spells::set_spell_skill(
                        SpellId::bless,
                        SpellSkill::basic);
        };
        d.trait_prereqs = {Trait::cast_bless_i};
        d.bg_prereq = Bg::exorcist;
        set_trait_data(d);

        // --- Cast Cleansing Fire ---
        d.id = Trait::cast_cleansing_fire_i;
        d.title = "Cast Cleansing Fire";
        d.descr =
                trait_descr_for_spell(
                        SpellId::cleansing_fire,
                        SpellSkill::basic);
        d.on_picked = []() {
                player_spells::learn_spell(
                        SpellId::cleansing_fire,
                        Verbose::no);
        };
        d.on_removed = []() {
                player_spells::remove_learned_spell(SpellId::cleansing_fire);
        };
        d.bg_prereq = Bg::exorcist;
        set_trait_data(d);

        // --- Cast Cleansing Fire II ---
        d.id = Trait::cast_cleansing_fire_ii;
        d.title = "Cast Cleansing Fire II";
        d.descr =
                trait_descr_for_spell(
                        SpellId::cleansing_fire,
                        SpellSkill::expert);
        d.on_picked = []() {
                player_spells::incr_spell_skill(
                        SpellId::cleansing_fire,
                        Verbose::no);
        };
        d.on_removed = []() {
                player_spells::set_spell_skill(
                        SpellId::cleansing_fire,
                        SpellSkill::basic);
        };
        d.trait_prereqs = {Trait::cast_cleansing_fire_i};
        d.bg_prereq = Bg::exorcist;
        set_trait_data(d);

        // --- Cast Heal ---
        d.id = Trait::cast_heal_i;
        d.title = "Cast Heal";
        d.descr =
                trait_descr_for_spell(
                        SpellId::heal,
                        SpellSkill::basic);
        d.on_picked = []() {
                player_spells::learn_spell(
                        SpellId::heal,
                        Verbose::no);
        };
        d.on_removed = []() {
                player_spells::remove_learned_spell(SpellId::heal);
        };
        d.bg_prereq = Bg::exorcist;
        set_trait_data(d);

        // --- Cast Heal II ---
        d.id = Trait::cast_heal_ii;
        d.title = "Cast Heal II";
        d.descr =
                trait_descr_for_spell(
                        SpellId::heal,
                        SpellSkill::expert);
        d.on_picked = []() {
                player_spells::incr_spell_skill(
                        SpellId::heal,
                        Verbose::no);
        };
        d.on_removed = []() {
                player_spells::set_spell_skill(
                        SpellId::heal,
                        SpellSkill::basic);
        };
        d.trait_prereqs = {Trait::cast_heal_i};
        d.bg_prereq = Bg::exorcist;
        set_trait_data(d);

        // --- Cast Light ---
        d.id = Trait::cast_light_i;
        d.title = "Cast Light";
        d.descr =
                trait_descr_for_spell(
                        SpellId::light,
                        SpellSkill::basic);
        d.on_picked = []() {
                player_spells::learn_spell(
                        SpellId::light,
                        Verbose::no);
        };
        d.on_removed = []() {
                player_spells::remove_learned_spell(SpellId::light);
        };
        d.bg_prereq = Bg::exorcist;
        set_trait_data(d);

        // --- Cast Light II ---
        d.id = Trait::cast_light_ii;
        d.title = "Cast Light II";
        d.descr =
                trait_descr_for_spell(
                        SpellId::light,
                        SpellSkill::expert);
        d.on_picked = []() {
                player_spells::incr_spell_skill(
                        SpellId::light,
                        Verbose::no);
        };
        d.on_removed = []() {
                player_spells::set_spell_skill(
                        SpellId::light,
                        SpellSkill::basic);
        };
        d.trait_prereqs = {Trait::cast_light_i};
        d.bg_prereq = Bg::exorcist;
        set_trait_data(d);

        // --- Cast Sanctuary ---
        d.id = Trait::cast_sanctuary_i;
        d.title = "Cast Sanctuary";
        d.descr =
                trait_descr_for_spell(
                        SpellId::sanctuary,
                        SpellSkill::basic);
        d.on_picked = []() {
                player_spells::learn_spell(
                        SpellId::sanctuary,
                        Verbose::no);
        };
        d.on_removed = []() {
                player_spells::remove_learned_spell(SpellId::sanctuary);
        };
        d.bg_prereq = Bg::exorcist;
        set_trait_data(d);

        // --- Cast Sanctuary II ---
        d.id = Trait::cast_sanctuary_ii;
        d.title = "Cast Sanctuary II";
        d.descr =
                trait_descr_for_spell(
                        SpellId::sanctuary,
                        SpellSkill::expert);
        d.on_picked = []() {
                player_spells::incr_spell_skill(
                        SpellId::sanctuary,
                        Verbose::no);
        };
        d.on_removed = []() {
                player_spells::set_spell_skill(
                        SpellId::sanctuary,
                        SpellSkill::basic);
        };
        d.trait_prereqs = {Trait::cast_sanctuary_i};
        d.bg_prereq = Bg::exorcist;
        set_trait_data(d);

        // --- Cast See Invisible ---
        d.id = Trait::cast_see_invisible_i;
        d.title = "Cast See Invisible";
        d.descr =
                trait_descr_for_spell(
                        SpellId::see_invis,
                        SpellSkill::basic);
        d.on_picked = []() {
                player_spells::learn_spell(
                        SpellId::see_invis,
                        Verbose::no);
        };
        d.on_removed = []() {
                player_spells::remove_learned_spell(SpellId::see_invis);
        };
        d.bg_prereq = Bg::exorcist;
        set_trait_data(d);

        // --- Cast See Invisible II ---
        d.id = Trait::cast_see_invisible_ii;
        d.title = "Cast See Invisible II";
        d.descr =
                trait_descr_for_spell(
                        SpellId::see_invis,
                        SpellSkill::expert);
        d.on_picked = []() {
                player_spells::incr_spell_skill(
                        SpellId::see_invis,
                        Verbose::no);
        };
        d.on_removed = []() {
                player_spells::set_spell_skill(
                        SpellId::see_invis,
                        SpellSkill::basic);
        };
        d.trait_prereqs = {Trait::cast_see_invisible_i};
        d.bg_prereq = Bg::exorcist;
        set_trait_data(d);

        // --- Prolonged Life ---
        d.id = Trait::prolonged_life;
        d.title = "Prolonged Life";
        d.descr =
                "Any fatal damage received is instead drained fom your "
                "spirit points";
        d.bg_prereq = Bg::exorcist;
        set_trait_data(d);

        // --- Ravenous ---
        d.id = Trait::ravenous;
        d.title = "Ravenous";
        d.descr =
                "You occasionally feed on living victims when attacking "
                "with claws";
        d.trait_prereqs = {Trait::adept_melee};
        d.bg_prereq = Bg::ghoul;
        set_trait_data(d);

        // --- Foul ---
        d.id = Trait::foul;
        d.title = "Foul";
        d.descr =
                "+1 claw damage, when attacking with claws, vicious worms "
                "occasionally burst out from the corpses of your victims to "
                "attack your enemies";
        d.bg_prereq = Bg::ghoul;
        set_trait_data(d);

        // --- Toxic ---
        d.id = Trait::toxic;
        d.title = "Toxic";
        d.descr =
                "+1 claw damage, you are immune to poison, and attacks with "
                "your claws often poisons your victims";
        d.on_picked = []() {
                auto* prop = property_factory::make(PropId::r_poison);

                prop->set_indefinite();

                map::g_player->m_properties.apply(
                        prop,
                        PropSrc::intr,
                        true,
                        Verbose::no);
        };
        d.on_removed = []() {
                map::g_player->m_properties.end_prop(PropId::r_poison);
        };
        d.trait_prereqs = {Trait::foul};
        d.bg_prereq = Bg::ghoul;
        set_trait_data(d);

        // --- Indomitable Fury ---
        d.id = Trait::indomitable_fury;
        d.title = "Indomitable Fury";
        d.descr =
                "While frenzied, you are immune to wounds, and your claw "
                "attacks cause fear";
        d.trait_prereqs = {Trait::adept_melee, Trait::tough};
        d.bg_prereq = Bg::ghoul;
        set_trait_data(d);

        // --- Vicious ---
        d.id = Trait::vicious;
        d.title = "Vicious";
        d.descr = "+100% backstab damage (in addition to the normal +50%)";
        d.trait_prereqs = {Trait::stealthy, Trait::dexterous};
        d.bg_prereq = Bg::rogue;
        set_trait_data(d);

        // --- Ruthless ---
        d.id = Trait::ruthless;
        d.title = "Ruthless";
        d.descr = "+100% backstab damage";
        d.trait_prereqs = {Trait::vicious};
        d.bg_prereq = Bg::rogue;
        set_trait_data(d);

        // --- Steady Aimer ---
        d.id = Trait::steady_aimer;
        d.title = "Steady Aimer";
        d.descr =
                "Standing still gives ranged attacks maximum damage and +10% "
                "hit chance on the following turn, unless damage is taken";
        d.bg_prereq = Bg::war_vet;
        set_trait_data(d);

        // --- Galvanization ---
        d.id = Trait::galvanization;
        d.title = "Galvanization";
        d.descr =
                "Casting any spell from the Blood domain grants "
                "Regeneration for 4-8 turns "
                "(+1 extra hit point regenerated per turn)";
        d.bg_prereq = Bg::flagellant;
        set_trait_data(d);

        // --- Death Sense ---
        d.id = Trait::death_sense;
        d.title = "Death Sense";
        d.descr =
                "Doubles all Flagellant bonuses gained when health is low";
        d.bg_prereq = Bg::flagellant;
        set_trait_data(d);
}

static bool is_trait_blocked_for_bg(
        const Trait trait,
        const Bg bg,
        const OccultistDomain occultist_domain)
{
        const auto d = trait_data(trait);

        const bool is_blocked_for_bg =
                std::find(
                        std::begin(d.blocked_for_bgs),
                        std::end(d.blocked_for_bgs),
                        bg) != std::end(d.blocked_for_bgs);

        const bool is_blocked_for_domain =
                std::find(
                        std::begin(d.blocked_for_occultist_domains),
                        std::end(d.blocked_for_occultist_domains),
                        occultist_domain) !=
                std::end(d.blocked_for_occultist_domains);

        return is_blocked_for_bg || is_blocked_for_domain;
}

static void incr_spell_skills(const SpellDomain spell_domain)
{
        for (int i = 0; i < (int)SpellId::END; ++i) {
                const auto id = (SpellId)i;

                const std::unique_ptr<Spell> spell(spells::make(id));

                if (spell->player_can_learn() &&
                    (spell->domain() == spell_domain)) {
                        player_spells::incr_spell_skill(id, Verbose::yes);
                }
        }
}

static bool is_spell_upgrade_clvl(const int clvl)
{
        return (
                (clvl == s_spell_upgrade_lvl_1) ||
                (clvl == s_spell_upgrade_lvl_2));
}

// -----------------------------------------------------------------------------
// player_bon
// -----------------------------------------------------------------------------
namespace player_bon
{
void init()
{
        s_player_bg = Bg::END;

        s_player_occultist_domain = OccultistDomain::END;

        for (size_t i = 0; i < (size_t)Trait::END; ++i) {
                s_traits_picked[i] = false;
        }

        update_trait_data();

        s_trait_log.clear();
}

void save()
{
        saving::put_int((int)s_player_bg);

        saving::put_int((int)s_player_occultist_domain);

        for (size_t i = 0; i < (size_t)Trait::END; ++i) {
                saving::put_bool(s_traits_picked[i]);
        }

        saving::put_int((int)s_trait_log.size());

        for (const auto& e : s_trait_log) {
                saving::put_int(e.clvl);

                saving::put_int((int)e.trait_id);

                saving::put_bool(e.is_removal);
        }
}

void load()
{
        s_player_bg = (Bg)saving::get_int();

        s_player_occultist_domain = (OccultistDomain)saving::get_int();

        for (size_t i = 0; i < (size_t)Trait::END; ++i) {
                s_traits_picked[i] = saving::get_bool();
        }

        const int nr_trait_log_entries = saving::get_int();

        s_trait_log.resize(nr_trait_log_entries);

        for (auto& e : s_trait_log) {
                e.clvl = saving::get_int();

                e.trait_id = (Trait)saving::get_int();

                e.is_removal = saving::get_bool();
        }
}

std::string bg_title(const Bg id)
{
        switch (id) {
        case Bg::exorcist:
                return "Exorcist";

        case Bg::flagellant:
                return "Flagellant";

        case Bg::ghoul:
                return "Ghoul";

        case Bg::occultist:
                return "Occultist";

        case Bg::rogue:
                return "Rogue";

        case Bg::war_vet:
                return "War Veteran";

        case Bg::END:
                break;
        }

        ASSERT(false);

        return "";
}

std::string occultist_profession_title(const OccultistDomain domain)
{
        switch (domain) {
        case OccultistDomain::clairvoyant:
                return "Clairvoyant";

        case OccultistDomain::enchanter:
                return "Enchanter";

        case OccultistDomain::invoker:
                return "Invoker";

        case OccultistDomain::transmuter:
                return "Transmuter";

        case OccultistDomain::END:
                break;
        }

        ASSERT(false);

        return "";
}

SpellDomain occultist_to_spell_domain(
        const OccultistDomain occultist_domain)
{
        switch (occultist_domain) {
        case OccultistDomain::clairvoyant:
                return SpellDomain::clairvoyance;
                break;

        case OccultistDomain::enchanter:
                return SpellDomain::enchantment;
                break;

        case OccultistDomain::invoker:
                return SpellDomain::invocation;
                break;

        case OccultistDomain::transmuter:
                return SpellDomain::transmutation;
                break;

        case OccultistDomain::END:
                break;
        }

        return SpellDomain::END;
}

std::string trait_title(const Trait id)
{
        return trait_data(id).title;
}

std::vector<ColoredString> bg_descr(const Bg id)
{
        std::vector<ColoredString> descr;

        auto put = [&descr](const std::string& str) {
                descr.emplace_back(str, colors::text());
        };

        auto put_trait = [&descr](const Trait trait_id) {
                const auto t = trait_title(trait_id);
                const auto d = trait_descr(trait_id);

                descr.emplace_back(
                        "{white}" + t + "{color_reset}: " + d,
                        colors::gray());
        };

        switch (id) {
        case Bg::exorcist:
                put("Starts with a Holy Symbol, which can restore "
                    "spirit points and grant resistance against "
                    "shock and fear");
                put("");
                put("Cannot use manuscripts, altars, monoliths, or gongs, "
                    "but gains experience and spirit points for destroying "
                    "these (manuscripts are destroyed when picking them up)");
                put("");
                put("Spirit points gained above the maximum level can be kept "
                    "indefinitely until they are spent");
                put("");
                put("Gains a bonus trait at character levels " +
                    std::to_string(s_exorcist_bon_trait_lvl_1) +
                    ", " +
                    std::to_string(s_exorcist_bon_trait_lvl_2) +
                    ", and " +
                    std::to_string(s_exorcist_bon_trait_lvl_3));
                put("");
                put_trait(Trait::stout_spirit);
                put("");
                put_trait(Trait::undead_bane);
                break;

        case Bg::flagellant:
                put("No shock received for taking damage.");
                put("");
                put("While health is reduced to 6 hit points or below, "
                    "the following bonuses are received: "
                    "+3 melee damage, +30% melee hit chance, +3 armor points.");
                put("");
                put("Wears a torture collar which cannot be taken off; "
                    "walking requires extra turns, and stealth and evasion "
                    "are reduced by 20%. However, wearing the collar hardens "
                    "the Flagellant against physical suffering, armor is "
                    "increased by 3 points.");
                put("");
                put("Specializes in spells belonging to the Blood domain. "
                    "At character levels " +
                    std::to_string(s_spell_upgrade_lvl_1) +
                    " and " +
                    std::to_string(s_spell_upgrade_lvl_2) +
                    ", all spells belonging to this domain are cast at "
                    "a higher skill level.");
                put("");
                put("-25% shock taken from casting memorized spells "
                    "from the Blood domain.");
                put("");
                put_trait(Trait::self_aware);
                put("");
                put_trait(Trait::tough);
                break;

        case Bg::ghoul:
                put("Does not regenerate hit points and cannot use medical "
                    "equipment - heals by feeding on corpses (feeding is done "
                    "while waiting on a corpse)");
                put("");
                put("Can incite frenzy at will, and does not become weakened "
                    "when frenzy ends");
                put("");
                put("+8 hit points");
                put("");
                put("Is immune to disease and infections");
                put("");
                put("Does not get sprains");
                put("");
                put("Can see in darkness");
                put("");
                put("-50% shock taken from seeing monsters and standing "
                    "in darkness, but also -50% shock reduction bonus "
                    "from light");
                put("");
                put("-15% hit chance with firearms and thrown weapons");
                put("");
                put("All ghouls are allied");
                break;

        case Bg::occultist:
                put("Specializes in a spell domain (selected at character "
                    "creation). At character levels " +
                    std::to_string(s_spell_upgrade_lvl_1) +
                    " and " +
                    std::to_string(s_spell_upgrade_lvl_2) +
                    ", all spells belonging to the chosen domain are cast at "
                    "a higher skill level. This choice also determines "
                    "starting spells.");
                put("");
                put("-50% shock taken from casting memorized spells, "
                    "and from using or identifying strange items "
                    "(e.g. drinking a potion, or casting a spell from "
                    "a manuscript)");
                put("");
                put("Can dispel magic traps, doing so grants spirit points");
                put("");
                put("+3 spirit points (in addition to \"Stout Spirit\")");
                put("");
                put_trait(Trait::stout_spirit);
                break;

        case Bg::rogue:
                put("Shock received passively over time is reduced by 25%");
                put("");
                put("+10% chance to spot hidden monsters, doors, and traps");
                put("");
                put("Remains aware of the presence of other creatures longer");
                put("");
                put("Can sense the presence of unique monsters or powerful "
                    "artifacts");
                put("");
                put("Has acquired an artifact which can cloud the minds of all "
                    "enemies, causing them to forget the presence of the user");
                put("");
                put_trait(Trait::stealthy);
                break;

        case Bg::war_vet:
                put("Switches to prepared weapon instantly");
                put("");
                put("Starts with a Flak Jacket");
                put("");
                put("Maintains armor twice as long before it breaks");
                put("");
                put_trait(Trait::adept_marksman);
                put("");
                put_trait(Trait::adept_melee);
                put("");
                put_trait(Trait::tough);
                put("");
                put_trait(Trait::healer);
                break;

        case Bg::END:
                ASSERT(false);
                break;
        }

        return descr;
}

std::string occultist_domain_descr(const OccultistDomain domain)
{
        switch (domain) {
        case OccultistDomain::clairvoyant:
                return "Specializes in detection and learning. "
                       "Has an intrinsic ability to detect doors, traps, "
                       "stairs, and other locations of interest in the "
                       "surrounding area. At character level 4, this ability "
                       "also reveals items, and at level 8 it reveals "
                       "creatures";

        case OccultistDomain::enchanter:
                return "Specializes in aiding, debilitating, entrancing, and "
                       "beguiling";

        case OccultistDomain::invoker:
                return "Specializes in channeling destructive powers";

        case OccultistDomain::transmuter:
                return "Specializes in manipulating matter, energy, and time";

        case OccultistDomain::END:
                ASSERT(false);
                break;
        }

        return "";
}

std::string trait_descr(const Trait id)
{
        return trait_data(id).descr;
}

TraitPrereqData trait_prereqs(
        const Trait trait,
        const Bg bg,
        const OccultistDomain occultist_domain)
{
        const auto& d = trait_data(trait);

        TraitPrereqData result;

        result.traits = d.trait_prereqs;
        result.bg = d.bg_prereq;

        // Remove traits which are blocked for this background (prerequisites
        // are considered fulfilled).
        for (auto it = std::begin(result.traits);
             it != std::end(result.traits);) {
                if (is_trait_blocked_for_bg(*it, bg, occultist_domain)) {
                        it = result.traits.erase(it);
                }
                else {
                        // Not blocked
                        ++it;
                }
        }

        // Sort lexicographically.
        std::sort(
                std::begin(result.traits),
                std::end(result.traits),
                [](const Trait& t1, const Trait& t2) {
                        const std::string str1 = trait_title(t1);
                        const std::string str2 = trait_title(t2);
                        return str1 < str2;
                });

        return result;
}

Bg bg()
{
        return s_player_bg;
}

OccultistDomain occultist_domain()
{
        return s_player_occultist_domain;
}

bool is_bg(Bg bg)
{
        ASSERT(bg != Bg::END);

        return bg == s_player_bg;
}

bool has_trait(const Trait id)
{
        return s_traits_picked[(size_t)id];
}

std::vector<Bg> pickable_bgs()
{
        std::vector<Bg> result;

        result.reserve((int)Bg::END);

        for (int i = 0; i < (int)Bg::END; ++i) {
                result.push_back((Bg)i);
        }

        // Sort lexicographically.
        std::sort(
                std::begin(result),
                std::end(result),
                [](const Bg bg1, const Bg bg2) {
                        const std::string str1 = bg_title(bg1);
                        const std::string str2 = bg_title(bg2);
                        return str1 < str2;
                });

        return result;
}

std::vector<OccultistDomain> pickable_occultist_domains()
{
        std::vector<OccultistDomain> result;

        result.reserve((int)OccultistDomain::END);

        for (int i = 0; i < (int)OccultistDomain::END; ++i) {
                result.push_back((OccultistDomain)i);
        }

        // Sort lexicographically.
        std::sort(
                std::begin(result),
                std::end(result),
                [](
                        const OccultistDomain domain_1,
                        const OccultistDomain domain_2) {
                        const SpellDomain spell_domain_1 =
                                occultist_to_spell_domain(domain_1);

                        const SpellDomain spell_domain_2 =
                                occultist_to_spell_domain(domain_2);

                        const std::string str1 =
                                spells::spell_domain_title(spell_domain_1);

                        const std::string str2 =
                                spells::spell_domain_title(spell_domain_2);

                        return str1 < str2;
                });

        return result;
}

UnpickedTraitsData unpicked_traits(
        const Bg bg,
        const OccultistDomain occultist_domain)
{
        update_trait_data();

        UnpickedTraitsData result;

        for (const auto& d : s_trait_data) {
                if (s_traits_picked[(size_t)d.id]) {
                        continue;
                }

                // Check if trait is explicitly blocked for this background.
                const bool is_blocked_for_bg =
                        is_trait_blocked_for_bg(
                                d.id,
                                bg,
                                occultist_domain);

                if (is_blocked_for_bg) {
                        continue;
                }

                // Check trait prerequisites (traits and background).

                // NOTE: Traits blocked for the current background are not
                // considered prerequisites.
                const auto prereq_data =
                        trait_prereqs(
                                d.id,
                                bg,
                                occultist_domain);

                const bool is_bg_ok =
                        (s_player_bg == prereq_data.bg) ||
                        (prereq_data.bg == Bg::END);

                if (!is_bg_ok) {
                        continue;
                }

                bool is_trait_prereqs_ok = true;

                for (const auto& prereq : prereq_data.traits) {
                        if (!s_traits_picked[(size_t)prereq]) {
                                is_trait_prereqs_ok = false;
                                break;
                        }
                }

                if (is_trait_prereqs_ok) {
                        result.traits_can_be_picked.push_back(d.id);
                }
                else {
                        result.traits_prereqs_not_met.push_back(d.id);
                }

        }  // Trait loop

        // Sort lexicographically
        std::sort(
                std::begin(result.traits_can_be_picked),
                std::end(result.traits_can_be_picked),
                [](const Trait& t1, const Trait& t2) {
                        const std::string str1 = trait_title(t1);
                        const std::string str2 = trait_title(t2);
                        return str1 < str2;
                });

        std::sort(
                std::begin(result.traits_prereqs_not_met),
                std::end(result.traits_prereqs_not_met),
                [](const Trait& t1, const Trait& t2) {
                        const std::string str1 = trait_title(t1);
                        const std::string str2 = trait_title(t2);
                        return str1 < str2;
                });

        return result;
}  // unpicked_traits

std::vector<Trait> traits_can_be_removed()
{
        update_trait_data();

        std::vector<Trait> result;

        for (const auto& d : s_trait_data) {
                if (!s_traits_picked[(size_t)d.id]) {
                        continue;
                }

                bool is_prereq_for_other_trait = false;

                for (const auto& d_other : s_trait_data) {
                        if (!s_traits_picked[(size_t)d_other.id]) {
                                continue;
                        }

                        const auto match =
                                std::find(
                                        std::begin(d_other.trait_prereqs),
                                        std::end(d_other.trait_prereqs),
                                        d.id);

                        if (match != std::end(d_other.trait_prereqs)) {
                                is_prereq_for_other_trait = true;
                                break;
                        }
                }

                if (is_prereq_for_other_trait) {
                        continue;
                }

                result.push_back(d.id);
        }

        return result;
}

void pick_bg(const Bg bg)
{
        TRACE_FUNC_BEGIN;

        ASSERT(bg != Bg::END);

        s_player_bg = bg;

        switch (s_player_bg) {
        case Bg::exorcist: {
                pick_trait(Trait::stout_spirit);
                pick_trait(Trait::undead_bane);

                // Mark all scrolls as found, so that they do not yield XP.
                for (auto& d : item::g_data) {
                        if (d.type == ItemType::scroll) {
                                d.is_found = true;
                        }
                }
        } break;

        case Bg::flagellant: {
                pick_trait(Trait::self_aware);
                pick_trait(Trait::tough);

                Prop* moribund = property_factory::make(PropId::moribund);

                moribund->set_indefinite();

                map::g_player->m_properties.apply(
                        moribund,
                        PropSrc::intr,
                        true,
                        Verbose::no);
        } break;

        case Bg::ghoul: {
                Prop* r_disease = property_factory::make(PropId::r_disease);

                r_disease->set_indefinite();

                map::g_player->m_properties.apply(
                        r_disease,
                        PropSrc::intr,
                        true,
                        Verbose::no);

                Prop* darkvis = property_factory::make(PropId::darkvision);

                darkvis->set_indefinite();

                map::g_player->m_properties.apply(
                        darkvis,
                        PropSrc::intr,
                        true,
                        Verbose::no);

                player_spells::learn_spell(SpellId::frenzy, Verbose::no);

                map::g_player->change_max_hp(8, Verbose::no);
        } break;

        case Bg::occultist: {
                pick_trait(Trait::stout_spirit);

                map::g_player->change_max_sp(3, Verbose::no);
        } break;

        case Bg::rogue: {
                pick_trait(Trait::stealthy);
        } break;

        case Bg::war_vet: {
                pick_trait(Trait::adept_marksman);
                pick_trait(Trait::adept_melee);
                pick_trait(Trait::tough);
                pick_trait(Trait::healer);
        } break;

        case Bg::END:
                break;
        }

        TRACE_FUNC_END;
}

void pick_occultist_domain(const OccultistDomain domain)
{
        ASSERT(domain != OccultistDomain::END);

        s_player_occultist_domain = domain;

        switch (domain) {
        case OccultistDomain::clairvoyant: {
                auto* prop =
                        static_cast<PropMagicSearching*>(
                                property_factory::make(
                                        PropId::magic_searching));

                prop->set_indefinite();

                prop->set_range(g_fov_radi_int);

                map::g_player->m_properties.apply(
                        prop,
                        PropSrc::intr,
                        true,
                        Verbose::no);
        } break;

        case OccultistDomain::enchanter: {
        } break;

        case OccultistDomain::invoker: {
        } break;

        case OccultistDomain::transmuter: {
        } break;

        case OccultistDomain::END: {
                ASSERT(false);
        } break;
        }
}

void on_player_gained_lvl(const int new_lvl)
{
        TRACE_FUNC_BEGIN;

        switch (s_player_bg) {
        case Bg::exorcist: {
                const bool is_exorcist_extra_trait =
                        (new_lvl == s_exorcist_bon_trait_lvl_1) ||
                        (new_lvl == s_exorcist_bon_trait_lvl_2) ||
                        (new_lvl == s_exorcist_bon_trait_lvl_3);

                if (is_exorcist_extra_trait) {
                        states::push(
                                std::make_unique<PickTraitState>(
                                        "You gain an extra trait!"));
                }
        } break;

        case Bg::flagellant: {
                if (is_spell_upgrade_clvl(new_lvl)) {
                        incr_spell_skills(SpellDomain::blood);
                }
        } break;

        case Bg::ghoul: {
        } break;

        case Bg::occultist: {
                switch (s_player_occultist_domain) {
                case OccultistDomain::clairvoyant: {
                        if (is_spell_upgrade_clvl(new_lvl)) {
                                incr_spell_skills(SpellDomain::clairvoyance);
                        }

                        Prop* const prop =
                                map::g_player->m_properties.prop(
                                        PropId::magic_searching);

                        ASSERT(prop);

                        auto* const searching =
                                static_cast<PropMagicSearching*>(prop);

                        if (new_lvl == s_spell_upgrade_lvl_1) {
                                searching->set_allow_reveal_items();
                        }
                        else if (new_lvl == s_spell_upgrade_lvl_2) {
                                searching->set_allow_reveal_creatures();
                        }
                } break;

                case OccultistDomain::enchanter: {
                        if (is_spell_upgrade_clvl(new_lvl)) {
                                incr_spell_skills(SpellDomain::enchantment);
                        }
                } break;

                case OccultistDomain::invoker: {
                        if (is_spell_upgrade_clvl(new_lvl)) {
                                incr_spell_skills(SpellDomain::invocation);
                        }
                } break;

                case OccultistDomain::transmuter: {
                        if (is_spell_upgrade_clvl(new_lvl)) {
                                incr_spell_skills(SpellDomain::transmutation);
                        }
                } break;

                case OccultistDomain::END: {
                        ASSERT(false);
                } break;
                }
        } break;

        case Bg::rogue: {
        } break;

        case Bg::war_vet: {
        } break;

        case Bg::END: {
                ASSERT(false);
        } break;
        }

        TRACE_FUNC_END;
}

void set_all_traits_to_picked()
{
        for (size_t i = 0; i < (size_t)Trait::END; ++i) {
                s_traits_picked[i] = true;
        }
}

void pick_trait(const Trait id)
{
        TRACE_FUNC_BEGIN;

        ASSERT(id != Trait::END);

        s_traits_picked[(size_t)id] = true;

        TraitLogEntry trait_log_entry;

        trait_log_entry.trait_id = id;
        trait_log_entry.clvl = game::clvl();
        trait_log_entry.is_removal = false;

        s_trait_log.push_back(trait_log_entry);

        const TraitData& d = trait_data(id);

        if (d.on_picked) {
                // Has trait pick function
                trait_data(id).on_picked();
        }

        TRACE_FUNC_END;
}

void remove_trait(const Trait id)
{
        TRACE_FUNC_BEGIN;

        ASSERT(id != Trait::END);

        s_traits_picked[(size_t)id] = false;

        TraitLogEntry trait_log_entry;

        trait_log_entry.trait_id = id;
        trait_log_entry.clvl = game::clvl();
        trait_log_entry.is_removal = true;

        s_trait_log.push_back(trait_log_entry);

        const TraitData& d = trait_data(id);

        // If the trait applies effects when picked, it must also revert those
        ASSERT(!(d.on_picked && !d.on_removed));

        if (d.on_removed) {
                // Has trait removal function
                trait_data(id).on_removed();
        }

        TRACE_FUNC_END;
}

std::vector<TraitLogEntry> trait_log()
{
        return s_trait_log;
}

}  // namespace player_bon
