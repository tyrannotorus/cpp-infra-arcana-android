// =============================================================================
// Copyright 2011-2024 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "property_data.hpp"

#include <algorithm>
#include <unordered_map>

#include "panel.hpp"
#include "property.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static const std::unordered_map<std::string, prop::Id> s_str_to_prop_id_map = {
        {"PROP_ALTERS_ENVIRONMENT", prop::Id::alters_env},
        {"PROP_AURA OF DECAY", prop::Id::aura_of_decay},
        {"PROP_BLESSED", prop::Id::blessed},
        {"PROP_BLIND", prop::Id::blind},
        {"PROP_BREEDS", prop::Id::breeds},
        {"PROP_VOMITS_OOZE", prop::Id::vomits_ooze},
        {"PROP_BURNING", prop::Id::burning},
        {"PROP_BURROWING", prop::Id::burrowing},
        {"PROP_CLOAKED", prop::Id::cloaked},
        {"PROP_CONFLICT", prop::Id::conflict},
        {"PROP_CONFUSED", prop::Id::confused},
        {"PROP_CONFUSES_ADJACENT", prop::Id::confuses_adjacent},
        {"PROP_FRENZY_PLAYER_ON_SEEN", prop::Id::frenzy_player_on_seen},
        {"PROP_CORPSE_EATER", prop::Id::corpse_eater},
        {"PROP_CORPSE_RISES", prop::Id::corpse_rises},
        {"PROP_CORRUPTS_ENVIRONMENT_COLOR", prop::Id::corrupts_env_color},
        {"PROP_CURSED", prop::Id::cursed},
        {"PROP_DARKVISION", prop::Id::darkvision},
        {"PROP_DEAF", prop::Id::deaf},
        {"PROP_DISEASED", prop::Id::diseased},
        {"PROP_ENTANGLED", prop::Id::entangled},
        {"PROP_STUCK", prop::Id::stuck},
        {"PROP_ETHEREAL", prop::Id::ethereal},
        {"PROP_EXPLODES_ON_DEATH", prop::Id::explodes_on_death},
        {"PROP_FAINTED", prop::Id::fainted},
        {"PROP_FLYING", prop::Id::flying},
        {"PROP_TINY_FLYING", prop::Id::tiny_flying},
        {"PROP_FRENZIED", prop::Id::frenzied},
        {"PROP_HASTED", prop::Id::hasted},
        {"PROP_INFECTED", prop::Id::infected},
        {"PROP_INVIS", prop::Id::invis},
        {"PROP_LIGHT_SENSITIVE", prop::Id::light_sensitive},
        {"PROP_MAGIC_SEARCHING", prop::Id::magic_searching},
        {"PROP_MAJOR_CLAPHAM_SUMMON", prop::Id::major_clapham_summon},
        {"PROP_ALLIES_GHOUL_PLAYER", prop::Id::allies_ghoul_player},
        {"PROP_ACTOR_SPECTRAL_WPN", prop::Id::spectral_wpn},
        {"PROP_OOZE", prop::Id::ooze},
        {"PROP_PARALYZED", prop::Id::paralyzed},
        {"PROP_POISONED", prop::Id::poisoned},
        {"PROP_HALLUCINATING", prop::Id::hallucinating},
        {"PROP_PREMONITION", prop::Id::premonition},
        {"PROP_ERUDITION", prop::Id::erudition},
        {"PROP_R_ACID", prop::Id::r_acid},
        {"PROP_R_BLIND", prop::Id::r_blind},
        {"PROP_R_BREATH", prop::Id::r_breath},
        {"PROP_R_CONF", prop::Id::r_conf},
        {"PROP_R_DISEASE", prop::Id::r_disease},
        {"PROP_R_ELEC", prop::Id::r_elec},
        {"PROP_R_FEAR", prop::Id::r_fear},
        {"PROP_R_FIRE", prop::Id::r_fire},
        {"PROP_R_PARA", prop::Id::r_para},
        {"PROP_R_PHYS", prop::Id::r_phys},
        {"PROP_R_POISON", prop::Id::r_poison},
        {"PROP_R_SLEEP", prop::Id::r_sleep},
        {"PROP_R_SLOW", prop::Id::r_slow},
        {"PROP_R_SPELL", prop::Id::r_spell},
        {"PROP_R_SHOCK", prop::Id::r_shock},
        {"PROP_RADIANT_SELF", prop::Id::radiant_self},
        {"PROP_RADIANT_ADJACENT", prop::Id::radiant_adjacent},
        {"PROP_RADIANT_FOV", prop::Id::radiant_fov},
        {"PROP_RECLOAKS", prop::Id::recloaks},
        {"PROP_REDUCED_PIERCE_DMG", prop::Id::reduced_pierce_dmg},
        {"PROP_REGENERATING", prop::Id::regenerating},
        {"PROP_SEE_INVIS", prop::Id::see_invis},
        {"PROP_SHORT_HEARING_RANGE", prop::Id::short_hearing_range},
        {"PROP_FRENZIES_SELF", prop::Id::frenzies_self},
        {"PROP_FRENZIES_FOLLOWERS", prop::Id::frenzies_followers},
        {"PROP_SUMMONS_LOCUSTS", prop::Id::summons_locusts},
        {"PROP_OTHERS_TERRIFIED_ON_DEATH", prop::Id::others_terrified_on_death},
        {"PROP_SLOWED", prop::Id::slowed},
        {"PROP_SMALL_CRAWLING", prop::Id::small_crawling},
        {"PROP_SPAWNS_ZOMBIE_PARTS_ON_DESTROYED", prop::Id::spawns_zombie_parts_on_destroyed},
        {"PROP_SPELL_REFLECT", prop::Id::spell_reflect},
        {"PROP_SPLITS_ON_DEATH", prop::Id::splits_on_death},
        {"PROP_STUNNED", prop::Id::stunned},
        {"PROP_TELE_CTRL", prop::Id::tele_ctrl},
        {"PROP_TELEPORTS", prop::Id::teleports},
        {"PROP_TELEPORTS_AWAY", prop::Id::teleports_away},
        {"PROP_ALWAYS_AWARE", prop::Id::always_aware},
        {"PROP_TERRIFIED", prop::Id::terrified},
        {"PROP_VORTEX", prop::Id::vortex},
        {"PROP_WEAKENED", prop::Id::weakened},
        {"PROP_ZUUL_POSSESS_PRIEST", prop::Id::zuul_possess_priest},
        {"PROP_SHAPESHIFTS", prop::Id::shapeshifts},
        {"PROP_ZEALOT_STOP", prop::Id::zealot_stop},
        {"PROP_MELEE_COOLDOWN", prop::Id::melee_cooldown},
};

