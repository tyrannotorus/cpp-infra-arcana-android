// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "property_data.hpp"

#include <algorithm>
#include <unordered_map>

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static const std::unordered_map<std::string, PropId> s_str_to_prop_id_map = {
        {"alters_environment", PropId::alters_env},
        {"aura of decay", PropId::aura_of_decay},
        {"blessed", PropId::blessed},
        {"blind", PropId::blind},
        {"breeds", PropId::breeds},
        {"vomits_ooze", PropId::vomits_ooze},
        {"burning", PropId::burning},
        {"burrowing", PropId::burrowing},
        {"cloaked", PropId::cloaked},
        {"conflict", PropId::conflict},
        {"confused", PropId::confused},
        {"confuses_adjacent", PropId::confuses_adjacent},
        {"frenzy_player_on_seen", PropId::frenzy_player_on_seen},
        {"corpse_eater", PropId::corpse_eater},
        {"corpse_rises", PropId::corpse_rises},
        {"corrupts_environment_color", PropId::corrupts_env_color},
        {"cursed", PropId::cursed},
        {"darkvision", PropId::darkvision},
        {"deaf", PropId::deaf},
        {"diseased", PropId::diseased},
        {"entangled", PropId::entangled},
        {"ethereal", PropId::ethereal},
        {"explodes_on_death", PropId::explodes_on_death},
        {"fainted", PropId::fainted},
        {"flying", PropId::flying},
        {"frenzied", PropId::frenzied},
        {"hasted", PropId::hasted},
        {"infected", PropId::infected},
        {"invis", PropId::invis},
        {"light_sensitive", PropId::light_sensitive},
        {"magic_searching", PropId::magic_searching},
        {"major_clapham_summon", PropId::major_clapham_summon},
        {"ooze", PropId::ooze},
        {"paralyzed", PropId::paralyzed},
        {"poisoned", PropId::poisoned},
        {"hallucinating", PropId::hallucinating},
        {"premonition", PropId::premonition},
        {"erudition", PropId::erudition},
        {"r_acid", PropId::r_acid},
        {"r_blind", PropId::r_blind},
        {"r_breath", PropId::r_breath},
        {"r_conf", PropId::r_conf},
        {"r_disease", PropId::r_disease},
        {"r_elec", PropId::r_elec},
        {"r_fear", PropId::r_fear},
        {"r_fire", PropId::r_fire},
        {"r_para", PropId::r_para},
        {"r_phys", PropId::r_phys},
        {"r_poison", PropId::r_poison},
        {"r_sleep", PropId::r_sleep},
        {"r_slow", PropId::r_slow},
        {"r_spell", PropId::r_spell},
        {"r_shock", PropId::r_shock},
        {"radiant_adjacent", PropId::radiant_adjacent},
        {"radiant_fov", PropId::radiant_fov},
        {"recloaks", PropId::recloaks},
        {"reduced_pierce_dmg", PropId::reduced_pierce_dmg},
        {"regenerates", PropId::regenerates},
        {"see_invis", PropId::see_invis},
        {"short_hearing_range", PropId::short_hearing_range},
        {"slowed", PropId::slowed},
        {"small_crawling", PropId::small_crawling},
        {"spawns_zombie_parts_on_destroyed",
         PropId::spawns_zombie_parts_on_destroyed},
        {"spell_reflect", PropId::spell_reflect},
        {"splits_on_death", PropId::splits_on_death},
        {"stunned", PropId::stunned},
        {"tele_ctrl", PropId::tele_ctrl},
        {"teleports", PropId::teleports},
        {"teleports_away", PropId::teleports_away},
        {"always_aware", PropId::always_aware},
        {"terrified", PropId::terrified},
        {"vortex", PropId::vortex},
        {"weakened", PropId::weakened},
        {"zuul_possess_priest", PropId::zuul_possess_priest},
        {"shapeshifts", PropId::shapeshifts}};

static void add(PropData& d)
{
        property_data::g_data[(size_t)d.id] = d;

        d = {};
}

