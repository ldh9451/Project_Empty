#pragma once

#include "game_system.h"
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot
{

    class GameInput : public Node, public project_h::IGameSystem
    {
        GDCLASS(GameInput, Node)

        bool initialized = false;
        bool looking = false;
        bool fast = false;
        Input::MouseMode previous_mouse_mode = Input::MOUSE_MODE_VISIBLE;
        Vector3 movement;
        Vector2 pending_look;
        Vector2 look_delta;

        void EndLook();

    protected:
        static void _bind_methods();
        void _notification(int what);

    public:
        void Init() override;
        void Update(double delta) override;
        void Release() override;
        void _input(const Ref<InputEvent> &event) override;
        void _unhandled_input(const Ref<InputEvent> &event) override;

        Vector3 get_movement() const { return movement; }
        Vector2 get_look_delta() const { return look_delta; }
        bool is_looking() const { return looking; }
        bool is_fast() const { return fast; }
    };

} // namespace godot
