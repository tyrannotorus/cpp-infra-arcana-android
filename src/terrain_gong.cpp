// =============================================================================
// Copyright 2011-2023 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "terrain_gong.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <memory>
#include <utility>
#include <vector>

#include "actor.hpp"
#include "actor_factory.hpp"
#include "actor_player_state.hpp"
#include "array2.hpp"
#include "audio_data.hpp"
#include "common_text.hpp"
#include "debug.hpp"
#include "game.hpp"
#include "game_time.hpp"
#include "global.hpp"
#include "inventory.hpp"
#include "item.hpp"
#include "item_curse.hpp"
#include "item_data.hpp"
#include "item_factory.hpp"
#include "item_scroll.hpp"
#include "map.hpp"
#include "msg_log.hpp"
#include "player_bon.hpp"
#include "player_spells.hpp"
#include "property.hpp"
#include "property_data.hpp"
#include "property_factory.hpp"
#include "property_handler.hpp"
#include "query.hpp"
#include "random.hpp"
#include "sound.hpp"
#include "spells.hpp"
#include "terrain.hpp"
#include "terrain_event.hpp"
#include "terrain_factory.hpp"

struct P;

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static item::Item* get_random_cursed_player_item()
{
        std::vector<item::Item*> cursed_items;

        for (const InvSlot& slot : map::g_player->m_inv.m_slots) {
                if (slot.item && slot.item->is_cursed()) {
                        cursed_items.push_back(slot.item);
                }
        }

        for (item::Item* const item : map::g_player->m_inv.m_backpack) {
                if (item->is_cursed()) {
                        cursed_items.push_back(item);
                }
        }

        if (cursed_items.empty()) {
                return nullptr;
        }
        else {
                return rnd::element(cursed_items);
        }
}

static std::unique_ptr<terrain::gong::Bonus> make_bonus(
        terrain::gong::BonusId id)
{
        // If the player has a cursed item, make the bless bonus most probable.
        auto bless = std::make_unique<terrain::gong::Blessed>();

        if (get_random_cursed_player_item() && bless->is_allowed() && rnd::coin_toss()) {
                return bless;
        }

        switch (id) {
        case terrain::gong::BonusId::upgrade_spell:
                return std::make_unique<terrain::gong::UpgradeSpell>();

        case terrain::gong::BonusId::gain_hp:
                return std::make_unique<terrain::gong::GainHp>();

        case terrain::gong::BonusId::gain_sp:
                return std::make_unique<terrain::gong::GainSp>();

        case terrain::gong::BonusId::gain_xp:
                return std::make_unique<terrain::gong::GainXp>();

        case terrain::gong::BonusId::remove_insanity:
                return std::make_unique<terrain::gong::RemoveInsanity>();

        case terrain::gong::BonusId::gain_item:
                return std::make_unique<terrain::gong::GainItem>();

        case terrain::gong::BonusId::healed:
                return std::make_unique<terrain::gong::Healed>();

        case terrain::gong::BonusId::blessed:
                return bless;

        case terrain::gong::BonusId::undefined:
        case terrain::gong::BonusId::END:
                break;
        }

        ASSERT(false);

        return nullptr;
}

static std::unique_ptr<terrain::gong::Toll> make_toll(terrain::gong::TollId id)
{
        switch (id) {
        case terrain::gong::TollId::hp_reduced:
                return std::make_unique<terrain::gong::HpReduced>();

        case terrain::gong::TollId::sp_reduced:
                return std::make_unique<terrain::gong::SpReduced>();

        case terrain::gong::TollId::xp_reduced:
                return std::make_unique<terrain::gong::XpReduced>();

        case terrain::gong::TollId::deaf:
                return std::make_unique<terrain::gong::Deaf>();

        case terrain::gong::TollId::cursed:
                return std::make_unique<terrain::gong::Cursed>();

        case terrain::gong::TollId::forget_spell:
                return std::make_unique<terrain::gong::ForgetSpell>();

        case terrain::gong::TollId::spawn_monsters:
                return std::make_unique<terrain::gong::SpawnMonsters>();

        case terrain::gong::TollId::END:
                break;
        }

        ASSERT(false);

        return nullptr;
}

