extends SceneTree

var game: Node3D
var failures := 0

func check(condition: bool, message: String) -> void:
    if not condition:
        failures += 1
        push_error(message)

func _initialize() -> void:
    run.call_deferred()

func capture(name: String) -> void:
    if not OS.get_cmdline_user_args().has("capture"):
        return
    await RenderingServer.frame_post_draw
    var error := root.get_texture().get_image().save_png("res://cpp/.cache/meltdown-v012-%s.png" % name)
    check(error == OK, "Feedback screenshot saves")

func charge_heat(amount: float) -> void:
    var camera: Camera3D = game.get_node("Camera")
    for frame in 400:
        if game.get_snapshot().heat >= amount:
            break
        var mouse := InputEventMouseMotion.new()
        mouse.position = camera.unproject_position(Vector3(0, 0, -2))
        root.push_input(mouse, true)
        Input.action_press("md_heavy")
        await process_frame
    Input.action_release("md_heavy")
    check(game.get_snapshot().heat >= amount, "Training strikes supply heat through real input")

func run() -> void:
    game = load("res://scenes/meltdown.tscn").instantiate()
    root.add_child(game)
    game.set_training(true)
    game.set_muted(true)
    await process_frame
    await capture("camera")
    await charge_heat(40)
    Input.action_press("md_vent")
    await process_frame
    await process_frame
    Input.action_release("md_vent")
    check(game.get_snapshot().heat == 0, "Q still costs exactly 40")
    check(game.get_node("VentShockwave").visible, "Q creates a visible shockwave")
    check(game.get_node("HUD/Skills").text.contains("-40"), "Q consumption is readable")
    for frame in 12:
        await process_frame
    check(game.get_node("VentShockwave").visible, "Q effect persists beyond initial impact")
    await capture("vent")
    for frame in 20:
        await process_frame
    check(not game.get_node("VentShockwave").visible, "Q shockwave expires")
    game.restart()
    await charge_heat(80)
    Input.action_press("md_overdrive")
    await process_frame
    await process_frame
    Input.action_release("md_overdrive")
    check(game.get_snapshot().heat_mode == 1, "E enters overdrive")
    check(game.get_node("HeatTimer").visible and game.get_node("Player/OverdriveCore").visible,
        "Overdrive has persistent timer ring and core")
    check(game.get_node("HUD/Skills").text.contains("ACTIVE"), "E slot displays remaining time")
    Input.action_press("md_right")
    for frame in 20:
        await process_frame
    Input.action_release("md_right")
    check(game.get_node("HeatTimer").position.distance_to(game.get_snapshot().player) < 0.11,
        "Timer ring follows moving player")
    await capture("overdrive")
    for frame in 310:
        await process_frame
    check(game.get_snapshot().heat_mode == 2 and not game.get_node("Player/OverdriveCore").visible,
        "Overdrive shuts off at cooling transition")
    check(game.get_node("HeatTimer").visible and game.get_node("HUD/Skills").text.contains("COOLING"),
        "Cooling is distinct and timed")
    await capture("cooling")
    game.restart()
    check(not game.get_node("HeatTimer").visible and not game.get_node("VentShockwave").visible,
        "Restart clears skill effects")
    var camera: Camera3D = game.get_node("Camera")
    var camera_offset: Vector3 = camera.position - game.get_snapshot().player
    Input.action_press("md_left")
    for frame in 600:
        await process_frame
    Input.action_release("md_left")
    var position: Vector3 = game.get_snapshot().player
    check(position.length() > 25 and absf(position.x) <= 23.001 and absf(position.z) <= 23.001,
        "Player traverses enlarged map and stops inside boundary")
    check((camera.position - position).is_equal_approx(camera_offset), "Camera keeps following at far map boundary")
    check(not game.get_node("HUD/EnemyDirection").text.is_empty(), "Offscreen targets remain discoverable")
    await capture("boundary")
    game.restart()
    check(camera.position.is_equal_approx(camera_offset), "Restart recenters camera")
    game.queue_free()
    await process_frame
    print("MELTDOWN_FEEDBACK_TEST: %d failures" % failures)
    quit(0 if failures == 0 else 1)
