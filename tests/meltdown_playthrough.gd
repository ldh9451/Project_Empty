extends SceneTree

var game: Node3D
var failures := 0
const ACTIONS = ["md_left", "md_right", "md_forward", "md_back", "md_light", "md_heavy", "md_dodge", "md_vent", "md_overdrive", "md_upgrade_1", "md_upgrade_2", "md_upgrade_3"]

func _initialize() -> void:
    run.call_deferred()

func release_controls() -> void:
    for action in ACTIONS:
        Input.action_release(action)

func drive(state: Dictionary, run_index: int) -> void:
    release_controls()
    if state.choosing_upgrade:
        Input.action_press("md_upgrade_%d" % (1 + (state.wave - 1) % 3))
        return
    var nearest := INF
    var target: Dictionary = {}
    var position: Vector3 = state.player
    for enemy in state.enemies:
        var distance: float = position.distance_to(enemy.position)
        if enemy.hp > 0 and distance < nearest:
            target = enemy
            nearest = distance
    if target.is_empty():
        return
    var camera: Camera3D = game.get_node("Camera")
    var mouse := InputEventMouseMotion.new()
    mouse.position = camera.unproject_position(target.position)
    mouse.global_position = mouse.position
    root.push_input(mouse, true)
    var direction: Vector3 = (target.position - position).normalized()
    var retreat: bool = target.warning and nearest < 2.0
    if retreat:
        direction *= -1
        Input.action_press("md_dodge")
    if nearest > 1.9 or retreat:
        var right := camera.basis.x
        var back := camera.basis.z
        right.y = 0
        back.y = 0
        var x := direction.dot(right.normalized())
        var z := direction.dot(back.normalized())
        if absf(x) > 0.2:
            Input.action_press("md_right" if x > 0 else "md_left", absf(x))
        if absf(z) > 0.2:
            Input.action_press("md_back" if z > 0 else "md_forward", absf(z))
    if nearest < (4.0 if state.heat >= 50 or state.heat_mode == 1 else 2.5):
        Input.action_press("md_heavy")
    if state.heat >= 80:
        Input.action_press("md_overdrive")
    if run_index == 1 and state.heat >= 40 and nearest < 2.5:
        Input.action_press("md_vent")

func run() -> void:
    game = load("res://scenes/meltdown.tscn").instantiate()
    root.add_child(game)
    # Fast fixed-fps replay is faster than the audio mixer wall clock.
    game.set_muted(true)
    # Advance ordinary physics frames at fixed delta, without waiting for vsync.
    for run_index in 3:
        game.set_training(false)
        var peak_bullets := 0
        var peak_fragments := 0
        for frame in 18000:
            var state: Dictionary = game.get_snapshot()
            if state.finished:
                break
            drive(state, run_index)
            await process_frame
            peak_bullets = maxi(peak_bullets, state.projectiles)
            peak_fragments = maxi(peak_fragments, state.fragments)
        release_controls()
        var result: Dictionary = game.get_snapshot()
        print("ENGINE_RUN %d: result=%s wave=%d time=%.2f kills=%d hp=%.0f peak_bullets=%d peak_fragments=%d" % [run_index + 1, "WIN" if result.won else "DEATH" if result.finished else "TIMEOUT", result.wave, result.elapsed, result.kills, result.hp, peak_bullets, peak_fragments])
        if not result.finished or result.wave < 3 or result.kills < 10:
            failures += 1
        game.restart()
        var reset: Dictionary = game.get_snapshot()
        if reset.hp != 100 or reset.heat != 0 or reset.wave != 1 or reset.projectiles != 0 or reset.fragments != 0:
            failures += 1
        await process_frame
    game.queue_free()
    await process_frame
    print("MELTDOWN_PLAYTHROUGH: %d failures" % failures)
    quit(0 if failures == 0 else 1)
