extends Node3D

# Presentation only. Native simulation owns HP, collision, timing and damage.
@export var intact: Node3D
@export var damaged: Node3D
@export var spent: Node3D
@export var burst: Node3D

func set_environment_state(state: int, _health_ratio: float, remaining: float) -> void:
    if intact:
        intact.visible = state == 0
    if damaged:
        damaged.visible = state == 1 or state == 2
        damaged.scale = Vector3.ONE * (1.0 + 0.035 * sin(remaining * 30.0) if state == 2 else 1.0)
    if spent:
        spent.visible = state >= 3
    if burst:
        burst.visible = state == 3
        burst.position.y = (0.4 - remaining) * 2.0
        burst.scale = Vector3.ONE * (1.0 + (0.4 - remaining) * 2.0)
