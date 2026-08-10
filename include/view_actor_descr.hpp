// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef VIEW_ACTOR_DESCR_HPP
#define VIEW_ACTOR_DESCR_HPP

#include <string>

#include "state.hpp"
#include "text_page.hpp"

namespace actor
{
class Actor;
}  // namespace actor

// The description of a monster - a standard text page, the same kind the
// story beats are told on (see TextPageState). The text is built once when
// the page is started (nothing about the described actor changes while it
// is open).
class ViewActorDescr : public TextPageState
{
public:
        ViewActorDescr(actor::Actor& actor) :
                m_actor(actor) {}

        StateId id() const override;

private:
        std::string page_title() const override;

        std::string page_text() const override;

        actor::Actor& m_actor;
};

#endif  // VIEW_ACTOR_DESCR_HPP
