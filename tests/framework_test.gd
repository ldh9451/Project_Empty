extends SceneTree

var failures := 0
var game: Node3D
var camera: Camera3D
var controls: Node

func check(condition: bool, message: String) -> void:
    if not condition:
        failures += 1
        push_error(message)

func right_mouse(pressed: bool) -> void:
    var event := InputEventMouseButton.new()
    event.button_index = MOUSE_BUTTON_RIGHT
    event.pressed = pressed
    event.position = Vector2(640, 400)
    root.push_input(event)

func mouse_move(delta: Vector2) -> void:
    var event := InputEventMouseMotion.new()
    event.screen_relative = delta
    root.push_input(event)

func _initialize() -> void:
    run.call_deferred()

func run() -> void:
    var packed := load("res://scenes/main.tscn") as PackedScene
    check(packed != null, "Main scene loads")
    if packed == null:
        quit(1)
        return
    game = packed.instantiate()
    root.add_child(game)
    await process_frame
    game.set_process(false)
    camera = game.get_node("Camera")
    controls = game.get_node("Input")
    check(game.is_initialized(), "Ready calls Init")
    check(camera.is_current(), "Framework camera is current")
    check(game.find_child("Terrain", true, false) == null, "Main scene is independent of voxel terrain")
    check(InputMap.action_get_events("camera_fast")[0].physical_keycode == KEY_SHIFT, "Shift binding")
    var initial_transform := camera.transform
    var child_count := game.get_child_count()
    game.Init()
    check(game.get_child_count() == child_count, "Repeated Init does not duplicate children")

    Input.action_press("camera_forward")
    game.Update(0.5)
    check(camera.transform == initial_transform, "Movement requires right mouse")
    right_mouse(true)
    check(controls.is_looking(), "Right mouse starts camera control")
    game.Update(0.5)
    check(is_equal_approx(camera.position.distance_to(initial_transform.origin), 4.0), "Movement uses delta and speed")
    camera.transform = initial_transform
    game.Update(0.25)
    game.Update(0.25)
    check(is_equal_approx(camera.position.distance_to(initial_transform.origin), 4.0), "Movement is frame-rate independent")

    Input.action_press("camera_right")
    Input.action_press("camera_up")
    camera.transform = initial_transform
    game.Update(0.5)
    check(is_equal_approx(camera.position.distance_to(initial_transform.origin), 4.0), "Diagonal movement is normalized")
    Input.action_press("camera_fast")
    camera.transform = initial_transform
    game.Update(0.5)
    check(is_equal_approx(camera.position.distance_to(initial_transform.origin), 12.0), "Shift enables fast movement")
    for action in ["camera_forward", "camera_right", "camera_up", "camera_fast"]:
        Input.action_release(action)

    mouse_move(Vector2(20, -10))
    mouse_move(Vector2(20, -10))
    var before_look := camera.rotation
    game.Update(0.0)
    check(is_equal_approx(camera.rotation.y - before_look.y, -0.1), "Mouse motion accumulates within a frame")
    var after_look := camera.rotation
    game.Update(0.0)
    check(camera.rotation.is_equal_approx(after_look), "Mouse motion is consumed once")
    mouse_move(Vector2(0, -100000))
    game.Update(0.0)
    check(absf(camera.rotation.x) <= deg_to_rad(89.0) + 0.0001, "Pitch is clamped")

    right_mouse(false)
    check(not controls.is_looking(), "Mouse release stops control")
    check(Input.mouse_mode == Input.MOUSE_MODE_VISIBLE, "Cursor is restored")
    right_mouse(true)
    var escape := InputEventKey.new()
    escape.keycode = KEY_ESCAPE
    escape.pressed = true
    root.push_input(escape)
    check(not controls.is_looking(), "Escape releases cursor")

    right_mouse(true)
    mouse_move(Vector2(30, 30))
    controls.notification(Node.NOTIFICATION_APPLICATION_FOCUS_OUT)
    after_look = camera.rotation
    game.Update(0.1)
    check(not controls.is_looking() and camera.rotation.is_equal_approx(after_look), "Focus loss clears pending input")
    right_mouse(true)
    paused = true
    check(not controls.is_looking(), "Pausing releases cursor")
    paused = false

    right_mouse(true)
    game.Release()
    game.Release()
    check(not game.is_initialized() and not controls.is_looking(), "Release is safe to repeat and clears input")
    var released_transform := camera.transform
    game.Update(1.0)
    check(camera.transform == released_transform, "Update after Release is harmless")
    game.Init()
    check(game.is_initialized(), "Init works after Release")
    root.remove_child(game)
    check(not game.is_initialized(), "Exit tree calls Release")
    root.add_child(game)
    check(game.is_initialized(), "Reentering the tree initializes again")
    check(game.get_child_count() == child_count, "Reentry preserves scene ownership")
    game.queue_free()
    await process_frame
    print("FRAMEWORK_TEST: %s failures" % failures)
    quit(0 if failures == 0 else 1)
