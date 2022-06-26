// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef TERRAIN_HPP
#define TERRAIN_HPP

#include <optional>
#include <string>
#include <vector>

#include "colors.hpp"
#include "direction.hpp"
#include "gfx.hpp"
#include "global.hpp"
#include "pos.hpp"
#include "terrain_data.hpp"

namespace io
{
enum class GraphicsCycle;
}  // namespace io

namespace actor
{
class Actor;
}  // namespace actor

namespace item
{
class Item;
}  // namespace item

enum class Bg;

template <typename T>
class Array2;

enum class AllowAction;
enum class Article;
enum class DmgType;
enum class Matl;
enum class Verbose;

namespace terrain
{
enum class BurnState
{
        not_burned,
        burning,
        has_burned
};

enum class DidTriggerTrap
{
        no,
        yes
};

enum class DidOpen
{
        no,
        yes
};

enum class DidClose
{
        no,
        yes
};

enum class PrintRevealMsg
{
        no,
        if_seen,
        yes,
};

enum class Id;
class Lever;

void make_blood(const P& origin);
void make_gore(const P& origin);

class ItemContainer
{
public:
        ItemContainer();

        ~ItemContainer();

        void init(Id terrain_id, int nr_items_to_attempt);

        const std::vector<item::Item*>& items() const
        {
                return m_items;
        }

        bool is_empty() const
        {
                return m_items.empty();
        }

        void open(const P& terrain_pos, actor::Actor* actor_opening);

        void clear();

        void destroy_single_fragile();

private:
        void on_item_found(item::Item* item, const P& terrain_pos);

        std::vector<item::Item*> m_items;
};

class Terrain
{
public:
        Terrain(const P& p, const TerrainData* const data) :
                m_data(data),
                m_pos(p) {}

        Terrain() = delete;

        virtual ~Terrain() = default;

        Id id() const
        {
                return m_data->id;
        }

        P pos() const
        {
                return m_pos;
        }

        bool is_hidden() const
        {
                return m_is_hidden;
        }

        bool is_burning() const
        {
                return m_burn_state == BurnState::burning;
        }

        void try_make_bloody();

        void try_put_gore();

        bool is_bloody() const;

        bool has_gore() const;

        gfx::TileId gore_tile() const
        {
                return m_gore_tile;
        }

        char gore_character() const
        {
                return m_gore_character;
        }

        void clear_gore();

        void try_corrupt_color();

        bool is_corrupted_color() const;

        int shock_when_adj() const;

        void cycle_graphics(io::GraphicsCycle cycle);

        virtual Color color() const;

        virtual Color color_bg() const;

        virtual bool is_walkable() const
        {
                return m_data->move_rules.is_walkable;
        }

        // Is this given property allowing movement into this terrain, when it
        // normally wouldn't be?
        virtual bool is_property_allowing_move(PropId id) const
        {
                return m_data->move_rules.is_property_allowing_move(id);
        }

        virtual bool is_sound_passable() const
        {
                return m_data->is_sound_passable;
        }

        virtual bool is_floor_like() const
        {
                return m_data->is_floor_like;
        }

        virtual bool is_los_passable() const
        {
                return m_data->is_los_passable;
        }

        virtual bool is_projectile_passable() const
        {
                return m_data->is_projectile_passable;
        }

        virtual bool is_smoke_passable() const
        {
                return m_data->is_smoke_passable;
        }

        virtual char character() const
        {
                return m_data->character;
        }

        virtual gfx::TileId tile() const
        {
                return m_data->tile;
        }

        virtual bool can_have_corpse() const
        {
                return m_data->can_have_corpse;
        }

        virtual bool can_have_blood() const
        {
                return m_data->can_have_blood;
        }

        virtual bool can_have_gore() const
        {
                return m_data->can_have_gore;
        }

        virtual bool can_have_trap() const
        {
                return m_data->can_have_trap;
        }

