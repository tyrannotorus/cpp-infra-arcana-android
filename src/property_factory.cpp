// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "property_factory.hpp"

#include "debug.hpp"
#include "property.hpp"

namespace property_factory
{
Prop* make(const PropId id)
{
        ASSERT(id != PropId::END);

        switch (id) {
        case PropId::nailed:
                return new PropNailed();

        case PropId::wound:
                return new PropWound();

        case PropId::moribund:
                return new PropMoribund();

        case PropId::blind:
                return new PropBlind();

        case PropId::deaf:
                return new Prop(id);

        case PropId::burning:
                return new PropBurning();

        case PropId::paralyzed:
                return new PropParalyzed();

        case PropId::delayed_by_liquid:
                return new PropDelayedByLiquid();

        case PropId::terrified:
                return new PropTerrified();

        case PropId::weakened:
                return new Prop(id);

        case PropId::confused:
                return new PropConfused();

        case PropId::hallucinating:
                return new PropHallucinating();

        case PropId::astral_opium_addiction:
                return new PropAstralOpiumAddict();

        case PropId::stunned:
                return new Prop(id);

        case PropId::waiting:
                return new PropWaiting();

        case PropId::slowed:
                return new PropSlowed();

        case PropId::hasted:
                return new PropHasted();

        case PropId::extra_hasted:
                return new PropExtraHasted();

        case PropId::summoned:
                return new PropSummoned();

        case PropId::infected:
                return new PropInfected();

        case PropId::diseased:
                return new PropDiseased();

        case PropId::descend:
                return new PropDescend();

        case PropId::poisoned:
                return new PropPoisoned();

        case PropId::fainted:
                return new PropFainted();

        case PropId::frenzied:
                return new PropFrenzied();

        case PropId::aiming:
                return new PropAiming();

        case PropId::disabled_attack:
                return new PropDisabledAttack();

        case PropId::disabled_melee:
                return new PropDisabledMelee();

        case PropId::disabled_ranged:
                return new PropDisabledRanged();

        case PropId::blessed:
                return new PropBlessed();

        case PropId::doomed:
                return new PropDoomed();

        case PropId::cursed:
                return new PropCursed();

        case PropId::premonition:
                return new PropPremonition();

        case PropId::erudition:
                return new PropErudition();

        case PropId::magic_searching:
                return new PropMagicSearching();

        case PropId::entangled:
                return new PropEntangled();

        case PropId::r_acid:
                return new PropRAcid();

        case PropId::r_conf:
                return new PropRConf();

        case PropId::r_breath:
                return new PropRBreath();

        case PropId::r_elec:
                return new PropRElec();

        case PropId::r_fear:
                return new PropRFear();

        case PropId::r_slow:
                return new PropRSlow();

        case PropId::r_phys:
                return new PropRPhys();

        case PropId::r_fire:
                return new PropRFire();

        case PropId::r_spell:
                return new Prop(id);

        case PropId::r_poison:
                return new PropRPoison();

        case PropId::r_sleep:
                return new PropRSleep();

        case PropId::light_sensitive:
                return new PropLgtSens();

        case PropId::zuul_possess_priest:
                return new PropZuulPossessPriest();

        case PropId::possessed_by_zuul:
                return new PropPossessedByZuul();

        case PropId::shapeshifts:
                return new PropShapeshifts();

        case PropId::spectral_wpn:
                return new PropSpectralWpn();

        case PropId::zealot_stop:
                return new PropZealotStop();

        case PropId::major_clapham_summon:
                return new PropMajorClaphamSummon();

        case PropId::allies_ghoul_player:
                return new PropAlliesPlayerGhoul();

        case PropId::flying:
                return new Prop(id);

        case PropId::tiny_flying:
                return new Prop(id);

        case PropId::ethereal:
                return new Prop(id);

        case PropId::ooze:
                return new Prop(id);

        case PropId::small_crawling:
                return new Prop(id);

        case PropId::burrowing:
                return new PropBurrowing();

        case PropId::radiant_self:
                return new Prop(id);

        case PropId::radiant_adjacent:
                return new Prop(id);

        case PropId::radiant_fov:
                return new Prop(id);

        case PropId::darkvision:
                return new Prop(id);

        case PropId::r_disease:
                return new PropRDisease();

        case PropId::r_blind:
                return new PropRBlind();

        case PropId::r_para:
                return new PropRPara();

        case PropId::r_shock:
                return new PropRShock();

        case PropId::tele_ctrl:
                return new Prop(id);

        case PropId::spell_reflect:
                return new Prop(id);

        case PropId::conflict:
                return new Prop(id);

        case PropId::vortex:
                return new PropVortex();

        case PropId::explodes_on_death:
                return new PropExplodesOnDeath();

        case PropId::splits_on_death:
                return new PropSplitsOnDeath();

        case PropId::corpse_eater:
                return new PropCorpseEater();

        case PropId::teleports:
                return new PropTeleports();

        case PropId::teleports_away:
                return new PropTeleportsAway();

        case PropId::always_aware:
                return new PropAlwaysAware();

        case PropId::alters_env:
                return new PropAltersEnv();

        case PropId::corrupts_env_color:
                return new PropCorruptsEnvColor();

        case PropId::regenerating:
                return new PropRegenerating();

        case PropId::corpse_rises:
                return new PropCorpseRises();

        case PropId::breeds:
                return new PropBreeds();

        case PropId::frenzies_self:
                return new PropFrenziesSelf();

        case PropId::summons_locusts:
                return new PropSummonsLocusts();

        case PropId::vomits_ooze:
                return new PropVomitsOoze();

        case PropId::confuses_adjacent:
                return new PropConfusesAdjacent();

        case PropId::frenzy_player_on_seen:
                return new PropFrenzyPlayerOnSeen();

        case PropId::aura_of_decay:
                return new PropAuraOfDecay();

        case PropId::reduced_pierce_dmg:
                return new Prop(id);

        case PropId::short_hearing_range:
                return new Prop(id);

        case PropId::spawns_zombie_parts_on_destroyed:
                return new PropSpawnsZombiePartsOnDestroyed();

        case PropId::invis:
                return new Prop(id);

        case PropId::cloaked:
                return new Prop(id);

        case PropId::recloaks:
                return new PropRecloaks();

        case PropId::see_invis:
                return new PropSeeInvis();

        case PropId::hp_sap:
                return new PropHpSap();

        case PropId::spi_sap:
                return new PropSpiSap();

        case PropId::mind_sap:
                return new PropMindSap();

        case PropId::hit_chance_penalty_curse:
                return new PropHitChancePenaltyCurse();

        case PropId::increased_shock_curse:
                return new PropIncreasedShockCurse();

        case PropId::cannot_read_curse:
                return new PropCannotReadCurse();

        case PropId::light_sensitive_curse:
                return new Prop(id);

        case PropId::disabled_hp_regen:
                return new Prop(id);

        case PropId::sanctuary:
                return new PropSanctuary();

        case PropId::meditative_focused:
                return new Prop(id);

        case PropId::accrue_pain:
                return new PropAccruePain();

        case PropId::crimson_passage:
                return new PropCrimsonPassage();

        case PropId::END:
                break;
        }

        return nullptr;
}

}  // namespace property_factory
