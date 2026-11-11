extends SceneTree

const BASE := "res://resources/characters/zombie_starter/"
var failures := 0

func check(condition: bool, message: String) -> void:
    if not condition:
        failures += 1
        push_error(message)

func _initialize() -> void:
    run.call_deferred()

func run() -> void:
    var packed := load(BASE + "scenes/zombie_motusman.tscn") as PackedScene
    if packed == null:
        quit(1)
        return
    var character := packed.instantiate()
    root.add_child(character)
    var skeleton: Skeleton3D = character.get_node("Skeleton3D")
    var mesh: MeshInstance3D = skeleton.get_node("MotusMan_v55")
    var player: AnimationPlayer = character.get_node("AnimationPlayer")
    player.callback_mode_process = AnimationMixer.ANIMATION_CALLBACK_MODE_PROCESS_MANUAL
    check(skeleton.get_bone_count() == 73, "73 bones preserved")
    check(character.find_children("*", "Camera3D", true, false).is_empty(), "Authoring cameras removed")
    check(mesh.mesh.get_surface_count() == 1 and mesh.skin != null, "Skinned model loads")
    for bind in mesh.skin.get_bind_count():
        check(skeleton.find_bone(mesh.skin.get_bind_name(bind)) >= 0, "Skin binding resolves")
    var material := mesh.get_active_material(0) as ShaderMaterial
    check(material != null, "Ported material loads")
    if material:
        for parameter in ["diffuse_texture", "specular_texture"]:
            var texture := material.get_shader_parameter(parameter) as Texture2D
            check(texture != null and texture.get_size() == Vector2(2048, 2048), "Original 2K map: " + parameter)
    for variant in ["in_place", "root_motion"]:
        check(player.get_animation_library(variant).get_animation_list().size() == 26, "26 clips in " + variant)
    check(player.get_animation_list().size() == 54, "52 motion clips plus RESET and T_Pose")
    for clip in player.get_animation_list():
        var animation := player.get_animation(clip)
        check(animation.length > 0 and animation.get_track_count() == 219, "Complete bone channels: " + clip)
        for track in animation.get_track_count():
            var path := animation.track_get_path(track)
            check(str(path.get_concatenated_names()) == "Skeleton3D" and path.get_subname_count() == 1, "Skeleton track path")
            check(skeleton.find_bone(path.get_subname(0)) >= 0, "Animation bone resolves")
        for fraction in [0.0, 0.25, 0.5, 0.75]:
            player.play(clip)
            player.seek(animation.length * fraction, true)
            for bone in skeleton.get_bone_count():
                check(skeleton.get_bone_pose(bone).is_finite(), "Finite pose: " + clip)
    var displacement: Dictionary = {}
    for variant: String in ["in_place", "root_motion"]:
        var animation := player.get_animation(variant + "/Zombie_Walk_F_1_Loop")
        var track := animation.find_track(NodePath("Skeleton3D:Hips"), Animation.TYPE_POSITION_3D)
        var delta: Vector3 = animation.track_get_key_value(track, animation.track_get_key_count(track) - 1) - animation.track_get_key_value(track, 0)
        displacement[variant] = Vector2(delta.x, delta.z).length()
    check(displacement.in_place < 0.001, "In-place walk does not drift")
    check(displacement.root_motion > 0.8, "Root-motion walk preserves forward movement")
    player.play("in_place/Zombie_Walk_F_1_Loop")
    player.seek(0, true)
    var foot := skeleton.find_bone("LeftFoot")
    check(foot >= 0, "Left foot exists")
    var start_pose := skeleton.get_bone_pose(foot)
    player.seek(0.5, true)
    check(not start_pose.is_equal_approx(skeleton.get_bone_pose(foot)), "Walk actually animates")
    character.free()
    if "--capture" in OS.get_cmdline_user_args():
        var preview := (load(BASE + "scenes/preview.tscn") as PackedScene).instantiate()
        root.add_child(preview)
        await create_timer(3.0).timeout
        await RenderingServer.frame_post_draw
        check(root.get_texture().get_image().save_png("res://cpp/.cache/zombie_starter_preview.png") == OK, "Preview captured")
        var clips := ["Zombie_Crawl_1_Loop", "Zombie_Death_Back_Mid_1", "Zombie_HyperChase_1_Loop"]
        var names := ["Idle", "Walk", "Attack"]
        for i in names.size():
            var actor := preview.get_node(names[i])
            var actor_player: AnimationPlayer = actor.get_node("AnimationPlayer")
            var clip := "in_place/" + str(clips[i])
            actor_player.callback_mode_process = AnimationMixer.ANIMATION_CALLBACK_MODE_PROCESS_MANUAL
            actor_player.play(clip)
            actor_player.seek(actor_player.get_animation(clip).length * (0.95 if i == 1 else 0.5), true)
            preview.get_node(names[i] + "Label").text = ["CRAWL", "DEATH", "HYPER CHASE"][i]
        await process_frame
        await RenderingServer.frame_post_draw
        check(root.get_texture().get_image().save_png("res://cpp/.cache/zombie_starter_poses.png") == OK, "Additional poses captured")
        preview.free()
    print("Zombie Starter: ", "PASS" if failures == 0 else "FAIL (%d)" % failures, "; walk displacement ", displacement)
    quit(0 if failures == 0 else 1)
