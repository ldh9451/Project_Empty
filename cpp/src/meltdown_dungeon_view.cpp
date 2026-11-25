#include "meltdown_arena.h"
#include <godot_cpp/classes/array_mesh.hpp>

using namespace godot;

void MeltdownArena::set_dungeon_material(const Ref<Material> &value)
{
    dungeon_material = value;
    for (auto &entry : dungeon_visuals)
        entry.second.revision = 0;
}

Ref<Material> MeltdownArena::get_dungeon_material() const
{
    return dungeon_material;
}

void MeltdownArena::clear_dungeon_visuals()
{
    for (const auto &entry : dungeon_visuals)
    {
        remove_child(entry.second.mesh);
        entry.second.mesh->queue_free();
    }
    dungeon_visuals.clear();
}

void MeltdownArena::present_dungeon()
{
    for (const auto &entry : simulation.dungeon.chunks)
    {
        auto &visual = dungeon_visuals[entry.first];
        if (!visual.mesh)
        {
            visual.mesh = memnew(MeshInstance3D);
            visual.mesh->set_name("DungeonChunk_" + String::num_int64(entry.first.x) + "_" +
                String::num_int64(entry.first.y) + "_" + String::num_int64(entry.first.z));
            add_child(visual.mesh);
        }
        if (visual.revision == entry.second.revision)
            continue;
        visual.revision = entry.second.revision;
        PackedVector3Array vertices, normals;
        PackedVector2Array uvs;
        PackedColorArray colors;
        constexpr int n = meltdown::dungeon_chunk_size;
        const meltdown::CellPosition origin{entry.first.x * n, entry.first.y * n, entry.first.z * n};
        for (int i = 0; i < n * n * n; ++i)
        {
            const auto &cell = entry.second.cells[i];
            // Pillars retain their replaceable block scenes and warning animation.
            if (!cell.solid() || cell.kind == meltdown::VoxelKind::Pillar)
                continue;
            const auto p = origin + meltdown::CellPosition{i % n, i / n % n, i / (n * n)};
            const Color color = cell.warning ? Color(1, 0.55, 0.12) : cell.hp < 44 ? Color(0.65, 0.35, 0.19) :
                cell.kind == meltdown::VoxelKind::Floor ? Color(0.035, 0.045, 0.06) : Color(0.28, 0.30, 0.32);
            for (const auto direction : meltdown::DungeonGrid::neighbors)
            {
                if (simulation.dungeon.get(p + direction).solid())
                    continue;
                const Vector3 normal(direction.x, direction.y, direction.z);
                const Vector3 u = direction.y ? Vector3(1, 0, 0) : Vector3(0, 1, 0);
                const Vector3 v = normal.cross(u);
                const Vector3 center = Vector3(p.x + 0.5f, p.y + 0.5f, p.z + 0.5f) + normal * 0.5f;
                const Vector3 corners[] = {center - u * 0.5f - v * 0.5f, center + u * 0.5f - v * 0.5f,
                    center + u * 0.5f + v * 0.5f, center - u * 0.5f + v * 0.5f};
                const Vector2 coordinates[] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
                for (int index : {0, 2, 1, 0, 3, 2})
                {
                    vertices.push_back(corners[index]);
                    normals.push_back(normal);
                    uvs.push_back(coordinates[index]);
                    colors.push_back(color);
                }
            }
        }
        if (vertices.is_empty())
        {
            visual.mesh->set_mesh(Ref<Mesh>());
            continue;
        }
        Array arrays;
        arrays.resize(Mesh::ARRAY_MAX);
        arrays[Mesh::ARRAY_VERTEX] = vertices;
        arrays[Mesh::ARRAY_NORMAL] = normals;
        arrays[Mesh::ARRAY_TEX_UV] = uvs;
        arrays[Mesh::ARRAY_COLOR] = colors;
        Ref<ArrayMesh> mesh;
        mesh.instantiate();
        mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
        visual.mesh->set_mesh(mesh);
        if (dungeon_material.is_valid())
            visual.mesh->set_material_override(dungeon_material);
        else
        {
            Ref<StandardMaterial3D> fallback;
            fallback.instantiate();
            fallback->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
            visual.mesh->set_material_override(fallback);
        }
    }
}
