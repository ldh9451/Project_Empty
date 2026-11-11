extends Node3D

@onready var terrain: Node3D = $Terrain
@onready var camera: Camera3D = $Camera3D
@onready var brush: MeshInstance3D = $Brush
@onready var stats: Label = $CanvasLayer/Panel/Margin/VBox/Stats
@onready var radius_label: Label = $CanvasLayer/Panel/Margin/VBox/Radius

var radius := 3.0
var yaw := 0.7
var pitch := 0.62
var distance := 80.0
var target := Vector3(0, -2, 0)
var stroke_time := 0.0
var digging := false
var building := false
var orbiting := false
var terrain_seed := 7
var brush_material: StandardMaterial3D

func _ready() -> void:
    brush_material = brush.material_override as StandardMaterial3D
    _update_camera()
    _update_stats()

func _update_camera() -> void:
    camera.position = target + Vector3(sin(yaw) * cos(pitch), sin(pitch), cos(yaw) * cos(pitch)) * distance
    camera.look_at(target)

func _update_stats() -> void:
    stats.text = "%s triangles  /  %s chunks\nLast update  %.1f ms" % [terrain.get_triangle_count(), terrain.get_chunk_count(), terrain.get_last_build_ms()]
    radius_label.text = "BRUSH RADIUS    %.1f m" % radius

func _unhandled_input(event: InputEvent) -> void:
    if event is InputEventMouseButton:
        match event.button_index:
            MOUSE_BUTTON_LEFT:
                digging = event.pressed
                if event.pressed: stroke_time = 0.06
            MOUSE_BUTTON_RIGHT:
                building = event.pressed
                if event.pressed: stroke_time = 0.06
            MOUSE_BUTTON_MIDDLE:
                orbiting = event.pressed
            MOUSE_BUTTON_WHEEL_UP:
                if event.pressed:
                    distance = clampf(distance * 0.9, 12.0, 130.0)
                    _update_camera()
            MOUSE_BUTTON_WHEEL_DOWN:
                if event.pressed:
                    distance = clampf(distance / 0.9, 12.0, 130.0)
                    _update_camera()
    elif event is InputEventMouseMotion and orbiting:
        if Input.is_key_pressed(KEY_SHIFT):
            target += (-camera.global_basis.x * event.relative.x + camera.global_basis.y * event.relative.y) * distance * 0.0015
        else:
            yaw -= event.relative.x * 0.006
            pitch = clampf(pitch + event.relative.y * 0.006, -0.15, 1.4)
        _update_camera()
    elif event is InputEventKey and event.pressed and not event.echo:
        match event.physical_keycode:
            KEY_Q: radius = maxf(1.5, radius - 0.5)
            KEY_E: radius = minf(8.0, radius + 0.5)
            KEY_R:
                terrain.reset_terrain(terrain_seed)
            KEY_N:
                terrain_seed += 1
                terrain.reset_terrain(terrain_seed)
            KEY_F:
                yaw = 0.7
                pitch = 0.62
                distance = 80.0
                target = Vector3(0, -2, 0)
                _update_camera()
        _update_stats()

func _notification(what: int) -> void:
    if what == NOTIFICATION_APPLICATION_FOCUS_OUT:
        digging = false
        building = false
        orbiting = false

func _physics_process(delta: float) -> void:
    var mouse := get_viewport().get_mouse_position()
    var ray_from := camera.project_ray_origin(mouse)
    var ray_to := ray_from + camera.project_ray_normal(mouse) * 300.0
    var query := PhysicsRayQueryParameters3D.create(ray_from, ray_to, 1)
    var hit := get_world_3d().direct_space_state.intersect_ray(query)
    brush.visible = not hit.is_empty() and not orbiting
    if hit.is_empty() or orbiting:
        stroke_time = 0.0
        return
    brush.global_position = hit.position
    brush.scale = Vector3.ONE * radius
    brush_material.albedo_color = Color(1.0, 0.62, 0.24, 0.16) if building else Color(0.25, 0.95, 0.79, 0.16)
    if digging == building:
        stroke_time = 0.0
        return
    stroke_time += delta
    if stroke_time >= 0.06:
        # Sculpt outside the physics query callback; collision updates are safe deferred.
        var amount := minf(stroke_time, 0.12) * 16.0 * (-1.0 if building else 1.0)
        stroke_time = 0.0
        _apply_brush.call_deferred(terrain.to_local(hit.position), radius, amount)

func _apply_brush(center: Vector3, brush_radius: float, amount: float) -> void:
    terrain.sculpt(center, brush_radius, amount)
    _update_stats()
