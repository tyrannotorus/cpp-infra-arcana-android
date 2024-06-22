// =============================================================================
// Copyright 2011-2024 Martin Törnqvist <m.tornq@gmail.com>
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
#include "query.hpp"
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
ConsumeItem Explosive::activate(actor::Actor* const actor)
{
        (void)actor;

        if (map::g_player->m_properties.has(prop::Id::burning)) {
                msg_log::add("Not while burning.");

                return ConsumeItem::no;
        }

        const Explosive* const held_explosive =
                actor::player_state::g_active_explosive.get();

        if (held_explosive) {
                const std::string name_held =
                        held_explosive->name(
                                ItemNameType::a,
                                ItemNameInfo::none);

                msg_log::add("I am already holding " + name_held + ".");

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

        // Make a copy to use as the held ignited explosive.
        auto* cpy = static_cast<Explosive*>(item::make(data().id, 1));

        cpy->m_fuse_turns = std_fuse_turns();

        actor::player_state::g_active_explosive.reset(cpy);

        cpy->on_player_ignite();

        return ConsumeItem::yes;
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

                actor::player_state::g_active_explosive.reset();

                // NOTE: This object is now deleted.

                explosion::run(map::g_player->m_pos, ExplType::expl);
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

void Dynamite::on_player_paralyzed()
{
        msg_log::add("The lit Dynamite stick falls from my hand!");

        const int fuse_turns = m_fuse_turns;

        actor::player_state::g_active_explosive.reset();

        // NOTE: This object is now deleted.

        const P& p = map::g_player->m_pos;

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

                actor::player_state::g_active_explosive.reset();

                // NOTE: This object is now deleted.

                const P player_pos = map::g_player->m_pos;

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

void Molotov::on_player_paralyzed()
{
        msg_log::add("The lit Molotov Cocktail falls from my hand!");

        actor::player_state::g_active_explosive.reset();

        // NOTE: This object is now deleted.

        const P player_pos = map::g_player->m_pos;

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

                actor::player_state::g_active_explosive.reset();
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

void Flare::on_player_paralyzed()
{
        msg_log::add("The lit Flare falls from my hand!");

        const int fuse_turns = m_fuse_turns;

        actor::player_state::g_active_explosive.reset();

        // NOTE: This object is now deleted.

        const P& p = map::g_player->m_pos;

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

                actor::player_state::g_active_explosive.reset();

                // NOTE: This object is now deleted.
        }
}

void SmokeGrenade::on_thrown_ignited_landing(const P& p)
{
        explosion::run_smoke_explosion_at(p, 0);
}

void SmokeGrenade::on_player_paralyzed()
{
        msg_log::add("The ignited smoke grenade falls from my hand!");

        actor::player_state::g_active_explosive.reset();

        // NOTE: This object is now deleted.

        const P& p = map::g_player->m_pos;

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
