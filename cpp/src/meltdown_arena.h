#pragma once

#include "meltdown_sim.h"
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/audio_stream_player.hpp>
#include <godot_cpp/classes/audio_stream_wav.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <map>
#include <array>

namespace godot
{
    class MeltdownArena : public Node3D
    {
        GDCLASS(MeltdownArena, Node3D)

        meltdown::Simulation simulation;
        Camera3D *camera = nullptr;
        Vector3 camera_offset;
        Node3D *player = nullptr;
        Node3D *weapon_pivot = nullptr;
        MeshInstance3D *attack_mesh = nullptr;
        Label *status = nullptr;
        Label *message = nullptr;
        Label *center_message = nullptr;
        Label *skills = nullptr;
        Label *enemy_direction = nullptr;
        Node *heat_bar = nullptr;
        Node *hp_bar = nullptr;
        MeshInstance3D *player_body = nullptr;
        MeshInstance3D *vent_mesh = nullptr;
        MeshInstance3D *vent_ring = nullptr;
        MeshInstance3D *heat_ring = nullptr;
        MeshInstance3D *heat_ring_background = nullptr;
        MeshInstance3D *heat_core = nullptr;
        MeshInstance3D *weapon = nullptr;
        std::array<MeshInstance3D *, 8> steam_meshes = {};
        struct Trail
        {
            MeshInstance3D *mesh = nullptr;
            float life = 0;
        };
        std::array<Trail, 6> trails;
        float trail_clock = 0;
        float steam_left = 0;
        float heat_spent_left = 0;
        meltdown::Vec steam_origin;
        bool ignition = false;
        Vector3 last_trail_position;
        std::map<int, MeshInstance3D *> enemy_meshes;
        std::map<int, MeshInstance3D *> warning_meshes;
        std::map<int, MeshInstance3D *> health_meshes;
        std::map<int, MeshInstance3D *> projectile_meshes;
        std::map<int, MeshInstance3D *> fragment_meshes;
        struct EnvironmentVisual
        {
            Node3D *body = nullptr;
            MeshInstance3D *danger = nullptr;
            MeshInstance3D *health = nullptr;
            MeshInstance3D *direction = nullptr;
            meltdown::EnvironmentState state = meltdown::EnvironmentState::Intact;
        };
        std::map<int, EnvironmentVisual> environment_visuals;
        Ref<PackedScene> pipe_visual;
        Ref<PackedScene> wall_visual;
        Ref<PackedScene> pillar_block_visual;
        struct BlockVisual
        {
            Node3D *body = nullptr;
            MeshInstance3D *shadow = nullptr;
            MeshInstance3D *landing = nullptr;
            bool settled = false;
        };
        std::map<int, BlockVisual> block_visuals;
        Ref<AudioStream> environment_warning_sound;
        Ref<AudioStream> environment_burst_sound;
        Ref<StandardMaterial3D> environment_active_material;
        Label *environment_hint = nullptr;
        Ref<StandardMaterial3D> cyan;
        Ref<StandardMaterial3D> orange;
        Ref<StandardMaterial3D> white;
        Ref<StandardMaterial3D> pink;
        Ref<StandardMaterial3D> gold;
        Ref<StandardMaterial3D> warning_material;
        Ref<StandardMaterial3D> slash_material;
        Ref<StandardMaterial3D> player_material;
        Ref<StandardMaterial3D> vent_material;
        Ref<StandardMaterial3D> vent_ring_material;
        Ref<StandardMaterial3D> heat_ring_material;
        Ref<StandardMaterial3D> steam_material;
        bool practice = false;
        bool paused = false;
        bool muted = false;
        Vector2 pointer_position;
        bool pointer_seen = false;
        AudioStreamPlayer *swing_audio = nullptr;
        AudioStreamPlayer *hit_audio = nullptr;
        Ref<AudioStreamWAV> swing_sound;
        Ref<AudioStreamWAV> hit_sound;
        Ref<AudioStreamWAV> heat_sound;
        Ref<AudioStreamWAV> vent_sound;
        Ref<AudioStreamWAV> cooling_sound;
        Ref<AudioStreamWAV> warning_sound;
        void setup_input();
        void present();
        MeshInstance3D *box(Node3D *parent, Vector3 position, Vector3 size, const Ref<StandardMaterial3D> &material);
        void draw_sector(MeshInstance3D *target, meltdown::Vec position, meltdown::Vec direction, float radius, float angle);
        void draw_charge(MeshInstance3D *target, const meltdown::Enemy &enemy);
        void draw_ring(MeshInstance3D *target, meltdown::Vec position, float radius, float width, float fraction);
        void update_feedback(float delta);
        void present_environment();
        void present_blocks();
        void clear_environment_visuals();

    protected:
        static void _bind_methods();
        void _notification(int what);

    public:
        void _ready() override;
        void _physics_process(double delta) override;
        void _input(const Ref<InputEvent> &event) override;
        void _exit_tree() override;
        Dictionary get_snapshot() const;
        void restart();
        void set_training(bool enabled);
        void set_muted(bool enabled);
        void set_pipe_visual(const Ref<PackedScene> &scene);
        Ref<PackedScene> get_pipe_visual() const;
        void set_wall_visual(const Ref<PackedScene> &scene);
        Ref<PackedScene> get_wall_visual() const;
        void set_pillar_block_visual(const Ref<PackedScene> &scene);
        Ref<PackedScene> get_pillar_block_visual() const;
        void set_environment_warning_sound(const Ref<AudioStream> &sound);
        Ref<AudioStream> get_environment_warning_sound() const;
        void set_environment_burst_sound(const Ref<AudioStream> &sound);
        Ref<AudioStream> get_environment_burst_sound() const;
    };
}
