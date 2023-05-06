// =============================================================================
// Copyright 2011-2023 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include <algorithm>
#include <cstddef>
#include <vector>

#include "actor.hpp"
#include "actor_data.hpp"
#include "array2.hpp"
#include "audio_data.hpp"
#include "explosion.hpp"
#include "fov.hpp"
#include "game_time.hpp"
#include "inventory.hpp"
#include "item.hpp"
#include "item_data.hpp"
#include "item_head.hpp"
#include "map.hpp"
#include "map_parsing.hpp"
#include "msg_log.hpp"
#include "pos.hpp"
#include "property.hpp"
#include "property_data.hpp"
#include "property_factory.hpp"
#include "property_handler.hpp"
#include "random.hpp"
#include "rect.hpp"
#include "sound.hpp"
#include "terrain.hpp"
#include "terrain_mob.hpp"

namespace terrain
{
// -----------------------------------------------------------------------------
// Smoke
// -----------------------------------------------------------------------------
void Smoke::on_placed()
{
        // Expire any existing smoke in the current position, and set the
        // duration of the new smoke to whatever was higher
        for (auto* const terrain : game_time::g_mobs) {
                if ((terrain == this) ||
                    (terrain->id() != Id::smoke) ||
                    (terrain->pos() != m_pos)) {
                        continue;
                }

                auto* other_smoke = static_cast<Smoke*>(terrain);

                if (other_smoke->m_nr_turns_left == -1) {
                        m_nr_turns_left = -1;
                }
                else if (m_nr_turns_left != -1) {
                        m_nr_turns_left =
                                std::max(
                                        m_nr_turns_left,
                                        other_smoke->m_nr_turns_left);
                }

                other_smoke->m_nr_turns_left = 0;
        }
}

void Smoke::on_new_turn()
{
        // If smoke has turns left, or is permanent, harm the actor here
        auto* actor = map::living_actor_at(m_pos);

        if (actor && ((m_nr_turns_left > 0) || (m_nr_turns_left == -1))) {
                const bool is_player = actor::is_player(actor);

                // TODO: There needs to be some criteria here, so that e.g. a
                // statue-monster or a very alien monster can't get blinded by
                // smoke. Perhaps add something like "has_eyes"?

                bool allow_blind = true;

                bool allow_cough = !actor->m_properties.has(prop::Id::r_breath);

                if (actor->m_properties.has(prop::Id::fainted)) {
                        // A fainted creature supposedly has their eyes closed.
                        // Also, while a fainted creatures would still need to
                        // breathe, they at least should not cough.
                        allow_blind = false;
                        allow_cough = false;
                }

                if (is_player) {
                        auto* const head_item =
                                map::g_player->m_inv
                                        .m_slots[(size_t)SlotId::head]
                                        .item;

                        auto* const body_item =
                                map::g_player->m_inv
                                        .m_slots[(size_t)SlotId::body]
                                        .item;

                        if (head_item &&
                            (head_item->data().id == item::Id::gas_mask)) {
                                allow_blind = false;
                                allow_cough = false;

                                // This may destroy the gasmask
                                static_cast<item::GasMask*>(head_item)
                                        ->decr_turns_left(map::g_player->m_inv);
                        }

                        if (body_item &&
                            (body_item->data().id == item::Id::armor_asb_suit)) {
                                allow_blind = false;
                                allow_cough = false;
                        }
                }

                // Blinded?
                if (allow_blind && rnd::one_in(4)) {
                        if (is_player) {
                                msg_log::add("I am getting smoke in my eyes.");
                        }

                        auto* const prop =
                                prop::make(prop::Id::blind);

                        prop->set_duration(rnd::range(1, 3));

                        actor->m_properties.apply(prop);
                }

                // Coughing?
                if (allow_cough && rnd::one_in(4)) {
                        std::string snd_msg;

                        if (is_player) {
                                msg_log::add("I cough.");
                        }
                        else {
                                // Is monster
                                if (actor->m_data->is_humanoid) {
                                        snd_msg = "I hear coughing.";
                                }
                        }

                        const auto alerts =
                                is_player
                                ? AlertsMon::yes
                                : AlertsMon::no;

                        Snd snd(
                                snd_msg,
                                audio::SfxId::END,
                                IgnoreMsgIfOriginSeen::yes,
                                actor->m_pos,
                                actor,
                                SndVol::low,
                                alerts);

                        snd.run();
                }
        }

        // If not permanent, count down turns left and possibly erase self
        if (m_nr_turns_left > -1) {
                --m_nr_turns_left;

                if (m_nr_turns_left <= 0) {
                        game_time::erase_mob(this, true);
                }
        }
}

std::string Smoke::name(const Article article) const
{
        std::string name;

        if (article == Article::the) {
                name = "the ";
        }

        return name + "smoke";
}

Color Smoke::color() const
{
        return colors::gray();
}

// -----------------------------------------------------------------------------
// Force Field
// -----------------------------------------------------------------------------
void ForceField::on_new_turn()
{
        // If not permanent, count down turns left and possibly erase self
        if (m_nr_turns_left <= -1) {
                return;
        }

        --m_nr_turns_left;

        if (m_nr_turns_left <= 0) {
                game_time::erase_mob(this, true);
        }
}

std::string ForceField::name(const Article article) const
{
        std::string name =
                (article == Article::a)
                ? "a"
                : "the";

        name += " force field";

        return name;
}

Color ForceField::color() const
{
        return colors::orange();
}

// -----------------------------------------------------------------------------
// Dynamite
// -----------------------------------------------------------------------------
void LitDynamite::on_new_turn()
{
        --m_nr_turns_left;

        if (m_nr_turns_left <= 0) {
                const P p(m_pos);

                // Removing the dynamite before the explosion, so it can't be
                // rendered after the explosion (e.g. if there are "more"
                // prompts).
                game_time::erase_mob(this, true);

                // NOTE: This object is now deleted

                explosion::run(p, ExplType::expl, EmitExplSnd::yes);
        }
}

std::string LitDynamite::name(const Article article) const
{
        std::string name =
                (article == Article::a)
                ? "a"
                : "the";

        return name + " lit stick of dynamite";
}

Color LitDynamite::color() const
{
        return colors::light_red();
}

// -----------------------------------------------------------------------------
// Flare
// -----------------------------------------------------------------------------
void LitFlare::on_new_turn()
{
        --m_nr_turns_left;

        if (m_nr_turns_left <= 0) {
                game_time::erase_mob(this, true);
        }
}

void LitFlare::add_light(Array2<bool>& light) const
{
        const int radi = g_fov_radi_int;

        P p0(std::max(0, m_pos.x - radi),
             std::max(0, m_pos.y - radi));

        P p1(std::min(map::w() - 1, m_pos.x + radi),
             std::min(map::h() - 1, m_pos.y + radi));

        Array2<bool> hard_blocked(map::dims());

        map_parsers::BlocksLos()
                .run(hard_blocked,
                     R(p0, p1),
                     MapParseMode::overwrite);

        FovMap fov_map;
        fov_map.hard_blocked = &hard_blocked;
        fov_map.light = &map::g_light;
        fov_map.dark = &map::g_dark;

        const auto light_fov = fov::run(m_pos, fov_map);

        for (int y = p0.y; y <= p1.y; ++y) {
                for (int x = p0.x; x <= p1.x; ++x) {
                        if (!light_fov.at(x, y).is_blocked_hard) {
                                light.at(x, y) = true;
                        }
                }
        }
}

std::string LitFlare::name(const Article article) const
{
        std::string name =
                (article == Article::a)
                ? "a"
                : "the";

        name += " lit flare";

        return name;
}

Color LitFlare::color() const
{
        return colors::yellow();
}

}  // namespace terrain
