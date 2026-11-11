#pragma once

#include <godot_cpp/classes/sprite2d.hpp>

namespace godot {

class ProjectHCppExample : public Sprite2D {
    GDCLASS(ProjectHCppExample, Sprite2D)

    double elapsed = 0.0;
    double speed = 2.0;
    double amplitude = 80.0;
    Vector2 starting_position;

protected:
    static void _bind_methods();

public:
    void _ready() override;
    void _process(double delta) override;
    void set_speed(double value);
    double get_speed() const;
    void set_amplitude(double value);
    double get_amplitude() const;
};

} // namespace godot
