#pragma once

#include <array>
#include <map>
#include <vector>
#include <tuple>

namespace meltdown
{
    constexpr float voxel_size = 1.0f;
    constexpr int dungeon_chunk_size = 8;

    struct CellPosition
    {
        int x = 0, y = 0, z = 0;
        bool operator<(const CellPosition &b) const
        {
            return std::tie(x, y, z) < std::tie(b.x, b.y, b.z);
        }
        bool operator==(const CellPosition &b) const
        {
            return x == b.x && y == b.y && z == b.z;
        }
        CellPosition operator+(const CellPosition &b) const
        {
            return {x + b.x, y + b.y, z + b.z};
        }
    };

    enum class VoxelKind
    {
        Air, Floor, Stone, Pillar
    };

    struct DungeonCell
    {
        VoxelKind kind = VoxelKind::Air;
        float hp = 44;
        int owner = 0;
        bool warning = false;
        bool solid() const
        {
            return kind != VoxelKind::Air;
        }
    };

    struct DungeonChunk
    {
        std::array<DungeonCell, dungeon_chunk_size * dungeon_chunk_size * dungeon_chunk_size> cells;
        unsigned revision = 1;
    };

    struct GridHit
    {
        float fraction = 2;
        CellPosition cell;
    };

    class DungeonGrid
    {
        void touch(CellPosition p);

    public:
        std::map<CellPosition, DungeonChunk> chunks;
        static const std::array<CellPosition, 6> neighbors;
        static int floor_div(int value);
        static CellPosition chunk_of(CellPosition p);
        static int local_index(CellPosition p);
        static CellPosition world_cell(float x, float y, float z);
        DungeonCell get(CellPosition p) const;
        void set(CellPosition p, DungeonCell cell);
        void create_floor();
        // Enumerate only chunks intersecting the query, including negative coordinates.
        std::vector<CellPosition> occupied(CellPosition lo, CellPosition hi) const;
        GridHit sweep(const std::array<float, 3> &start, const std::array<float, 3> &end,
            const std::array<float, 3> &half, int ignore_owner = 0, bool include_floor = false) const;
        // Six-face connectivity, crossing chunk boundaries. Only Stone participates.
        std::vector<CellPosition> unsupported() const;
    };
}
