#pragma once

#include "game_camera.h"
#include <godot_cpp/classes/node3d.hpp>

namespace godot
{

    class EntryPoint : public Node3D, public project_h::IGameSystem
    {
        GDCLASS(EntryPoint, Node3D)

        GameInput *input = nullptr;
        GameCamera *camera = nullptr;
        bool initialized = false;

    protected:
        static void _bind_methods();

    public:
        void Init() override;
        void Update(double delta) override;
        void Release() override;
        void _ready() override;
        void _process(double delta) override;
        void _exit_tree() override;
        bool is_initialized() const { return initialized; }
    };

} // namespace godot
