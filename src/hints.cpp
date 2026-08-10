// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "hints.hpp"

#include <cstring>
#include <memory>
#include <string>
#include <utility>

#include "actor.hpp"
#include "config.hpp"
#include "debug.hpp"
#include "io.hpp"
#include "map.hpp"
#include "msg_log.hpp"
#include "saving.hpp"
#include "state.hpp"
#include "text_page.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static bool s_hints_displayed[(size_t)hints::Id::END];

static const std::string s_title_prefix = "Hint: ";

static std::pair<std::string, std::string> id_to_text(const hints::Id id)
{
        switch (id) {
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
                        "health, spirit, and mental shock (but they can sometimes "
                        "have other effects, both good and bad!). Fountains "
                        "can be drunk from several times, but each time there "
                        "is a chance that it will dry up permanently."};

        case hints::Id::destroying_corpses:
                return {
                        "Destroying corpses",
                        "Corpses can be destroyed by kicking them, which is "
                        "very useful against certain types of monsters. Drag "
                        "the map onto the corpse and tap the [ kick ] button, "
                        "or use the kick action and swipe towards it."

                        "\n\nThe wielded weapon is used instead of the boot if "
                        "it is suited for the work - some weapons, such as "
                        "Machetes, makes it easier to destroy corpses. Check "
                        "the item description to see if a weapon has such a "
                        "bonus. Also, a well-placed stick of dynamite or "
                        "Molotov Cocktail is usually an effective way of "
                        "stopping persistent monsters."};

        case hints::Id::unload_weapons:
                return {
                        "Unloading weapons",
                        "Ammunition can be taken from a firearm on the "
                        "ground, leaving the weapon itself behind, with the "
                        "[ take ammo ] button beside [ pick up ]. A loaded "
                        "firearm found in a container can be emptied the "
                        "same way, when asked whether to pick it up."};

        case hints::Id::infected:
                return {
                        "Infected",
                        "Infections should be treated as soon as possible. "
                        "The common way of doing this is by using the "
                        "Medical Bag. It only requires a small number of turns "
                        "and resources, but if the work is interrupted, the "
                        "effort is wasted (no medical resources are lost "
                        "on interruption however)."

                        "\n\nAn untreated infection will eventually turn into a "
                        "disease (50% maximum hit points), "
                        "which can only be removed through special means such as "
                        "drinking certain potions."};

        case hints::Id::overburdened:
                return {
                        "Overburdened",
                        "Carrying too much weight makes movement take twice "
                        "as much time. This is a very dangerous and "
                        "detrimental situation."};

        case hints::Id::high_shock:
                return {
                        "High shock",
                        "Being in a state of extreme mental shock (stress, paranoia) "
                        "will cause a sanity hit. One way to reduce shock, "
                        "and thereby avoiding or prolonging the sanity hit, "
                        "is to find a source of light - for example through "
                        "activating an Electric Lantern or igniting a Flare."};

        case hints::Id::status_effects:
                return {
                        "Status effects",
                        "A status effect has been applied. "
                        "Status effects are various positive, negative or neutral effects "
                        "applied on a creature. "
                        "Some examples are confusion, burning, invisibility, or "
                        "electricity resistance. "
                        "A simple list of active status effects is shown in the normal "
                        "game screen. "

                        "\n\nIn the character screen (the menu button, then "
                        "\"Character description\"), a more "
                        "detailed list can be seen, including a description of each effect. "

                        "\n\nStatus effects shown with CAPITAL LETTERS are \"permanent\", "
                        "and are only removed if some special action is taken, for example "
                        "using the medical bag to treat a wound."};

        case hints::Id::study_inscription:
                return {
                        "Inscriptions",
                        "There is an inscription here, studying it will yield some experience."

                        "\n\nIt may also recall a spell that you have forgotten, "
                        "or reveal something about carried manuscripts or potions. "
                        "The chance to reveal information about such items is higher with "
                        "more unknown items carried."};

        case hints::Id::kick_brazier:
                return {
                        "Kicking braziers",
                        "Braziers can be kicked over to set creatures on fire "
                        "in a small area."};

        case hints::Id::kick_statue:
                return {
                        "Kicking statues",
                        "Statues can be kicked over to "
                        "damage and stun a creature on the other side."};

        case hints::Id::temporary_and_permanent_shock:
                return {
                        "Temporary and permanent mental shock",

                        "Some situations cause \"temporary\" mental shock, "
                        "which is removed when the situation changes. "
                        "Entering a dark area or standing next to bloodsplatter will "
                        "cause your shock to spike until you move away, for example."

                        "\n\nStanding in bright light will similarly reduce your shock "
                        "until you return to the ambient subterranean gloom."

                        "\n\nSeeing monsters, casting spells, spending time, etc cause "
                        "\"permanent\" shock, which will not go away until "
                        "the next floor is reached, insanity rises (due to shock at 100%), "
                        "or the shock is cured somehow."};

        default:
                ASSERT(false);
                return {"", ""};
        }
}

// A hint is a descriptive page like any other (see TextPageState): the
// title in the top border, the text framed by the divider rules, and
// "tap to continue" in the footer
class HintState : public TextPageState
{
public:
        HintState(std::string title, std::string text) :
                m_title(std::move(title)),
                m_text(std::move(text))
        {}

        void on_start() override
        {
                TextPageState::on_start();

                // Hints are kept in the message history, for reading again
                // later on
                add_text_to_msg_history();
        }

        StateId id() const override
        {
                return StateId::hint;
        }

protected:
        std::string page_title() const override
        {
                return s_title_prefix + m_title;
        }

        std::string page_text() const override
        {
                return m_text;
        }

private:
        const std::string m_title;
        const std::string m_text;
};

static bool should_display_hint(const hints::Id id)
{
        const auto hints_mode = config::hints_mode();

        switch (hints_mode) {
        case HintsMode::once_per_game:
                return !s_hints_displayed[(size_t)id];

        case HintsMode::once:
                return !config::has_seen_hint_global(id);

        case HintsMode::never:
                return false;

        case HintsMode::END:
                break;
        }

        ASSERT(false);

        return false;
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
        for (size_t i = 0; i < (size_t)Id::END; ++i) {
                saving::put_bool(s_hints_displayed[i]);
        }
}

void load()
{
        for (size_t i = 0; i < (size_t)Id::END; ++i) {
                s_hints_displayed[i] = saving::get_bool();
        }
}

void display(const Id id)
{
        if (!should_display_hint(id)) {
                return;
        }

        if (!actor::is_alive(*map::g_player)) {
                return;
        }

        msg_log::more_prompt();

        states::draw();
        io::update_screen();

        io::sleep(100);

        const auto text = id_to_text(id);

        if (text.second.empty()) {
                ASSERT(false);

                return;
        }

        states::run_until_state_done(
                std::make_unique<HintState>(text.first, text.second));

        s_hints_displayed[(size_t)id] = true;

        config::set_hint_seen_global(id);
}

}  // namespace hints