static void init_data_list()
{
        PropData d;

        d.id = PropId::r_phys;
        d.std_rnd_turns = Range(50, 100);
        d.name = "Physical Resistance";
        d.name_short = "Physical Res.";
        d.descr = "Cannot be harmed by plain physical force.";
        d.msg_start_player = "I feel resistant to physical harm.";
        d.msg_start_mon = "is resistant to physical harm.";
        d.msg_end_player = "I feel vulnerable to physical harm.";
        d.msg_end_mon = "is vulnerable to physical harm.";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = PropAlignment::good;
        add(d);

        d.id = PropId::r_fire;
        d.std_rnd_turns = Range(50, 100);
        d.name = "Fire Resistance";
        d.name_short = "Fire Res.";
        d.descr = "Cannot be harmed by fire.";
        d.msg_start_player = "I feel resistant to fire.";
        d.msg_start_mon = "is resistant to fire.";
        d.msg_end_player = "I feel vulnerable to fire.";
        d.msg_end_mon = "is vulnerable to fire.";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = PropAlignment::good;
        add(d);

        d.id = PropId::r_poison;
        d.std_rnd_turns = Range(50, 100);
        d.name = "Poison Resistance";
        d.name_short = "Poison Res.";
        d.descr = "Cannot be harmed by poison.";
        d.msg_start_player = "I feel resistant to poison.";
        d.msg_start_mon = "is resistant to poison.";
        d.msg_end_player = "I feel vulnerable to poison.";
        d.msg_end_mon = "is vulnerable to poison.";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = PropAlignment::good;
        add(d);

        d.id = PropId::r_elec;
        d.std_rnd_turns = Range(50, 100);
        d.name = "Electric Resistance";
        d.name_short = "Electric Res.";
        d.descr = "Cannot be harmed by electricity.";
        d.msg_start_player = "I feel resistant to electricity.";
        d.msg_start_mon = "is resistant to electricity.";
        d.msg_end_player = "I feel vulnerable to electricity.";
        d.msg_end_mon = "is vulnerable to electricity.";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = PropAlignment::good;
        add(d);

        d.id = PropId::r_acid;
        d.std_rnd_turns = Range(50, 100);
        d.name = "Acid Resistance";
        d.name_short = "Acid Res.";
        d.descr = "Cannot be harmed by acid.";
        d.msg_start_player = "I feel resistant to acid.";
        d.msg_start_mon = "is resistant to acid.";
        d.msg_end_player = "I feel vulnerable to acid.";
        d.msg_end_mon = "is vulnerable to acid.";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = PropAlignment::good;
        add(d);

        d.id = PropId::r_sleep;
        d.std_rnd_turns = Range(50, 100);
        d.name = "Sleep Resistance";
        d.name_short = "Sleep Res.";
        d.descr = "Cannot faint or become hypnotized.";
        d.msg_start_player = "I feel wide awake.";
        d.msg_start_mon = "is wide awake.";
        d.msg_end_player = "I feel less awake.";
        d.msg_end_mon = "is less awake.";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = PropAlignment::good;
        add(d);

        d.id = PropId::r_fear;
        d.std_rnd_turns = Range(50, 100);
        d.name = "Fear Resistance";
        d.name_short = "Fear Res.";
        d.descr = "Unaffected by fear.";
        d.msg_start_player = "I cannot be swayed by fear.";
        d.msg_start_mon = "is resistant to fear.";
        d.msg_end_player = "I feel vulnerable to fear.";
        d.msg_end_mon = "is vulnerable to fear.";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = PropAlignment::good;
        add(d);

        d.id = PropId::r_slow;
        d.std_rnd_turns = Range(50, 100);
        d.name = "Slow Resistance";
        d.name_short = "Slow Res.";
        d.descr = "Cannot be magically slowed.";
        d.msg_start_player = "I feel steadfast.";
        d.msg_end_player = "I feel more susceptible to time.";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = PropAlignment::good;
        add(d);

        d.id = PropId::r_conf;
        d.std_rnd_turns = Range(50, 100);
        d.name = "Confusion Resistance";
        d.name_short = "Confusion Res.";
        d.descr = "Cannot become confused.";
        d.msg_start_player = "I feel resistant to confusion.";
        d.msg_start_mon = "is resistant to confusion.";
        d.msg_end_player = "I feel vulnerable to confusion.";
        d.msg_end_mon = "is vulnerable to confusion.";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = PropAlignment::good;
        add(d);

        d.id = PropId::r_disease;
        d.std_rnd_turns = Range(50, 100);
        d.name = "Disease Resistance";
        d.name_short = "Disease Res.";
        d.descr = "Cannot become diseased.";
        d.msg_start_player = "I feel resistant to disease.";
        d.msg_start_mon = "is resistant to disease.";
        d.msg_end_player = "I feel vulnerable to disease.";
        d.msg_end_mon = "is vulnerable to disease.";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = PropAlignment::good;
        add(d);

        d.id = PropId::r_blind;
        d.name = "Blindness Resistance";
        d.name_short = "Blindness Res.";
        d.descr = "Cannot be blinded.";
        d.allow_display_turns = false;
        d.allow_test_on_bot = false;
        d.alignment = PropAlignment::good;
        add(d);

        d.id = PropId::r_para;
        d.name = "Paralysis Resistance";
        d.name_short = "Paralysis Res.";
        d.descr = "Cannot be paralyzed.";
        d.allow_display_turns = false;
        d.allow_test_on_bot = false;
        d.alignment = PropAlignment::good;
        add(d);

        d.id = PropId::r_breath;
        d.std_rnd_turns = Range(50, 100);
        d.descr = "Cannot be harmed by constricted breathing.";
        d.msg_start_player = "I can breath without harm.";
        d.msg_start_mon = "can breath without harm.";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = PropAlignment::good;
        add(d);

        d.id = PropId::r_spell;
        d.name = "Spell Resistance";
        d.name_short = "Spell Res.";
        d.descr = "Cannot be affected by harmful spells.";
        d.msg_start_player = "I defy harmful spells!";
        d.msg_start_mon = "is defying harmful spells.";
        d.msg_end_player = "I feel vulnerable to spells.";
        d.msg_end_mon = "is vulnerable to spells.";
        d.allow_test_on_bot = false;
        d.alignment = PropAlignment::good;
        add(d);

        d.id = PropId::r_shock;
        d.std_rnd_turns = Range(8, 12);
        d.name = "Shock Resistance";
        d.name_short = "Shock Res.";
        d.descr = "Unaffected by shocking events.";
        d.msg_start_player = "Nothing can disturb my mind!";
        d.msg_start_mon = "";
        d.msg_end_player =
                "I feel susceptible to the horrors of this place again.";
        d.msg_end_mon = "";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = PropAlignment::good;
        add(d);

        d.id = PropId::light_sensitive;
        d.std_rnd_turns = Range(50, 100);
        d.name = "Light Sensitive";
        d.name_short = "Light Sensitive";
        d.descr = "Is vulnerable to light.";
        d.msg_start_player = "I feel vulnerable to light!";
        d.msg_start_mon = "is vulnerable to light.";
        d.msg_end_player = "I no longer feel vulnerable to light.";
        d.msg_end_mon = "no longer is vulnerable to light.";
        d.allow_display_turns = true;
        d.alignment = PropAlignment::bad;
        add(d);

        d.id = PropId::blind;
        d.std_rnd_turns = Range(20, 30);
        d.name = "Blind";
        d.name_short = "Blind";
        d.descr = "Cannot see, -20% hit chance, -50% chance to evade attacks.";
        d.msg_start_player = "I am blinded!";
        d.msg_start_mon = "is blinded.";
        d.msg_end_player = "I can see again!";
        d.msg_end_mon = "can see again.";
        d.historic_msg_start_permanent = "I became permanently blind";
        d.historic_msg_end_permanent = "My sight came back";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = PropAlignment::bad;
        add(d);

        d.id = PropId::deaf;
        d.std_rnd_turns = Range(200, 300);
        d.name = "Deaf";
        d.name_short = "Deaf";
        d.msg_start_player = "I am deaf!";
        d.msg_end_player = "I can hear again.";
        d.historic_msg_start_permanent = "I became permanently deaf";
        d.historic_msg_end_permanent = "My hearing came back";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = PropAlignment::bad;
        add(d);

        d.id = PropId::fainted;
        d.std_rnd_turns = Range(100, 200);
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
        d.allow_test_on_bot = true;
        d.alignment = PropAlignment::bad;
        add(d);

        d.id = PropId::burning;
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
        d.update_vision_on_toggled = true;
        d.is_preventable_by_player_trait = true;
        d.allow_test_on_bot = true;
        d.alignment = PropAlignment::bad;
        add(d);

        d.id = PropId::poisoned;
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
        d.alignment = PropAlignment::bad;
        add(d);

        d.id = PropId::paralyzed;
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
        d.is_preventable_by_player_trait = true;
        d.allow_test_on_bot = true;
        d.alignment = PropAlignment::bad;
        add(d);

        d.id = PropId::terrified;
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
        d.alignment = PropAlignment::bad;
        add(d);

        d.id = PropId::confused;
        d.std_rnd_turns = Range(80, 120);
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
        d.alignment = PropAlignment::bad;
        add(d);

        d.id = PropId::hallucinating;
        d.std_rnd_turns = Range(75, 150);
        d.name = "Hallucinating";
        d.name_short = "Halluc.";
        d.descr = "The senses cannot always be trusted.";
        d.msg_start_player = "I am starting to doubt my senses.";
        d.msg_start_mon = "";
        d.msg_end_player = "I feel more sure of my senses.";
        d.msg_end_mon = "";
        d.msg_res_player = "I manage to maintain a grip on what is real.";
        d.msg_res_mon = "";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = PropAlignment::bad;
        add(d);

        d.id = PropId::astral_opium_addiction;
        d.std_rnd_turns = Range(100, 200);
        d.std_rnd_dlvls = Range(3, 6);
        d.name = "Astral Opium Addiction";
        d.name_short = "Addict";
        d.descr =
                "Addicted to Astral Opium - the addiction will cease "
                "eventually if Astral Opium is not used again, however the "
                "abstinence will soon cause withdrawal symptoms "
                "(increased minimum shock).";
        d.msg_start_player = "That felt amazing!";
        d.msg_start_mon = "";
        d.msg_end_player =
                "I suddenly realize that I no longer crave Astral Opium.";
        d.msg_end_mon = "";
        d.msg_res_player = "";
        d.msg_res_mon = "";
        d.allow_display_turns = false;
        d.allow_test_on_bot = false;
        d.alignment = PropAlignment::bad;
        add(d);

        d.id = PropId::stunned;
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
        d.alignment = PropAlignment::bad;
        add(d);

        d.id = PropId::slowed;
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
        d.historic_msg_start_permanent = "I became perpetually slowed";
        d.historic_msg_end_permanent = "My slowness ceased";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = PropAlignment::bad;
        add(d);

        d.id = PropId::hasted;
        d.std_rnd_turns = Range(12, 16);
        d.name = "Hasted";
        d.name_short = "Hasted";
        d.descr = "Moves faster.";
        d.msg_start_player = "Everything around me seems to slow down.";
        d.msg_start_mon = "speeds up.";
        d.msg_end_player = "Everything around me seems to speed up.";
        d.msg_end_mon = "slows down.";
        d.historic_msg_start_permanent = "I became perpetually hasted";
        d.historic_msg_end_permanent = "My hastiness ceased";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = PropAlignment::good;
        add(d);

        d.id = PropId::extra_hasted;
        d.std_rnd_turns = Range(7, 11);
        d.name = "Extra Hasted";
        d.name_short = "Extra Hasted";
        d.descr = "Moves very fast.";
        d.msg_start_player = "Everything around me suddenly seems very still.";
        d.msg_end_player = "Everything around me seems to speed up a lot.";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = PropAlignment::good;
        add(d);

        d.id = PropId::summoned;
        d.std_rnd_turns = Range(80, 120);
        d.msg_end_mon = "suddenly disappears.";
        d.name = "Summoned";
        d.descr = "Was magically summoned here.";
        d.alignment = PropAlignment::neutral;
        add(d);

        d.id = PropId::flared;
        d.std_rnd_turns = Range(3, 4);
        d.msg_start_mon = "is perforated by a flare!";
        d.allow_display_turns = false;
        d.update_vision_on_toggled = true;
        d.alignment = PropAlignment::bad;
        add(d);

        d.id = PropId::nailed;
        d.name = "Nailed";
        d.descr =
                "Fastened by a spike. Tearing it out will be rather painful.";
        d.msg_start_player = "I am fastened by a spike!";
        d.msg_start_mon = "is fastened by a spike.";
        d.msg_end_player = "I tear free!";
        d.msg_end_mon = "tears free!";
        d.allow_display_turns = false;
        d.alignment = PropAlignment::bad;
        add(d);

        d.id = PropId::wound;
        d.name = "Wounded";
        d.descr =
                "For each wound: -5% melee hit chance, -5% chance to evade "
                "attacks, -10% hit points, and reduced hit point generation "
                "rate - also, walking takes extra turns if three or more "
                "wounds are received.";
        d.msg_start_player = "I am wounded!";
        d.msg_res_player = "I resist wounding!";
        d.allow_display_turns = false;
        d.allow_test_on_bot = false;
        d.alignment = PropAlignment::bad;
        add(d);

        d.id = PropId::hp_sap;
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
        d.alignment = PropAlignment::bad;
        add(d);

        d.id = PropId::spi_sap;
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
        d.alignment = PropAlignment::bad;
        add(d);

        d.id = PropId::mind_sap;
        d.name = "Mind Sapped";
        d.descr = "Increased Shock.";
        d.msg_start_player = "My mind is sapped!";
        d.msg_end_player = "My mind returns.";
        d.msg_res_player = "I resist sapping.";
        d.allow_display_turns = false;
        d.allow_test_on_bot = false;
        d.alignment = PropAlignment::bad;
        add(d);

        d.id = PropId::infected;
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
        d.alignment = PropAlignment::bad;
        add(d);

        d.id = PropId::diseased;
        d.std_rnd_turns = Range(50, 100);
        d.name = "Diseased";
        d.name_short = "Diseased";
        d.descr = "-50% maximum hit points.";
        d.msg_start_player = "I am diseased!";
        d.msg_start_mon = "is diseased.";
        d.msg_end_player = "My disease is cured!";
        d.msg_end_mon = "is no longer diseased.";
        d.msg_res_player = "I resist disease.";
        d.msg_res_mon = "resists disease.";
        d.historic_msg_start_permanent = "I caught a horrible disease";
        d.historic_msg_end_permanent = "I was cured from a horrible disease";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = PropAlignment::bad;
        add(d);

        d.id = PropId::descend;
        d.std_rnd_turns = Range(20, 30);
        d.name = "Descending";
        d.name_short = "Descending";
        d.descr = "Soon moved to a deeper level.";
        d.msg_start_player = "I feel a sinking sensation.";
        d.allow_display_turns = true;
        d.allow_test_on_bot = false;
        d.alignment = PropAlignment::neutral;
        add(d);

        d.id = PropId::weakened;
        d.std_rnd_turns = Range(50, 100);
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
        d.alignment = PropAlignment::bad;
        add(d);

        d.id = PropId::frenzied;
        d.std_rnd_turns = Range(50, 100);
        d.name = "Frenzied";
        d.name_short = "Frenzied";
        d.descr =
                "Cannot move away from seen enemies, moves faster, +1 melee "
                "damage, +10% melee hit chance, immune to confusion, fainting, "
                "fear, and weakening, cannot read or cast spells, becomes "
                "weakened when the frenzy ends.";
        d.msg_start_player = "I feel ferocious!!!";
        d.msg_start_mon = "Looks ferocious!";
        d.msg_end_player = "I calm down.";
        d.msg_end_mon = "Calms down a little.";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = PropAlignment::neutral;
        add(d);

        d.id = PropId::blessed;
        d.std_rnd_turns = Range(400, 600);
        d.name = "Blessed";
        d.name_short = "Blessed";
        d.descr = "+10% to hit chance, evasion, stealth, and searching.";
        d.msg_start_player = "I feel luckier.";
        d.msg_end_player = "I have normal luck.";
        d.historic_msg_start_permanent = "I received an everlasting blessing";
        d.historic_msg_end_permanent = "My great blessing ceased";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = PropAlignment::good;
        add(d);

        d.id = PropId::cursed;
        d.std_rnd_turns = Range(50, 100);
        d.name = "Cursed";
        d.name_short = "Cursed";
        d.descr =
                "-10% to hit chance, evasion, stealth, and searching, "
                "5% chance to fail when attempting to read or cast spells.";
        d.msg_start_player = "I feel misfortunate.";
        d.msg_start_mon = "is cursed.";
        d.msg_end_player = "I feel more fortunate.";
        d.msg_end_player = "is no longer cursed.";
        d.msg_res_player = "I resist misfortune.";
        d.historic_msg_start_permanent = "A perpetual curse was put upon me";
        d.historic_msg_end_permanent = "A terrible curse was lifted from me";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = PropAlignment::bad;
        add(d);

        d.id = PropId::doomed;
        d.std_rnd_turns = Range(50, 100);
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
        d.alignment = PropAlignment::bad;
        add(d);

        d.id = PropId::premonition;
        d.std_rnd_turns = Range(5, 9);
        d.name = "Premonition";
        d.name_short = "Premonition";
        d.descr = "+75% chance to evade attacks.";
        d.msg_start_player = "I feel unassailable.";
        d.msg_end_player = "I feel more vulnerable.";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = PropAlignment::good;
        add(d);

        d.id = PropId::erudition;
        d.name = "Erudition";
        d.name_short = "Erudition";
        d.descr = "Spell skill is improved by one level.";
        d.msg_start_player = "Mystic secrets are revealed to me!";
        d.msg_end_player = "I feel ignorant.";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = PropAlignment::good;
        add(d);

        d.id = PropId::magic_searching;
        d.std_rnd_turns = Range(20, 60);
        d.name = "Magic Searching";
        d.name_short = "Magic Searching";
        d.descr =
                "Magically detects objects and creatures in the surrounding "
                "area.";
        d.msg_start_player = "Hidden secrets are revealed to me.";
        d.msg_end_player = "I can no longer see hidden things.";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = PropAlignment::good;
        add(d);

        d.id = PropId::entangled;
        d.name = "Entangled";
        d.name_short = "Entangled";
        d.descr = "Entangled in something.";
        d.msg_start_player = "I am entangled!";
        d.msg_start_mon = "is entangled.";
        d.msg_end_player = "I tear free!";
        d.msg_end_mon = "tears free!";
        d.allow_display_turns = false;
        d.alignment = PropAlignment::bad;
        add(d);

        d.id = PropId::radiant_adjacent;
        d.std_rnd_turns = Range(50, 100);
        d.allow_display_turns = true;
        d.update_vision_on_toggled = true;
        d.allow_test_on_bot = true;
        d.alignment = PropAlignment::neutral;
        add(d);

        d.id = PropId::radiant_fov;
        d.std_rnd_turns = Range(50, 100);
        d.name = "Radiant";
        d.name_short = "Radiant";
        d.descr = "Emanating a bright light.";
        d.msg_start_player = "A bright light shines around me.";
        d.msg_end_player = "It suddenly gets darker.";
        d.allow_display_turns = true;
        d.update_vision_on_toggled = true;
        d.allow_test_on_bot = true;
        d.alignment = PropAlignment::neutral;
        add(d);

        d.id = PropId::invis;
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
        d.alignment = PropAlignment::good;
        add(d);

        d.id = PropId::cloaked;
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
        d.alignment = PropAlignment::good;
        add(d);

        d.id = PropId::recloaks;
        add(d);

        d.id = PropId::see_invis;
        d.std_rnd_turns = Range(50, 100);
        d.name = "See Invisible";
        d.name_short = "See Invisible";
        d.descr = "Can see invisible creatures, cannot be blinded.";
        d.msg_start_player = "My eyes perceive the invisible.";
        d.msg_start_mon = "seems very keen.";
        d.msg_end_player = "My eyes can no longer perceive the invisible.";
        d.msg_end_mon = "seems less keen.";
        d.allow_display_turns = true;
        d.update_vision_on_toggled = true;
        d.allow_test_on_bot = true;
        d.alignment = PropAlignment::good;
        add(d);

        d.id = PropId::darkvision;
        d.std_rnd_turns = Range(50, 100);
        d.allow_display_turns = true;
        d.update_vision_on_toggled = true;
        d.allow_test_on_bot = true;
        d.alignment = PropAlignment::good;
        add(d);

        d.id = PropId::tele_ctrl;
        d.std_rnd_turns = Range(50, 100);
        d.name = "Teleport control";
        d.name_short = "Teleport control";
        d.descr = "Can control teleport destination.";
        d.msg_start_player = "I feel in control.";
        d.msg_end_player = "I feel less in control.";
        d.allow_display_turns = true;
        d.allow_test_on_bot = false;
        d.alignment = PropAlignment::good;
        add(d);

        d.id = PropId::spell_reflect;
        d.std_rnd_turns = Range(50, 100);
        d.allow_display_turns = false;
        d.allow_test_on_bot = true;
        d.alignment = PropAlignment::good;
        add(d);

        d.id = PropId::aiming;
        d.std_rnd_turns = Range(1, 1);
        d.name = "Aiming";
        d.name_short = "Aiming";
        d.descr = "Increased range attack effectiveness.";
        d.allow_display_turns = false;
        d.alignment = PropAlignment::good;
        add(d);

        d.id = PropId::conflict;
        d.name = "Conflicted";
        d.name_short = "Conflicted";
        d.descr = "Considers every creature as an enemy.";
        d.std_rnd_turns = Range(10, 20);
        d.msg_start_mon = "Looks conflicted.";
        d.msg_end_mon = "Looks more determined.";
        d.allow_display_turns = false;
        d.alignment = PropAlignment::bad;
        add(d);

        d.id = PropId::aura_of_decay;
        d.std_rnd_turns = Range(6, 12);
        d.name = "Aura of Decay";
        d.name_short = "Aura of Decay";
        d.descr = "Adjacent creatures take damage.";
        d.msg_start_player = "Withering surrounds me.";
        d.msg_start_mon = "appears to exude death and decay.";
        d.msg_end_player = "The decay subsides.";
        d.msg_end_mon = "no longer exudes decay.";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = PropAlignment::good;
        add(d);

        d.id = PropId::reduced_pierce_dmg;
        add(d);

        d.id = PropId::vortex;
        add(d);

        d.id = PropId::explodes_on_death;
        add(d);

        d.id = PropId::splits_on_death;
        add(d);

        d.id = PropId::corpse_eater;
        add(d);

        d.id = PropId::teleports;
        add(d);

        d.id = PropId::teleports_away;
        add(d);

        d.id = PropId::always_aware;
        add(d);

        d.id = PropId::corrupts_env_color;
        add(d);

        d.id = PropId::alters_env;
        add(d);

        d.id = PropId::regenerates;
        d.std_rnd_turns = Range(50, 100);
        d.name = "Regenerates";
        d.name_short = "Regenerates";
        d.descr =
                "+1 extra hit point regenerated per turn, and "
                "10% chance per turn to heal one wound.";
        d.msg_start_player = "My body starts healing itself much faster.";
        d.msg_start_mon = "starts regenerating damage very quickly.";
        d.msg_end_player = "My body heals itself slower now.";
        d.msg_end_mon = "stops regenerating damage quickly.";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = PropAlignment::good;
        add(d);

        d.id = PropId::corpse_rises;
        add(d);

        d.id = PropId::breeds;
        add(d);

        d.id = PropId::vomits_ooze;
        add(d);

        d.id = PropId::confuses_adjacent;
        add(d);

        d.id = PropId::frenzy_player_on_seen;
        add(d);

        d.id = PropId::zuul_possess_priest;
        add(d);

        d.id = PropId::possessed_by_zuul;
        add(d);

        d.id = PropId::shapeshifts;
        add(d);

        d.id = PropId::major_clapham_summon;
        add(d);

        d.id = PropId::flying;
        d.name = "Flying";
        d.name_short = "Flying";
        d.descr = "Can fly over low obstacles.";
        d.allow_display_turns = false;
        d.allow_test_on_bot = false;
        d.alignment = PropAlignment::neutral;
        add(d);

        d.id = PropId::ethereal;
        d.name = "Ethereal";
        d.name_short = "Ethereal";
        d.descr =
                "Can pass through solid objects, "
                "+50% chance to evade attacks.";
        d.allow_display_turns = false;
        d.allow_test_on_bot = false;
        d.alignment = PropAlignment::neutral;
        add(d);

        d.id = PropId::ooze;
        d.allow_test_on_bot = false;
        d.alignment = PropAlignment::neutral;
        add(d);

        d.id = PropId::small_crawling;
        d.allow_test_on_bot = false;
        d.alignment = PropAlignment::neutral;
        add(d);

        d.id = PropId::burrowing;
        d.allow_display_turns = false;
        d.allow_test_on_bot = false;
        d.alignment = PropAlignment::neutral;
        add(d);

        d.id = PropId::waiting;
        d.std_rnd_turns = Range(1, 1);
        d.allow_display_turns = false;
        d.alignment = PropAlignment::neutral;
        add(d);

        d.id = PropId::disabled_attack;
        d.std_rnd_turns = Range(1, 1);
        d.allow_display_turns = false;
        d.alignment = PropAlignment::neutral;
        add(d);

        d.id = PropId::disabled_melee;
        d.std_rnd_turns = Range(1, 1);
        d.allow_display_turns = false;
        d.alignment = PropAlignment::neutral;
        add(d);

        d.id = PropId::disabled_ranged;
        d.std_rnd_turns = Range(1, 1);
        d.allow_display_turns = false;
        d.alignment = PropAlignment::neutral;
        add(d);

        d.id = PropId::swimming;
        d.std_rnd_turns = Range(1, 1);
        d.name = "Swimming";
        d.name_short = "Swimming";
        d.descr =
                "Cannot read, fire ranged weapons, or light explosives, "
                "-10% melee hit chance, -10% chance to evade attacks.";
        d.allow_display_turns = false;
        d.allow_test_on_bot = false;
        d.alignment = PropAlignment::neutral;
        add(d);

        d.id = PropId::hit_chance_penalty_curse;
        d.std_rnd_turns = Range(1, 1);
        d.name = "";
        d.name_short = "";
        d.descr = "";
        d.msg_start_player = "My aiming feels worse.";
        d.msg_end_player = "My aiming feels better.";
        d.allow_display_turns = false;
        d.allow_test_on_bot = true;
        d.alignment = PropAlignment::bad;
        add(d);

        d.id = PropId::increased_shock_curse;
        d.std_rnd_turns = Range(1, 1);
        d.name = "";
        d.name_short = "";
        d.descr = "";
        d.msg_start_player = "I feel more anxious!";
        d.msg_end_player = "I feel less anxious.";
        d.allow_display_turns = false;
        d.allow_test_on_bot = true;
        d.alignment = PropAlignment::bad;
        add(d);

        d.id = PropId::cannot_read_curse;
        d.std_rnd_turns = Range(1, 1);
        d.name = "";
        d.name_short = "";
        d.descr = "";
        d.msg_start_player = "I feel illiterate!";
        d.msg_end_player = "I can read again.";
        d.allow_display_turns = false;
        d.allow_test_on_bot = true;
        d.alignment = PropAlignment::bad;
        add(d);

        // NOTE: This property reuses messages from 'light_sensitive', so order
        // is important here
        d.id = PropId::light_sensitive_curse;
        d.std_rnd_turns = Range(1, 1);
        d.name = "";
        d.name_short = "";
        d.descr = "";
        d.msg_start_player =
                property_data::g_data[(size_t)PropId::light_sensitive]
                        .msg_start_player;
        d.msg_end_player =
                property_data::g_data[(size_t)PropId::light_sensitive]
                        .msg_end_player;
        d.allow_display_turns = false;
        d.allow_test_on_bot = true;
        d.alignment = PropAlignment::bad;
        add(d);

        d.id = PropId::disabled_hp_regen;
        d.std_rnd_turns = Range(1, 1);
        d.name = "";
        d.name_short = "";
        d.descr = "";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = PropAlignment::bad;
        add(d);

        d.id = PropId::sanctuary;
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
        d.alignment = PropAlignment::good;
        add(d);
}

// -----------------------------------------------------------------------------
// property_data
// -----------------------------------------------------------------------------
namespace property_data
{
PropData g_data[(size_t)PropId::END];

void init()
{
        init_data_list();
}

PropId str_to_prop_id(const std::string& str)
{
        return s_str_to_prop_id_map.at(str);
}

std::string descr(PropId id)
{
        ASSERT(id != PropId::END);

        return g_data[(size_t)id].descr;
}

}  // namespace property_data
