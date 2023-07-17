// =============================================================================
// Copyright 2011-2023 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "property_factory.hpp"

#include "debug.hpp"
#include "property.hpp"

namespace prop
{
Prop* make(const prop::Id id)
{
        ASSERT(id != prop::Id::END);

        switch (id) {
        case prop::Id::nailed:
                return new Nailed();

        case prop::Id::wound:
                return new Wound();

        case prop::Id::flagellant:
                return new Flagellant();

        case prop::Id::moribund:
                return new Moribund();

        case prop::Id::blind:
                return new Blind();

        case prop::Id::deaf:
                return new Prop(id);

        case prop::Id::burning:
                return new Burning();

        case prop::Id::paralyzed:
                return new Paralyzed();

        case prop::Id::delayed_by_liquid:
                return new DelayedByLiquid();

        case prop::Id::terrified:
                return new Terrified();

        case prop::Id::weakened:
                return new Prop(id);

        case prop::Id::confused:
                return new Confused();

        case prop::Id::hallucinating:
                return new Hallucinating();

        case prop::Id::astral_opium_addiction:
                return new AstralOpiumAddict();

        case prop::Id::stunned:
                return new Prop(id);

        case prop::Id::waiting:
                return new Waiting();

        case prop::Id::slowed:
                return new Slowed();

        case prop::Id::hasted:
                return new Hasted();

        case prop::Id::extra_hasted:
                return new ExtraHasted();

        case prop::Id::summoned:
                return new Summoned();

        case prop::Id::infected:
                return new Infected();

        case prop::Id::diseased:
                return new Diseased();

        case prop::Id::descend:
                return new Descend();

        case prop::Id::poisoned:
                return new Poisoned();

        case prop::Id::fainted:
                return new Fainted();

        case prop::Id::frenzied:
                return new Frenzied();

        case prop::Id::aiming:
                return new Aiming();

        case prop::Id::disabled_attack:
                return new DisabledAttack();

        case prop::Id::disabled_melee:
                return new DisabledMelee();

        case prop::Id::disabled_ranged:
                return new DisabledRanged();

        case prop::Id::melee_cooldown:
                return new MeleeCooldown();

        case prop::Id::blessed:
                return new Blessed();

        case prop::Id::doomed:
                return new Doomed();

        case prop::Id::cursed:
                return new Cursed();

        case prop::Id::premonition:
                return new Premonition();

        case prop::Id::erudition:
                return new Erudition();

        case prop::Id::magic_searching:
                return new MagicSearching();

        case prop::Id::entangled:
                return new Entangled();

        case prop::Id::stuck:
                return new Stuck();

        case prop::Id::r_acid:
                return new RAcid();

        case prop::Id::r_conf:
                return new RConf();

        case prop::Id::r_breath:
                return new RBreath();

        case prop::Id::r_elec:
                return new RElec();

        case prop::Id::r_fear:
                return new RFear();

        case prop::Id::r_slow:
                return new RSlow();

        case prop::Id::r_phys:
                return new RPhys();

        case prop::Id::r_fire:
                return new RFire();

        case prop::Id::r_spell:
                return new Prop(id);

        case prop::Id::r_poison:
                return new RPoison();

        case prop::Id::r_sleep:
                return new RSleep();

        case prop::Id::light_sensitive:
                return new LgtSens();

        case prop::Id::zuul_possess_priest:
                return new ZuulPossessPriest();

        case prop::Id::possessed_by_zuul:
                return new PossessedByZuul();

        case prop::Id::shapeshifts:
                return new Shapeshifts();

        case prop::Id::spectral_wpn:
                return new SpectralWpn();

        case prop::Id::zealot_stop:
                return new ZealotStop();

        case prop::Id::major_clapham_summon:
                return new MajorClaphamSummon();

        case prop::Id::allies_ghoul_player:
                return new AlliesPlayerGhoul();

        case prop::Id::flying:
                return new Prop(id);

        case prop::Id::tiny_flying:
                return new Prop(id);

        case prop::Id::ethereal:
                return new Prop(id);

        case prop::Id::ooze:
                return new Prop(id);

        case prop::Id::small_crawling:
                return new Prop(id);

        case prop::Id::burrowing:
                return new Burrowing();

        case prop::Id::radiant_self:
                return new Prop(id);

        case prop::Id::radiant_adjacent:
                return new Prop(id);

        case prop::Id::radiant_fov:
                return new Prop(id);

        case prop::Id::darkvision:
                return new Prop(id);

        case prop::Id::r_disease:
                return new RDisease();

        case prop::Id::r_blind:
                return new RBlind();

        case prop::Id::r_para:
                return new RPara();

        case prop::Id::r_shock:
                return new RShock();

        case prop::Id::tele_ctrl:
                return new Prop(id);

        case prop::Id::spell_reflect:
                return new Prop(id);

        case prop::Id::conflict:
                return new Prop(id);

        case prop::Id::vortex:
                return new Vortex();

        case prop::Id::explodes_on_death:
                return new ExplodesOnDeath();

        case prop::Id::splits_on_death:
                return new SplitsOnDeath();

        case prop::Id::corpse_eater:
                return new CorpseEater();

        case prop::Id::teleports:
                return new Teleports();

        case prop::Id::teleports_away:
                return new TeleportsAway();

        case prop::Id::always_aware:
                return new AlwaysAware();

        case prop::Id::alters_env:
                return new AltersEnv();

        case prop::Id::corrupts_env_color:
                return new CorruptsEnvColor();

        case prop::Id::regenerating:
                return new Regenerating();

        case prop::Id::corpse_rises:
                return new CorpseRises();

        case prop::Id::breeds:
                return new Breeds();

        case prop::Id::frenzies_self:
                return new FrenziesSelf();

        case prop::Id::frenzies_followers:
                return new FrenziesFollowers();

        case prop::Id::summons_locusts:
                return new SummonsLocusts();

        case prop::Id::others_terrified_on_death:
                return new OthersTerrifiedOnDeath();

        case prop::Id::vomits_ooze:
                return new VomitsOoze();

        case prop::Id::confuses_adjacent:
                return new ConfusesAdjacent();

        case prop::Id::frenzy_player_on_seen:
                return new FrenzyPlayerOnSeen();

        case prop::Id::aura_of_decay:
                return new AuraOfDecay();

        case prop::Id::reduced_pierce_dmg:
                return new Prop(id);

        case prop::Id::short_hearing_range:
                return new Prop(id);

        case prop::Id::spawns_zombie_parts_on_destroyed:
                return new SpawnsZombiePartsOnDestroyed();

        case prop::Id::invis:
                return new Prop(id);

        case prop::Id::cloaked:
                return new Prop(id);

        case prop::Id::recloaks:
                return new Recloaks();

        case prop::Id::see_invis:
                return new SeeInvis();

        case prop::Id::hp_sap:
                return new HpSap();

        case prop::Id::spi_sap:
                return new SpiSap();

        case prop::Id::mind_sap:
                return new MindSap();

        case prop::Id::hit_chance_penalty_curse:
                return new HitChancePenaltyCurse();

        case prop::Id::increased_shock_curse:
                return new IncreasedShockCurse();

        case prop::Id::cannot_read_curse:
                return new CannotReadCurse();

        case prop::Id::light_sensitive_curse:
                return new Prop(id);

        case prop::Id::disabled_hp_regen:
                return new Prop(id);

        case prop::Id::sanctuary:
                return new Sanctuary();

        case prop::Id::meditative_focused:
                return new Prop(id);

        case prop::Id::thorns:
                return new Thorns();

        case prop::Id::crimson_passage:
                return new CrimsonPassage();

        case prop::Id::END:
                break;
        }

        return nullptr;
}

}  // namespace prop