static std::vector<std::unique_ptr<terrain::gong::Bonus>>
make_all_allowed_bonuses()
{
        std::vector<std::unique_ptr<terrain::gong::Bonus>> bonuses;

        for (int i = 0; i < (int)terrain::gong::BonusId::END; ++i) {
                auto bonus = make_bonus((terrain::gong::BonusId)i);

                if (!bonus) {
                        ASSERT(false);

                        continue;
                }

                if (!bonus->is_allowed()) {
                        continue;
                }

                bonuses.push_back(std::move(bonus));
        }

        return bonuses;
}

static bool is_toll_blacklist_allowing_bonus(
        const terrain::gong::Toll& toll,
        const terrain::gong::BonusId bonus_id)
{
        const auto bonuses_not_allowed_with =
                toll.bonuses_not_allowed_with();

        const auto search =
                std::find(
                        std::begin(bonuses_not_allowed_with),
                        std::end(bonuses_not_allowed_with),
                        bonus_id);

        const bool is_in_blacklist =
                (search != std::end(bonuses_not_allowed_with));

        return !is_in_blacklist;
}

static bool is_toll_whitelist_allowing_bonus(
        const terrain::gong::Toll& toll,
        const terrain::gong::BonusId bonus_id)
{
        const auto bonuses_only_allowed_with =
                toll.bonuses_only_allowed_with();

        if (bonuses_only_allowed_with.empty()) {
                // The toll does not have a bonus whitelist
                return true;
        }

        // The toll has a bonus whitelist

        const auto search =
                std::find(
                        std::begin(bonuses_only_allowed_with),
                        std::end(bonuses_only_allowed_with),
                        bonus_id);

        const bool is_in_whitelist =
                (search != std::end(bonuses_only_allowed_with));

        return is_in_whitelist;
}

static bool is_toll_allowing_bonus(
        const terrain::gong::Toll& toll,
        const terrain::gong::BonusId bonus_id)
{
        if (!is_toll_blacklist_allowing_bonus(toll, bonus_id)) {
                return false;
        }

        if (!is_toll_whitelist_allowing_bonus(toll, bonus_id)) {
                return false;
        }

        return true;
}

static std::vector<std::unique_ptr<terrain::gong::Toll>> make_all_allowed_tolls(
        const terrain::gong::BonusId bonus_id)
{
        ASSERT((bonus_id != terrain::gong::BonusId::undefined));
        ASSERT((bonus_id != terrain::gong::BonusId::END));

        std::vector<std::unique_ptr<terrain::gong::Toll>> tolls;

        for (int i = 0; i < (int)terrain::gong::TollId::END; ++i) {
                auto toll = make_toll((terrain::gong::TollId)i);

                if (!toll) {
                        ASSERT(false);

                        continue;
                }

                if (!is_toll_allowing_bonus(*toll, bonus_id)) {
                        continue;
                }

                if (!toll->is_allowed()) {
                        continue;
                }

                tolls.push_back(std::move(toll));
        }

        return tolls;
}

static std::unique_ptr<terrain::gong::Bonus> make_random_allowed_bonus()
{
        auto bonus_bucket = make_all_allowed_bonuses();

        if (bonus_bucket.empty()) {
                return nullptr;
        }

        const auto idx = rnd::idx(bonus_bucket);

        auto bonus = std::move(bonus_bucket[idx]);

        return bonus;
}

static std::unique_ptr<terrain::gong::Toll> make_random_allowed_toll(
        const terrain::gong::BonusId bonus_id)
{
        auto toll_bucket = make_all_allowed_tolls(bonus_id);

        if (toll_bucket.empty()) {
                return nullptr;
        }

        const auto idx = rnd::idx(toll_bucket);

        auto toll = std::move(toll_bucket[idx]);

        return toll;
}

static void run_gong_effect()
{
        const auto bonus = make_random_allowed_bonus();

        if (!bonus) {
                return;
        }

        bonus->run_effect();

        const auto toll = make_random_allowed_toll(bonus->id());

        if (!toll) {
                return;
        }

        msg_log::more_prompt();

        toll->run_effect();
}

// -----------------------------------------------------------------------------
// terrain
// -----------------------------------------------------------------------------
namespace terrain
{
// -----------------------------------------------------------------------------
// gong
// -----------------------------------------------------------------------------
namespace gong
{
// -----------------------------------------------------------------------------
// Upgrade spell
// -----------------------------------------------------------------------------
UpgradeSpell::UpgradeSpell() :

