// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef TERRAIN_HPP
#define TERRAIN_HPP

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include "colors.hpp"
#include "direction.hpp"
#include "gfx.hpp"
#include "global.hpp"
#include "pos.hpp"
#include "property_data.hpp"
#include "terrain_data.hpp"

namespace map
{
struct MinimapAppearance;
}  // namespace map

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
enum class Material;
enum class Verbose;

namespace terrain
{
class Door;

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

        virtual Color color_default() const
        {
                return colors::white();
        }

        Color color_bg() const;

        virtual std::optional<map::MinimapAppearance> minimap_appearance() const;

        virtual bool is_walkable() const
        {
                return m_data->move_rules.is_walkable;
        }

        virtual bool can_move(const actor::Actor& actor) const
        {
                return m_data->move_rules.can_move(actor);
        }

        // Is this given property allowing movement into this terrain, when it
        // normally wouldn't be?
        virtual bool is_property_allowing_move(prop::Id id) const
        {
                return m_data->move_rules.is_property_allowing_move(id);
        }

        virtual bool is_sound_passable() const
        {
                return m_data->is_sound_passable;
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

        bool can_have_trap() const
        {
                return m_data->can_have_trap;
        }

        virtual bool can_have_item() const
        {
                return m_data->can_have_item;
        }

        virtual Material material() const
        {
                return m_data->material_type;
        }

        virtual void on_placed()
        {
        }

        virtual void on_new_turn();

        virtual WasDestroyed on_finished_burning();

        virtual bool allow_player_melee_attack(
                const DmgType dmg_type,
                const item::Item& wpn) const
        {
                (void)dmg_type;
                (void)wpn;

                return false;
        }

        virtual void hit(
                DmgType dmg_type,
                actor::Actor* actor,
                const P& from_pos = {-1, -1},
                int dmg = -1)
        {
                (void)dmg_type;
                (void)actor;
                (void)from_pos;
                (void)dmg;
        }

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

        virtual void add_light(Array2<bool>& light) const;

        virtual std::string name(Article article) const = 0;

        virtual void set_inscribed() {}

        virtual bool allow_inscribe() const
        {
                return false;
        }

        virtual bool can_be_studied() const
        {
                return false;
        }

        const TerrainData* m_data {nullptr};
        ItemContainer m_item_container {};
        BurnState m_burn_state {BurnState::not_burned};
        bool m_started_burning_this_turn {false};

protected:
        virtual void on_new_turn_hook() {}

        void try_start_burning(Verbose verbose);

        virtual DidTriggerTrap trigger_trap(actor::Actor* actor)
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
        bool m_is_bloody {false};
        P m_pos {};

private:
        virtual void cycle_graphics_hook(io::GraphicsCycle cycle)
        {
                (void)cycle;
        }

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

        Color color_default() const override;

        std::string name(Article article) const override;

        void hit(
                DmgType dmg_type,
                actor::Actor* actor,
                const P& from_pos,
                int dmg) override;

        FloorType m_type;
};

class Carpet : public Terrain
{
public:
        Carpet(const P& p, const TerrainData* data);

        Carpet() = delete;

        std::string name(Article article) const override;

        Color color_default() const override;

        WasDestroyed on_finished_burning() override;

        void hit(
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

        Color color_default() const override;

        void hit(
                DmgType dmg_type,
                actor::Actor* actor,
                const P& from_pos,
                int dmg) override;

        GrassType m_type;
};

class Bush : public Terrain
{
public:
        Bush(const P& p, const TerrainData* data);

        Bush() = delete;

        std::string name(Article article) const override;

        Color color_default() const override;

        WasDestroyed on_finished_burning() override;

        void hit(
                DmgType dmg_type,
                actor::Actor* actor,
                const P& from_pos,
                int dmg) override;

        GrassType m_type;
};

class Vines : public Terrain
{
public:
        Vines(const P& p, const TerrainData* data);

        Vines() = delete;

        std::string name(Article article) const override;

        Color color_default() const override;

        WasDestroyed on_finished_burning() override;

