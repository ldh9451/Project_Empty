extends "res://tests/meltdown_playthrough.gd"

func capture(name: String) -> void:
    await RenderingServer.frame_post_draw
    var result := root.get_texture().get_image().save_png("res://cpp/.cache/meltdown-v012-%s.png" % name)
    if result != OK:
        failures += 1
    print("CAPTURE ", name, ": ", result)

func run() -> void:
    game = load("res://scenes/meltdown.tscn").instantiate()
    root.add_child(game)
    game.set_muted(true)
    var captured_heat := false
    var captured_brute := false
    var captured_warning := false
    var frame_times: Array[float] = []
    var previous := Time.get_ticks_usec()
    for frame in 6000:
        var state: Dictionary = game.get_snapshot()
        if frame == 1:
            await capture("start")
        if state.heat >= 80 and not captured_heat:
            await capture("critical")
            captured_heat = true
        if state.wave >= 3 and state.projectiles >= 15 and not captured_warning:
            await capture("combat")
            captured_warning = true
        if state.wave >= 4 and not state.choosing_upgrade and not captured_brute:
            for enemy in state.enemies:
                if enemy.kind == 4 and enemy.warning:
                    await capture("brute")
                    captured_brute = true
        if state.finished:
            await capture("result")
            print("VISUAL_RUN: wave=%d kills=%d time=%.2f" % [state.wave, state.kills, state.elapsed])
            break
        drive(state, 0)
        await process_frame
        var current := Time.get_ticks_usec()
        if frame > 60:
            frame_times.append((current - previous) / 1000.0)
        previous = current
    release_controls()
    frame_times.sort()
    if not frame_times.is_empty():
        print("RENDER_TIMING: frames=%d median_ms=%.2f p95_ms=%.2f (fixed-fps replay, includes capture stalls)" % [frame_times.size(), frame_times[frame_times.size() / 2], frame_times[int(frame_times.size() * 0.95)]])
    if not captured_heat or not captured_brute or not captured_warning:
        failures += 1
    game.queue_free()
    await process_frame
    print("MELTDOWN_VISUAL: %d failures" % failures)
    quit(0 if failures == 0 else 1)