static void add(prop::PropData& d)
{
#ifndef NDEBUG
        const std::string worst_case_str =
                d.name_short +
                prop::g_property_ending_suffix;

        const size_t worst_case_w = worst_case_str.length();

        const size_t panel_w = panels::w(Panel::map_gui_stats);

        if (worst_case_w > panel_w) {
                TRACE
                        << "The string '"
                        << worst_case_str
                        << "' of length '"
                        << worst_case_w
                        << "' will not fit in panel of width '"
                        << panel_w
                        << "'"
                        << std::endl;

                PANIC;
        }

#endif  // NDEBUG

        prop::g_data[(size_t)d.id] = d;

        d = {};
}

static void init_data_list()
{
        prop::PropData d;

        d.id = prop::Id::r_phys;
        d.std_rnd_turns = Range(50, 100);
        d.name = "Physical Resistance";
        d.name_short = "Phys Res";
        d.descr = "Cannot be harmed by physical attacks.";
        d.msg_start_player = "I feel impervious to physical attacks.";
        d.msg_start_mon = "looks tough as steel.";
        d.msg_end_player = "I feel vulnerable to physical attacks.";
        d.msg_end_mon = "looks less tough.";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::good;
        add(d);

        d.id = prop::Id::r_fire;
        d.std_rnd_turns = Range(50, 100);
        d.name = "Fire Resistance";
        d.name_short = "Fire Res";
        d.descr = "Cannot be harmed by fire.";
        d.msg_start_player = "I feel resistant to fire.";
        d.msg_start_mon = "is resistant to fire.";
        d.msg_end_player = "I feel vulnerable to fire.";
        d.msg_end_mon = "is vulnerable to fire.";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::good;
        add(d);

        d.id = prop::Id::r_poison;
        d.std_rnd_turns = Range(50, 100);
        d.name = "Poison Resistance";
        d.name_short = "Poison Res";
        d.descr = "Cannot be harmed by poison.";
        d.msg_start_player = "I feel resistant to poison.";
        d.msg_start_mon = "is resistant to poison.";
        d.msg_end_player = "I feel vulnerable to poison.";
        d.msg_end_mon = "is vulnerable to poison.";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::good;
        add(d);

        d.id = prop::Id::r_elec;
        d.std_rnd_turns = Range(50, 100);
        d.name = "Electric Resistance";
        d.name_short = "Elec Res";
        d.descr = "Cannot be harmed by electricity.";
        d.msg_start_player = "I feel resistant to electricity.";
        d.msg_start_mon = "is resistant to electricity.";
        d.msg_end_player = "I feel vulnerable to electricity.";
        d.msg_end_mon = "is vulnerable to electricity.";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::good;
        add(d);

        d.id = prop::Id::r_acid;
        d.std_rnd_turns = Range(50, 100);
        d.name = "Acid Resistance";
        d.name_short = "Acid Res";
        d.descr = "Cannot be harmed by acid.";
        d.msg_start_player = "I feel resistant to acid.";
        d.msg_start_mon = "is resistant to acid.";
        d.msg_end_player = "I feel vulnerable to acid.";
        d.msg_end_mon = "is vulnerable to acid.";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::good;
        add(d);

        d.id = prop::Id::r_sleep;
        d.std_rnd_turns = Range(50, 100);
        d.name = "Sleep Resistance";
        d.name_short = "Sleep Res";
        d.descr = "Cannot faint or become hypnotized.";
        d.msg_start_player = "I feel wide awake.";
        d.msg_start_mon = "is wide awake.";
        d.msg_end_player = "I feel less awake.";
        d.msg_end_mon = "is less awake.";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::good;
        add(d);

        d.id = prop::Id::r_fear;
        d.std_rnd_turns = Range(50, 100);
        d.name = "Fear Resistance";
        d.name_short = "Fear Res";
        d.descr = "Unaffected by fear.";
        d.msg_start_player = "I cannot be swayed by fear.";
        d.msg_start_mon = "is resistant to fear.";
        d.msg_end_player = "I feel vulnerable to fear.";
        d.msg_end_mon = "is vulnerable to fear.";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::good;
        add(d);

        d.id = prop::Id::r_slow;
        d.std_rnd_turns = Range(50, 100);
        d.name = "Slow Resistance";
        d.name_short = "Slow Res";
        d.descr = "Cannot be magically slowed.";
        d.msg_start_player = "I feel steadfast.";
        d.msg_end_player = "I feel more susceptible to time.";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::good;
        add(d);

        d.id = prop::Id::r_conf;
        d.std_rnd_turns = Range(50, 100);
        d.name = "Confusion Resistance";
        d.name_short = "Conf Res";
        d.descr = "Cannot become confused.";
        d.msg_start_player = "I feel resistant to confusion.";
        d.msg_start_mon = "is resistant to confusion.";
        d.msg_end_player = "I feel vulnerable to confusion.";
        d.msg_end_mon = "is vulnerable to confusion.";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::good;
        add(d);

        d.id = prop::Id::r_disease;
        d.std_rnd_turns = Range(50, 100);
        d.name = "Disease Resistance";
        d.name_short = "Disease Res";
        d.descr = "Cannot become diseased.";
        d.msg_start_player = "I feel resistant to disease.";
        d.msg_start_mon = "is resistant to disease.";
        d.msg_end_player = "I feel vulnerable to disease.";
        d.msg_end_mon = "is vulnerable to disease.";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::good;
        add(d);

        d.id = prop::Id::r_blind;
        d.name = "Blindness Resistance";
        d.name_short = "Blind Res";
        d.descr = "Cannot be blinded.";
        d.allow_display_turns = false;
        d.allow_test_on_bot = false;
        d.alignment = prop::PropAlignment::good;
        add(d);

        d.id = prop::Id::r_para;
        d.name = "Paralysis Resistance";
        d.name_short = "Paralys Res";
        d.descr = "Cannot be paralyzed.";
        d.allow_display_turns = false;
        d.allow_test_on_bot = false;
        d.alignment = prop::PropAlignment::good;
        add(d);

        d.id = prop::Id::r_breath;
        d.std_rnd_turns = Range(50, 100);
        d.descr = "Cannot be harmed by constricted breathing.";
        d.msg_start_player = "I can breath without harm.";
        d.msg_start_mon = "can breath without harm.";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::good;
        add(d);

        d.id = prop::Id::r_spell;
        d.name = "Spell Resistance";
        d.name_short = "Spell Res";
        d.descr = "Cannot be affected by harmful spells.";
        d.msg_start_player = "I defy harmful spells!";
        d.msg_start_mon = "is defying harmful spells.";
        d.msg_end_player = "I feel vulnerable to spells.";
        d.msg_end_mon = "is vulnerable to spells.";
        d.allow_test_on_bot = false;
        d.alignment = prop::PropAlignment::good;
        add(d);

        d.id = prop::Id::r_shock;
        d.std_rnd_turns = Range(8, 12);
        d.name = "Shock Resistance";
        d.name_short = "Shock Res";
        d.descr = "Unaffected by shocking events.";
        d.msg_start_player = "Nothing can disturb my mind!";
        d.msg_start_mon = "";
        d.msg_end_player =
                "I feel susceptible to the horrors of this place again.";
        d.msg_end_mon = "";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::good;
        add(d);

        d.id = prop::Id::light_sensitive;
        d.std_rnd_turns = Range(30, 60);
        d.name = "Light Sensitive";
        d.name_short = "Lgt Sens";
        d.descr = "Is vulnerable to light.";
        d.msg_start_player = "I feel vulnerable to light!";
        d.msg_start_mon = "is vulnerable to light.";
        d.msg_end_player = "I no longer feel vulnerable to light.";
        d.msg_end_mon = "no longer is vulnerable to light.";
        d.allow_display_turns = true;
        d.alignment = prop::PropAlignment::bad;
        add(d);

        d.id = prop::Id::blind;
        d.std_rnd_turns = Range(6, 12);
        d.name = "Blind";
        d.name_short = "Blind";
        d.descr = "Cannot see, -20% hit chance, -50% chance to evade attacks.";
        d.msg_start_player = "I am blinded!";
        d.msg_start_mon = "is blinded.";
        d.msg_end_player = "I can see again!";
        d.msg_end_mon = "can see again.";
        d.historic_msg_start_permanent = "Became permanently blind";
        d.historic_msg_end_permanent = "My sight came back";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::bad;
        add(d);

        d.id = prop::Id::deaf;
        d.std_rnd_turns = Range(50, 100);
        d.name = "Deaf";
        d.name_short = "Deaf";
        d.descr = "Cannot hear sounds.";
        d.msg_start_player = "I am deaf!";
        d.msg_end_player = "I can hear again.";
        d.historic_msg_start_permanent = "Became permanently deaf";
        d.historic_msg_end_permanent = "My hearing came back";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::bad;
        add(d);

        d.id = prop::Id::fainted;
        d.std_rnd_turns = Range(30, 60);
        d.name = "Fainted";
        d.name_short = "Fainted";
        d.descr =
                "Temporarily lost consciousness, will wake up if any damage "
                "is taken or enough time passes.";
        d.msg_start_player = "I faint!";
        d.msg_start_mon = "faints.";
        d.msg_end_player = "I am awake.";
        d.msg_end_mon = "wakes up.";
        d.msg_res_player = "I resist fainting.";
        d.msg_res_mon = "resists fainting.";
        d.allow_display_turns = true;
        d.force_interrupt_player_on_start = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::bad;
        add(d);

        d.id = prop::Id::burning;
        d.std_rnd_turns = Range(6, 8);
        d.name = "Burning";
        d.name_short = "Burning";
        d.descr =
                "Takes damage each turn, 50% chance to fail when attempting to "
                "read or cast spells.";
        d.msg_start_player = "I am Burning!";
        d.msg_start_mon = "is burning.";
        d.msg_end_player = "The flames are put out.";
        d.msg_end_mon = "is no longer burning.";
        d.msg_res_player = "I resist burning.";
        d.msg_res_mon = "resists burning.";
        d.allow_display_turns = true;
        d.force_interrupt_player_on_start = true;
        d.update_vision_on_toggled = true;
        d.is_preventable_by_player_trait = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::bad;
        add(d);

        d.id = prop::Id::poisoned;
        d.std_rnd_turns = Range(40, 80);
        d.name = "Poisoned";
        d.name_short = "Poisoned";
        d.descr = "Takes damage each turn.";
        d.msg_start_player = "I am poisoned!";
        d.msg_start_mon = "is poisoned.";
        d.msg_end_player = "My body is cleansed from poisoning!";
        d.msg_end_mon = "is cleansed from poisoning.";
        d.msg_res_player = "I resist poisoning.";
        d.msg_res_mon = "resists poisoning.";
        d.allow_display_turns = true;
        d.is_preventable_by_player_trait = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::bad;
        add(d);

        d.id = prop::Id::paralyzed;
        d.std_rnd_turns = Range(3, 5);
        d.name = "Paralyzed";
        d.name_short = "Paralyzed";
        d.descr = "Cannot move.";
        d.msg_start_player = "I am paralyzed!";
        d.msg_start_mon = "is paralyzed.";
        d.msg_end_player = "I can move again!";
        d.msg_end_mon = "can move again.";
        d.msg_res_player = "I resist paralyzation.";
        d.msg_res_mon = "resists paralyzation.";
        d.allow_display_turns = true;
        d.force_interrupt_player_on_start = true;
        d.is_preventable_by_player_trait = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::bad;
        add(d);

        d.id = prop::Id::delayed_by_liquid;
        d.std_rnd_turns = Range(1, 1);
        d.allow_display_turns = false;
        d.alignment = prop::PropAlignment::neutral;
        add(d);

        d.id = prop::Id::terrified;
        d.std_rnd_turns = Range(20, 30);
        d.name = "Terrified";
        d.name_short = "Terrified";
        d.descr =
                "Cannot perform melee attacks, -20% ranged hit chance, +20% "
                "chance to evade attacks.";
        d.msg_start_player = "I am terrified!";
        d.msg_start_mon = "looks terrified.";
        d.msg_end_player = "I am no longer terrified!";
        d.msg_end_mon = "is no longer terrified.";
        d.msg_res_player = "I resist fear.";
        d.msg_res_mon = "resists fear.";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::bad;
        add(d);

        d.id = prop::Id::confused;
        d.std_rnd_turns = Range(30, 60);
        d.name = "Confused";
        d.name_short = "Confused";
        d.descr =
                "Occasionally moving in random directions, cannot read or "
                "cast spells, cannot search for hidden doors or traps.";
        d.msg_start_player = "I am confused!";
        d.msg_start_mon = "looks confused.";
        d.msg_end_player = "I come to my senses.";
        d.msg_end_mon = "is no longer confused.";
        d.msg_res_player = "I manage to keep my head together.";
        d.msg_res_mon = "resists confusion.";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::bad;
        add(d);

        d.id = prop::Id::hallucinating;
        d.std_rnd_turns = Range(75, 150);
        d.name = "Hallucinating";
        d.name_short = "Halluc";
        d.descr = "The senses cannot always be trusted.";
        d.msg_start_player = "I am starting to doubt my senses.";
        d.msg_start_mon = "";
        d.msg_end_player = "I feel more sure of my senses.";
        d.msg_end_mon = "";
        d.msg_res_player = "I manage to maintain a grip on what is real.";
        d.msg_res_mon = "";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::bad;
        add(d);

        d.id = prop::Id::astral_opium_addiction;
        d.std_rnd_turns = Range(100, 200);
        d.std_rnd_dlvls = Range(3, 6);
        d.name = "Astral Opium Addiction";
        d.name_short = "Addict";
        d.descr =
                "Addicted to Astral Opium - the addiction will cease "
                "eventually if Astral Opium is not used again, however the "
                "abstinence will soon cause withdrawal symptoms "
                "(increased minimum shock). "
                "The addiction is too powerful and otherwordly to be cured by a "
                "Potion of Fortitude.";
        d.msg_start_player = "That felt amazing!";
        d.msg_start_mon = "";
        d.msg_end_player =
                "I suddenly realize that I no longer crave Astral Opium.";
        d.msg_end_mon = "";
        d.msg_res_player = "";
        d.msg_res_mon = "";
        d.allow_display_turns = false;
        d.allow_test_on_bot = false;
        d.alignment = prop::PropAlignment::bad;
        add(d);

        d.id = prop::Id::meditative_focused;
        d.name = "Focused";
        d.name_short = "Focused";
        d.descr =
                "The next spell is cast without spending a turn, "
                "and the cost is reduced by 1 point.";
        d.msg_start_player = "I feel very focused.";
        d.msg_end_player = "I feel less focused.";
        d.allow_test_on_bot = false;
        d.alignment = prop::PropAlignment::good;
        add(d);

        d.id = prop::Id::thorns;
        d.name = "Thorns";
        d.name_short = "Thorns";
        d.descr =
                "Any time the caster is harmed by a melee attack, "
                "ranged attack or damaging spell, the attacker is "
                "struck by an irresistible force.";
        d.msg_start_player = "Reprisal is sealed!";
        d.msg_end_player = "I cease to return harm.";
        d.allow_test_on_bot = false;
        d.alignment = prop::PropAlignment::good;
        add(d);

        d.id = prop::Id::crimson_passage;
        d.name = "Crimson Passage";
        d.name_short = "Crims Psg";
        d.descr =
                "Walking does not spend any time, "
                "but every step taken drains 2 hit points "
                "(waiting in place can still be done as normal). "
                "The effect ends after a certain number of steps have "
                "been taken, "
                "or if there is not enough hit points to drain.";
        d.msg_start_player = "A gruesome path lies ahead.";
        d.msg_end_player = "My movement is normal again.";
        d.allow_test_on_bot = false;
        d.alignment = prop::PropAlignment::good;
        add(d);

        d.id = prop::Id::stunned;
        d.std_rnd_turns = Range(5, 9);
        d.name = "Stunned";
        d.name_short = "Stunned";
        d.msg_start_player = "I am stunned!";
        d.msg_start_mon = "is stunned.";
        d.msg_end_player = "I am no longer stunned.";
        d.msg_end_mon = "is no longer stunned.";
        d.msg_res_player = "I resist stunning.";
        d.msg_res_mon = "resists stunning.";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::bad;
        add(d);

        d.id = prop::Id::slowed;
        d.std_rnd_turns = Range(16, 24);
        d.name = "Slowed";
        d.name_short = "Slowed";
        d.descr = "Moves slower.";
        d.msg_start_player = "Everything around me seems to speed up.";
        d.msg_start_mon = "slows down.";
        d.msg_end_player = "Everything around me seems to slow down.";
        d.msg_end_mon = "speeds up.";
        d.msg_res_player = "I resist slowing.";
        d.msg_res_mon = "resists slowing.";
        d.historic_msg_start_permanent = "Became perpetually slowed";
        d.historic_msg_end_permanent = "My slowness ceased";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::bad;
        add(d);

        d.id = prop::Id::hasted;
        d.std_rnd_turns = Range(12, 16);
        d.name = "Hasted";
        d.name_short = "Hasted";
        d.descr = "Moves faster.";
        d.msg_start_player = "Everything around me seems to slow down.";
        d.msg_start_mon = "speeds up.";
        d.msg_end_player = "Everything around me seems to speed up.";
        d.msg_end_mon = "slows down.";
        d.historic_msg_start_permanent = "Became perpetually hasted";
        d.historic_msg_end_permanent = "My hastiness ceased";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::good;
        add(d);

        d.id = prop::Id::extra_hasted;
        d.std_rnd_turns = Range(7, 11);
        d.name = "Extra Hasted";
        d.name_short = "Extra Hasted";
        d.descr = "Moves very fast.";
        d.msg_start_player = "Everything around me suddenly seems very still.";
        d.msg_end_player = "Everything around me seems to speed up a lot.";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::good;
        add(d);

        d.id = prop::Id::summoned;
        d.std_rnd_turns = Range(80, 120);
        d.msg_end_mon = "suddenly disappears.";
        d.name = "Summoned";
        d.descr = "Was magically summoned here.";
        d.alignment = prop::PropAlignment::neutral;
        add(d);

        d.id = prop::Id::nailed;
        d.name = "Nailed";
        d.descr =
                "Fastened by a spike. Tearing it out will be rather painful.";
        d.msg_start_player = "I am fastened by a spike!";
        d.msg_start_mon = "is fastened by a spike.";
        d.msg_end_player = "I tear free!";
        d.msg_end_mon = "tears free!";
        d.allow_display_turns = false;
        d.alignment = prop::PropAlignment::bad;
        add(d);

        d.id = prop::Id::wound;
        d.name = "Wounded";
        d.descr =
                "For each wound: "
                "-5% melee hit chance, "
                "-5% chance to evade attacks, "
                "-10% hit points, "
                "and reduced hit point generation rate. "
                "With three concurrent wounds, walking takes extra turns. "
                "Five concurrent wounds is fatal.";
        d.msg_start_player = "I am wounded!";
        d.msg_res_player = "I resist wounding!";
        d.allow_display_turns = false;
        d.allow_test_on_bot = false;
        d.alignment = prop::PropAlignment::bad;
        add(d);

        d.id = prop::Id::hp_sap;
        d.name = "Life Sapped";
        d.descr = "Fewer hit points.";
        d.msg_start_player = "My life force is sapped!";
        d.msg_start_mon = "is sapped of life.";
        d.msg_end_player = "My life force returns.";
        d.msg_end_mon = "looks restored.";
        d.msg_res_player = "I resist sapping.";
        d.msg_res_mon = "resists sapping.";
        d.allow_display_turns = false;
        d.allow_test_on_bot = false;
        d.alignment = prop::PropAlignment::bad;
        add(d);

        d.id = prop::Id::spi_sap;
        d.name = "Spirit Sapped";
        d.descr = "Fewer spirit points.";
        d.msg_start_player = "My spirit is sapped!";
        d.msg_start_mon = "is sapped of spirit.";
        d.msg_end_player = "My spirit returns.";
        d.msg_end_mon = "looks restored.";
        d.msg_res_player = "I resist sapping.";
        d.msg_res_mon = "resists sapping.";
        d.allow_display_turns = false;
        d.allow_test_on_bot = false;
        d.alignment = prop::PropAlignment::bad;
        add(d);

        d.id = prop::Id::mind_sap;
        d.name = "Mind Sapped";
        d.descr = "Increased Shock.";
        d.msg_start_player = "My mind is sapped!";
        d.msg_end_player = "My mind returns.";
        d.msg_res_player = "I resist sapping.";
        d.allow_display_turns = false;
        d.allow_test_on_bot = false;
        d.alignment = prop::PropAlignment::bad;
        add(d);

        d.id = prop::Id::infected;
        d.std_rnd_turns = Range(50, 100);
        d.name = "Infected";
        d.name_short = "Infected";
        d.descr = "A nasty infection that will get worse if left untreated.";
        d.msg_start_player = "I am infected!";
        d.msg_start_mon = "is infected.";
        d.msg_end_player = "My infection is cured!";
        d.msg_end_mon = "is no longer infected.";
        d.allow_display_turns = false;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::bad;
        add(d);

        d.id = prop::Id::diseased;
        d.std_rnd_turns = Range(30, 60);
        d.name = "Diseased";
        d.name_short = "Diseased";
        d.descr = "-50% maximum hit points.";
        d.msg_start_player = "I am diseased!";
        d.msg_start_mon = "is diseased.";
        d.msg_end_player = "My disease is cured!";
        d.msg_end_mon = "is no longer diseased.";
        d.msg_res_player = "I resist disease.";
        d.msg_res_mon = "resists disease.";
        d.historic_msg_start_permanent = "Caught a horrible disease";
        d.historic_msg_end_permanent = "Was cured from a horrible disease";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::bad;
        add(d);

        d.id = prop::Id::descend;
        d.std_rnd_turns = Range(20, 30);
        d.name = "Descending";
        d.name_short = "Descending";
        d.descr = "Soon moved to a deeper level.";
        d.msg_start_player = "I feel a sinking sensation.";
        d.allow_display_turns = true;
        d.allow_test_on_bot = false;
        d.alignment = prop::PropAlignment::neutral;
        add(d);

        d.id = prop::Id::weakened;
        d.std_rnd_turns = Range(30, 60);
        d.name = "Weakened";
        d.name_short = "Weakened";
        d.descr =
                "Halved melee damage, cannot bash doors or chests open, knock "
                "heavy objects over, etc.";
        d.msg_start_player = "I feel weaker.";
        d.msg_start_mon = "looks weaker.";
        d.msg_end_player = "I feel stronger!";
        d.msg_end_mon = "looks stronger!";
        d.msg_res_player = "I resist weakness.";
        d.msg_res_mon = "resists weakness.";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::bad;
        add(d);

        d.id = prop::Id::frenzied;
        d.std_rnd_turns = Range(30, 60);
        d.name = "Frenzied";
        d.name_short = "Frenzied";
        d.descr =
                "Cannot move away from seen enemies, moves faster, +1 melee "
                "damage, +10% melee hit chance, immune to confusion, fainting, "
                "fear, and weakening, cannot read or cast spells, becomes "
                "weakened when the frenzy ends.";
        d.msg_start_player = "I feel ferocious!!!";
        d.msg_start_mon = "looks ferocious!";
        d.msg_end_player = "I calm down.";
        d.msg_end_mon = "calms down a little.";
        d.allow_display_turns = true;
        d.force_interrupt_player_on_start = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::neutral;
        add(d);

        d.id = prop::Id::blessed;
        d.std_rnd_turns = Range(400, 600);
        d.name = "Blessed";
        d.name_short = "Blessed";
        d.descr = "+10% to hit chance, evasion, stealth, and searching.";
        d.msg_start_player = "I feel luckier.";
        d.msg_end_player = "I have normal luck.";
        d.historic_msg_start_permanent = "Received an everlasting blessing";
        d.historic_msg_end_permanent = "My great blessing ceased";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::good;
        add(d);

        d.id = prop::Id::cursed;
        d.std_rnd_turns = Range(30, 60);
        d.name = "Cursed";
        d.name_short = "Cursed";
        d.descr = "-10% to hit chance, evasion, stealth, and searching.";
        d.msg_start_player = "I feel misfortunate.";
        d.msg_start_mon = "is cursed.";
        d.msg_end_player = "I feel more fortunate.";
        d.msg_end_mon = "is no longer cursed.";
        d.msg_res_player = "I resist misfortune.";
        d.msg_res_mon = "resists misfortune.";
        d.historic_msg_start_permanent = "A perpetual curse was put upon me";
        d.historic_msg_end_permanent = "A terrible curse was lifted from me";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::bad;
        add(d);

        d.id = prop::Id::doomed;
        d.std_rnd_turns = Range(30, 60);
        d.name = "Doomed";
        d.name_short = "Doomed";
        d.descr =
                "-20% to hit chance, evasion, stealth, and searching, "
                "10% chance to fail when attempting to read or cast spells.";
        d.msg_start_player = "I feel doomed!";
        d.msg_start_mon = "is doomed!";
        d.msg_end_player = "My doom does not feel so certain anymore.";
        d.msg_end_mon = "is no longer doomed.";
        d.msg_res_player = "I resist a great misfortune.";
        d.historic_msg_start_permanent = "My doom was written";
        d.historic_msg_end_permanent = "Hope returned again";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::bad;
        add(d);

        d.id = prop::Id::premonition;
        d.std_rnd_turns = Range(5, 9);
        d.name = "Premonition";
        d.name_short = "Premonition";
        d.descr = "+75% chance to evade attacks.";
        d.msg_start_player = "I feel unassailable.";
        d.msg_end_player = "I feel more vulnerable.";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::good;
        add(d);

        d.id = prop::Id::erudition;
        d.name = "Erudition";
        d.name_short = "Erudition";
        d.descr = "Spell skill is improved by one level.";
        d.msg_start_player = "Mystic secrets are revealed to me!";
        d.msg_end_player = "I feel ignorant.";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::good;
        add(d);

        d.id = prop::Id::magic_searching;
        d.std_rnd_turns = Range(20, 60);
        d.name = "Magic Searching";
        d.name_short = "Magic Search";
        d.descr =
                "Magically detects objects and creatures in the surrounding "
                "area.";
        d.msg_start_player = "Hidden secrets are revealed to me.";
        d.msg_end_player = "I can no longer see hidden things.";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::good;
        add(d);

        d.id = prop::Id::entangled;
        d.name = "Entangled";
        d.name_short = "Entangled";
        d.descr = "Entangled in something.";
        d.msg_start_player = "I am entangled!";
        d.msg_start_mon = "is entangled.";
        d.msg_end_player = "I tear free!";
        d.msg_end_mon = "tears free!";
        d.allow_display_turns = false;
        d.force_interrupt_player_on_start = true;
        d.alignment = prop::PropAlignment::bad;
        add(d);

        d.id = prop::Id::stuck;
        d.name = "Stuck";
        d.name_short = "Stuck";
        d.descr = "Stuck in something.";
        d.msg_start_player = "I am stuck!";
        d.msg_start_mon = "is stuck.";
        d.msg_end_player = "I pull myself free!";
        d.msg_end_mon = "pulls free!";
        d.allow_display_turns = false;
        d.force_interrupt_player_on_start = true;
        d.alignment = prop::PropAlignment::bad;
        add(d);

        d.id = prop::Id::radiant_self;
        d.std_rnd_turns = Range(50, 100);
        d.allow_display_turns = true;
        d.update_vision_on_toggled = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::neutral;
        add(d);

        d.id = prop::Id::radiant_adjacent;
        d.std_rnd_turns = Range(50, 100);
        d.allow_display_turns = true;
        d.update_vision_on_toggled = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::neutral;
        add(d);

        d.id = prop::Id::radiant_fov;
        d.std_rnd_turns = Range(50, 100);
        d.name = "Radiant";
        d.name_short = "Radiant";
        d.descr = "Emanating a bright light.";
        d.msg_start_player = "A bright light shines around me.";
        d.msg_end_player = "It suddenly gets darker.";
        d.allow_display_turns = true;
        d.update_vision_on_toggled = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::neutral;
        add(d);

        d.id = prop::Id::invis;
        d.std_rnd_turns = Range(50, 100);
        d.name = "Invisible";
        d.name_short = "Invisible";
        d.descr = "Cannot be detected by normal sight.";
        d.msg_start_player = "I am out of sight!";
        d.msg_start_mon = "is out of sight!";
        d.msg_end_player = "I am visible.";
        d.msg_end_mon = "";
        d.allow_display_turns = true;
        d.update_vision_on_toggled = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::good;
        add(d);

        d.id = prop::Id::cloaked;
        d.std_rnd_turns = Range(5, 7);
        d.name = "Cloaked";
        d.name_short = "Cloaked";
        d.descr =
                "Cannot be detected by normal sight, ends if attacking or "
                "casting spells.";
        d.msg_start_player = "I am out of sight!";
        d.msg_start_mon = "is out of sight!";
        d.msg_end_player = "I am visible.";
        d.msg_end_mon = "";
        d.allow_display_turns = true;
        d.update_vision_on_toggled = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::good;
        add(d);

        d.id = prop::Id::recloaks;
        add(d);

        d.id = prop::Id::see_invis;
        d.std_rnd_turns = Range(50, 100);
        d.name = "See Invisible";
        d.name_short = "See Invis";
        d.descr = "Can see invisible creatures, cannot be blinded.";
        d.msg_start_player = "My eyes perceive the invisible.";
        d.msg_start_mon = "seems very keen.";
        d.msg_end_player = "My eyes can no longer perceive the invisible.";
        d.msg_end_mon = "seems less keen.";
        d.allow_display_turns = true;
        d.update_vision_on_toggled = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::good;
        add(d);

        d.id = prop::Id::darkvision;
        d.std_rnd_turns = Range(50, 100);
        d.allow_display_turns = true;
        d.update_vision_on_toggled = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::good;
        add(d);

        d.id = prop::Id::tele_ctrl;
        d.std_rnd_turns = Range(50, 100);
        d.name = "Teleport control";
        d.name_short = "Tele Ctrl";
        d.descr = "Can control teleport destination.";
        d.msg_start_player = "I feel in control.";
        d.msg_end_player = "I feel less in control.";
        d.allow_display_turns = true;
        d.allow_test_on_bot = false;
        d.alignment = prop::PropAlignment::good;
        add(d);

        d.id = prop::Id::spell_reflect;
        d.std_rnd_turns = Range(50, 100);
        d.allow_display_turns = false;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::good;
        add(d);

        d.id = prop::Id::aiming;
        d.std_rnd_turns = Range(1, 1);
        d.name = "Aiming";
        d.name_short = "Aiming";
        d.descr = "Increased range attack effectiveness.";
        d.allow_display_turns = false;
        d.alignment = prop::PropAlignment::good;
        add(d);

        d.id = prop::Id::conflict;
        d.name = "Conflicted";
        d.name_short = "Conflicted";
        d.descr = "Considers every creature as an enemy.";
        d.std_rnd_turns = Range(10, 20);
        d.msg_start_mon = "Looks conflicted.";
        d.msg_end_mon = "Looks more determined.";
        d.allow_display_turns = false;
        d.alignment = prop::PropAlignment::bad;
        add(d);

        d.id = prop::Id::aura_of_decay;
        d.std_rnd_turns = Range(6, 12);
        d.name = "Aura of Decay";
        d.name_short = "Decay Aura";
        d.descr = "Creatures within a distance of two moves take damage each standard turn.";
        d.msg_start_player = "Withering surrounds me.";
        d.msg_start_mon = "appears to exude death and decay.";
        d.msg_end_player = "The decay subsides.";
        d.msg_end_mon = "no longer exudes decay.";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::good;
        add(d);

        d.id = prop::Id::reduced_pierce_dmg;
        add(d);

        d.id = prop::Id::vortex;
        add(d);

        d.id = prop::Id::explodes_on_death;
        add(d);

        d.id = prop::Id::splits_on_death;
        add(d);

        d.id = prop::Id::corpse_eater;
        add(d);

        d.id = prop::Id::teleports;
        add(d);

        d.id = prop::Id::teleports_away;
        add(d);

        d.id = prop::Id::always_aware;
        add(d);

        d.id = prop::Id::corrupts_env_color;
        add(d);

        d.id = prop::Id::alters_env;
        add(d);

        d.id = prop::Id::regenerating;
        d.std_rnd_turns = Range(50, 100);
        d.name = "Regenerating";
        d.name_short = "Regenerating";
        d.descr = "+1 extra hit point regenerated per turn.";
        d.msg_start_player = "My body starts healing itself much faster.";
        d.msg_start_mon = "starts regenerating damage very quickly.";
        d.msg_end_player = "My body heals itself slower now.";
        d.msg_end_mon = "stops regenerating damage quickly.";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::good;
        add(d);

        d.id = prop::Id::corpse_rises;
        add(d);

        d.id = prop::Id::breeds;
        add(d);

        d.id = prop::Id::frenzies_self;
        add(d);

        d.id = prop::Id::frenzies_followers;
        add(d);

        d.id = prop::Id::others_terrified_on_death;
        add(d);

        d.id = prop::Id::summons_locusts;
        add(d);

        d.id = prop::Id::vomits_ooze;
        add(d);

        d.id = prop::Id::confuses_adjacent;
        add(d);

        d.id = prop::Id::frenzy_player_on_seen;
        add(d);

        d.id = prop::Id::zuul_possess_priest;
        add(d);

        d.id = prop::Id::possessed_by_zuul;
        add(d);

        d.id = prop::Id::shapeshifts;
        add(d);

        d.id = prop::Id::zealot_stop;
        add(d);

        d.id = prop::Id::major_clapham_summon;
        add(d);

        d.id = prop::Id::allies_ghoul_player;
        add(d);

        d.id = prop::Id::flying;
        d.name = "";
        d.name_short = "";
        d.descr = "";
        d.allow_display_turns = false;
        d.allow_test_on_bot = false;
        d.alignment = prop::PropAlignment::neutral;
        add(d);

        d.id = prop::Id::tiny_flying;
        d.name = "";
        d.name_short = "";
        d.descr = "";
        d.allow_display_turns = false;
        d.allow_test_on_bot = false;
        d.alignment = prop::PropAlignment::neutral;
        add(d);

        d.id = prop::Id::ethereal;
        d.name = "Ethereal";
        d.name_short = "Ethereal";
        d.descr =
                "Can pass through solid objects, "
                "+50% chance to evade attacks.";
        d.allow_display_turns = false;
        d.allow_test_on_bot = false;
        d.alignment = prop::PropAlignment::neutral;
        add(d);

        d.id = prop::Id::ooze;
        d.allow_test_on_bot = false;
        d.alignment = prop::PropAlignment::neutral;
        add(d);

        d.id = prop::Id::small_crawling;
        d.allow_test_on_bot = false;
        d.alignment = prop::PropAlignment::neutral;
        add(d);

        d.id = prop::Id::burrowing;
        d.allow_display_turns = false;
        d.allow_test_on_bot = false;
        d.alignment = prop::PropAlignment::neutral;
        add(d);

        d.id = prop::Id::waiting;
        d.std_rnd_turns = Range(1, 1);
        d.allow_display_turns = false;
        d.alignment = prop::PropAlignment::neutral;
        add(d);

        d.id = prop::Id::disabled_attack;
        d.std_rnd_turns = Range(1, 1);
        d.allow_display_turns = false;
        d.alignment = prop::PropAlignment::neutral;
        add(d);

        d.id = prop::Id::disabled_melee;
        d.std_rnd_turns = Range(1, 1);
        d.allow_display_turns = false;
        d.alignment = prop::PropAlignment::neutral;
        add(d);

        d.id = prop::Id::disabled_ranged;
        d.std_rnd_turns = Range(1, 1);
        d.allow_display_turns = false;
        d.alignment = prop::PropAlignment::neutral;
        add(d);

        d.id = prop::Id::melee_cooldown;
        d.std_rnd_turns = Range(1, 1);
        d.allow_display_turns = false;
        d.alignment = prop::PropAlignment::neutral;
        add(d);

        d.id = prop::Id::hit_chance_penalty_curse;
        d.std_rnd_turns = Range(1, 1);
        d.name = "";
        d.name_short = "";
        d.descr = "";
        d.msg_start_player = "My aiming feels worse.";
        d.msg_end_player = "My aiming feels better.";
        d.allow_display_turns = false;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::bad;
        add(d);

        d.id = prop::Id::increased_shock_curse;
        d.std_rnd_turns = Range(1, 1);
        d.name = "";
        d.name_short = "";
        d.descr = "";
        d.msg_start_player = "I feel more anxious!";
        d.msg_end_player = "I feel less anxious.";
        d.allow_display_turns = false;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::bad;
        add(d);

        d.id = prop::Id::cannot_read_curse;
        d.std_rnd_turns = Range(1, 1);
        d.name = "";
        d.name_short = "";
        d.descr = "";
        d.msg_start_player = "I feel illiterate!";
        d.msg_end_player = "I can read again.";
        d.allow_display_turns = false;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::bad;
        add(d);

        // NOTE: This property reuses messages from 'light_sensitive', so order
        // is important here
        d.id = prop::Id::light_sensitive_curse;
        d.std_rnd_turns = Range(1, 1);
        d.name = "";
        d.name_short = "";
        d.descr = "";
        d.msg_start_player =
                prop::g_data[(size_t)prop::Id::light_sensitive]
                        .msg_start_player;
        d.msg_end_player =
                prop::g_data[(size_t)prop::Id::light_sensitive]
                        .msg_end_player;
        d.allow_display_turns = false;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::bad;
        add(d);

        d.id = prop::Id::disabled_hp_regen;
        d.std_rnd_turns = Range(1, 1);
        d.name = "";
        d.name_short = "";
        d.descr = "";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::bad;
        add(d);

        d.id = prop::Id::sanctuary;
        d.std_rnd_turns = Range(5, 7);
        d.name = "Sanctuary";
        d.name_short = "Sanctuary";
        d.descr =
                "Is ignored by all hostile creatures. The effect ends if "
                "moving or performing a melee or ranged attack.";
        d.msg_start_player = "I feel very secure.";
        d.msg_end_player = "I feel much less secure.";
        d.allow_display_turns = true;
        d.update_vision_on_toggled = false;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::good;
        add(d);

        d.id = prop::Id::flagellant;
        d.allow_display_turns = false;
        d.allow_test_on_bot = false;
        d.alignment = prop::PropAlignment::good;
        add(d);

        d.id = prop::Id::moribund;
        d.descr = "+3 melee damage, +30% melee hit chance, +3 armor points.";
        d.name = "Moribund";
        d.name_short = "Moribund";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::good;
        add(d);
}

// -----------------------------------------------------------------------------
// property_data
// -----------------------------------------------------------------------------
namespace prop
{
PropData g_data[(size_t)prop::Id::END];

void init()
{
        init_data_list();
}

prop::Id str_to_prop_id(const std::string& str)
{
        return s_str_to_prop_id_map.at(str);
}

std::string descr(prop::Id id)
{
        ASSERT(id != prop::Id::END);

        return g_data[(size_t)id].descr;
}

}  // namespace prop