        virtual bool can_have_item() const
        {
                return m_data->can_have_item;
        }

        virtual Matl matl() const
        {
                return m_data->matl_type;
        }

        virtual void on_placed()
        {
        }

        virtual void on_new_turn();

        virtual WasDestroyed on_finished_burning();

        virtual void hit(
                DmgType dmg_type,
                actor::Actor* actor,
                std::optional<P> from_pos = std::nullopt,
                std::optional<int> dmg = std::nullopt);

        virtual void reveal(const PrintRevealMsg print_reveal_msg)
        {
                (void)print_reveal_msg;
        }

        virtual void on_revealed_from_searching() {}

        virtual AllowAction pre_bump(actor::Actor& actor_bumping);

        virtual void bump(actor::Actor& actor_bumping);

        virtual void on_leave(actor::Actor& actor_leaving)
        {
                (void)actor_leaving;
        }

        virtual DidOpen open(actor::Actor* const actor_opening)
        {
                (void)actor_opening;

                return DidOpen::no;
        }

        virtual DidClose close(actor::Actor* const actor_closing)
        {
                (void)actor_closing;

                return DidClose::no;
        }

        virtual void on_lever_pulled(Lever* const lever)
        {
                (void)lever;
        }

        virtual void add_light(Array2<bool>& light) const;

        virtual std::string name(Article article) const = 0;

        const TerrainData* m_data {nullptr};
        ItemContainer m_item_container {};
        BurnState m_burn_state {BurnState::not_burned};
        bool m_started_burning_this_turn {false};

protected:
        virtual void on_new_turn_hook() {}

        virtual void on_hit(
                const DmgType dmg_type,
                actor::Actor* const actor,
                const P& from_pos,
                const int dmg)
        {
                (void)dmg_type;
                (void)actor;
                (void)from_pos;
                (void)dmg;
        }

        virtual Color color_default() const
        {
                return colors::white();
        }

        virtual Color color_bg_default() const;

        void try_start_burning(Verbose verbose);

        virtual DidTriggerTrap trigger_trap(actor::Actor* const actor)
        {
                (void)actor;

                return DidTriggerTrap::no;
        }

        virtual void add_light_hook(Array2<bool>& light) const
        {
                (void)light;
        }

        virtual int base_shock_when_adj() const;

        bool m_is_hidden {false};
        gfx::TileId m_gore_tile {gfx::TileId::END};
        char m_gore_character {0};
        P m_pos {};

private:
        bool m_is_bloody {false};

        // Corrupted by a Strange Color monster
        int m_nr_turns_color_corrupted {-1};

        Color m_burn_color_bg {};
        Color m_corrupt_color {};
};

enum class FloorType
{
        common,
        cave,
        stone_path
};

class Floor : public Terrain
{
public:
        Floor(const P& p, const TerrainData* data);

        Floor() = delete;

        gfx::TileId tile() const override;

        std::string name(Article article) const override;

        FloorType m_type;

private:
        Color color_default() const override;

        void on_hit(
                DmgType dmg_type,
                actor::Actor* actor,
                const P& from_pos,
                int dmg) override;
};

class Carpet : public Terrain
{
public:
        Carpet(const P& p, const TerrainData* data);

        Carpet() = delete;

        std::string name(Article article) const override;

        WasDestroyed on_finished_burning() override;

private:
        Color color_default() const override;

        void on_hit(
                DmgType dmg_type,
                actor::Actor* actor,
                const P& from_pos,
                int dmg) override;
};

enum class GrassType
{
        common,
        withered
};

class Grass : public Terrain
{
public:
        Grass(const P& p, const TerrainData* data);

        Grass() = delete;

        gfx::TileId tile() const override;
        std::string name(Article article) const override;

        GrassType m_type;

private:
        Color color_default() const override;

        void on_hit(
                DmgType dmg_type,
                actor::Actor* actor,
                const P& from_pos,
                int dmg) override;
};

class Bush : public Terrain
{
public:
        Bush(const P& p, const TerrainData* data);

