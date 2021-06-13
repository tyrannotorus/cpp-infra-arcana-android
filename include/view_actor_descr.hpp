// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef VIEW_ACTOR_DESCR_HPP
#define VIEW_ACTOR_DESCR_HPP

#include <string>
#include <vector>

#include "colors.hpp"
#include "info_screen_state.hpp"
#include "state.hpp"

namespace actor
{
class Actor;
}  // namespace actor

class ViewActorDescr : public InfoScreenState
{
public:
        ViewActorDescr(actor::Actor& actor) :
                m_actor(actor) {}

        void draw() override;

        void update() override;

        StateId id() const override;

private:
        std::string title() const override;

        InfoScreenType type() const override
        {
                return InfoScreenType::single_screen;
        }

        actor::Actor& m_actor;
};

#endif  // VIEW_ACTOR_DESCR_HPP