        m_spell_id(SpellId::END)
{
        const auto bucket = find_spells_can_upgrade();

        if (!bucket.empty()) {
                m_spell_id = rnd::element(bucket);
        }
}

bool UpgradeSpell::is_allowed() const
{
        return m_spell_id != SpellId::END;
}

void UpgradeSpell::run_effect()
{
        player_spells::incr_spell_skill(m_spell_id, Verbose::yes);
}

std::vector<SpellId> UpgradeSpell::find_spells_can_upgrade() const
{
        std::vector<SpellId> spells;

        spells.reserve((size_t)SpellId::END);

        for (int i = 0; i < (int)SpellId::END; ++i) {
                const auto id = (SpellId)i;

                if (!player_spells::is_spell_learned(id)) {
                        continue;
                }

                if (player_spells::spell_skill(id) == SpellSkill::master) {
                        continue;
                }

                const std::unique_ptr<const Spell> spell(spells::make(id));

                if (!spell->can_be_improved_with_skill()) {
                        continue;
                }

                // Spell can be improved
                spells.push_back(id);
        }

        return spells;
}

// -----------------------------------------------------------------------------
// Gain HP
// -----------------------------------------------------------------------------
bool GainHp::is_allowed() const
{
        return true;
}

void GainHp::run_effect()
{
        map::g_player->change_max_hp(2);
}

// -----------------------------------------------------------------------------
// Gain SP
// -----------------------------------------------------------------------------
bool GainSp::is_allowed() const
{
        return true;
}

void GainSp::run_effect()
{
        map::g_player->change_max_sp(1);
}

// -----------------------------------------------------------------------------
// Gain XP
// -----------------------------------------------------------------------------
bool GainXp::is_allowed() const
{
        return game::xp_pct() < 50;
}

void GainXp::run_effect()
{
        msg_log::add("I feel more experienced.");

        game::incr_player_xp(50, Verbose::no);
}

// -----------------------------------------------------------------------------
// Remove insanity
// -----------------------------------------------------------------------------
bool RemoveInsanity::is_allowed() const
{
        return actor::player_state::g_insanity >= 25;
}

void RemoveInsanity::run_effect()
{
        msg_log::add("I feel more sane.");

        actor::player_state::g_insanity -= 25;
}

// -----------------------------------------------------------------------------
// Gain item
// -----------------------------------------------------------------------------
GainItem::GainItem() :

