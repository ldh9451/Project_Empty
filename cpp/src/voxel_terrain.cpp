#include "voxel_terrain.h"
#include "marching_cubes_table.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/concave_polygon_shape3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/fast_noise_lite.hpp>
#include <godot_cpp/classes/static_body3d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/packed_color_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>

using namespace godot;
namespace {
constexpr int CORNERS[8][3] = {
    {0,0,0}, {1,0,0}, {1,1,0}, {0,1,0},
    {0,0,1}, {1,0,1}, {1,1,1}, {0,1,1}
};
constexpr int EDGES[12][2] = {
    {0,1}, {1,2}, {2,3}, {3,0}, {4,5}, {5,6},
    {6,7}, {7,4}, {0,4}, {1,5}, {2,6}, {3,7}
};
const Vector3 OFFSET(-24, -12, -24);
using Clock = std::chrono::steady_clock;
double elapsed_ms(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}
}

void ProjectHVoxelTerrain::_bind_methods() {
    ClassDB::bind_method(D_METHOD("reset_terrain", "seed"), &ProjectHVoxelTerrain::reset_terrain, DEFVAL(7));
    ClassDB::bind_method(D_METHOD("sculpt", "center", "radius", "amount"), &ProjectHVoxelTerrain::sculpt);
    ClassDB::bind_method(D_METHOD("sample_density", "position"), &ProjectHVoxelTerrain::sample_density);
    ClassDB::bind_method(D_METHOD("get_triangle_count"), &ProjectHVoxelTerrain::get_triangle_count);
    ClassDB::bind_method(D_METHOD("get_chunk_count"), &ProjectHVoxelTerrain::get_chunk_count);
    ClassDB::bind_method(D_METHOD("get_last_build_ms"), &ProjectHVoxelTerrain::get_last_build_ms);
}

int ProjectHVoxelTerrain::index(int x, int y, int z) const {
    return x + (NX + 1) * (y + (NY + 1) * z);
}
float ProjectHVoxelTerrain::at(int x, int y, int z) const {
    return density[index(std::clamp(x, 0, NX), std::clamp(y, 0, NY), std::clamp(z, 0, NZ))];
}
Vector3 ProjectHVoxelTerrain::gradient(int x, int y, int z) const {
    return Vector3(at(x+1,y,z)-at(x-1,y,z), at(x,y+1,z)-at(x,y-1,z), at(x,y,z+1)-at(x,y,z-1));
}

void ProjectHVoxelTerrain::_ready() {
    if (!Engine::get_singleton()->is_editor_hint()) reset_terrain();
}

void ProjectHVoxelTerrain::create_chunks() {
    if (!chunks.empty()) return;
    material.instantiate();
    material->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
    material->set_flag(BaseMaterial3D::FLAG_SRGB_VERTEX_COLOR, true);
    material->set_roughness(0.92);
    for (int z = 0; z < NZ; z += CHUNK)
        for (int y = 0; y < NY; y += CHUNK)
            for (int x = 0; x < NX; x += CHUNK) {
                Chunk chunk;
                chunk.origin = Vector3i(x, y, z);
                chunk.mesh = memnew(MeshInstance3D);
                chunk.mesh->set_name(String("Chunk_") + String::num_int64(x) + "_" + String::num_int64(y) + "_" + String::num_int64(z));
                chunk.mesh->set_material_override(material);
                add_child(chunk.mesh);
                auto *body = memnew(StaticBody3D);
                body->set_collision_layer(1);
                body->set_collision_mask(0);
                chunk.mesh->add_child(body);
                chunk.collision = memnew(CollisionShape3D);
                body->add_child(chunk.collision);
                chunks.push_back(chunk);
            }
}

