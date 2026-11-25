#include "dungeon_grid.h"
#include <algorithm>
#include <cmath>
#include <set>

namespace meltdown
{
    const std::array<CellPosition, 6> DungeonGrid::neighbors = {{{1, 0, 0}, {-1, 0, 0},
        {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}}};

    int DungeonGrid::floor_div(int value)
    {
        const int remainder = value % dungeon_chunk_size;
        return value / dungeon_chunk_size - (remainder < 0 ? 1 : 0);
    }

    CellPosition DungeonGrid::chunk_of(CellPosition p)
    {
        return {floor_div(p.x), floor_div(p.y), floor_div(p.z)};
    }

    int DungeonGrid::local_index(CellPosition p)
    {
        const auto c = chunk_of(p);
        constexpr int n = dungeon_chunk_size;
        return p.x - c.x * n + n * (p.y - c.y * n + n * (p.z - c.z * n));
    }

    CellPosition DungeonGrid::world_cell(float x, float y, float z)
    {
        return {static_cast<int>(std::floor(x / voxel_size)), static_cast<int>(std::floor(y / voxel_size)),
            static_cast<int>(std::floor(z / voxel_size))};
    }

    DungeonCell DungeonGrid::get(CellPosition p) const
    {
        const auto found = chunks.find(chunk_of(p));
        return found == chunks.end() ? DungeonCell{} : found->second.cells[local_index(p)];
    }

    void DungeonGrid::touch(CellPosition p)
    {
        const auto found = chunks.find(chunk_of(p));
        if (found != chunks.end())
            ++found->second.revision;
    }

    void DungeonGrid::set(CellPosition p, DungeonCell cell)
    {
        chunks[chunk_of(p)].cells[local_index(p)] = cell;
        touch(p);
        for (const auto &direction : neighbors)
        {
            const auto adjacent = p + direction;
            if (!(chunk_of(p) == chunk_of(adjacent)))
                touch(adjacent);
        }
    }

    void DungeonGrid::create_floor()
    {
        chunks.clear();
        for (int z = -24; z < 24; ++z)
        {
            for (int x = -24; x < 24; ++x)
                set({x, -1, z}, {VoxelKind::Floor});
        }
    }

    std::vector<CellPosition> DungeonGrid::occupied(CellPosition lo, CellPosition hi) const
    {
        std::vector<CellPosition> result;
        constexpr int n = dungeon_chunk_size;
        const auto first = chunk_of(lo);
        const auto last = chunk_of(hi);
        for (int z = first.z; z <= last.z; ++z)
        {
            for (int y = first.y; y <= last.y; ++y)
            {
                for (int x = first.x; x <= last.x; ++x)
                {
                    const auto found = chunks.find({x, y, z});
                    if (found == chunks.end())
                        continue;
                    for (int cz = std::max(lo.z, z * n); cz <= std::min(hi.z, z * n + n - 1); ++cz)
                    {
                        for (int cy = std::max(lo.y, y * n); cy <= std::min(hi.y, y * n + n - 1); ++cy)
                        {
                            for (int cx = std::max(lo.x, x * n); cx <= std::min(hi.x, x * n + n - 1); ++cx)
                            {
                                const CellPosition p{cx, cy, cz};
                                if (found->second.cells[local_index(p)].solid())
                                    result.push_back(p);
                            }
                        }
                    }
                }
            }
        }
        return result;
    }

    GridHit DungeonGrid::sweep(const std::array<float, 3> &start, const std::array<float, 3> &end,
        const std::array<float, 3> &half, int ignore_owner, bool include_floor) const
    {
        const auto lo = world_cell(std::min(start[0], end[0]) - half[0],
            std::min(start[1], end[1]) - half[1], std::min(start[2], end[2]) - half[2]);
        const auto hi = world_cell(std::max(start[0], end[0]) + half[0],
            std::max(start[1], end[1]) + half[1], std::max(start[2], end[2]) + half[2]);
        GridHit hit;
        for (const auto p : occupied(lo, hi))
        {
            const auto cell = get(p);
            if ((!include_floor && cell.kind == VoxelKind::Floor) || (ignore_owner && cell.owner == ignore_owner))
                continue;
            float enter = 0, leave = 1;
            const float position[] = {float(p.x), float(p.y), float(p.z)};
            for (int axis = 0; axis < 3; ++axis)
            {
                const float movement = end[axis] - start[axis];
                const float low = position[axis] - half[axis];
                const float high = position[axis] + voxel_size + half[axis];
                if (std::abs(movement) < 0.000001f)
                {
                    if (start[axis] <= low + 0.00001f || start[axis] >= high - 0.00001f)
                        leave = -1;
                    continue;
                }
                float a = (low - start[axis]) / movement;
                float b = (high - start[axis]) / movement;
                if (a > b)
                    std::swap(a, b);
                enter = std::max(enter, a);
                leave = std::min(leave, b);
            }
            if (enter <= leave && leave > 0.00001f && enter < hit.fraction)
                hit = {enter, p};
        }
        return hit;
    }

    std::vector<CellPosition> DungeonGrid::unsupported() const
    {
        std::set<CellPosition> remaining;
        constexpr int n = dungeon_chunk_size;
        for (const auto &entry : chunks)
        {
            const CellPosition origin{entry.first.x * n, entry.first.y * n, entry.first.z * n};
            for (int i = 0; i < n * n * n; ++i)
            {
                if (entry.second.cells[i].kind == VoxelKind::Stone && !entry.second.cells[i].warning)
                    remaining.insert(origin + CellPosition{i % n, i / n % n, i / (n * n)});
            }
        }
        std::vector<CellPosition> result;
        while (!remaining.empty())
        {
            std::vector<CellPosition> component{*remaining.begin()};
            remaining.erase(component.front());
            bool supported = false;
            for (size_t i = 0; i < component.size(); ++i)
            {
                for (const auto &direction : neighbors)
                {
                    const auto next = component[i] + direction;
                    const auto cell = get(next);
                    if (cell.kind == VoxelKind::Floor || cell.kind == VoxelKind::Pillar)
                        supported = true;
                    if (remaining.erase(next))
                        component.push_back(next);
                }
            }
            if (!supported)
                result.insert(result.end(), component.begin(), component.end());
        }
        return result;
    }
}
