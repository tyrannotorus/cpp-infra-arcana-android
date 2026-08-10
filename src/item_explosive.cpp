// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "item_explosive.hpp"

#include <memory>

#include "actor.hpp"
#include "actor_player_state.hpp"
#include "array2.hpp"
#include "audio_data.hpp"
#include "common_text.hpp"
#include "config.hpp"
#include "explosion.hpp"
#include "game_time.hpp"
#include "item_data.hpp"
#include "item_factory.hpp"
#include "map.hpp"
#include "msg_log.hpp"
#include "pos.hpp"
#include "property.hpp"
#include "property_data.hpp"
#include "property_factory.hpp"
#include "property_handler.hpp"
#include "inventory.hpp"
#include "query.hpp"
#include "saving.hpp"
#include "random.hpp"
#include "sound.hpp"
#include "terrain.hpp"
#include "terrain_data.hpp"
#include "terrain_factory.hpp"
#include "terrain_mob.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// item
// -----------------------------------------------------------------------------
namespace item
{
bool Explosive::can_stack_with(const Item& other) const
{
        if (!Item::can_stack_with(other)) {
                return false;
        }

        // A lit explosive is a thing of its own - never stacked with the
        // unlit ones it came from, and never with another lit one (each
        // burns down on its own)
        return !is_lit() &&
                !static_cast<const Explosive&>(other).is_lit();
}

std::string Explosive::name_info_str(const ItemNameIdentified id_type) const
{
        (void)id_type;

        return is_lit() ? "(Lit)" : "";
}

void Explosive::save_hook() const
{
        saving::put_int(m_fuse_turns);
}

void Explosive::load_hook()
{
        m_fuse_turns = saving::get_int();
}

void Explosive::destroy_self()
{
        // NOTE: This deletes the item
        map::g_player->m_inv.remove_item_in_backpack_with_ptr(this, true);
}

ConsumeItem Explosive::activate(actor::Actor* const actor)
{
        (void)actor;

        if (map::g_player->m_properties.has(prop::Id::burning)) {
                msg_log::add("Not while burning.");

                return ConsumeItem::no;
        }

        if (is_lit()) {
                // Already burning - there is nothing left to light
                return ConsumeItem::no;
        }

        if (config::warn_on_light_explosive()) {
                const std::string name = this->name(ItemNameType::a);

                const std::string msg =
                        "Light " +
                        name +
                        "? " +
                        common_text::g_yes_or_no_hint;

                msg_log::add(
                        msg,
                        colors::light_white(),
                        MsgInterruptPlayer::no,
                        MorePromptOnMsg::no,
                        CopyToMsgHistory::no);

                BinaryAnswer result = query::yes_or_no();

                msg_log::clear();

                if (result == BinaryAnswer::no) {
                        return ConsumeItem::no;
                }
        }

        // One is taken from the stack and lit - the lit one is carried on
        // its own from here (see the class comment). NOTE: Taking it from
        // the stack may delete THIS object, so everything needed from it
        // is read first.
        auto* const lit = static_cast<Explosive*>(item::make(data().id, 1));

        lit->m_fuse_turns = lit->std_fuse_turns();

        map::g_player->m_inv.decr_item(this);

        // NOTE: This object may now be deleted!

        map::g_player->m_inv.put_in_backpack(lit);

        lit->on_player_ignite();

        // The stack was already decremented above
        return ConsumeItem::no;
}

std::vector<Explosive*> player_lit_explosives()
{
        std::vector<Explosive*> lit;

        if (!map::g_player) {
                return lit;
        }

        for (auto* const item : map::g_player->m_inv.m_backpack) {
                if (item->data().type != ItemType::explosive) {
                        continue;
                }

                auto* const explosive = static_cast<Explosive*>(item);

                if (explosive->is_lit()) {
                        lit.push_back(explosive);
                }
        }

        return lit;
}

void Dynamite::on_player_ignite() const
{
        msg_log::add("I light a dynamite stick.");

        game_time::tick();
}

void Dynamite::on_std_turn_player_hold_ignited()
{
        --m_fuse_turns;

        if (m_fuse_turns > 0) {
                std::string fuse_msg = "***F";

                for (int i = 0; i < m_fuse_turns; ++i) {
                        fuse_msg += "Z";
                }

                fuse_msg += "***";

                const auto more_prompt =
                        (m_fuse_turns <= 2)
                        ? MorePromptOnMsg::yes
                        : MorePromptOnMsg::no;

                msg_log::add(
                        fuse_msg,
                        colors::yellow(),
                        MsgInterruptPlayer::yes,
                        more_prompt);
        }
        else {
                // Fuse has run out
                msg_log::add("The dynamite explodes in my hand!");

                const P player_pos = map::g_player->m_pos;

                destroy_self();

                // NOTE: This object is now deleted.

                explosion::run(player_pos, ExplType::expl);
        }
}

void Dynamite::on_thrown_ignited_landing(const P& p)
{
        auto* const t =
                static_cast<terrain::LitDynamite*>(
                        terrain::make(terrain::Id::lit_dynamite, p));

        t->set_nr_turns(m_fuse_turns);

        game_time::add_mob(t);
}

void Dynamite::drop_lit_at_player(const bool is_deliberate)
{
        msg_log::add(
                is_deliberate
                        ? "I put down the lit Dynamite stick."
                        : "The lit Dynamite stick falls from my hand!");

        const int fuse_turns = m_fuse_turns;

        const P p = map::g_player->m_pos;

        destroy_self();

        // NOTE: This object is now deleted.

        const terrain::Id t_id = map::g_terrain.at(p)->id();

        if (t_id != terrain::Id::chasm) {
                auto* const t =
                        static_cast<terrain::LitDynamite*>(
                                terrain::make(terrain::Id::lit_dynamite, p));

                t->set_nr_turns(fuse_turns);

                game_time::add_mob(t);
        }
}

void Molotov::on_player_ignite() const
{
        msg_log::add("I light a Molotov Cocktail.");

        game_time::tick();
}

void Molotov::on_std_turn_player_hold_ignited()
{
        --m_fuse_turns;

        if (m_fuse_turns == 2) {
                msg_log::add(
                        "The Molotov Cocktail will soon explode.",
                        colors::text(),
                        MsgInterruptPlayer::no,
                        MorePromptOnMsg::yes);
        }

        if (m_fuse_turns == 1) {
                msg_log::add(
                        "The Molotov Cocktail is about to explode!",
                        colors::text(),
                        MsgInterruptPlayer::yes,
                        MorePromptOnMsg::yes);
        }

        if (m_fuse_turns <= 0) {
                msg_log::add("The Molotov Cocktail explodes in my hand!");

                const P player_pos = map::g_player->m_pos;

                destroy_self();

                // NOTE: This object is now deleted.

                Snd snd(
                        "I hear an explosion!",
                        audio::SfxId::explosion_molotov,
                        IgnoreMsgIfOriginSeen::yes,
                        player_pos,
                        nullptr,
                        SndVol::high,
                        AlertsMon::yes);

                snd.run();

                explosion::run(
                        player_pos,
                        ExplType::apply_prop,
                        EmitExplSnd::no,
                        0,
                        ExplExclCenter::no,
                        {prop::make(prop::Id::burning)});
        }
}

void Molotov::on_thrown_ignited_landing(const P& p)
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