void ProjectHVoxelTerrain::reset_terrain(int seed) {
    const auto start = Clock::now();
    create_chunks();
    density.resize((NX+1)*(NY+1)*(NZ+1));
    Ref<FastNoiseLite> noise;
    noise.instantiate();
    noise->set_seed(seed);
    noise->set_frequency(0.055);
    noise->set_fractal_octaves(3);
    for (int z = 0; z <= NZ; ++z)
        for (int y = 0; y <= NY; ++y)
            for (int x = 0; x <= NX; ++x) {
                const Vector3 p = Vector3(x, y, z) + OFFSET;
                const float height = 1.5f + noise->get_noise_2d(p.x, p.z) * 7.0f;
                // Closed, rounded land block; the outer lattice remains air.
                const float side = std::max(std::abs(p.x), std::abs(p.z)) - 21.5f;
                float value = std::max({float(p.y)-height, -9.5f-float(p.y), side});
                if (x==0 || x==NX || y==0 || y==NY || z==0 || z==NZ) value = std::max(value, 1.0f);
                density[index(x,y,z)] = value;
            }
    for (auto &chunk : chunks) rebuild_chunk(chunk);
    last_build_ms = elapsed_ms(start);
}

void ProjectHVoxelTerrain::rebuild_chunk(Chunk &chunk) {
    PackedVector3Array vertices, normals;
    PackedColorArray colors;
    const Vector3i origin = chunk.origin;
    for (int z = origin.z; z < origin.z+CHUNK; ++z)
        for (int y = origin.y; y < origin.y+CHUNK; ++y)
            for (int x = origin.x; x < origin.x+CHUNK; ++x) {
                float values[8];
                int mask = 0;
                for (int c = 0; c < 8; ++c) {
                    values[c] = at(x+CORNERS[c][0], y+CORNERS[c][1], z+CORNERS[c][2]);
                    if (values[c] < 0.0f) mask |= 1 << c;
                }
                if (mask == 0 || mask == 255) continue;
                Vector3 points[12], edge_normals[12];
                for (int e = 0; e < 12; ++e) {
                    int a = EDGES[e][0], b = EDGES[e][1];
                    if ((values[a] < 0) == (values[b] < 0)) continue;
                    const float t = values[a] / (values[a] - values[b]);
                    Vector3i ca(x+CORNERS[a][0], y+CORNERS[a][1], z+CORNERS[a][2]);
                    Vector3i cb(x+CORNERS[b][0], y+CORNERS[b][1], z+CORNERS[b][2]);
                    points[e] = Vector3(ca).lerp(Vector3(cb), t) + OFFSET;
                    edge_normals[e] = gradient(ca.x,ca.y,ca.z).lerp(gradient(cb.x,cb.y,cb.z), t).normalized();
                }
                const int *tri = project_h::TRIANGLES[mask];
                for (int i = 0; i < 15 && tri[i] != -1; i += 3) {
                    const int ids[3] = {tri[i], tri[i+1], tri[i+2]};
                    // The classic table with negative-inside matches Godot's clockwise front faces.
                    if ((points[ids[1]]-points[ids[0]]).cross(points[ids[2]]-points[ids[0]]).length_squared() < 1e-12f) continue;
                    for (int id : ids) {
                        const Vector3 p = points[id], n = edge_normals[id];
                        vertices.push_back(p);
                        normals.push_back(n);
                        const Color soil(0.30,0.19,0.12), grass(0.24,0.48,0.29), rock(0.43,0.45,0.43);
                        float top = std::clamp((n.y - 0.35f) * 2.0f, 0.0f, 1.0f);
                        Color base = soil.lerp(rock, std::clamp((-p.y-3.0f)/6.0f, 0.0f, 1.0f));
                        colors.push_back(base.lerp(grass, top));
                    }
                }
            }
    chunk.triangles = vertices.size() / 3;
    if (vertices.is_empty()) {
        chunk.mesh->set_mesh(Ref<Mesh>());
        chunk.collision->set_shape(Ref<Shape3D>());
        return;
    }
    Array arrays;
    arrays.resize(Mesh::ARRAY_MAX);
    arrays[Mesh::ARRAY_VERTEX] = vertices;
    arrays[Mesh::ARRAY_NORMAL] = normals;
    arrays[Mesh::ARRAY_COLOR] = colors;
    Ref<ArrayMesh> mesh;
    mesh.instantiate();
    mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
    chunk.mesh->set_mesh(mesh);
    chunk.collision->set_shape(mesh->create_trimesh_shape());
}

