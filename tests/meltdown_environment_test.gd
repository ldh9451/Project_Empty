extends SceneTree

var failures := 0
var game: Node3D
var capture_enabled := false

func check(condition: bool, message: String) -> void:
    if not condition:
        failures += 1
        push_error(message)

func _initialize() -> void:
    capture_enabled = "capture" in OS.get_cmdline_user_args()
    run.call_deferred()

func release_movement() -> void:
    for action in ["md_left", "md_right", "md_forward", "md_back"]:
        Input.action_release(action)

func move_to(destination: Vector3) -> void:
    var camera: Camera3D = game.get_node("Camera")
    var right := camera.basis.x
    var back := camera.basis.z
    right.y = 0
    back.y = 0
    right = right.normalized()
    back = back.normalized()
    for frame in 600:
        var offset: Vector3 = destination - game.get_snapshot().player
        offset.y = 0
        if offset.length() < 0.13:
            break
        var movement := offset.normalized()
        var x := movement.dot(right)
        var y := movement.dot(back)
        release_movement()
        Input.action_press("md_right" if x >= 0 else "md_left", absf(x))
        Input.action_press("md_back" if y >= 0 else "md_forward", absf(y))
        await process_frame
    release_movement()
    check(game.get_snapshot().player.distance_to(destination) < 0.2, "Input movement reaches environment test position")

func aim_at(position: Vector3) -> void:
    var mouse := InputEventMouseMotion.new()
    mouse.position = game.get_node("Camera").unproject_position(position)
    root.push_input(mouse, true)

func capture(name: String) -> void:
    if not capture_enabled:
        return
    await RenderingServer.frame_post_draw
    var error := root.get_texture().get_image().save_png("res://cpp/.cache/meltdown-v015-" + name + ".png")
    check(error == OK, "Environment screenshot saved")

func wait_state(index: int, state: int, attacking: bool = false) -> void:
    if attacking:
        Input.action_press("md_heavy")
    for frame in 240:
        if game.get_snapshot().environment[index].state == state:
            break
        await process_frame
    Input.action_release("md_heavy")
    check(game.get_snapshot().environment[index].state == state, "Environment reaches state %s" % state)

func run() -> void:
    var outcomes: Array = []
    for variant in 2:
        game = load("res://scenes/meltdown.tscn").instantiate()
        var suffix := "_alternate" if variant else ""
        game.pillar_block_visual = load("res://scenes/environment/pillar_block" + suffix + ".tscn")
        root.add_child(game)
        game.set_muted(not "sound" in OS.get_cmdline_user_args())
        game.set_training(true)
        await process_frame
        check(game.get_snapshot().environment.size() == 4 and game.get_snapshot().blocks.size() == 20,
            "Four columns with five real blocks each spawn")
        await capture("layout-" + str(variant))
        await move_to(Vector3(-4, 0, -0.6))
        aim_at(Vector3(-4, 0, -2))
        await wait_state(0, 1, true)
        check(game.get_snapshot().environment[0].hp == 22, "First strike cracks pillar support")
        await capture("damaged-" + str(variant))
        await wait_state(0, 2, true)
        check(game.get_snapshot().hp == 100, "Breaking support causes no instant area damage")
        var before_swap: Dictionary = game.get_snapshot()
        game.pillar_block_visual = game.pillar_block_visual
        check(game.get_snapshot() == before_swap, "Visual replacement preserves all live simulation state")
        await physics_frame
        await process_frame
        var pillar: Dictionary = game.get_snapshot().environment[0]
        check(not game.get_node("EnvironmentDanger" + str(pillar.id)).visible, "Column has no cone damage indicator")
        var top: Dictionary = game.get_snapshot().blocks[4]
        var marker: MeshInstance3D = game.get_node("BlockLanding" + str(top.id))
        check(marker.visible and Vector2(marker.position.x, marker.position.z).distance_to(
            Vector2(top.landing.x, top.landing.z)) < 0.001, "Landing marker matches predicted block trajectory")
        check(not game.get_node("HUD/Skills").get_global_rect().intersects(game.get_node("HUD/EnvironmentHint").get_global_rect()),
            "Environment hint does not overlap Q/E status")
        await move_to(Vector3(-4.45, 0, 1.3))
        await capture("warning-" + str(variant))
        game.notification(Node.NOTIFICATION_APPLICATION_FOCUS_OUT)
        var frozen: Dictionary = game.get_snapshot()
        for frame in 4:
            await process_frame
        check(game.get_snapshot().blocks == frozen.blocks and game.get_snapshot().environment == frozen.environment,
            "Focus pause freezes collapse and blocks")
        Input.action_press("md_pause")
        await physics_frame
        await process_frame
        await process_frame
        Input.action_release("md_pause")
        await wait_state(0, 3)
        check(game.get_snapshot().hp == 100 and game.get_snapshot().enemies[2].hp == 100,
            "Airborne blocks do not damage actors before contact")
        for frame in 12:
            await process_frame
        for block in game.get_snapshot().blocks:
            check(game.get_node("PillarBlock" + str(block.id)).position.is_equal_approx(block.position),
                "Replacement visual uses model position including height")
        await capture("falling-" + str(variant))
        await wait_state(0, 4)
        check(game.get_snapshot().hp < 100 and game.get_snapshot().enemies[2].hp < 100 and game.get_snapshot().heat == 0,
            "Actual falling blocks hurt player and training enemy without charging heat")
        check(not game.get_snapshot().environment[0].blocking, "Collapsed support opens movement")
        var hp_after: float = game.get_snapshot().hp
        await move_to(Vector3(-4, 0, -2.5))
        for frame in 40:
            await process_frame
        check(game.get_snapshot().hp == hp_after, "Walking through settled rubble is safe")
        await capture("rubble-" + str(variant))
        var outcome: Dictionary = game.get_snapshot()
        outcomes.append([outcome.hp, outcome.heat, outcome.environment, outcome.blocks, outcome.enemies[2].hp])
        print("PILLAR_VARIANT %d: player_hp=%s target_hp=%s" % [variant, outcome.hp, outcome.enemies[2].hp])
        game.restart()
        await process_frame
        for object in game.get_snapshot().environment:
            check(object.state == 0 and object.hp > 0 and object.remaining == 0, "Restart restores environment model")
        for block in game.get_snapshot().blocks:
            check(not block.falling and not block.settled, "Restart restores stacked blocks")
            check(game.get_node("PillarBlock" + str(block.id)).position.is_equal_approx(block.position), "Restart restores block visual")
        check(game.get_snapshot().hp == 100, "Restart restores player")
        game.queue_free()
        await process_frame
    check(outcomes[0] == outcomes[1], "Two block resources produce identical HP, trajectories, geometry, states and timers")
    print("MELTDOWN_ENVIRONMENT_TEST: %s failures / two visual sets" % failures)
    quit(0 if failures == 0 else 1)
