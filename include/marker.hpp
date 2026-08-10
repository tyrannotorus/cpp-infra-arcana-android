// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef MARKER_HPP
#define MARKER_HPP

#include <memory>
#include <string>
#include <vector>

#include "array2.hpp"
#include "direction.hpp"
#include "global.hpp"
#include "pos.hpp"
#include "random.hpp"
#include "state.hpp"

enum class SpellSkill;

struct Color;

namespace item
{
class Explosive;
class Item;
class Wpn;
}  // namespace item

namespace terrain
{
class Terrain;
}  // namespace terrain

namespace io
{
struct InputData;
}  // namespace io

namespace marker
{
// Places the reticle of the NEXT marker state opened on this cell,
// instead of it auto-targeting or taking over the look pin (consumed by
// the first MarkerState::on_start that runs afterwards).
//
// Used for putting the player back exactly where they were aiming after
// an action taken from within the marker itself - the [ swap ] and
// [ reload ] pins, which close the marker so that the turn they cost can
// pass (see actor::player_state::g_is_aim_marker_pending).
void request_start_pos(const P& pos);

}  // namespace marker

// -----------------------------------------------------------------------------
// Abstract marker state base class
// -----------------------------------------------------------------------------
// All marker states share the center pin targeting interaction: dragging
// pans the map with the marker fixed at the view's centering point (the
// "pin" - drawn as the corner bracket reticle). One finger dragging is
// for looking/targeting, movement swipes/direction keys NEVER move the
// marker (blanket rule - swipes are reserved for player movement).
class MarkerState : public State
{
public:
        MarkerState(const P& origin) :
                m_origin(origin)
        {}

        virtual ~MarkerState() = default;

        void on_start() final;

        void on_popped() final;

        void on_map_panned() override;

        void draw() final;

        // A marker draws its aim line, reticle and blast overlay on the map
        // display and nowhere else, so a tile animation refresh can redraw
        // it along with the map underneath (see states::draw_map_display)
        bool has_map_display_draw() const final
        {
                return true;
        }

        void draw_map_display() final
        {
                draw();
        }

        bool draw_overlayed() const final
        {
                return true;
        }

        void on_window_resized() override;

        void update() final;

        StateId id() const final;

protected:
        virtual void on_start_hook() {}

        virtual void on_popped_hook() {}

        // Whether a movement swipe cancels the marker interaction and
        // performs the movement (markers used during normal play). Not
        // allowed e.g. when surveying the map from the trait pick menu
        // (moving mid character creation), or for a forced teleport.
        virtual bool allow_swipe_cancel() const
        {
                return true;
        }

        void draw_marker(
                const std::vector<P>& line,
                int warn_until_including_king_dist,
                int warn_from_king_dist,
                int out_of_range_from_king_dist,
                int blocked_from_idx);

        // The reticle and path colors. Looking and aiming are green, with
        // the stretches outside the effective range in orange; throwing
        // has a warm ramp of its own, so that a throw in progress is never
        // mistaken for the look pin. Out of range (and blocked) is red for
        // all of them.
        virtual const Color& marker_color_normal() const;

        virtual const Color& marker_color_warning() const;

        // Fire etc
        virtual void handle_input(const io::InputData& input) = 0;

        // Print messages
        virtual void on_moved() = 0;

        // Used for overlays, etc - it should be pretty rare that this is needed
        virtual void on_draw() {}

        virtual bool use_player_tgt() const
        {
                return false;
        }

        virtual bool is_pos_blocked(const P& pos) const;

        virtual bool show_blocked() const
        {
                return false;
        }

        virtual Range effective_king_dist_range() const
        {
                return {-1, -1};
        }

        virtual int max_king_dist() const
        {
                return -1;
        }

        const P m_origin;

        P m_pos {0, 0};

        // Can be set by child classes to temporarily enable/disable drawing
        bool m_allow_draw {true};

private:
        // Whether a start position was requested for this marker (see
        // marker::request_start_pos) - it then starts there. The request
        // is consumed either way.
        bool try_go_to_requested_pos();