        Bush() = delete;

        std::string name(Article article) const override;
        WasDestroyed on_finished_burning() override;

        GrassType m_type;

private:
        Color color_default() const override;

        void on_hit(
                DmgType dmg_type,
                actor::Actor* actor,
                const P& from_pos,
                int dmg) override;
};

class Vines : public Terrain
{
public:
        Vines(const P& p, const TerrainData* data);

        Vines() = delete;

        std::string name(Article article) const override;
        WasDestroyed on_finished_burning() override;

private:
        Color color_default() const override;

        void on_hit(
                DmgType dmg_type,
                actor::Actor* actor,
                const P& from_pos,
                int dmg) override;
};

class Chains : public Terrain
{
public:
        Chains(const P& p, const TerrainData* data);

        Chains() = delete;

        std::string name(Article article) const override;

        void bump(actor::Actor& actor_bumping) override;

private:
        Color color_default() const override;

        Color color_bg_default() const override;

        void on_hit(
                DmgType dmg_type,
                actor::Actor* actor,
                const P& from_pos,
                int dmg) override;
};

class Grate : public Terrain
{
public:
        Grate(const P& p, const TerrainData* data);

        Grate() = delete;

        std::string name(Article article) const override;

private:
        Color color_default() const override;

        void on_hit(
                DmgType dmg_type,
                actor::Actor* actor,
                const P& from_pos,
                int dmg) override;
};

class Brazier : public Terrain
{
public:
        Brazier(const P& p, const TerrainData* const data) :
                Terrain(p, data) {}

        Brazier() = delete;

        std::string name(Article article) const override;

private:
        Color color_default() const override;

        void on_hit(
                DmgType dmg_type,
                actor::Actor* actor,
                const P& from_pos,
                int dmg) override;

        void add_light_hook(Array2<bool>& light) const override;
};

enum class WallType
{
        common,
        common_alt,
        pillar,
        pillar_broken,
        cave,
        egypt,
        cliff,
        leng_monestary
};

class Wall : public Terrain
{
public:
        Wall(const P& p, const TerrainData* data);

        Wall() = delete;

        gfx::TileId tile() const override;

        std::string name(Article article) const override;
        gfx::TileId front_wall_tile() const;
        gfx::TileId top_wall_tile() const;

        void set_rnd_common_wall();
        void set_moss_grown();

        WallType m_type;
        bool m_is_mossy;

private:
        Color color_default() const override;

        void on_hit(
                DmgType dmg_type,
                actor::Actor* actor,
                const P& from_pos,
                int dmg) override;
};

class RubbleLow : public Terrain
{
public:
        RubbleLow(const P& p, const TerrainData* data);

        RubbleLow() = delete;

        std::string name(Article article) const override;

private:
        Color color_default() const override;

        void on_hit(
                DmgType dmg_type,
                actor::Actor* actor,
                const P& from_pos,
                int dmg) override;
};

class Bones : public Terrain
{
public:
        Bones(const P& p, const TerrainData* data);

        Bones() = delete;

        std::string name(Article article) const override;

private:
        Color color_default() const override;

        void on_hit(
                DmgType dmg_type,
                actor::Actor* actor,
                const P& from_pos,
                int dmg) override;
};

class RubbleHigh : public Terrain
{
public:
        RubbleHigh(const P& p, const TerrainData* data);

        RubbleHigh() = delete;

        std::string name(Article article) const override;

private:
        Color color_default() const override;

        void on_hit(
                DmgType dmg_type,
                actor::Actor* actor,
                const P& from_pos,
                int dmg) override;
};

class GraveStone : public Terrain
{
public:
        GraveStone(const P& p, const TerrainData* data);

        GraveStone() = delete;

        std::string name(Article article) const override;

        void set_inscription(const std::string& str)
        {
                m_inscr = str;
        }

