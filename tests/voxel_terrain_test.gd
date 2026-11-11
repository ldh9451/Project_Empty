extends SceneTree

var failures := 0
var terrain: Node3D

func check(condition: bool, message: String) -> void:
    if not condition:
        failures += 1
        push_error(message)

func ray_height(x: float, z: float) -> float:
    var query := PhysicsRayQueryParameters3D.create(Vector3(x, 25, z), Vector3(x, -15, z), 1)
    var hit := terrain.get_world_3d().direct_space_state.intersect_ray(query)
    check(not hit.is_empty(), "Ray must hit terrain")
    return hit.position.y if not hit.is_empty() else -100.0

func validate_meshes() -> void:
    var shared := {}
    var repeated := 0
    var winding_errors := 0
    var surface_count := 0
    for child in terrain.get_children():
        if child.mesh == null:
            continue
        surface_count += 1
        var arrays: Array = child.mesh.surface_get_arrays(0)
        var vertices: PackedVector3Array = arrays[Mesh.ARRAY_VERTEX]
        var normals: PackedVector3Array = arrays[Mesh.ARRAY_NORMAL]
        check(vertices.size() % 3 == 0, "Complete triangles")
        check(vertices.size() == normals.size(), "Normals match vertices")
        for i in range(vertices.size()):
            var v := vertices[i]
            var n := normals[i]
            check(v.is_finite() and n.is_finite(), "Finite mesh data")
            check(n.length() > 0.99 and n.length() < 1.01, "Unit normals")
            var key := Vector3i(roundi(v.x * 10000), roundi(v.y * 10000), roundi(v.z * 10000))
            if shared.has(key) and shared[key][1] != child.name:
                check(n.distance_to(shared[key][0]) < 0.001, "Seam normals agree")
                repeated += 1
            else:
                shared[key] = [n, child.name]
        for i in range(0, vertices.size(), 3):
            var face := (vertices[i+1]-vertices[i]).cross(vertices[i+2]-vertices[i])
            if face.dot(normals[i] + normals[i+1] + normals[i+2]) > 0.0001:
                winding_errors += 1
    check(surface_count > 0, "Nonempty terrain")
    check(repeated > 0, "Shared chunk boundary vertices exist")
    check(winding_errors == 0, "Clockwise outward faces: %s invalid" % winding_errors)

func _initialize() -> void:
    run.call_deferred()

func run() -> void:
    check(ClassDB.class_exists("ProjectHVoxelTerrain"), "Native terrain registered")
    if failures:
        quit(1)
        return
    terrain = ClassDB.instantiate("ProjectHVoxelTerrain")
    root.add_child(terrain)
    await physics_frame
    await physics_frame
    check(terrain.get_chunk_count() == 18, "18 chunks")
    check(terrain.get_triangle_count() > 1000, "Terrain has triangles")
    validate_meshes()
    var initial_triangles: int = terrain.get_triangle_count()
    var initial_height := ray_height(-8, -8) # Intersection of four horizontal chunks.
    var center := Vector3(-8, initial_height, -8)
    var initial_density: float = terrain.sample_density(center)
    terrain.sculpt(center, 4.0, 6.0)
    check(terrain.sample_density(center) > initial_density + 3.0, "Dig removes solid")
    check(terrain.sample_density(Vector3(20, -5, 20)) < 0, "Distant solid unchanged")
    validate_meshes()
    await physics_frame
    await physics_frame
    check(ray_height(-8, -8) < initial_height - 0.5, "Collision follows excavation")
    terrain.sculpt(center, 4.0, -6.0)
    check(absf(terrain.sample_density(center) - initial_density) < 0.0001, "Inverse brush restores density")
    terrain.sculpt(center, 4.0, -6.0)
    await physics_frame
    await physics_frame
    check(ray_height(-8, -8) > initial_height + 0.5, "Collision follows building")
    terrain.reset_terrain(7)
    check(terrain.get_triangle_count() == initial_triangles, "Reset is deterministic")
    check(absf(terrain.sample_density(center) - initial_density) < 0.0001, "Reset restores edits")
    var count: int = terrain.get_triangle_count()
    terrain.sculpt(Vector3.ZERO, -1, 1)
    terrain.sculpt(Vector3(1e20, 0, 0), 4, 1)
    terrain.sculpt(Vector3.ZERO, NAN, 1)
    check(terrain.get_triangle_count() == count, "Invalid/outside brushes are ignored")
    print("VOXEL_TEST: %s failures; %s triangles; %s chunks; reset %.1f ms" % [failures, count, terrain.get_chunk_count(), terrain.get_last_build_ms()])
    terrain.queue_free()
    await process_frame
    quit(0 if failures == 0 else 1)
