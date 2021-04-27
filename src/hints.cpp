// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "hints.hpp"

#include <string.h>
#include <string>
#include <utility>

#include "config.hpp"
#include "debug.hpp"
#include "io.hpp"
#include "msg_log.hpp"
#include "popup.hpp"
#include "saving.hpp"
#include "state.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static bool s_hints_displayed[(size_t)hints::Id::END];

static const std::string s_title_prefix = "Hint: ";

static const std::string s_msg_end =
        " (These hints can be disabled in the options menu.)";

static std::pair<std::string, std::string> id_to_text(const hints::Id id)
{
        switch (id)
        {
        case hints::Id::altars:
                return {
                        "Altars",
                        "All spells are cast at a higher level when standing "
                        "at an altar - this includes both spells cast from "
                        "manuscripts and from memory."};

        case hints::Id::fountains:
                return {
                        "Fountains",
                        "Drinking from a fountain usually restores a bit of "
                        "health, spirit, and shock (but they can sometimes "
                        "have other effects, both good and bad!). Fountains "
                        "can be drunk from several times, but each time there "
                        "is a chance that it will dry up permanently."};

        case hints::Id::destroying_corpses:
                return {
                        "Destroying corpses",
                        "Corpses can be destroyed by pressing [k] or [w]. This "
                        "can be very useful against certain types of monsters. "
                        "Some weapons, such as Machetes, makes it easier to "
                        "destroy corpses - check the item description to see "
                        "if a weapon has such a bonus. Also, a well-placed "
                        "stick of dynamite or Molotov Cocktail is usually an "
                        "effective way of stopping persistent monsters."};

        case hints::Id::unload_weapons:
                return {
                        "Unloading weapons",
                        "Ammunition can be unloaded from firearms on the "
                        "ground by pressing [u] or [G]."};

        case hints::Id::infected:
                return {
                        "Infected",
                        "Infections should be treated as soon as possible. "
                        "The common way of doing this is by using the "
                        "Medical Bag. It only requires a small number of turns "
                        "and resources, but if the work is interrupted, the "
                        "effort is wasted (no medical resources are lost "
                        "on interruption however). An untreated infection "
                        "will eventually turn into a disease (50% maximum "
                        "hit points), which can only be removed through "
                        "special means such as drinking certain potions."};

        case hints::Id::overburdened:
                return {
                        "Overburdened",
                        "Carrying too much weight makes movement take twice "
                        "as much time. This is a very dangerous and "
                        "detrimental situation."};

        case hints::Id::high_shock:
                return {
                        "High shock",
                        "Being in a state of extreme shock (stress, paranoia) "
                        "will cause a sanity hit. One way to reduce shock, "
                        "and thereby avoiding or prolonging the sanity hit, "
                        "is to find a source of light - for example through "
                        "activating an Electric Lantern or igniting a Flare."};

        default:
                ASSERT(false);
                return {"", ""};
        }
}

// -----------------------------------------------------------------------------
// hints
// -----------------------------------------------------------------------------
namespace hints
{
void init()
{
        memset(s_hints_displayed, 0, sizeof(s_hints_displayed));
}

void save()
{
        for (size_t i = 0; i < (size_t)Id::END; ++i)
        {
                saving::put_bool(s_hints_displayed[i]);
        }
}

void load()
{
        for (size_t i = 0; i < (size_t)Id::END; ++i)
        {
                s_hints_displayed[i] = saving::get_bool();
        }
}

void display(const Id id)
{
        if (!config::should_display_hints())
        {
                return;
        }

        if (s_hints_displayed[(size_t)id])
        {
                return;
        }

        msg_log::more_prompt();

        io::clear_screen();
        states::draw();
        io::update_screen();

        io::sleep(100);

        const auto text = id_to_text(id);

        if (text.second.empty())
        {
                ASSERT(false);

                return;
        }

        popup::Popup(popup::AddToMsgHistory::yes)
                .set_title(s_title_prefix + text.first)
                .set_msg(text.second + s_msg_end)
                .run();

        s_hints_displayed[(size_t)id] = true;
}

}  // namespace hints
