extends SceneTree

const BASE := "res://resources/characters/scifi_beast06_bull/"
var failures := 0

func check(condition: bool, message: String) -> void:
    if not condition:
        failures += 1
        push_error(message)

func _initialize() -> void:
    run.call_deferred()

func run() -> void:
    for skin in range(1, 4):
        var packed := load(BASE + "scenes/beast06_bull_skin%d.tscn" % skin) as PackedScene
        check(packed != null, "Skin %d loads" % skin)
        if packed == null:
            continue
        var character := packed.instantiate()
        root.add_child(character)
        var skeleton: Skeleton3D = character.get_node("Skeleton3D")
        var player: AnimationPlayer = character.get_node("AnimationPlayer")
        player.callback_mode_process = AnimationMixer.ANIMATION_CALLBACK_MODE_PROCESS_MANUAL
        check(skeleton.get_bone_count() == 71, "71 bones preserved")
        check(player.get_animation_list().size() == 30, "29 clips and RESET available")
        var meshes := character.find_children("*", "MeshInstance3D", true, false)
        check(meshes.size() == 15, "15 skinned mesh parts preserved")
        for mesh: MeshInstance3D in meshes:
            check(mesh.skin != null, "Skin binding exists: " + mesh.name)
            for bind in mesh.skin.get_bind_count():
                check(skeleton.find_bone(mesh.skin.get_bind_name(bind)) >= 0, "Skin bone resolves")
            var material := mesh.get_active_material(0) as StandardMaterial3D
            check(material != null, "Material exists: " + mesh.name)
            if mesh.name == "Glass1" or material == null:
                continue
            check("/skin%d/" % skin in material.resource_path, "Correct skin material")
            check(material.albedo_texture != null and material.normal_texture != null and material.emission_texture != null, "Color/normal/emission maps assigned")
            check(material.metallic_texture != null and material.roughness_texture != null and material.ao_texture != null, "Metallic/roughness/AO maps assigned")
            check(material.albedo_texture.get_width() == 4096, "Original 4K color resolution retained")
            check(material.emission_operator == BaseMaterial3D.EMISSION_OP_MULTIPLY, "Emission map modulates the source emission color")
        for clip in player.get_animation_list():
            var animation := player.get_animation(clip)
            check(animation.length > 0, "Clip has duration: " + clip)
            check(animation.get_track_count() == 213, "Every bone has position, rotation, scale: " + clip)
            for track in animation.get_track_count():
                var path := animation.track_get_path(track)
                check(str(path.get_concatenated_names()) == "Skeleton3D" and path.get_subname_count() == 1, "No dangling control tracks")
                check(skeleton.find_bone(path.get_subname(0)) >= 0, "Animation bone resolves")
            player.play(clip)
            player.seek(animation.length * 0.5, true)
            for bone in skeleton.get_bone_count():
                check(skeleton.get_bone_pose(bone).is_finite(), "Finite bone pose: " + clip)
        player.play("Idle1")
        player.seek(0.0, true)
        var poses: Array[Transform3D] = []
        for bone in skeleton.get_bone_count():
            poses.append(skeleton.get_bone_pose(bone))
        player.seek(1.0, true)
        var animated := false
        for bone in skeleton.get_bone_count():
            animated = animated or not poses[bone].is_equal_approx(skeleton.get_bone_pose(bone))
        check(animated, "Idle visibly animates the skeleton")
        character.free()
    if "--capture" in OS.get_cmdline_user_args():
        var preview := (load(BASE + "scenes/preview.tscn") as PackedScene).instantiate()
        root.add_child(preview)
        await create_timer(3.0).timeout
        await RenderingServer.frame_post_draw
        var image := root.get_texture().get_image()
        check(image.save_png("res://cpp/.cache/scifi_beast06_preview.png") == OK, "Preview screenshot saved")
        preview.free()
    print("SciFi Beast06: ", "PASS" if failures == 0 else "FAIL (%d)" % failures)
    quit(0 if failures == 0 else 1)
