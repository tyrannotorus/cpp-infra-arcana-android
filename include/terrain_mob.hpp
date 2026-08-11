// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef TERRAIN_MOB_HPP
#define TERRAIN_MOB_HPP

#include <string>

#include "colors.hpp"
#include "global.hpp"
#include "terrain.hpp"
#include "terrain_data.hpp"

struct P;
template <typename T>
class Array2;

namespace terrain
{
class Smoke : public Terrain
{
public:
        Smoke(const P& pos, const TerrainData* const data) :
                Terrain(pos, data) {}

        ~Smoke() = default;

        std::string name(Article article) const override;

        Color color() const override;

        void set_nr_turns(const int value)
        {
                m_nr_turns_left = value;
        }

        void on_placed() override;

        void on_new_turn() override;

        void expire()
        {
                m_nr_turns_left = 0;
        }

protected:
        int m_nr_turns_left {1};
};

class Mist : public Terrain
{
public:
        Mist(const P& pos, const TerrainData* const data) :
                Terrain(pos, data) {}

        ~Mist() = default;

        std::string name(Article article) const override;

        Color color() const override;

        void set_nr_turns(const int value)
        {
                m_nr_turns_left = value;
        }

        void on_placed() override;

        void on_new_turn() override;

        void expire()
        {
                m_nr_turns_left = 0;
        }

protected:
        int m_nr_turns_left {1};
};

class ForceField : public Terrain
{
public:
        ForceField(const P& pos, const TerrainData* const data) :
                Terrain(pos, data) {}

        ~ForceField() = default;

        void set_nr_turns(const int value)
        {
                m_nr_turns_left = value;
        }

        void on_new_turn() override;

        std::string name(Article article) const override;

        Color color() const override;

protected:
        int m_nr_turns_left {1};
};

class LitDynamite : public Terrain
{
public:
        LitDynamite(const P& pos, const TerrainData* const data) :
                Terrain(pos, data) {}

        ~LitDynamite() = default;

        std::string name(Article article) const override;

        Color color() const override;

        void set_nr_turns(const int value)
        {
                m_nr_turns_left = value;
        }

        // TODO: Lit dynamite should add light on their own cell (just one cell)
        // void add_light(Array2<bool>& light) const;

        void on_new_turn() override;

        // Explodes, whether the fuse has run out or a catalyst set it off.
        // NOTE: This object is deleted!
        void detonate();

private:
        int m_nr_turns_left {1};
};

class LitFlare : public Terrain
{
public:
        LitFlare(const P& pos, const TerrainData* const data) :
                Terrain(pos, data) {}

        ~LitFlare() = default;

        std::string name(Article article) const override;

        Color color() const override;

        void set_nr_turns(const int value)
        {
                m_nr_turns_left = value;
        }

        void on_new_turn() override;

        void add_light(Array2<bool>& light) const override;

private:
        int m_nr_turns_left {1};
};

}  // namespace terrain

#endif  // TERRAIN_MOB_HPP