        void bump(actor::Actor& actor_bumping) override;

private:
        Color color_default() const override;

        void on_hit(
                DmgType dmg_type,
                actor::Actor* actor,
                const P& from_pos,
                int dmg) override;

        std::string m_inscr {};
};

class ChurchBench : public Terrain
{
public:
        ChurchBench(const P& p, const TerrainData* data);

        ChurchBench() = delete;

        std::string name(Article article) const override;

private:
        Color color_default() const override;

        void on_hit(
                DmgType dmg_type,
                actor::Actor* actor,
                const P& from_pos,
                int dmg) override;
};

enum class StatueType
{
        common,
        ghoul
};

class Statue : public Terrain
{
public:
        Statue(const P& p, const TerrainData* data);
        Statue() = delete;

        std::string name(Article article) const override;

        void bump(actor::Actor& actor_bumping) override;

        void set_type(const StatueType type)
        {
                m_type = type;
        }

        void set_inscription(const std::string& str)
        {
                m_inscr = str;
        }

        void set_player_bg(Bg bg);

        gfx::TileId tile() const override;

        void topple(Dir direction, actor::Actor* actor_toppling = nullptr);

private:
        Color color_default() const override;

        void on_hit(
                DmgType dmg_type,
                actor::Actor* actor,
                const P& from_pos,
                int dmg) override;

        int base_shock_when_adj() const override;

        StatueType m_type;
        std::string m_inscr {};
        Bg m_player_bg;
};

class Stalagmite : public Terrain
{
public:
        Stalagmite(const P& p, const TerrainData* data);
        Stalagmite() = delete;

        std::string name(Article article) const override;

private:
        Color color_default() const override;

        void on_hit(
                DmgType dmg_type,
                actor::Actor* actor,
                const P& from_pos,
                int dmg) override;
};

class Stairs : public Terrain
{
public:
        Stairs(const P& p, const TerrainData* data);
        Stairs() = delete;

        std::string name(Article article) const override;

        void bump(actor::Actor& actor_bumping) override;

        void on_new_turn_hook() override;

        void add_light_hook(Array2<bool>& light) const override;

        void set_fake()
        {
                m_is_fake = true;
        }

        bool is_fake() const
        {
                return m_is_fake;
        }

private:
        Color color_default() const override;

        void on_hit(
                DmgType dmg_type,
                actor::Actor* actor,
                const P& from_pos,
                int dmg) override;

        void player_use_fake_stairs();

        bool m_is_fake {false};
};

class Bridge : public Terrain
{
public:
        Bridge(const P& p, const TerrainData* const data) :
                Terrain(p, data),
                m_axis(Axis::hor) {}

        Bridge() = delete;

        std::string name(Article article) const override;
        gfx::TileId tile() const override;
        char character() const override;

        void set_axis(const Axis axis)
        {
                m_axis = axis;
        }

private:
        Color color_default() const override;

        void on_hit(
                DmgType dmg_type,
                actor::Actor* actor,
                const P& from_pos,
                int dmg) override;

        Axis m_axis;
};

class Liquid : public Terrain
{
public:
        Liquid(const P& p, const TerrainData* data);
        Liquid() = delete;

        std::string name(Article article) const override;

        void bump(actor::Actor& actor_bumping) override;

        LiquidType m_type;

private:
        Color color_default() const override;

        Color color_bg_default() const override;

        void on_hit(
                DmgType dmg_type,
                actor::Actor* actor,
                const P& from_pos,
                int dmg) override;

        void run_magic_pool_effects_on_player();
};

class Chasm : public Terrain
{
public:
        Chasm(const P& p, const TerrainData* data);
        Chasm() = delete;

        std::string name(Article article) const override;

private:
        Color color_default() const override;

        void on_hit(
                DmgType dmg_type,
                actor::Actor* actor,
                const P& from_pos,
                int dmg) override;
};

class Lever : public Terrain
{
public:
        Lever(const P& p, const TerrainData* data);

