extends SceneTree

var failures := 0

func check(condition: bool, message: String) -> void:
    if not condition:
        failures += 1
        push_error(message)

func _initialize() -> void:
    run.call_deferred()

func screenshot() -> Image:
    for frame in 3:
        await process_frame
    await RenderingServer.frame_post_draw
    return root.get_texture().get_image()

func run() -> void:
    var game = load("res://scenes/meltdown.tscn").instantiate()
    root.add_child(game)
    game.set_training(true)
    game.set_muted(true)
    await process_frame
    paused = true
    var state: Dictionary = game.get_snapshot()
    var camera: Camera3D = game.get_node("Camera")
    var outline: MeshInstance3D = camera.get_node("InkOutline")
    var material: ShaderMaterial = outline.material_override
    material.set_shader_parameter("width_pixels", 0.0)
    var plain := await screenshot()
    material.set_shader_parameter("width_pixels", 1.0)
    var ink := await screenshot()
    var changed := 0
    var hud_changed := 0
    for y in ink.get_height():
        for x in ink.get_width():
            var a := plain.get_pixel(x, y)
            var b := ink.get_pixel(x, y)
            var difference := maxf(absf(a.r - b.r), maxf(absf(a.g - b.g), absf(a.b - b.b)))
            if difference > 0.02:
                changed += 1
                if Rect2i(28, 22, 200, 111).has_point(Vector2i(x, y)):
                    hud_changed += 1
    check(changed > 500 and changed < ink.get_width() * ink.get_height() / 10,
        "Outline affects a limited set of silhouette pixels")
    check(hud_changed == 0, "Outline leaves the HUD region unchanged")
    check(state == game.get_snapshot(), "Presentation toggle preserves simulation state")
    check(camera.size == 20.0, "Existing camera scale is preserved")
    check(ink.save_png("res://cpp/.cache/meltdown-v023-style.png") == OK, "Style capture saves")
    print("OUTLINE_PIXELS: changed=%d hud_changed=%d" % [changed, hud_changed])
    paused = false
    game.queue_free()
    await process_frame
    print("MELTDOWN_RENDER_TEST: %d failures" % failures)
    quit(0 if failures == 0 else 1)
