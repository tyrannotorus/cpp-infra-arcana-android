// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef MAP_BUILDER_HPP
#define MAP_BUILDER_HPP

#include <memory>
#include <vector>

#include "array2.hpp"
#include "map_templates.hpp"
#include "pos.hpp"

class MapController;
class MapBuilder;

enum class MapType
{
        deep_one_lair,
        magic_pool,
        egypt,
        high_priest,
        intro_forest,
        rat_cave,
        std,
        trapez
};

// -----------------------------------------------------------------------------
// map_builder
// -----------------------------------------------------------------------------
namespace map_builder
{
std::unique_ptr<MapBuilder> make(MapType map_type);

}  // namespace map_builder

// -----------------------------------------------------------------------------
// MapBuilder
// -----------------------------------------------------------------------------
class MapBuilder
{
public:
        virtual ~MapBuilder() = default;

        void build();

private:
        virtual bool build_specific() = 0;

        virtual std::unique_ptr<MapController> map_controller() const;
};

// -----------------------------------------------------------------------------
// MapBuilderTemplateLevel
// -----------------------------------------------------------------------------
class MapBuilderTemplateLevel : public MapBuilder
{
public:
        virtual ~MapBuilderTemplateLevel() = default;

protected:
        const Array2<char>& get_template() const
        {
                return m_template;
        }

private:
        bool build_specific() final;

        virtual LevelTemplId template_id() const = 0;

        virtual bool allow_transform_template() const
        {
                return true;
        }

        virtual void handle_template_pos(const P& p, char c) = 0;

        virtual void on_template_built() {}

        Array2<char> m_template {P(0, 0)};
};

// -----------------------------------------------------------------------------
// MapBuilderStd
// -----------------------------------------------------------------------------
class MapBuilderStd : public MapBuilder
{
public:
        ~MapBuilderStd() = default;

private:
        bool build_specific() override;

        std::unique_ptr<MapController> map_controller() const override;
};

// -----------------------------------------------------------------------------
// MapBuilderDeepOneLair
// -----------------------------------------------------------------------------
class MapBuilderDeepOneLair : public MapBuilderTemplateLevel
{
public:
        MapBuilderDeepOneLair();

        ~MapBuilderDeepOneLair() = default;

private:
        LevelTemplId template_id() const override
        {
                return LevelTemplId::deep_one_lair;
        }

        void handle_template_pos(const P& p, char c) override;

        void on_template_built() override;

        const char m_passage_symbol;
};

// -----------------------------------------------------------------------------
// MapBuilderMagicPool
// -----------------------------------------------------------------------------
class MapBuilderMagicPool : public MapBuilderTemplateLevel
{
public:
        MapBuilderMagicPool();

        ~MapBuilderMagicPool() = default;

private:
        LevelTemplId template_id() const override
        {
                return LevelTemplId::magic_pool;
        }

        void handle_template_pos(const P& p, char c) override;

        void on_template_built() override;
};

// -----------------------------------------------------------------------------
// MapBuilderIntroForest
// -----------------------------------------------------------------------------
class MapBuilderIntroForest : public MapBuilderTemplateLevel
{
public:
        MapBuilderIntroForest() = default;

        ~MapBuilderIntroForest() = default;

private:
        LevelTemplId template_id() const override
        {
                return LevelTemplId::intro_forest;
        }

        bool allow_transform_template() const override
        {
                return false;
        }

        void handle_template_pos(const P& p, char c) override;

        void on_template_built() override;

        std::vector<P> m_possible_grave_positions {};
        std::vector<P> m_possible_statue_positions {};
};

// -----------------------------------------------------------------------------
// MapBuilderEgypt
// -----------------------------------------------------------------------------
class MapBuilderEgypt : public MapBuilderTemplateLevel
{
public:
        MapBuilderEgypt();

        ~MapBuilderEgypt() = default;

private:
        LevelTemplId template_id() const override
        {
                return LevelTemplId::egypt;
        }

        void handle_template_pos(const P& p, char c) override;

        void on_template_built() override;

        const char m_stair_symbol;
};

// -----------------------------------------------------------------------------
// MapBuilderRatCave
// -----------------------------------------------------------------------------
class MapBuilderRatCave : public MapBuilderTemplateLevel
{
public:
        MapBuilderRatCave() = default;

        ~MapBuilderRatCave() = default;

private:
        LevelTemplId template_id() const override
        {
                return LevelTemplId::rat_cave;
        }

        bool allow_transform_template() const override
        {
                return false;
        }

        void handle_template_pos(const P& p, char c) override;

        void on_template_built() override;
};

// -----------------------------------------------------------------------------
// MapBuilderBoss
// -----------------------------------------------------------------------------
class MapBuilderBoss : public MapBuilderTemplateLevel
{
public:
        MapBuilderBoss() = default;

        ~MapBuilderBoss() = default;

private:
        LevelTemplId template_id() const override
        {
                return LevelTemplId::high_priest;
        }

        bool allow_transform_template() const override
        {
                return false;
        }

        void handle_template_pos(const P& p, char c) override;

        void on_template_built() override;

        std::unique_ptr<MapController> map_controller() const override;
};

// -----------------------------------------------------------------------------
// MapBuilderTrapez
// -----------------------------------------------------------------------------
class MapBuilderTrapez : public MapBuilderTemplateLevel
{
public:
        MapBuilderTrapez() = default;

        virtual ~MapBuilderTrapez() = default;

private:
        LevelTemplId template_id() const override
        {
                return LevelTemplId::trapez;
        }

        void handle_template_pos(const P& p, char c) override;
};

#endif  // MAP_BUILDER_HPP