        Lever() = delete;

        std::string name(Article article) const override;

        gfx::TileId tile() const override;

        void toggle();

        void bump(actor::Actor& actor_bumping) override;

        bool is_left_pos() const
        {
                return m_is_left_pos;
        }

        bool is_linked_to(const Terrain& terrain) const
        {
                return m_linked_terrain == &terrain;
        }

        void set_linked_terrain(Terrain& terrain)
        {
                m_linked_terrain = &terrain;
        }

        void unlink()
        {
                m_linked_terrain = nullptr;
        }

        // Levers linked to the same terrain
        void add_sibbling(Lever* const lever)
        {
                m_sibblings.push_back(lever);
        }

private:
        Color color_default() const override;

        void on_hit(
                DmgType dmg_type,
                actor::Actor* actor,
                const P& from_pos,
                int dmg) override;

        bool m_is_left_pos;

        Terrain* m_linked_terrain;

        std::vector<Lever*> m_sibblings;
};

class Altar : public Terrain
{
public:
        Altar(const P& p, const TerrainData* data);

        Altar() = delete;

        void bump(actor::Actor& actor_bumping) override;

        void on_new_turn() override;

        std::string name(Article article) const override;

private:
        Color color_default() const override;

        void on_hit(
                DmgType dmg_type,
                actor::Actor* actor,
                const P& from_pos,
                int dmg) override;
};

class Tree : public Terrain
{
public:
        Tree(const P& p, const TerrainData* data);
        Tree() = delete;

        gfx::TileId tile() const override;

        std::string name(Article article) const override;

        WasDestroyed on_finished_burning() override;

private:
        Color color_default() const override;

        void on_hit(
                DmgType dmg_type,
                actor::Actor* actor,
                const P& from_pos,
                int dmg) override;

        bool is_fungi() const;

        Color m_color {};
};

// NOTE: In some previous versions, it was possible to inspect the tomb and get
// a hint about its trait ("It has an aura of unrest", "There are foreboding
// carved signs", etc). This is currently not possible - you open the tomb and
// any "trap" it has will trigger. Therefore the TombTrait type could be
// removed, and instead an effect is just randomized when the tomb is
// opened. But it should be kept the way it is; it could be useful. Maybe some
// sort of hint will be re-implemented.
enum class TombTrait
{
        ghost,
        other_undead,  // Zombies, Mummies, ...
        stench,  // Fumes, Ooze-type monster
        cursed,
        END
};

enum class TombAppearance
{
        common,  // Common items
        ornate,  // Minor treasure
        marvelous,  // Major treasure
        END
};

class Tomb : public Terrain
{
public:
        Tomb(const P& pos, const TerrainData* data);
        Tomb() = delete;

        std::string name(Article article) const override;

        gfx::TileId tile() const override;

        void bump(actor::Actor& actor_bumping) override;

        bool is_open() const
        {
                return m_is_open;
        }

        DidOpen open(actor::Actor* actor_opening) override;

private:
        Color color_default() const override;

        void on_hit(
                DmgType dmg_type,
                actor::Actor* actor,
                const P& from_pos,
                int dmg) override;

        DidTriggerTrap trigger_trap(actor::Actor* actor) override;

        void player_loot();

        bool m_is_open;
        bool m_is_trait_known;

        int m_push_lid_one_in_n;
        TombAppearance m_appearance;
        TombTrait m_trait;
};

enum class ChestMatl
{
        wood,
        iron,
        END
};

class Chest : public Terrain
{
public:
        Chest(const P& pos, const TerrainData* data);
        Chest() = delete;

        std::string name(Article article) const override;

        gfx::TileId tile() const override;

        void bump(actor::Actor& actor_bumping) override;

        bool is_open() const
        {
                return m_is_open;
        }

        DidOpen open(actor::Actor* actor_opening) override;