        m_item_id(item::Id::END)
{
        const auto item_ids = find_allowed_item_ids();

        if (!item_ids.empty()) {
                m_item_id = rnd::element(item_ids);
        }
}

bool GainItem::is_allowed() const
{
        return m_item_id != item::Id::END;
}

void GainItem::run_effect()
{
        auto* const item = item::make(m_item_id);

        item::randomize_item_properties(*item);

        const std::string name_a = item->name(ItemNameType::a);

        msg_log::add("I have received " + name_a + ".");

        map::g_player->m_inv.put_in_backpack(item);
}

std::vector<item::Id> GainItem::find_allowed_item_ids() const
{
        std::vector<item::Id> ids;

        for (size_t i = 0; i < (size_t)item::Id::END; ++i) {
                const auto& d = item::g_data[i];

                if (d.allow_spawn && d.value >= item::Value::supreme_treasure) {
                        ids.push_back((item::Id)i);
                }
        }

        return ids;
}

// -----------------------------------------------------------------------------
// Healed
// -----------------------------------------------------------------------------
bool Healed::is_allowed() const
{
        const auto& player = *map::g_player;

        if (player.m_properties.has(prop::Id::poisoned) && (player.m_hp <= 6)) {
                return true;
        }

        const auto* const prop = player.m_properties.prop(prop::Id::wound);

        if (prop) {
                const auto* const wound = static_cast<const prop::Wound*>(prop);

                if (wound->nr_wounds() >= 3) {
                        return true;
                }
        }

        return false;
}

void Healed::run_effect()
{
        std::vector<prop::Id> props_can_heal = {
                prop::Id::blind,
                prop::Id::deaf,
                prop::Id::poisoned,
                prop::Id::infected,
                prop::Id::diseased,
                prop::Id::weakened,
                prop::Id::hp_sap,
                prop::Id::wound};

        for (prop::Id prop_id : props_can_heal) {
                map::g_player->m_properties.end_prop(prop_id);
        }

        map::g_player->restore_hp(
                999,     // HP restored
                false);  // Not allowed above max
}

// -----------------------------------------------------------------------------
// Blessed
// -----------------------------------------------------------------------------
bool Blessed::is_allowed() const
{
        const bool is_blessed = map::g_player->m_properties.has(prop::Id::blessed);

        const bool has_cursed_item = (get_random_cursed_player_item() != nullptr);

        return !is_blessed || has_cursed_item;
}

void Blessed::run_effect()
{
        auto* const blessed = prop::make(prop::Id::blessed);

        blessed->set_indefinite();

        map::g_player->m_properties.apply(blessed);

        auto* const cursed_item = get_random_cursed_player_item();

        if (cursed_item) {
                const auto name =
                        cursed_item->name(
                                ItemNameType::plain,
                                ItemNameInfo::none);

                msg_log::add("The " + name + " seems cleansed!");

                cursed_item->current_curse().on_curse_end();

                cursed_item->remove_curse();
        }
}

// -----------------------------------------------------------------------------
// HP reduced
// -----------------------------------------------------------------------------
std::vector<BonusId> HpReduced::bonuses_only_allowed_with() const
{
        return {BonusId::gain_sp};
}

void HpReduced::run_effect()
{
        map::g_player->change_max_hp(-2);
}

// -----------------------------------------------------------------------------
// SP reduced
// -----------------------------------------------------------------------------
std::vector<BonusId> SpReduced::bonuses_only_allowed_with() const
{
        return {BonusId::gain_hp};
}

void SpReduced::run_effect()
{
        map::g_player->change_max_sp(-1);
}

// -----------------------------------------------------------------------------
// XP reduced
// -----------------------------------------------------------------------------
bool XpReduced::is_allowed() const
{
        return game::xp_pct() >= 50;
}

std::vector<BonusId> XpReduced::bonuses_not_allowed_with() const
{
        return {BonusId::gain_xp};
}

void XpReduced::run_effect()
{
        msg_log::add("I feel less experienced.");

        game::decr_player_xp(50);
}

// -----------------------------------------------------------------------------
// Deaf
// -----------------------------------------------------------------------------
bool Deaf::is_allowed() const
{
        auto* const prop = map::g_player->m_properties.prop(prop::Id::deaf);

        return !prop || (prop->duration_mode() != prop::PropDurationMode::indefinite);
}

void Deaf::run_effect()
{
        auto* const deaf = prop::make(prop::Id::deaf);

        deaf->set_indefinite();

        map::g_player->m_properties.apply(deaf);
}

// -----------------------------------------------------------------------------
// Cursed
// -----------------------------------------------------------------------------
bool Cursed::is_allowed() const
{
        auto* const prop = map::g_player->m_properties.prop(prop::Id::cursed);

        return !prop || (prop->duration_mode() != prop::PropDurationMode::indefinite);
}

std::vector<BonusId> Cursed::bonuses_not_allowed_with() const
{
        return {BonusId::blessed};
}

void Cursed::run_effect()
{
        auto* const cursed = prop::make(prop::Id::cursed);

        cursed->set_indefinite();

        map::g_player->m_properties.apply(cursed);
}

// -----------------------------------------------------------------------------
// Spawn monsters
// -----------------------------------------------------------------------------
SpawnMonsters::SpawnMonsters()
{
        std::vector<std::string> summon_bucket;

        summon_bucket.reserve(actor::g_data.size());

        const Range allowed_spawn_lvl_range(map::g_dlvl - 2, map::g_dlvl + 2);

        for (const auto& it : actor::g_data) {
                const actor::ActorData& data = it.second;

                if (data.can_be_summoned_by_mon) {
                        if (allowed_spawn_lvl_range.is_in_range(data.spawn_min_dlvl)) {
                                summon_bucket.push_back(data.id);
                        }
                }
        }

        if (!summon_bucket.empty()) {
                m_id_to_spawn = rnd::element(summon_bucket);
        }
}

bool SpawnMonsters::is_allowed() const
{
        return !m_id_to_spawn.empty();
}

void SpawnMonsters::run_effect()
{
        if (m_id_to_spawn.empty()) {
                ASSERT(false);

                return;
        }

        msg_log::add("Something approaches...");

        const size_t nr_mon = rnd::range(3, 4);

        auto* const event =
                static_cast<EventSpawnMonstersDelayed*>(
                        make(
                                Id::event_spawn_monsters_delayed,
                                map::g_player->m_pos));

        event->set_mon_id(m_id_to_spawn);

        event->set_nr_mon((int)nr_mon);

        game_time::add_mob(event);
}

// -----------------------------------------------------------------------------
// Forget spell
// -----------------------------------------------------------------------------
ForgetSpell::ForgetSpell()
{
        const std::vector<SpellId> spell_bucket = make_spell_bucket();

        if (!spell_bucket.empty()) {
                m_spell_to_forget = rnd::element(spell_bucket);
        }
}

std::vector<BonusId> ForgetSpell::bonuses_not_allowed_with() const
{
        return {BonusId::upgrade_spell};
}

bool ForgetSpell::is_allowed() const
{
        return m_spell_to_forget != SpellId::END;
}

void ForgetSpell::run_effect()
{
        player_spells::forget_spell(m_spell_to_forget);
}

std::vector<SpellId> ForgetSpell::make_spell_bucket() const
{
        std::vector<SpellId> result;

        for (size_t i = 0; i < (size_t)item::Id::END; ++i) {
                const item::ItemData& d = item::g_data[i];

                if (d.type != ItemType::scroll) {
                        continue;
                }

                const SpellId spell_id = d.spell_cast_from_scroll;

                if (spell_id == SpellId::END) {
                        ASSERT(false);

                        continue;
                }

                if (!player_spells::is_spell_learned(spell_id)) {
                        continue;
                }

                if (player_spells::is_spell_forgotten(spell_id)) {
                        continue;
                }

                result.push_back(spell_id);
        }

        return result;
}

}  // namespace gong

// -----------------------------------------------------------------------------
// Gong
// -----------------------------------------------------------------------------
Gong::Gong(const P& p, const TerrainData* const data) :
        Terrain(p, data) {}

void Gong::bump(actor::Actor& actor_bumping)
{
        if (!actor::is_player(&actor_bumping)) {
                return;
        }

        map::memorize_terrain_at(m_pos);
        map::update_vision();

        if (!map::g_seen.at(m_pos)) {
                msg_log::clear();

                msg_log::add("There is a temple gong here.");

                if (!player_bon::is_bg(Bg::exorcist)) {
                        msg_log::add(
                                "Strike it? " + common_text::g_yes_or_no_hint,
                                colors::light_white(),
                                MsgInterruptPlayer::no,
                                MorePromptOnMsg::no,
                                CopyToMsgHistory::no);

                        const auto answer = query::yes_or_no();

                        if (answer == BinaryAnswer::no) {
                                msg_log::clear();

                                return;
                        }
                }
        }

        if (player_bon::is_bg(Bg::exorcist)) {
                msg_log::add("This unholy instrument must be destroyed!");

                return;
        }

        msg_log::add("I strike the temple gong!");

        Snd snd(
                "The crash resonates through the air!",
                audio::SfxId::gong,
                IgnoreMsgIfOriginSeen::no,
                m_pos,
                map::g_player,
                SndVol::high,
                AlertsMon::yes);

        snd.run();

        if (m_is_used) {
                msg_log::add("Nothing happens.");
        }
        else {
                msg_log::more_prompt();

                run_gong_effect();

                m_is_used = true;
        }

        map::g_player->incr_shock(8.0, ShockSrc::misc);

        map::memorize_terrain_at(m_pos);
        map::update_vision();

        game_time::tick();
}

void Gong::hit(
        DmgType dmg_type,
        actor::Actor* actor,
        const P& from_pos,
        int dmg)
{
        (void)actor;
        (void)from_pos;
        (void)dmg;

        switch (dmg_type) {
        case DmgType::explosion:
        case DmgType::pure:
                if (map::g_seen.at(m_pos)) {
                        msg_log::add("The gong is destroyed.");
                }

                map::update_terrain(terrain::make(terrain::Id::rubble_low, m_pos));

                map::update_vision();

                if (player_bon::is_bg(Bg::exorcist)) {
                        const std::string msg =
                                rnd::element(
                                        common_text::g_exorcist_purge_phrases);

                        msg_log::add(msg);

                        game::incr_player_xp(10);

                        map::g_player->restore_sp(999, false, Verbose::no);
                        map::g_player->restore_sp(10, true);
                }
                break;

        default:
                break;
        }
}

std::string Gong::name(const Article article) const
{
        std::string a = (article == Article::a) ? "a " : "the ";

        return a + "temple gong";
}

Color Gong::color_default() const
{
        return m_is_used ? colors::gray() : colors::brown();
}

}  // namespace terrain
