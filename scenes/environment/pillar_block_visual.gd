extends Node3D

# All transforms and collisions come from the C++ model.
@export var body: MeshInstance3D
@export var cracks: Node3D
@export var stone: Material
@export var damaged: Material
@export var rubble: Material

func set_environment_state(state: int, _health_ratio: float, _remaining: float) -> void:
    if body:
        body.material_override = stone if state == 0 else rubble if state == 4 else damaged
    if cracks:
        cracks.visible = state == 1 or state == 2
