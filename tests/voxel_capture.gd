extends SceneTree

func _initialize() -> void:
    run.call_deferred()

func run() -> void:
    var scene: Node = load("res://voxel_demo.tscn").instantiate()
    root.add_child(scene)
    for i in 12:
        await process_frame
    await RenderingServer.frame_post_draw
    root.get_texture().get_image().save_png("res://cpp/.cache/voxel_before.png")
    var terrain: Node3D = scene.get_node("Terrain")
    # Carve into the visible face, then build a mound on the top surface.
    for i in 6:
        terrain.sculpt(Vector3(7, -2, 20), 5.0, 5.0)
        terrain.sculpt(Vector3(0, 3, 7), 5.0, -4.0)
    scene._update_stats()
    for i in 6:
        await process_frame
    await RenderingServer.frame_post_draw
    root.get_texture().get_image().save_png("res://cpp/.cache/voxel_after.png")
    print("VOXEL_CAPTURE: before/after saved")
    quit()