        explosion::run(
                p,
                ExplType::apply_prop,
                EmitExplSnd::no,
                0,
                ExplExclCenter::no,
                {prop::make(prop::Id::burning)});
}

void Molotov::drop_lit_at_player(const bool is_deliberate)
{
        msg_log::add(
                is_deliberate
                        ? "I put down the lit Molotov Cocktail."
                        : "The lit Molotov Cocktail falls from my hand!");

        const P player_pos = map::g_player->m_pos;

        destroy_self();

        // NOTE: This object is now deleted.

        Snd snd(
                "I hear an explosion!",
                audio::SfxId::explosion_molotov,
                IgnoreMsgIfOriginSeen::yes,
                player_pos,
                nullptr,
                SndVol::high,
                AlertsMon::yes);

        snd.run();

        explosion::run(
                player_pos,
                ExplType::apply_prop,
                EmitExplSnd::no,
                0,
                ExplExclCenter::no,
                {prop::make(prop::Id::burning)});
}

void Flare::on_player_ignite() const
{
        msg_log::add("I light a Flare.");

        game_time::tick();
}

void Flare::on_std_turn_player_hold_ignited()
{
        --m_fuse_turns;

        if (m_fuse_turns <= 0) {
                msg_log::add("The flare is extinguished.");

                destroy_self();

                // NOTE: This object is now deleted.
        }
}

void Flare::on_thrown_ignited_landing(const P& p)
{
        auto* const t =
                static_cast<terrain::LitDynamite*>(
                        terrain::make(terrain::Id::lit_flare, p));

        t->set_nr_turns(m_fuse_turns);

        game_time::add_mob(t);
}

void Flare::drop_lit_at_player(const bool is_deliberate)
{
        msg_log::add(
                is_deliberate
                        ? "I put down the lit Flare."
                        : "The lit Flare falls from my hand!");

        const int fuse_turns = m_fuse_turns;

        const P p = map::g_player->m_pos;

        destroy_self();

        // NOTE: This object is now deleted.

        const terrain::Id t_id = map::g_terrain.at(p)->id();

        if (t_id != terrain::Id::chasm) {
                auto* const t =
                        static_cast<terrain::LitDynamite*>(
                                terrain::make(terrain::Id::lit_flare, p));

                t->set_nr_turns(fuse_turns);

                game_time::add_mob(t);
        }
}

void SmokeGrenade::on_player_ignite() const
{
        msg_log::add("I ignite a smoke grenade.");

        game_time::tick();
}

void SmokeGrenade::on_std_turn_player_hold_ignited()
{
        if (m_fuse_turns < std_fuse_turns() && rnd::coin_toss()) {
                explosion::run_smoke_explosion_at(map::g_player->m_pos);
        }

        --m_fuse_turns;

        if (m_fuse_turns <= 0) {
                msg_log::add("The smoke grenade is extinguished.");

                destroy_self();

                // NOTE: This object is now deleted.
        }
}

void SmokeGrenade::on_thrown_ignited_landing(const P& p)
{
        explosion::run_smoke_explosion_at(p, 0);
}

void SmokeGrenade::drop_lit_at_player(const bool is_deliberate)
{
        msg_log::add(
                is_deliberate
                        ? "I put down the ignited smoke grenade."
                        : "The ignited smoke grenade falls from my hand!");

        const P p = map::g_player->m_pos;

        destroy_self();

        // NOTE: This object is now deleted.

        const terrain::Id t_id = map::g_terrain.at(p)->id();

        if (t_id != terrain::Id::chasm) {
                explosion::run_smoke_explosion_at(map::g_player->m_pos);
        }
}

Color SmokeGrenade::ignited_projectile_color() const
{
        return data().color;
}

}  // namespace item
