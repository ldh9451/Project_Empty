#pragma once

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <vector>

namespace godot {

// Negative density is solid; the surface is density == 0.
// One shared lattice prevents independently edited chunks from disagreeing.
class ProjectHVoxelTerrain : public Node3D {
    GDCLASS(ProjectHVoxelTerrain, Node3D)

    static constexpr int NX = 48, NY = 32, NZ = 48, CHUNK = 16;
    struct Chunk {
        Vector3i origin;
        MeshInstance3D *mesh = nullptr;
        CollisionShape3D *collision = nullptr;
        int triangles = 0;
    };
    std::vector<float> density;
    std::vector<Chunk> chunks;
    Ref<StandardMaterial3D> material;
    double last_build_ms = 0.0;

    int index(int x, int y, int z) const;
    float at(int x, int y, int z) const;
    Vector3 gradient(int x, int y, int z) const;
    void rebuild_chunk(Chunk &chunk);
    void create_chunks();

protected:
    static void _bind_methods();

public:
    void _ready() override;
    void reset_terrain(int seed = 7);
    // center is terrain-local, radius is in lattice/world units (unit scale).
    // Positive amount removes material; negative amount adds it.
    void sculpt(Vector3 center, double radius, double amount);
    double sample_density(Vector3 position) const;
    int get_triangle_count() const;
    int get_chunk_count() const;
    double get_last_build_ms() const;
};
}
