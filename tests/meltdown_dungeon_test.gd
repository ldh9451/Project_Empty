extends SceneTree

var failures := 0
var game: Node3D
var captures := false
var frame_times: Array[float] = []
var last_frame := 0

func check(value: bool, label: String) -> void:
    if not value:
        failures += 1
        push_error(label)

func _initialize() -> void:
    captures = "capture" in OS.get_cmdline_user_args()
    process_frame.connect(record_frame)
    run.call_deferred()

func record_frame() -> void:
    var now := Time.get_ticks_usec()
    if last_frame:
        frame_times.append((now - last_frame) / 1000.0)
    last_frame = now

func release_movement() -> void:
    for action in ["md_left", "md_right", "md_forward", "md_back"]:
        Input.action_release(action)

func move_input(offset: Vector3) -> void:
    release_movement()
    var camera: Camera3D = game.get_node("Camera")
    var right := camera.basis.x
    var back := camera.basis.z
    right.y = 0
    back.y = 0
    offset.y = 0
    var x := offset.normalized().dot(right.normalized())
    var z := offset.normalized().dot(back.normalized())
    Input.action_press("md_right" if x >= 0 else "md_left", absf(x))
    Input.action_press("md_back" if z >= 0 else "md_forward", absf(z))

func move_to(destination: Vector3) -> void:
    for frame in 480:
        var offset: Vector3 = destination - game.get_snapshot().player
        if offset.length() < 0.12:
            break
        move_input(offset)
        await process_frame
    release_movement()
    check(game.get_snapshot().player.distance_to(destination) < 0.2, "Dungeon input reaches destination")

func aim_at(position: Vector3) -> void:
    var event := InputEventMouseMotion.new()
    event.position = game.get_node("Camera").unproject_position(position)
    root.push_input(event, true)

func capture(label: String) -> void:
    if captures:
        await RenderingServer.frame_post_draw
        check(root.get_texture().get_image().save_png("res://cpp/.cache/meltdown-v020-" + label + ".png") == OK,
            "Dungeon capture saved")

func validate_meshes() -> void:
    var faces := {}
    var count := 0
    for child in game.get_children():
        if not str(child.name).begins_with("DungeonChunk_") or child.mesh == null:
            continue
        count += 1
        var data: Array = child.mesh.surface_get_arrays(0)
        var vertices: PackedVector3Array = data[Mesh.ARRAY_VERTEX]
        var normals: PackedVector3Array = data[Mesh.ARRAY_NORMAL]
        for i in range(0, vertices.size(), 6):
            var center := Vector3.ZERO
            for j in 6:
                center += vertices[i + j] / 6.0
            var key := Vector3i(roundi(center.x * 2), roundi(center.y * 2), roundi(center.z * 2))
            check(not faces.has(key), "No duplicated/internal faces at chunk seams")
            faces[key] = true
            var cross := (vertices[i + 1] - vertices[i]).cross(vertices[i + 2] - vertices[i])
            check(cross.dot(normals[i]) < 0, "Dungeon face winding points outward")
            check(absf(cross.length() - 1) < 0.001, "Each terrain face is exactly 1 square metre")
    check(count > 36, "Floor and wall chunk meshes exist")

func run() -> void:
    game = load("res://scenes/meltdown.tscn").instantiate()
    root.add_child(game)
    game.set_muted(true)
    game.set_training(true)
    await process_frame
    var initial: Dictionary = game.get_snapshot()
    check(initial.voxel_size == 1 and initial.chunk_size == 8 and initial.voxel_cells.size() == 74,
        "1m dungeon with 54 wall cells and 20 pillar cells")
    validate_meshes()
    var before: Dictionary = game.get_snapshot()
    var replacement: ShaderMaterial = game.dungeon_material.duplicate()
    replacement.set_shader_parameter("tint", Color(0.8, 0.9, 1))
    game.dungeon_material = replacement
    check(before == game.get_snapshot(), "Terrain resource replacement does not change gameplay")
    await process_frame
    await move_to(Vector3(8.5, 0, -2.8))
    await capture("dungeon")
    move_input(Vector3(0, 0, -1))
    Input.action_press("md_dodge")
    for frame in 45:
        await process_frame
    Input.action_release("md_dodge")
    release_movement()
    check(game.get_snapshot().player.z >= -3.56, "Actual movement/dodge respects wall")
    await move_to(Vector3(6.5, 0, -2.8))
    var distant_mesh: Mesh = game.get_node("DungeonChunk_-2_0_0").mesh
    for x in [6.5, 7.5, 8.5, 9.5, 10.5, 11.5]:
        await move_to(Vector3(x, 0, -2.8))
        aim_at(Vector3(x, 0, -4.5))
        Input.action_press("md_heavy")
        for frame in 180:
            aim_at(Vector3(x, 0, -4.5))
            if not game.get_snapshot().voxel_cells.any(func(cell): return cell.cell.x == floori(x) and cell.cell.z == -5 and cell.cell.y < 2):
                break
            await process_frame
        Input.action_release("md_heavy")
        if x == 11.5:
            check(game.get_snapshot().voxel_cells.any(func(cell): return cell.warning), "Unsupported wall has warning")
            await capture("wall-warning")
            game.notification(Node.NOTIFICATION_APPLICATION_FOCUS_OUT)
            var frozen: Dictionary = game.get_snapshot()
            for frame in 5:
                await process_frame
            check(game.get_snapshot() == frozen, "Pause freezes voxel destruction and debris")
            Input.action_press("md_pause")
            await process_frame
            await process_frame
            Input.action_release("md_pause")
    for frame in 40:
        await process_frame
    await capture("wall-falling")
    for frame in 90:
        await process_frame
    var after: Dictionary = game.get_snapshot()
    check(after.voxel_cells.size() == 56, "Entire six-column wall removed across chunk boundary")
    check(after.blocks.size() == 26, "Six unsupported top cells become falling blocks")
    check(after.heat == 0, "Environment destruction gives no heat")
    check(game.get_node("DungeonChunk_-2_0_0").mesh == distant_mesh, "Distant chunk mesh not rebuilt")
    await move_to(Vector3(9.5, 0, -6))
    await capture("wall-open")
    validate_meshes()
    game.restart()
    await process_frame
    check(game.get_snapshot().voxel_cells == initial.voxel_cells, "Restart restores exact voxel state")
    print("MELTDOWN_DUNGEON_TEST: %s failures / %s chunks / %s rubble blocks" % [failures, initial.chunk_count, after.blocks.size() - 20])
    frame_times.sort()
    print("DUNGEON_FRAME_INTERVAL: %s samples / median %.2f ms / p95 %.2f ms / max %.2f ms" % [frame_times.size(),
        frame_times[frame_times.size() / 2], frame_times[int(frame_times.size() * 0.95)], frame_times.back()])
    game.queue_free()
    await process_frame
    quit(0 if failures == 0 else 1)