        void hit(
                DmgType dmg_type,
                actor::Actor* actor,
                std::optional<P> from_pos = std::nullopt,
                std::optional<int> dmg = std::nullopt) override;

private:
        Color color_default() const override;

        void on_player_kick();

        void player_loot();

        bool m_is_open;
        bool m_is_locked;

        ChestMatl m_matl;
};

class Cabinet : public Terrain
{
public:
        Cabinet(const P& pos, const TerrainData* data);
        Cabinet() = delete;

        std::string name(Article article) const override;

        gfx::TileId tile() const override;

        void bump(actor::Actor& actor_bumping) override;

        bool is_open() const
        {
                return m_is_open;
        }

        DidOpen open(actor::Actor* actor_opening) override;

        WasDestroyed on_finished_burning() override;

private:
        Color color_default() const override;

        void on_hit(
                DmgType dmg_type,
                actor::Actor* actor,
                const P& from_pos,
                int dmg) override;

        void player_loot();

        bool m_is_open;
};

class Bookshelf : public Terrain
{
public:
        Bookshelf(const P& pos, const TerrainData* data);
        Bookshelf() = delete;

        std::string name(Article article) const override;

        gfx::TileId tile() const override;

        void bump(actor::Actor& actor_bumping) override;

        WasDestroyed on_finished_burning() override;

private:
        Color color_default() const override;

        void on_hit(
                DmgType dmg_type,
                actor::Actor* actor,
                const P& from_pos,
                int dmg) override;

        void player_loot();

        bool m_is_looted;
};

class AlchemistBench : public Terrain
{
public:
        AlchemistBench(const P& pos, const TerrainData* data);
        AlchemistBench() = delete;

        std::string name(Article article) const override;

        gfx::TileId tile() const override;

        void bump(actor::Actor& actor_bumping) override;

        WasDestroyed on_finished_burning() override;

private:
        Color color_default() const override;

        void on_hit(
                DmgType dmg_type,
                actor::Actor* actor,
                const P& from_pos,
                int dmg) override;

        void player_loot();

        bool m_is_looted;
};

enum class FountainEffect
{
        refreshing,
        xp,

        START_OF_BAD_EFFECTS,
        curse,
        disease,
        poison,
        frenzy,
        paralyze,
        blind,
        faint,
        END
};

class Fountain : public Terrain
{
public:
        Fountain(const P& pos, const TerrainData* data);

        Fountain() = delete;

        std::string name(Article article) const override;

        void on_new_turn() override;

        void bump(actor::Actor& actor_bumping) override;

        bool has_drinks_left() const
        {
                return m_has_drinks_left;
        }

        FountainEffect effect() const
        {
                return m_fountain_effect;
        }

        void set_effect(const FountainEffect effect)
        {
                m_fountain_effect = effect;
        }

        void bless();

        void curse();

private:
        Color color_default() const override;

        std::string type_name() const;

        std::string type_indefinite_article() const;

        void on_hit(
                DmgType dmg_type,
                actor::Actor* actor,
                const P& from_pos,
                int dmg) override;

        FountainEffect m_fountain_effect {FountainEffect::END};
        bool m_has_drinks_left {true};
        bool m_is_tried = {false};
};

class Cocoon : public Terrain
{
public:
        Cocoon(const P& pos, const TerrainData* data);

        Cocoon() = delete;

        std::string name(Article article) const override;

        gfx::TileId tile() const override;

        void bump(actor::Actor& actor_bumping) override;

        DidOpen open(actor::Actor* actor_opening) override;

        WasDestroyed on_finished_burning() override;

private:
        Color color_default() const override;

        void on_hit(
                DmgType dmg_type,
                actor::Actor* actor,
                const P& from_pos,
                int dmg) override;

        void player_loot();

        DidTriggerTrap trigger_trap(actor::Actor* actor) override;

        bool m_is_trapped;
        bool m_is_open;
};

}  // namespace terrain

#endif  // TERRAIN_HPP
