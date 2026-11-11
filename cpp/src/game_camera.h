#pragma once

#include "game_input.h"
#include <godot_cpp/classes/camera3d.hpp>

namespace godot
{

    class GameCamera : public Camera3D, public project_h::IGameSystem
    {
        GDCLASS(GameCamera, Camera3D)

        const GameInput *input = nullptr;
        bool initialized = false;
        double yaw = 0.0;
        double pitch = 0.0;

    protected:
        static void _bind_methods() {}

    public:
        void SetInput(const GameInput *value) { input = value; }
        void Init() override;
        void Update(double delta) override;
        void Release() override;
    };

} // namespace godot
