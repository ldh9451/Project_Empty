extends SceneTree

var failures := 0

func check(condition: bool, message: String) -> void:
    if not condition:
        failures += 1
        push_error(message)

func _initialize() -> void:
    run.call_deferred()

func run() -> void:
    var packed := load("res://scenes/meltdown.tscn") as PackedScene
    check(packed != null, "MELTDOWN scene loads")
    if packed == null:
        quit(1)
        return
    var game = packed.instantiate()
    root.add_child(game)
    game.set_training(true)
    await physics_frame
    await physics_frame
    check(game.get_node("Camera").is_current(), "Quarter-view camera is active")
    check(game.get_snapshot().enemy_count == 3, "Combat and pillar training targets spawn")
    var start: Vector3 = game.get_snapshot().player
    var camera: Camera3D = game.get_node("Camera")
    var camera_offset := camera.position - start
    check(is_equal_approx(camera.size, 20.0), "Closer orthographic framing")
    Input.action_press("md_right")
    for i in 30:
        await physics_frame
    Input.action_release("md_right")
    var finish: Vector3 = game.get_snapshot().player
    check(finish.distance_to(start) > 2.0, "Real input moves the native player")
    check((camera.position - finish).is_equal_approx(camera_offset), "Camera tracks player with fixed rotation and offset")
    check(camera.unproject_position(finish).distance_to(camera.unproject_position(start)) > 30, "Movement remains visible against the ground")
    game.restart()
    var aim := InputEventMouseMotion.new()
    aim.position = game.get_node("Camera").unproject_position(Vector3(0, 0, -2))
    root.push_input(aim, true)
    Input.action_press("md_heavy")
    for i in 20:
        await physics_frame
    Input.action_release("md_heavy")
    check(game.get_snapshot().hits == 1 and game.get_snapshot().heat == 20, "Mouse aim and real heavy input damage target and charge heat")
    game.notification(Node.NOTIFICATION_APPLICATION_FOCUS_OUT)
    var before_pause: Dictionary = game.get_snapshot()
    await physics_frame
    await physics_frame
    check(game.get_snapshot().paused and game.get_snapshot().elapsed == before_pause.elapsed, "Focus loss pauses simulation")
    for i in 3:
        game.restart()
        await physics_frame
        check(game.get_snapshot().hp == 100 and game.get_snapshot().hits == 0, "Repeated restart resets combat")
    game.queue_free()
    await process_frame
    print("MELTDOWN_SCENE_TEST: %s failures" % failures)
    quit(0 if failures == 0 else 1)