        // Whether the marker was opened while looking around (drag-to-look)
        // - it then starts on the cell being looked at
        bool try_go_to_look_pin();

        bool try_go_to_tgt();

        void try_go_to_closest_enemy();
};

// -----------------------------------------------------------------------------
// View marker state
// -----------------------------------------------------------------------------
// Detailed look mode: drag-to-look with actor descriptions available via
// the look command / the [ describe ] context pin.
//
// NOTE: Only used where the game view is otherwise unreachable (the trait
// pick info menu) - in the plain game state, dragging the map IS looking
// (see GameState::on_map_panned).
class Viewing : public MarkerState
{
public:
        Viewing(const P& origin) :
                MarkerState(origin) {}

protected:
        bool allow_swipe_cancel() const override
        {
                // Only used from the trait pick info menu - the player
                // must not move mid character creation
                return false;
        }

        void on_moved() override;

        void handle_input(const io::InputData& input) override;

        bool use_player_tgt() const override
        {
                return true;
        }

        bool show_blocked() const override
        {
                return false;
        }
};

// -----------------------------------------------------------------------------
// Aim (and fire) marker state
// -----------------------------------------------------------------------------
class Aiming : public MarkerState
{
public:
        Aiming(const P& origin, item::Wpn& wpn) :
                MarkerState(origin),
                m_wpn(wpn) {}

protected:
        void on_moved() override;

        void handle_input(const io::InputData& input) override;

        bool use_player_tgt() const override
        {
                return true;
        }

        bool show_blocked() const override
        {
                return true;
        }

        // Red: the line of fire never reads as the green look pin or the
        // orange throw
        const Color& marker_color_normal() const override;

        Range effective_king_dist_range() const override;

        int max_king_dist() const override;

        item::Wpn& m_wpn;
};

// -----------------------------------------------------------------------------
// Aim (and strike) melee weapon marker state (mostly for long reach weapons)
// -----------------------------------------------------------------------------
class AimingMeleeWpn : public MarkerState
{
public:
        AimingMeleeWpn(const P& origin, item::Wpn& wpn) :
                MarkerState(origin),
                m_wpn(wpn) {}

protected:
        void on_moved() override;

        void handle_input(const io::InputData& input) override;

        bool use_player_tgt() const override
        {
                return true;
        }

        bool is_pos_blocked(const P& pos) const override;

        bool show_blocked() const override
        {
                return true;
        }

        // Red, like any other attack (see Aiming)
        const Color& marker_color_normal() const override;

        int max_king_dist() const override;

        item::Wpn& m_wpn;
};

// -----------------------------------------------------------------------------
// Throw attack marker state
// -----------------------------------------------------------------------------
class Throwing : public MarkerState
{
public:
        Throwing(const P& origin, item::Item& inv_item) :
                MarkerState(origin),
                m_inv_item(&inv_item) {}

protected:
        void on_moved() override;

        void handle_input(const io::InputData& input) override;

        bool use_player_tgt() const override
        {
                // A throw does NOT auto-target: opened while looking around
                // it takes over the cell being looked at, and otherwise it
                // starts on the player, to be dragged onto a target (the
                // [ throw ] button appears once it is off the player).
                return false;
        }

        bool show_blocked() const override
        {
                return true;
        }

        // Orange: a throw being aimed must never read as the green look pin
        // or the red attack (see also ThrowingExplosive)
        const Color& marker_color_normal() const override;

        Range effective_king_dist_range() const override;

        int max_king_dist() const override;

        item::Item* m_inv_item;
};

// -----------------------------------------------------------------------------
// Throw explosive marker state
// -----------------------------------------------------------------------------
class ThrowingExplosive : public MarkerState
{
public:
        ThrowingExplosive(const P& origin, item::Explosive& explosive) :
                MarkerState(origin),
                m_explosive(explosive) {}

protected:
        void on_draw() override;

        void on_moved() override;

        void handle_input(const io::InputData& input) override;

        bool use_player_tgt() const override
        {
                return false;
        }

        bool show_blocked() const override
        {
                return true;
        }

        // As for any other throw (see Throwing)
        const Color& marker_color_normal() const override;

