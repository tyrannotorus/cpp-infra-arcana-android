// =============================================================================
// Copyright 2011-2021 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef PLAYER_BON_HPP
#define PLAYER_BON_HPP

#include <string>
#include <vector>

namespace actor
{
struct ActorData;
}  // namespace actor

struct ColoredString;

enum class Trait
{
        // Common (except some traits can be blocked for certain backgrounds)
        adept_melee,
        expert_melee,
        master_melee,
        adept_marksman,
        expert_marksman,
        master_marksman,
        cool_headed,
        courageous,
        dexterous,
        lithe,
        crippling_strikes,
        fearless,
        stealthy,
        imperceptible,
        silent,
        vigilant,
        treasure_hunter,
        self_aware,
        healer,
        rapid_recoverer,
        survivalist,
        stout_spirit,
        strong_spirit,
        mighty_spirit,
        meditative,
        absorb,
        tough,
        rugged,
        thick_skinned,
        resistant,
        strong_backed,
        undead_bane,
        elec_incl,

        // Unique for Exorcist
        cast_bless_i,
        cast_bless_ii,
        cast_cleansing_fire_i,
        cast_cleansing_fire_ii,
        cast_heal_i,
        cast_heal_ii,
        cast_light_i,
        cast_light_ii,
        cast_sanctuary_i,
        cast_sanctuary_ii,
        cast_see_invisible_i,
        cast_see_invisible_ii,
        prolonged_life,

        // Unique for Ghoul
        ravenous,
        foul,
        toxic,
        indomitable_fury,

        // Unique for Rogue
        vicious,
        ruthless,

        // Unique for War veteran
        steady_aimer,

        END
};

enum class Bg
{
        exorcist,
        ghoul,
        occultist,
        rogue,
        war_vet,

        END
};

enum class OccultistDomain
{
        clairvoyant,
        enchanter,
        invoker,
        // summoner,
        transmuter,

        END
};

namespace player_bon
{
struct TraitLogEntry
{
        Trait trait_id {Trait::END};
        int clvl {0};
        bool is_removal {false};
};

struct UnpickedTraitsData
{
        std::vector<Trait> traits_can_be_picked;
        std::vector<Trait> traits_prereqs_not_met;
};

struct TraitPrereqData
{
        std::vector<Trait> traits;
        Bg bg;
};

void init();

void save();

void load();

std::vector<Bg> pickable_bgs();

std::vector<OccultistDomain> pickable_occultist_domains();

UnpickedTraitsData unpicked_traits(
        Bg bg,
        OccultistDomain occultist_domain);

TraitPrereqData trait_prereqs(
        Trait trait,
        Bg bg,
        OccultistDomain occultist_domain);

std::vector<Trait> traits_can_be_removed();

Bg bg();

OccultistDomain occultist_domain();

bool is_bg(Bg bg);

bool has_trait(Trait id);

std::string trait_title(Trait id);

std::string trait_descr(Trait id);

std::string bg_title(Bg id);

std::string spell_domain_title(OccultistDomain domain);

std::string occultist_profession_title(OccultistDomain domain);

// NOTE: The string vector returned is not formatted. Each line still needs to
// be formatted by the caller. The reason for using a vector instead of a string
// is to separate the text into paragraphs.
std::vector<ColoredString> bg_descr(Bg id);

std::string occultist_domain_descr(OccultistDomain domain);

std::vector<TraitLogEntry> trait_log();

void pick_trait(Trait id);

void remove_trait(Trait id);

void pick_bg(Bg bg);

void pick_occultist_domain(OccultistDomain domain);

void on_player_gained_lvl(int new_lvl);

void set_all_traits_to_picked();

}  // namespace player_bon

#endif  // PLAYER_BON_HPP