void ProjectHVoxelTerrain::sculpt(Vector3 center, double radius, double amount) {
    if (density.empty() || !center.is_finite() || !std::isfinite(radius) || !std::isfinite(amount) || radius <= 0 || amount == 0) return;
    radius = std::clamp(radius, 1.0, 12.0);
    amount = std::clamp(amount, -8.0, 8.0);
    const Vector3 p = center - OFFSET;
    if (p.x < -radius || p.x > NX+radius || p.y < -radius || p.y > NY+radius || p.z < -radius || p.z > NZ+radius) return;
    const Vector3i lo(std::max(1,int(std::floor(p.x-radius))), std::max(1,int(std::floor(p.y-radius))), std::max(1,int(std::floor(p.z-radius))));
    const Vector3i hi(std::min(NX-1,int(std::ceil(p.x+radius))), std::min(NY-1,int(std::ceil(p.y+radius))), std::min(NZ-1,int(std::ceil(p.z+radius))));
    bool changed = false;
    const auto start = Clock::now();
    for (int z=lo.z; z<=hi.z; ++z)
        for (int y=lo.y; y<=hi.y; ++y)
            for (int x=lo.x; x<=hi.x; ++x) {
                double distance = Vector3(x,y,z).distance_to(p);
                if (distance >= radius) continue;
                double falloff = 1.0 - distance/radius;
                falloff = falloff*falloff*(3.0-2.0*falloff);
                float &value = density[index(x,y,z)];
                value = std::clamp(value+float(amount*falloff), -24.0f, 24.0f);
                changed = true;
            }
    if (!changed) return;
    // Include one extra sample for gradient normals on neighboring chunks.
    for (auto &chunk : chunks) {
        const Vector3i o = chunk.origin;
        if (o.x > hi.x+1 || o.x+CHUNK < lo.x-1 || o.y > hi.y+1 || o.y+CHUNK < lo.y-1 || o.z > hi.z+1 || o.z+CHUNK < lo.z-1) continue;
        rebuild_chunk(chunk);
    }
    last_build_ms = elapsed_ms(start);
}

double ProjectHVoxelTerrain::sample_density(Vector3 position) const {
    if (density.empty() || !position.is_finite()) return 24.0;
    const Vector3 p = position - OFFSET;
    if (p.x < 0 || p.x > NX || p.y < 0 || p.y > NY || p.z < 0 || p.z > NZ) return 24.0;
    const int x = std::min(int(p.x),NX-1), y = std::min(int(p.y),NY-1), z = std::min(int(p.z),NZ-1);
    const float tx=p.x-x, ty=p.y-y, tz=p.z-z;
    auto mix = [](float a,float b,float t) { return a+(b-a)*t; };
    return mix(mix(mix(at(x,y,z),at(x+1,y,z),tx),mix(at(x,y+1,z),at(x+1,y+1,z),tx),ty),
        mix(mix(at(x,y,z+1),at(x+1,y,z+1),tx),mix(at(x,y+1,z+1),at(x+1,y+1,z+1),tx),ty),tz);
}
int ProjectHVoxelTerrain::get_triangle_count() const {
    int count=0;
    for (const auto &chunk : chunks) count += chunk.triangles;
    return count;
}
int ProjectHVoxelTerrain::get_chunk_count() const { return int(chunks.size()); }
double ProjectHVoxelTerrain::get_last_build_ms() const { return last_build_ms; }