        void hit(
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

        Color color_default() const override;

        void bump(actor::Actor& actor_bumping) override;

        void hit(
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

        Color color_default() const override;

        void hit(
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

        Color color() const override;

        std::optional<map::MinimapAppearance> minimap_appearance() const override;

        bool allow_player_melee_attack(
                DmgType dmg_type,
                const item::Item& wpn) const override;

        void hit(
                DmgType dmg_type,
                actor::Actor* actor,
                const P& from_pos,
                int dmg) override;

        void on_new_turn() override;

private:
        void cycle_graphics_hook(io::GraphicsCycle cycle) override;

        void add_light_hook(Array2<bool>& light) const override;

        void topple(Dir direction, actor::Actor& actor);

        Color m_flicker_color {colors::yellow()};
};

enum class WallType
{
        common,
        common_alt,
        cave,
        egypt,
        mi_go,
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

        Color color_default() const override;

        void set_rnd_common_wall();
        void set_moss_grown();

        void hit(
                DmgType dmg_type,
                actor::Actor* actor,
                const P& from_pos,
                int dmg) override;

        WallType m_type {WallType::common};
        bool m_is_mossy {false};
};

class Pillar : public Terrain
{
public:
        Pillar(const P& p, const TerrainData* data);

        Pillar() = delete;

        gfx::TileId tile() const override;

        std::string name(Article article) const override;

        Color color_default() const override;

        std::optional<map::MinimapAppearance> minimap_appearance() const override;

        void bump(actor::Actor& actor_bumping) override;

        void hit(
                DmgType dmg_type,
                actor::Actor* actor,
                const P& from_pos,
                int dmg) override;

        void on_new_turn() override;

        void set_inscribed() override
        {
                m_is_inscribed = true;
                m_has_player_studied_inscription = false;
                m_is_broken = false;
                m_is_bloody = false;
        }

        bool can_be_studied() const override
        {
                return m_is_inscribed && !m_has_player_studied_inscription;
        }

        bool allow_inscribe() const override
        {
                return !m_is_inscribed;
        }

        bool can_have_blood() const override
        {
                return !m_is_inscribed;
        }

        void set_broken()
        {
                m_is_inscribed = false;
                m_has_player_studied_inscription = false;
                m_is_broken = false;
        }

private:
        bool m_is_inscribed {};
        bool m_has_player_studied_inscription {false};
        bool m_is_broken {};
};

class Petroglyph : public Terrain
{
public:
        Petroglyph(const P& p, const TerrainData* data);

        Petroglyph() = delete;

        std::string name(Article article) const override;

        Color color_default() const override;

        std::optional<map::MinimapAppearance> minimap_appearance() const override;

        void bump(actor::Actor& actor_bumping) override;

        void hit(
                DmgType dmg_type,
                actor::Actor* actor,
                const P& from_pos,
                int dmg) override;

        void on_new_turn() override;

        bool can_be_studied() const override
        {
                return !m_has_player_studied_inscription;
        }

private:
        bool m_has_player_studied_inscription {false};
};

class RubbleLow : public Terrain
{
public:
        RubbleLow(const P& p, const TerrainData* data);

        RubbleLow() = delete;

        std::string name(Article article) const override;

        Color color_default() const override;

        void hit(
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

        Color color_default() const override;

        void hit(
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

        Color color_default() const override;

        void hit(
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

        Color color_default() const override;

        void set_inscription(const std::string& str)
        {
                m_inscr = str;
        }

        void bump(actor::Actor& actor_bumping) override;

        void hit(
                DmgType dmg_type,
                actor::Actor* actor,
                const P& from_pos,
                int dmg) override;

private:
        std::string m_inscr {};
};

class ChurchBench : public Terrain
{
public:
        ChurchBench(const P& p, const TerrainData* data);

        ChurchBench() = delete;

        std::string name(Article article) const override;

        Color color_default() const override;

        void hit(
                DmgType dmg_type,
                actor::Actor* actor,
                const P& from_pos,
                int dmg) override;

private:
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

        gfx::TileId tile() const override;

        Color color_default() const override;

        void bump(actor::Actor& actor_bumping) override;

        void on_new_turn() override;

        void set_type(const StatueType type)
        {
                m_type = type;
        }

        void set_inscription(const std::string& str)
        {
                m_inscr = str;
        }

        void set_player_bg(Bg bg);

        void topple(Dir direction, actor::Actor* actor_toppling = nullptr);

        bool allow_player_melee_attack(
                DmgType dmg_type,
                const item::Item& wpn) const override;

        void hit(
                DmgType dmg_type,
                actor::Actor* actor,
                const P& from_pos,
                int dmg) override;

private:
        int base_shock_when_adj() const override;

        StatueType m_type;
        std::string m_inscr {};
        Bg m_player_bg;
};

class Urn : public Terrain
{
public:
        Urn(const P& p, const TerrainData* data);
        Urn() = delete;

        std::string name(Article article) const override;

        gfx::TileId tile() const override;

        Color color_default() const override;

        std::optional<map::MinimapAppearance> minimap_appearance() const override;

        void bump(actor::Actor& actor_bumping) override;

        void on_new_turn() override;

        void topple(Dir direction, actor::Actor* actor_toppling = nullptr);

        bool allow_player_melee_attack(
                DmgType dmg_type,
                const item::Item& wpn) const override;

        void hit(
                DmgType dmg_type,
                actor::Actor* actor,
                const P& from_pos,
                int dmg) override;

        void set_inscribed() override
        {
                m_is_inscribed = true;
                m_has_player_studied_inscription = false;
        }

        bool can_be_studied() const override
        {
                return m_is_inscribed && !m_has_player_studied_inscription;
        }

        bool allow_inscribe() const override
        {
                return !m_is_inscribed;
        }

private:
        bool m_is_inscribed {false};
        bool m_has_player_studied_inscription {false};
};

class Stalagmite : public Terrain
{
public:
        Stalagmite(const P& p, const TerrainData* data);
        Stalagmite() = delete;

        std::string name(Article article) const override;

        Color color_default() const override;

        void hit(
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

        Color color_default() const override;

        std::optional<map::MinimapAppearance> minimap_appearance() const override;

        void bump(actor::Actor& actor_bumping) override;

        void on_new_turn_hook() override;

        void add_light_hook(Array2<bool>& light) const override;

        void hit(
                DmgType dmg_type,
                actor::Actor* actor,
                const P& from_pos,
                int dmg) override;

        void set_fake()
        {
                m_is_fake = true;
        }

        bool is_fake() const
        {
                return m_is_fake;
        }

private:
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
        Color color_default() const override;

        void hit(
                DmgType dmg_type,
                actor::Actor* actor,
                const P& from_pos,
                int dmg) override;

        void set_axis(const Axis axis)
        {
                m_axis = axis;
        }

private:
        Axis m_axis;
};

class Liquid : public Terrain
{
public:
        Liquid(const P& p, const TerrainData* data);
        Liquid() = delete;

        std::string name(Article article) const override;

        Color color_default() const override;

        std::optional<map::MinimapAppearance> minimap_appearance() const override;

        void bump(actor::Actor& actor_bumping) override;

        void hit(
                DmgType dmg_type,
                actor::Actor* actor,
                const P& from_pos,
                int dmg) override;

        LiquidType m_type;

private:
        void run_magic_pool_effects_on_player();
};

class Chasm : public Terrain
{
public:
        Chasm(const P& p, const TerrainData* data);
        Chasm() = delete;

        std::string name(Article article) const override;

        Color color_default() const override;

        void hit(
                DmgType dmg_type,
                actor::Actor* actor,
                const P& from_pos,
                int dmg) override;
};

class CrystalKey : public Terrain
{
public:
        CrystalKey(const P& p, const TerrainData* data);

        CrystalKey() = delete;

        std::string name(Article article) const override;

        gfx::TileId tile() const override;

        Color color_default() const override;

        std::optional<map::MinimapAppearance> minimap_appearance() const override;

        void bump(actor::Actor& actor_bumping) override;

        // Prints message, gives XP etc, and deactivates the crystal.
        void player_deactivate();

        // Only deactives the crystal (no messages etc).
        void deactivate();

        void hit(
                DmgType dmg_type,
                actor::Actor* actor,
                const P& from_pos,
                int dmg) override;

        bool is_active() const
        {
                return m_is_active;
        }

        bool is_linked_to(const Door* const door) const
        {
                return m_linked_door == door;
        }

        void set_linked_door(Door& door)
        {
                m_linked_door = &door;
        }

        void unlink()
        {
                m_linked_door = nullptr;
        }

        // Crystals linked to the same door.
        void add_sibbling(CrystalKey* const crystal)
        {
                m_sibblings.push_back(crystal);
        }

private:
        void add_light_hook(Array2<bool>& light) const override;

        bool m_is_active {true};
        Door* m_linked_door {nullptr};
        std::vector<CrystalKey*> m_sibblings {};
};

class Altar : public Terrain
{
public:
        Altar(const P& p, const TerrainData* data);

        Altar() = delete;

        Color color_default() const override;

        std::optional<map::MinimapAppearance> minimap_appearance() const override;

        void bump(actor::Actor& actor_bumping) override;

        void on_new_turn() override;

        std::string name(Article article) const override;

        void hit(
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

        Color color_default() const override;

        WasDestroyed on_finished_burning() override;

        void hit(
                DmgType dmg_type,
                actor::Actor* actor,
                const P& from_pos,
                int dmg) override;

private:
        bool is_fungi() const;

        Color m_color {};
};

// NOTE: In some previous versions, it was possible to inspect the tomb and get a hint about its
// trait ("It has an aura of unrest", "There are foreboding carved signs", etc). This is currently
// not possible - you open the tomb and any "trap" it has will trigger. Therefore the TombTrait type
// could be removed, and instead an effect is just randomized when the tomb is opened. But it should
// be kept the way it is; it could be useful. Maybe some sort of hint will be re-implemented.
enum class TombTrapId
{
        monster,
        fumes,
        cursed,
        END
};

enum class TombAppearance
{
        common,     // Common items
        ornate,     // Minor treasure
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

        Color color_default() const override;

        std::optional<map::MinimapAppearance> minimap_appearance() const override;

        void bump(actor::Actor& actor_bumping) override;

        bool is_open() const
        {
                return m_is_open;
        }

        DidOpen open(actor::Actor* actor_opening) override;

        void hit(
                DmgType dmg_type,
                actor::Actor* actor,
                const P& from_pos,
                int dmg) override;

private:
        DidTriggerTrap trigger_trap(actor::Actor* actor) override;

        void trigger_trap_mon() const;
        std::string get_random_allowed_mon_id() const;
        std::string get_mon_appear_msg(const std::string& mon_id) const;

        void trigger_trap_fumes() const;

        void trigger_trap_cursed() const;

        void player_loot();

        bool m_is_open {false};
        bool m_is_trait_known {false};

        int m_push_lid_one_in_n;
        TombAppearance m_appearance {TombAppearance::common};
        TombTrapId m_trap_id {TombTrapId::END};
};

enum class ChestMaterial
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

        Color color_default() const override;

        void bump(actor::Actor& actor_bumping) override;

        bool is_open() const
        {
                return m_is_open;
        }

        DidOpen open(actor::Actor* actor_opening) override;

        bool allow_player_melee_attack(
                DmgType dmg_type,
                const item::Item& wpn) const override;

        void hit(
                DmgType dmg_type,
                actor::Actor* actor,
                const P& from_pos,
                int dmg) override;

private:
        void on_player_kick();

        void player_loot();

        bool m_is_open;
        bool m_is_locked;

        ChestMaterial m_material;
};

class Cabinet : public Terrain
{
public:
        Cabinet(const P& pos, const TerrainData* data);
        Cabinet() = delete;

        std::string name(Article article) const override;

        gfx::TileId tile() const override;

        Color color_default() const override;

        void bump(actor::Actor& actor_bumping) override;

        bool is_open() const
        {
                return m_is_open;
        }

        DidOpen open(actor::Actor* actor_opening) override;

        WasDestroyed on_finished_burning() override;

        void hit(
                DmgType dmg_type,
                actor::Actor* actor,
                const P& from_pos,
                int dmg) override;

private:
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

        Color color_default() const override;

        void bump(actor::Actor& actor_bumping) override;

        WasDestroyed on_finished_burning() override;

        void hit(
                DmgType dmg_type,
                actor::Actor* actor,
                const P& from_pos,
                int dmg) override;

private:
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

        Color color_default() const override;

        void bump(actor::Actor& actor_bumping) override;

        WasDestroyed on_finished_burning() override;

        void hit(
                DmgType dmg_type,
                actor::Actor* actor,
                const P& from_pos,
                int dmg) override;

private:
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

        Color color_default() const override;

        std::optional<map::MinimapAppearance> minimap_appearance() const override;

        void on_new_turn() override;

        void bump(actor::Actor& actor_bumping) override;

        void hit(
                DmgType dmg_type,
                actor::Actor* actor,
                const P& from_pos,
                int dmg) override;

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
        std::string type_name() const;

        std::string type_indefinite_article() const;

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

        Color color_default() const override;

        void bump(actor::Actor& actor_bumping) override;

        DidOpen open(actor::Actor* actor_opening) override;

        WasDestroyed on_finished_burning() override;

        void hit(
                DmgType dmg_type,
                actor::Actor* actor,
                const P& from_pos,
                int dmg) override;

private:
        void player_loot();

        DidTriggerTrap trigger_trap(actor::Actor* actor) override;

        bool m_is_trapped;
        bool m_is_open;
};

}  // namespace terrain

#endif  // TERRAIN_HPP
