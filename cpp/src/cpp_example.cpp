#include "cpp_example.h"

#include <cmath>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

void ProjectHCppExample::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_speed", "value"), &ProjectHCppExample::set_speed);
    ClassDB::bind_method(D_METHOD("get_speed"), &ProjectHCppExample::get_speed);
    ClassDB::bind_method(D_METHOD("set_amplitude", "value"), &ProjectHCppExample::set_amplitude);
    ClassDB::bind_method(D_METHOD("get_amplitude"), &ProjectHCppExample::get_amplitude);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "speed", PROPERTY_HINT_RANGE, "0,10,0.1"), "set_speed", "get_speed");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "amplitude", PROPERTY_HINT_RANGE, "0,300,1"), "set_amplitude", "get_amplitude");
}

void ProjectHCppExample::_ready() {
    starting_position = get_position();
    elapsed = 0.0;
    if (!Engine::get_singleton()->is_editor_hint()) {
        UtilityFunctions::print("[ProjectH C++] GDExtension loaded. The icon is moving from C++.");
    }
}

void ProjectHCppExample::_process(double delta) {
    if (Engine::get_singleton()->is_editor_hint()) {
        return;
    }
    elapsed += delta;
    set_position(starting_position + Vector2(std::sin(elapsed * speed) * amplitude, 0.0));
}

void ProjectHCppExample::set_speed(double value) { speed = value; }
double ProjectHCppExample::get_speed() const { return speed; }
void ProjectHCppExample::set_amplitude(double value) { amplitude = value; }
double ProjectHCppExample::get_amplitude() const { return amplitude; }
