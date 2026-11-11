#include "game_input.h"

#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

void GameInput::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("get_movement"), &GameInput::get_movement);
    ClassDB::bind_method(D_METHOD("get_look_delta"), &GameInput::get_look_delta);
    ClassDB::bind_method(D_METHOD("is_looking"), &GameInput::is_looking);
    ClassDB::bind_method(D_METHOD("is_fast"), &GameInput::is_fast);
}

void GameInput::Init()
{
    if (initialized)
        return;
    initialized = true;
    set_process_input(true);
    set_process_unhandled_input(true);
}

void GameInput::Update(double delta)
{
    movement = Vector3();
    look_delta = pending_look;
    pending_look = Vector2();
    fast = false;
    if (!initialized || !looking)
        return;

    Input *input = Input::get_singleton();
    const Vector2 horizontal = input->get_vector("camera_left", "camera_right", "camera_forward", "camera_back");
    movement = Vector3(horizontal.x, input->get_axis("camera_down", "camera_up"), horizontal.y);
    if (movement.length_squared() > 1.0)
        movement.normalize();
    fast = input->is_action_pressed("camera_fast");
}

void GameInput::EndLook()
{
    if (looking && Input::get_singleton()->get_mouse_mode() == Input::MOUSE_MODE_CAPTURED)
    {
        Input::get_singleton()->set_mouse_mode(previous_mouse_mode);
    }
    looking = false;
    fast = false;
    movement = Vector3();
    pending_look = Vector2();
    look_delta = Vector2();
}

void GameInput::_input(const Ref<InputEvent> &event)
{
    if (!initialized || !looking)
        return;
    const Ref<InputEventMouseButton> button = event;
    const Ref<InputEventKey> key = event;
    // Releases must reach us even when a GUI control consumes the event later.
    if ((button.is_valid() && button->get_button_index() == MOUSE_BUTTON_RIGHT && !button->is_pressed()) ||
        (key.is_valid() && key->get_keycode() == KEY_ESCAPE && key->is_pressed()))
    {
        EndLook();
        get_viewport()->set_input_as_handled();
    }
}

void GameInput::_unhandled_input(const Ref<InputEvent> &event)
{
    if (!initialized)
        return;
    const Ref<InputEventMouseButton> button = event;
    if (button.is_valid() && button->get_button_index() == MOUSE_BUTTON_RIGHT && button->is_pressed())
    {
        if (!looking)
        {
            previous_mouse_mode = Input::get_singleton()->get_mouse_mode();
            Input::get_singleton()->set_mouse_mode(Input::MOUSE_MODE_CAPTURED);
            looking = true;
        }
        get_viewport()->set_input_as_handled();
    }
    const Ref<InputEventMouseMotion> motion = event;
    if (looking && motion.is_valid())
    {
        pending_look += motion->get_screen_relative();
        get_viewport()->set_input_as_handled();
    }
}

void GameInput::_notification(int what)
{
    if (what == NOTIFICATION_APPLICATION_FOCUS_OUT || what == NOTIFICATION_PAUSED)
        EndLook();
}

void GameInput::Release()
{
    EndLook();
    initialized = false;
    set_process_input(false);
    set_process_unhandled_input(false);
}