        int max_king_dist() const override;

        item::Explosive& m_explosive;
};

// -----------------------------------------------------------------------------
// Teleport control marker state
// -----------------------------------------------------------------------------
class CtrlTele : public MarkerState
{
public:
        CtrlTele(const P& origin, Array2<bool> blocked, int max_dist = -1);

protected:
        bool allow_swipe_cancel() const override
        {
                // The teleport is not optional
                return false;
        }

        void on_start_hook() override;

        void on_moved() override;

        void handle_input(const io::InputData& input) override;

private:
        int chance_of_success_pct() const;

        P m_origin;
        int m_max_dist;
        Array2<bool> m_blocked;
};

// -----------------------------------------------------------------------------
// Control Object marker state
// -----------------------------------------------------------------------------
class CtrlObjAction
{
public:
        virtual ~CtrlObjAction() = default;

        virtual bool can_control(
                const terrain::Terrain& terrain,
                SpellSkill skill) const = 0;

        virtual DidAction run(
                terrain::Terrain& terrain,
                SpellSkill skill) const = 0;

        virtual std::string menu_label(
                const terrain::Terrain& terrain) const = 0;

        virtual char menu_key() const = 0;
};

class CtrlObjOpen : public CtrlObjAction
{
public:
        bool can_control(
                const terrain::Terrain& terrain,
                SpellSkill skill) const override;

        DidAction run(
                terrain::Terrain& terrain,
                SpellSkill skill) const override;

        std::string menu_label(const terrain::Terrain& terrain) const override;

        char menu_key() const override;
};

class CtrlObjCloseDoor : public CtrlObjAction
{
public:
        bool can_control(
                const terrain::Terrain& terrain,
                SpellSkill skill) const override;

        DidAction run(
                terrain::Terrain& terrain,
                SpellSkill skill) const override;

        std::string menu_label(const terrain::Terrain& terrain) const override;

        char menu_key() const override;
};

class CtrlObjJamDoor : public CtrlObjAction
{
public:
        bool can_control(
                const terrain::Terrain& terrain,
                SpellSkill skill) const override;

        DidAction run(
                terrain::Terrain& terrain,
                SpellSkill skill) const override;

        std::string menu_label(const terrain::Terrain& terrain) const override;

        char menu_key() const override;
};

class CtrlObjDeactivateCrystal : public CtrlObjAction
{
public:
        bool can_control(
                const terrain::Terrain& terrain,
                SpellSkill skill) const override;

        DidAction run(
                terrain::Terrain& terrain,
                SpellSkill skill) const override;

        std::string menu_label(const terrain::Terrain& terrain) const override;

        char menu_key() const override;
};

class CtrlObjStrike : public CtrlObjAction
{
public:
        bool can_control(
                const terrain::Terrain& terrain,
                SpellSkill skill) const override;

        DidAction run(
                terrain::Terrain& terrain,
                SpellSkill skill) const override;

        std::string menu_label(const terrain::Terrain& terrain) const override;

        char menu_key() const override;
};

class CtrlObjDestrWall : public CtrlObjAction
{
public:
        bool can_control(
                const terrain::Terrain& terrain,
                SpellSkill skill) const override;

        DidAction run(
                terrain::Terrain& terrain,
                SpellSkill skill) const override;

        std::string menu_label(const terrain::Terrain& terrain) const override;

        char menu_key() const override;
};

using CtrlObjActionPtr = std::shared_ptr<CtrlObjAction>;

class CtrlObj : public MarkerState
{
public:
        CtrlObj(const P& origin, int max_dist, SpellSkill skill);

protected:
        void on_start_hook() override;

        void on_moved() override;

        void handle_input(const io::InputData& input) override;

private:
        int current_dist() const;

        bool is_allowed_at_dist() const;

        void set_terrain();
        void set_possible_actions();

        CtrlObjActionPtr query_control() const;

        P m_origin;
        int m_max_dist;
        SpellSkill m_skill;
        std::vector<CtrlObjActionPtr> m_possible_actions {};
        terrain::Terrain* m_terrain {nullptr};
};

#endif  // MARKER_HPP
