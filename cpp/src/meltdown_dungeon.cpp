#include "meltdown_sim.h"

namespace meltdown
{
    void Simulation::create_dungeon()
    {
        dungeon.create_floor();
        // Short, open-ended walls leave the original combat routes and camera readable.
        for (const auto origin : {CellPosition{6, 0, -5}, CellPosition{-12, 0, 6}, CellPosition{10, 0, 10}})
        {
            for (int x = 0; x < 6; ++x)
            {
                for (int y = 0; y < 3; ++y)
                    dungeon.set(origin + CellPosition{x, y, 0}, {VoxelKind::Stone});
            }
        }
        for (const auto &object : environment)
        {
            if (object.kind != EnvironmentKind::Pillar)
                continue;
            for (int y = 0; y < pillar_block_count; ++y)
            {
                dungeon.set(DungeonGrid::world_cell(object.position.x, float(y), object.position.z),
                    {VoxelKind::Pillar, object.hp, object.id});
            }
        }
    }

    void Simulation::damage_voxel(CellPosition p, float damage, const std::shared_ptr<AttackRecord> &record)
    {
        auto cell = dungeon.get(p);
        if (cell.kind != VoxelKind::Stone || cell.warning)
            return;
        if (record)
        {
            if (std::find(record->hit_cells.begin(), record->hit_cells.end(), p) != record->hit_cells.end())
                return;
            record->hit_cells.push_back(p);
        }
        cell.hp = std::max(0.0f, cell.hp - damage);
        dungeon.set(p, cell.hp > 0 ? cell : DungeonCell{});
        burst({p.x + 0.5f, p.z + 0.5f}, 3);
    }

    void Simulation::warn_unsupported()
    {
        auto cells = dungeon.unsupported();
        if (cells.empty())
            return;
        for (const auto p : cells)
        {
            auto cell = dungeon.get(p);
            cell.warning = true;
            dungeon.set(p, cell);
        }
        collapses.push_back({std::move(cells), 1.0f});
    }

    void Simulation::update_dungeon(float delta)
    {
        for (auto &collapse : collapses)
        {
            collapse.remaining -= delta;
            if (collapse.remaining > 0)
                continue;
            // Remove the whole unsupported group before predicting any of its trajectories.
            for (const auto p : collapse.cells)
                dungeon.set(p, {});
            for (const auto p : collapse.cells)
            {
                FallingBlock block;
                block.id = next_id++;
                block.position = {p.x + 0.5f, p.z + 0.5f};
                block.height = p.y + block_half_height;
                block.layer = p.y;
                block.falling = true;
                predict_landing(block);
                falling_blocks.push_back(block);
            }
            collapse.cells.clear();
        }
        collapses.erase(std::remove_if(collapses.begin(), collapses.end(), [](const Collapse &collapse)
        {
            return collapse.cells.empty();
        }), collapses.end());
    }

    void Simulation::predict_landing(FallingBlock &block) const
    {
        Vec position = block.position;
        float height = block.height;
        float speed = block.vertical_speed;
        // Fixed small steps for terrain-aware markers; gameplay uses swept contact each tick.
        constexpr float step = 1.0f / 60.0f;
        for (int i = 0; i < 300; ++i)
        {
            const Vec end = position + block.velocity * step;
            const float bottom = height + speed * step - 0.5f * block_gravity * step * step;
            const auto hit = dungeon.sweep({position.x, height, position.z}, {end.x, bottom, end.z},
                {0.5f, 0.5f, 0.5f}, block.owner_id, true);
            if (hit.fraction <= 1 || bottom <= block_half_height)
            {
                const float fraction = hit.fraction <= 1 ? hit.fraction :
                    std::clamp((height - block_half_height) / (height - bottom), 0.0f, 1.0f);
                block.landing = position + (end - position) * fraction;
                block.landing_height = std::max(block_half_height, height + (bottom - height) * fraction);
                return;
            }
            position = end;
            height = bottom;
            speed -= block_gravity * step;
        }
        block.landing = position;
        block.landing_height = block_half_height;
    }
}
