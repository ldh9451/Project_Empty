#include "meltdown_arena.h"

#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/input_map.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

namespace
{
    Ref<StandardMaterial3D> material(Color color, bool unshaded = false)
    {
        Ref<StandardMaterial3D> result;
        result.instantiate();
        result->set_albedo(color);
        result->set_roughness(0.7f);
        result->set_diffuse_mode(BaseMaterial3D::DIFFUSE_TOON);
        result->set_specular_mode(BaseMaterial3D::SPECULAR_DISABLED);
        result->set_cull_mode(BaseMaterial3D::CULL_DISABLED);
        if (unshaded)
            result->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
        return result;
    }

    Vector3 world(meltdown::Vec value, float y = 0)
    {
        return Vector3(value.x, y, value.z);
    }

    Color heat_color(int tier)
    {
        const Color colors[] = {Color(0.33, 0.77, 0.86), Color(0.95, 0.7, 0.25),
            Color(1, 0.35, 0.12), Color(1, 0.17, 0.08), Color(1, 0.94, 0.76)};
        return colors[tier];
    }

    Ref<AudioStreamWAV> tone(float frequency, float duration)
    {
        Ref<AudioStreamWAV> result;
        result.instantiate();
        result->set_format(AudioStreamWAV::FORMAT_16_BITS);
        result->set_mix_rate(22050);
        const int samples = static_cast<int>(22050 * duration);
        PackedByteArray bytes;
        bytes.resize(samples * 2);
        for (int i = 0; i < samples; ++i)
        {
            const float progress = static_cast<float>(i) / samples;
            const float phase = 2 * meltdown::pi * frequency * i / 22050 * (1 - 0.3f * progress);
            const auto sample = static_cast<int16_t>(std::sin(phase) * (1 - progress) * (1 - progress) * 6500);
            bytes.set(i * 2, static_cast<uint8_t>(sample & 255));
            bytes.set(i * 2 + 1, static_cast<uint8_t>((static_cast<uint16_t>(sample) >> 8) & 255));
        }
        result->set_data(bytes);
        return result;
    }
}

void MeltdownArena::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("get_snapshot"), &MeltdownArena::get_snapshot);
    ClassDB::bind_method(D_METHOD("set_dungeon_material", "material"), &MeltdownArena::set_dungeon_material);
    ClassDB::bind_method(D_METHOD("get_dungeon_material"), &MeltdownArena::get_dungeon_material);
    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "dungeon_material", PROPERTY_HINT_RESOURCE_TYPE, "Material"),
        "set_dungeon_material", "get_dungeon_material");
    ClassDB::bind_method(D_METHOD("restart"), &MeltdownArena::restart);
    ClassDB::bind_method(D_METHOD("set_training", "enabled"), &MeltdownArena::set_training);
    ClassDB::bind_method(D_METHOD("set_muted", "enabled"), &MeltdownArena::set_muted);
    ClassDB::bind_method(D_METHOD("set_pipe_visual", "scene"), &MeltdownArena::set_pipe_visual);
    ClassDB::bind_method(D_METHOD("get_pipe_visual"), &MeltdownArena::get_pipe_visual);
    ClassDB::bind_method(D_METHOD("set_wall_visual", "scene"), &MeltdownArena::set_wall_visual);
    ClassDB::bind_method(D_METHOD("get_wall_visual"), &MeltdownArena::get_wall_visual);
    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "pipe_visual", PROPERTY_HINT_RESOURCE_TYPE, "PackedScene"), "set_pipe_visual", "get_pipe_visual");
    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "wall_visual", PROPERTY_HINT_RESOURCE_TYPE, "PackedScene"), "set_wall_visual", "get_wall_visual");
    ClassDB::bind_method(D_METHOD("set_pillar_block_visual", "scene"), &MeltdownArena::set_pillar_block_visual);
    ClassDB::bind_method(D_METHOD("get_pillar_block_visual"), &MeltdownArena::get_pillar_block_visual);
    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "pillar_block_visual", PROPERTY_HINT_RESOURCE_TYPE, "PackedScene"),
        "set_pillar_block_visual", "get_pillar_block_visual");
    ClassDB::bind_method(D_METHOD("set_environment_warning_sound", "sound"), &MeltdownArena::set_environment_warning_sound);
    ClassDB::bind_method(D_METHOD("get_environment_warning_sound"), &MeltdownArena::get_environment_warning_sound);
    ClassDB::bind_method(D_METHOD("set_environment_burst_sound", "sound"), &MeltdownArena::set_environment_burst_sound);
    ClassDB::bind_method(D_METHOD("get_environment_burst_sound"), &MeltdownArena::get_environment_burst_sound);
    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "environment_warning_sound", PROPERTY_HINT_RESOURCE_TYPE, "AudioStream"),
        "set_environment_warning_sound", "get_environment_warning_sound");
    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "environment_burst_sound", PROPERTY_HINT_RESOURCE_TYPE, "AudioStream"),
        "set_environment_burst_sound", "get_environment_burst_sound");
}

void MeltdownArena::set_environment_warning_sound(const Ref<AudioStream> &sound)
{
    environment_warning_sound = sound;
}

Ref<AudioStream> MeltdownArena::get_environment_warning_sound() const
{
    return environment_warning_sound;
}

void MeltdownArena::set_environment_burst_sound(const Ref<AudioStream> &sound)
{
    environment_burst_sound = sound;
}

Ref<AudioStream> MeltdownArena::get_environment_burst_sound() const
{
    return environment_burst_sound;
}

void MeltdownArena::clear_environment_visuals()
{
    for (const auto &entry : block_visuals)
    {
        for (Node3D *node : {entry.second.body, static_cast<Node3D *>(entry.second.shadow), static_cast<Node3D *>(entry.second.landing)})
        {
            remove_child(node);
            node->queue_free();
        }
    }
    block_visuals.clear();
    for (const auto &entry : environment_visuals)
    {
        for (Node3D *node : {entry.second.body, static_cast<Node3D *>(entry.second.danger),
            static_cast<Node3D *>(entry.second.health), static_cast<Node3D *>(entry.second.direction)})
        {
            remove_child(node);
            node->queue_free();
        }
    }
    environment_visuals.clear();
}

void MeltdownArena::set_pipe_visual(const Ref<PackedScene> &scene)
{
    pipe_visual = scene;
    clear_environment_visuals();
}

Ref<PackedScene> MeltdownArena::get_pipe_visual() const
{
    return pipe_visual;
}

void MeltdownArena::set_wall_visual(const Ref<PackedScene> &scene)
{
    wall_visual = scene;
    clear_environment_visuals();
}

Ref<PackedScene> MeltdownArena::get_wall_visual() const
{
    return wall_visual;
}

void MeltdownArena::set_pillar_block_visual(const Ref<PackedScene> &scene)
{
    pillar_block_visual = scene;
    clear_environment_visuals();
}

Ref<PackedScene> MeltdownArena::get_pillar_block_visual() const
{
    return pillar_block_visual;
}

void MeltdownArena::setup_input()
{
    InputMap *map = InputMap::get_singleton();
    const char *names[] = {"md_left", "md_right", "md_forward", "md_back", "md_dodge", "md_restart",
        "md_vent", "md_overdrive", "md_training", "md_pause", "md_upgrade_1", "md_upgrade_2", "md_upgrade_3", "md_mute"};
    const Key keys[] = {KEY_A, KEY_D, KEY_W, KEY_S, KEY_SPACE, KEY_R, KEY_Q, KEY_E, KEY_F1, KEY_ESCAPE, KEY_1, KEY_2, KEY_3, KEY_M};
    for (int i = 0; i < 14; ++i)
    {
        if (map->has_action(names[i]))
            continue;
        map->add_action(names[i]);
        Ref<InputEventKey> key;
        key.instantiate();
        key->set_physical_keycode(keys[i]);
        map->action_add_event(names[i], key);
    }
    for (int i = 0; i < 2; ++i)
    {
        const char *name = i == 0 ? "md_light" : "md_heavy";
        if (map->has_action(name))
            continue;
        map->add_action(name);
        Ref<InputEventMouseButton> mouse;
        mouse.instantiate();
        mouse->set_button_index(i == 0 ? MOUSE_BUTTON_LEFT : MOUSE_BUTTON_RIGHT);
        map->action_add_event(name, mouse);
    }
}

MeshInstance3D *MeltdownArena::box(Node3D *parent, Vector3 position, Vector3 size, const Ref<StandardMaterial3D> &surface)
{
    MeshInstance3D *node = memnew(MeshInstance3D);
    Ref<BoxMesh> mesh;
    mesh.instantiate();
    mesh->set_size(size);
    node->set_mesh(mesh);
    node->set_material_override(surface);
    parent->add_child(node);
    node->set_position(position);
    return node;
}

void MeltdownArena::_ready()
{
    if (Engine::get_singleton()->is_editor_hint())
        return;
    camera = Object::cast_to<Camera3D>(get_node_or_null("Camera"));
    status = Object::cast_to<Label>(get_node_or_null("HUD/Status"));
    message = Object::cast_to<Label>(get_node_or_null("HUD/Message"));
    center_message = Object::cast_to<Label>(get_node_or_null("HUD/Center"));
    skills = Object::cast_to<Label>(get_node_or_null("HUD/Skills"));
    enemy_direction = Object::cast_to<Label>(get_node_or_null("HUD/EnemyDirection"));
    environment_hint = Object::cast_to<Label>(get_node_or_null("HUD/EnvironmentHint"));
    heat_bar = get_node_or_null("HUD/Heat");
    hp_bar = get_node_or_null("HUD/Health");
    if (!camera || !status || !message)
    {
        set_physics_process(false);
        return;
    }
    setup_input();
    camera_offset = camera->get_position();
    cyan = material(Color(0.33, 0.77, 0.86));
    orange = material(Color(0.96, 0.37, 0.16));
    white = material(Color(1.0, 0.94, 0.76), true);
    pink = material(Color(1.0, 0.12, 0.42), true);
    gold = material(Color(0.96, 0.63, 0.27));
    warning_material = material(Color(0.85, 0.06, 0.24, 0.38f), true);
    warning_material->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
    environment_active_material = material(Color(1, 0.65, 0.12, 0.6f), true);
    environment_active_material->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
    slash_material = material(Color(0.85, 0.96, 1, 0.3f), true);
    slash_material->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
    player_material = material(Color(0.33, 0.77, 0.86));
    player_material->set_feature(BaseMaterial3D::FEATURE_EMISSION, true);
    vent_material = material(Color(0.45, 0.95, 1, 0.2f), true);
    vent_material->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
    vent_ring_material = material(Color(0.65, 1, 1, 0.9f), true);
    vent_ring_material->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
    heat_ring_material = material(Color(1, 0.8, 0.3), true);
    steam_material = material(Color(0.65, 0.95, 1, 0.3f), true);
    steam_material->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
    const float half = meltdown::arena_half_size;
    for (int side : {-1, 1})
    {
        box(this, Vector3(side * (half - 0.3f), 0.2, 0), Vector3(0.2, 0.4, half * 2 - 0.4f), cyan);
        box(this, Vector3(0, 0.2, side * (half - 0.3f)), Vector3(half * 2 - 0.4f, 0.4, 0.2), cyan);
    }
    player = memnew(Node3D);
    player->set_name("Player");
    add_child(player);
    player_body = box(player, Vector3(0, 0.65, 0), Vector3(0.65, 1.1, 0.65), player_material);
    weapon_pivot = memnew(Node3D);
    player->add_child(weapon_pivot);
    weapon = box(weapon_pivot, Vector3(0.52, 0.75, -0.5), Vector3(0.16, 0.18, 1.4), white);
    heat_core = box(player, Vector3(0, 1.3, 0), Vector3(0.22, 0.8, 0.22), white);
    heat_core->set_name("OverdriveCore");
    attack_mesh = memnew(MeshInstance3D);
    add_child(attack_mesh);
    attack_mesh->set_material_override(slash_material);
    vent_mesh = memnew(MeshInstance3D);
    add_child(vent_mesh);
    vent_mesh->set_material_override(vent_material);
    vent_ring = memnew(MeshInstance3D);
    vent_ring->set_name("VentShockwave");
    add_child(vent_ring);
    vent_ring->set_material_override(vent_ring_material);
    heat_ring = memnew(MeshInstance3D);
    heat_ring->set_name("HeatTimer");
    add_child(heat_ring);
    heat_ring->set_material_override(heat_ring_material);
    heat_ring_background = memnew(MeshInstance3D);
    add_child(heat_ring_background);
    heat_ring_background->set_material_override(material(Color(0.15, 0.22, 0.28), true));
    for (auto &steam : steam_meshes)
        steam = box(this, {}, Vector3(0.18, 0.35, 0.18), steam_material);
    for (auto &trail : trails)
    {
        const auto surface = material(Color(1, 0.83, 0.4, 0.2f), true);
        surface->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
        trail.mesh = box(this, {}, Vector3(0.65, 1.1, 0.65), surface);
    }
    swing_audio = memnew(AudioStreamPlayer);
    hit_audio = memnew(AudioStreamPlayer);
    add_child(swing_audio);
    add_child(hit_audio);
    swing_audio->set_volume_db(-12);
    hit_audio->set_volume_db(-10);
    swing_sound = tone(380, 0.09f);
    hit_sound = tone(140, 0.13f);
    heat_sound = tone(740, 0.28f);
    vent_sound = tone(85, 0.35f);
    cooling_sound = tone(190, 0.4f);
    warning_sound = tone(1100, 0.12f);
    restart();
}

void MeltdownArena::_notification(int what)
{
    if (what == NOTIFICATION_APPLICATION_FOCUS_OUT && player)
        paused = true;
}

void MeltdownArena::_input(const Ref<InputEvent> &event)
{
    const Ref<InputEventMouse> mouse = event;
    if (mouse.is_valid())
    {
        pointer_position = mouse->get_position();
        pointer_seen = true;
    }
}

void MeltdownArena::_exit_tree()
{
    if (swing_audio)
        swing_audio->stop();
    if (hit_audio)
        hit_audio->stop();
}

void MeltdownArena::restart()
{
    clear_dungeon_visuals();
    clear_environment_visuals();
    simulation.reset(practice);
    paused = false;
    steam_left = 0;
    heat_spent_left = 0;
    trail_clock = 0;
    ignition = false;
    last_trail_position = Vector3();
    for (auto &trail : trails)
    {
        trail.life = 0;
        if (trail.mesh)
            trail.mesh->hide();
    }
    for (const auto &entry : enemy_meshes)
        entry.second->queue_free();
    enemy_meshes.clear();
    health_meshes.clear();
    for (const auto &entry : warning_meshes)
        entry.second->queue_free();
    warning_meshes.clear();
    for (const auto &entry : projectile_meshes)
        entry.second->queue_free();
    projectile_meshes.clear();
    for (const auto &entry : fragment_meshes)
        entry.second->queue_free();
    fragment_meshes.clear();
    if (player)
        present();
}

void MeltdownArena::set_training(bool enabled)
{
    practice = enabled;
    restart();
}

void MeltdownArena::set_muted(bool enabled)
{
    muted = enabled;
    if (muted)
    {
        if (swing_audio)
            swing_audio->stop();
        if (hit_audio)
            hit_audio->stop();
    }
}

void MeltdownArena::_physics_process(double delta)
{
    if (!player)
        return;
    Input *input = Input::get_singleton();
    if (input->is_action_just_pressed("md_training"))
        set_training(!practice);
    if (input->is_action_just_pressed("md_pause"))
        paused = !paused;
    if (input->is_action_just_pressed("md_mute"))
        set_muted(!muted);
    if (input->is_action_just_pressed("md_restart") && (simulation.finished || practice))
        restart();
    if (paused)
    {
        present();
        return;
    }
    meltdown::Input command;
    const Vector2 movement = input->get_vector("md_left", "md_right", "md_forward", "md_back");
    Vector3 right = camera->get_basis().get_column(0);
    Vector3 back = camera->get_basis().get_column(2);
    right.y = 0;
    back.y = 0;
    const Vector3 motion = right.normalized() * movement.x + back.normalized() * movement.y;
    command.move = {motion.x, motion.z};
    const Vector2 mouse = pointer_seen ? pointer_position : get_viewport()->get_mouse_position();
    const Vector3 origin = camera->project_ray_origin(mouse);
    const Vector3 ray = camera->project_ray_normal(mouse);
    if (std::abs(ray.y) > 0.001f)
    {
        const Vector3 target = origin - ray * (origin.y / ray.y);
        command.aim = {target.x - simulation.player.x, target.z - simulation.player.z};
    }
    command.light = input->is_action_pressed("md_light");
    command.heavy = input->is_action_pressed("md_heavy");
    command.dodge = input->is_action_just_pressed("md_dodge");
    command.vent = input->is_action_just_pressed("md_vent");
    command.overdrive = input->is_action_just_pressed("md_overdrive");
    for (int i = 1; i <= 3; ++i)
    {
        if (input->is_action_just_pressed("md_upgrade_" + String::num_int64(i)))
            command.upgrade = i;
    }
    const int old_attack = simulation.attack.id;
    const int old_hits = simulation.hits;
    const auto old_mode = simulation.heat.mode;
    const float old_vent = simulation.vent_flash;
    const float old_remaining = simulation.heat.remaining;
    const float old_elapsed = simulation.elapsed;
    simulation.tick(static_cast<float>(delta), command);
    const bool vented = simulation.vent_flash > old_vent;
    const bool changed_mode = old_mode != simulation.heat.mode;
    const bool ending = old_mode == meltdown::HeatMode::Overdrive && old_remaining > 1 && simulation.heat.remaining <= 1;
    update_feedback(simulation.elapsed - old_elapsed);
    if (vented || changed_mode)
    {
        steam_left = 0.45f;
        steam_origin = vented ? simulation.vent_position : simulation.player;
        ignition = simulation.heat.mode == meltdown::HeatMode::Overdrive;
    }
    if (vented)
        heat_spent_left = 0.65f;
    if (!muted && old_attack != simulation.attack.id && simulation.attack.id != 0)
    {
        swing_audio->set_stream(swing_sound);
        swing_audio->play();
    }
    if (!muted && (old_hits != simulation.hits || changed_mode || vented || ending))
    {
        hit_audio->set_stream(vented ? vent_sound : changed_mode ?
            (simulation.heat.mode == meltdown::HeatMode::Overdrive ? heat_sound : cooling_sound) : ending ? warning_sound : hit_sound);
        hit_audio->play();
    }
    present();
}

void MeltdownArena::update_feedback(float delta)
{
    steam_left = std::max(0.0f, steam_left - delta);
    heat_spent_left = std::max(0.0f, heat_spent_left - delta);
    for (auto &trail : trails)
    {
        trail.life = std::max(0.0f, trail.life - delta);
        trail.mesh->set_visible(trail.life > 0);
        if (trail.life > 0)
        {
            Ref<StandardMaterial3D> surface = trail.mesh->get_material_override();
            surface->set_albedo(Color(1, 0.83, 0.4, 0.26f * trail.life / 0.3f));
        }
    }
    trail_clock += delta;
    const Vector3 position = world(simulation.player, 0.65f);
    if (simulation.heat.mode == meltdown::HeatMode::Overdrive && delta > 0 &&
        trail_clock >= 0.05f && position.distance_to(last_trail_position) >= 0.18f)
    {
        auto &trail = *std::min_element(trails.begin(), trails.end(),
            [](const Trail &a, const Trail &b)
            {
                return a.life < b.life;
            });
        trail.life = 0.3f;
        trail.mesh->set_position(position);
        trail.mesh->set_rotation(player->get_rotation());
        trail.mesh->show();
        last_trail_position = position;
        trail_clock = 0;
    }
}

void MeltdownArena::draw_ring(MeshInstance3D *target, meltdown::Vec position, float radius, float width, float fraction)
{
    PackedVector3Array vertices;
    const float inner = std::max(0.0f, radius - width);
    for (int i = 0; i < 64; ++i)
    {
        const float a = i * 2 * meltdown::pi * fraction / 64;
        const float b = (i + 1) * 2 * meltdown::pi * fraction / 64;
        const Vector3 u(std::sin(a), 0, std::cos(a));
        const Vector3 v(std::sin(b), 0, std::cos(b));
        vertices.push_back(u * inner);
        vertices.push_back(u * radius);
        vertices.push_back(v * radius);
        vertices.push_back(u * inner);
        vertices.push_back(v * radius);
        vertices.push_back(v * inner);
    }
    Array arrays;
    arrays.resize(Mesh::ARRAY_MAX);
    arrays[Mesh::ARRAY_VERTEX] = vertices;
    Ref<ArrayMesh> mesh;
    mesh.instantiate();
    mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
    target->set_mesh(mesh);
    target->set_position(world(position, 0.09f));
}

void MeltdownArena::draw_sector(MeshInstance3D *target, meltdown::Vec position, meltdown::Vec direction, float radius, float angle)
{
    PackedVector3Array vertices;
    const float center = std::atan2(direction.x, direction.z);
    const float half = angle * meltdown::pi / 360.0f;
    for (int i = 0; i < 32; ++i)
    {
        const float a = center - half + 2 * half * i / 32;
        const float b = center - half + 2 * half * (i + 1) / 32;
        vertices.push_back(Vector3());
        vertices.push_back(Vector3(std::sin(a) * radius, 0, std::cos(a) * radius));
        vertices.push_back(Vector3(std::sin(b) * radius, 0, std::cos(b) * radius));
    }
    Array arrays;
    arrays.resize(Mesh::ARRAY_MAX);
    arrays[Mesh::ARRAY_VERTEX] = vertices;
    Ref<ArrayMesh> mesh;
    mesh.instantiate();
    mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
    target->set_mesh(mesh);
    target->set_position(world(position, 0.06f));
}

void MeltdownArena::draw_charge(MeshInstance3D *target, const meltdown::Enemy &enemy)
{
    PackedVector3Array outline;
    for (int i = 0; i <= 16; ++i)
    {
        const float angle = -meltdown::pi / 2 + i * meltdown::pi / 16;
        outline.push_back(Vector3(std::sin(angle) * 1.05f, 0, 6 + std::cos(angle) * 1.05f));
    }
    for (int i = 0; i <= 16; ++i)
    {
        const float angle = meltdown::pi / 2 + i * meltdown::pi / 16;
        outline.push_back(Vector3(std::sin(angle) * 1.05f, 0, std::cos(angle) * 1.05f));
    }
    PackedVector3Array vertices;
    for (int i = 0; i < outline.size(); ++i)
    {
        vertices.push_back(Vector3(0, 0, 3));
        vertices.push_back(outline[i]);
        vertices.push_back(outline[(i + 1) % outline.size()]);
    }
    Array arrays;
    arrays.resize(Mesh::ARRAY_MAX);
    arrays[Mesh::ARRAY_VERTEX] = vertices;
    Ref<ArrayMesh> mesh;
    mesh.instantiate();
    mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
    target->set_mesh(mesh);
    target->set_position(world(enemy.position, 0.06f));
    target->set_rotation(Vector3(0, std::atan2(enemy.direction.x, enemy.direction.z), 0));
}

void MeltdownArena::present()
{
    present_dungeon();
    present_environment();
    present_player();
    present_enemies();
    present_attacks();
    present_projectiles();
    present_fragments();
    present_hud();
}

void MeltdownArena::present_player()
{
    player->set_position(world(simulation.player));
    camera->set_position(camera_offset + world(simulation.player));
    const auto facing = simulation.attack.pending ? simulation.attack.direction :
        simulation.slash.life > 0 ? simulation.slash.direction : simulation.aim;
    player->set_rotation(Vector3(0, std::atan2(-facing.x, -facing.z), 0));
    weapon_pivot->set_rotation(Vector3(0, simulation.attack.pending ? -0.9f :
        simulation.slash.life > 0 ? (0.5f - simulation.slash.life / 0.14f) * 2.2f : 0, 0));
    const Color current_color = heat_color(simulation.heat.tier());
    player_material->set_albedo(current_color);
    const bool overdrive = simulation.heat.mode == meltdown::HeatMode::Overdrive;
    const bool cooling = simulation.heat.mode == meltdown::HeatMode::Cooling;
    player_material->set_emission(current_color);
    player_material->set_emission_energy_multiplier(overdrive ? 1.6f : 0.05f);
    weapon->set_scale(overdrive ? Vector3(1.4, 1.4, 1.4) : Vector3(1, 1, 1));
    heat_core->set_visible(overdrive);
    heat_core->set_scale(Vector3(1, 1 + 0.15f * std::sin(simulation.elapsed * 12), 1));
    heat_ring->set_visible(overdrive || cooling);
    heat_ring_background->set_visible(overdrive || cooling);
    if (overdrive || cooling)
    {
        const auto &heat = simulation.heat;
        const float duration = overdrive ? heat.overdrive_duration : heat.cooling_duration;
        const float pulse = overdrive && heat.remaining < 1 ? 0.6f + 0.4f * std::abs(std::sin(heat.remaining * 14)) : 1;
        heat_ring_material->set_albedo(overdrive ? Color(1, 0.82, 0.3) * pulse : Color(0.35, 0.8, 1));
        draw_ring(heat_ring_background, simulation.player, 1.08f, 0.16f, 1);
        draw_ring(heat_ring, simulation.player, 1.08f, 0.16f, std::clamp(heat.remaining / duration, 0.0f, 1.0f));
        heat_ring->set_position(world(simulation.player, 0.10f));
    }
    for (int i = 0; i < static_cast<int>(steam_meshes.size()); ++i)
    {
        auto *steam = steam_meshes[i];
        steam->set_visible(steam_left > 0);
        if (steam_left > 0)
        {
            const float progress = 1 - steam_left / 0.45f;
            const float angle = i * meltdown::pi / 4;
            const float radius = 0.65f + progress * (ignition ? 0.2f : 1.2f);
            steam->set_position(world(steam_origin + meltdown::Vec{std::sin(angle), std::cos(angle)} * radius,
                0.5f + progress * 1.6f));
            steam->set_scale(Vector3(1 + progress, ignition ? 3 : 1 + progress * 2, 1 + progress));
        }
    }
    steam_material->set_albedo(ignition ? Color(1, 0.85, 0.45, steam_left) : Color(0.65, 0.95, 1, steam_left));
    player_body->set_material_override(simulation.hurt_left > 0 && static_cast<int>(simulation.hurt_left * 30) % 2 ? white :
        simulation.dash_left > 0 ? white : player_material);
}

void MeltdownArena::present_enemies()
{
    for (auto it = enemy_meshes.begin(); it != enemy_meshes.end();)
    {
        const bool alive = std::any_of(simulation.enemies.begin(), simulation.enemies.end(),
            [&](const meltdown::Enemy &enemy)
            {
                return enemy.id == it->first;
            });
        if (!alive)
        {
            it->second->queue_free();
            warning_meshes[it->first]->queue_free();
            warning_meshes.erase(it->first);
            health_meshes.erase(it->first);
            it = enemy_meshes.erase(it);
        }
        else
            ++it;
    }
    for (const auto &enemy : simulation.enemies)
    {
        if (!enemy_meshes.count(enemy.id))
        {
            const Vector3 size = enemy.kind == 1 ? Vector3(0.7, 0.5, 1.0) :
                enemy.kind == 4 ? Vector3(1.3, 1.3, 1.3) : Vector3(0.8, 1.2, 0.8);
            auto *body = box(this, world(enemy.position, 0.6f), size, orange);
            enemy_meshes[enemy.id] = body;
            if (enemy.kind == 2)
            {
                box(body, Vector3(0, 0.3, 0), Vector3(1.4, 0.15, 0.2), pink);
                box(body, Vector3(0, 0.3, 0), Vector3(0.2, 0.15, 1.4), pink);
            }
            if (enemy.kind == 3)
                box(body, Vector3(0, 0.3, -0.7), Vector3(0.3, 0.3, 1.0), gold);
            if (enemy.kind == 4)
            {
                for (int i = 0; i < 8; ++i)
                {
                    const float angle = i * meltdown::pi / 4;
                    auto *plate = box(body, Vector3(std::sin(angle) * 0.65f, 0.2, std::cos(angle) * 0.65f),
                        Vector3(0.3, 0.7, 0.3), gold);
                    plate->set_name("Armor" + String::num_int64(i));
                }
            }
            health_meshes[enemy.id] = box(body, Vector3(0, 1, 0), Vector3(0.85, 0.07, 0.08), cyan);
            auto *warning = memnew(MeshInstance3D);
            add_child(warning);
            warning->set_material_override(warning_material);
            warning_meshes[enemy.id] = warning;
        }
        MeshInstance3D *mesh = enemy_meshes[enemy.id];
        mesh->set_visible(enemy.hp > 0);
        mesh->set_position(world(enemy.position, enemy.kind == 1 ? 0.35f : 0.6f));
        mesh->set_rotation(Vector3(0, std::atan2(-enemy.direction.x, -enemy.direction.z), 0));
        mesh->set_material_override(enemy.flash > 0 ? white : enemy.warning ? pink : orange);
        health_meshes[enemy.id]->set_scale(Vector3(std::max(0.001f, enemy.hp / enemy.max_hp), 1, 1));
        if (enemy.kind == 4)
        {
            for (int i = 0; i < 8; ++i)
            {
                auto *plate = Object::cast_to<Node3D>(mesh->get_node_or_null("Armor" + String::num_int64(i)));
                if (plate)
                    plate->set_visible(enemy.hp / enemy.max_hp > i / 8.0f);
            }
        }
        auto *warning = warning_meshes[enemy.id];
        warning->set_visible(enemy.warning && enemy.hp > 0);
        if (enemy.warning && enemy.kind == 4)
            draw_charge(warning, enemy);
        else if (enemy.warning)
            draw_sector(warning, enemy.position, enemy.direction, enemy.kind == 1 ? 1.6f : enemy.kind == 3 ? 6.0f : 1.4f,
                enemy.kind == 1 ? 110.0f : enemy.kind == 3 ? 42.0f : 360.0f);
    }
}

void MeltdownArena::present_attacks()
{
    const Color current_color = heat_color(simulation.heat.tier());
    attack_mesh->set_visible(simulation.attack.pending || simulation.slash.life > 0);
    if (simulation.attack.pending)
    {
        draw_sector(attack_mesh, simulation.player, simulation.attack.direction, simulation.attack.radius, simulation.attack.angle);
        slash_material->set_albedo(Color(1, 0.63, 0.27, 0.18f));
    }
    else if (simulation.slash.life > 0)
    {
        const auto &slash = simulation.slash;
        draw_sector(attack_mesh, slash.position, slash.direction, slash.radius, slash.angle);
        slash_material->set_albedo(Color(current_color.r, current_color.g, current_color.b, 0.38f));
    }
    vent_mesh->set_visible(simulation.vent_flash > meltdown::vent_effect_duration - 0.08f);
    vent_ring->set_visible(simulation.vent_flash > 0);
    if (simulation.vent_flash > 0)
    {
        const float progress = 1 - simulation.vent_flash / meltdown::vent_effect_duration;
        draw_sector(vent_mesh, simulation.vent_position, {0, -1}, meltdown::vent_radius, 360);
        draw_ring(vent_ring, simulation.vent_position, meltdown::vent_radius * (0.2f + 0.8f * progress), 0.18f, 1);
        vent_ring_material->set_albedo(Color(0.65, 1, 1, 1 - progress));
    }
}

void MeltdownArena::present_projectiles()
{
    for (auto it = projectile_meshes.begin(); it != projectile_meshes.end();)
    {
        if (std::none_of(simulation.projectiles.begin(), simulation.projectiles.end(),
            [&](const meltdown::Projectile &p)
            {
                return p.id == it->first;
            }))
        {
            it->second->queue_free();
            it = projectile_meshes.erase(it);
        }
        else
            ++it;
    }
    for (const auto &projectile : simulation.projectiles)
    {
        if (!projectile_meshes.count(projectile.id))
            projectile_meshes[projectile.id] = box(this, {}, Vector3(projectile.radius * 2, 0.22, projectile.radius * 2),
                projectile.record ? white : pink);
        auto *mesh = projectile_meshes[projectile.id];
        mesh->set_position(world(projectile.position, 0.6f));
        mesh->set_rotation(Vector3(0, simulation.elapsed * 5, 0));
    }
}

void MeltdownArena::present_fragments()
{
    for (auto it = fragment_meshes.begin(); it != fragment_meshes.end();)
    {
        if (std::none_of(simulation.fragments.begin(), simulation.fragments.end(),
            [&](const meltdown::Fragment &f)
            {
                return f.id == it->first;
            }))
        {
            it->second->queue_free();
            it = fragment_meshes.erase(it);
        }
        else
            ++it;
    }
    for (const auto &fragment : simulation.fragments)
    {
        if (!fragment_meshes.count(fragment.id))
            fragment_meshes[fragment.id] = box(this, {}, Vector3(0.12, 0.12, 0.12), gold);
        auto *mesh = fragment_meshes[fragment.id];
        mesh->set_position(world(fragment.position, 0.2f + std::sin(fragment.life / 0.45f * meltdown::pi) * 0.6f));
        mesh->set_rotation(Vector3(fragment.life * 9, fragment.life * 15, 0));
    }
}

void MeltdownArena::present_hud()
{
    const bool overdrive = simulation.heat.mode == meltdown::HeatMode::Overdrive;
    const bool cooling = simulation.heat.mode == meltdown::HeatMode::Cooling;
    const auto &heat = simulation.heat;
    const char *tiers[] = {"NORMAL", "HEATING", "BURNING", "CRITICAL", "OVERDRIVE"};
    const String state = heat.mode == meltdown::HeatMode::Cooling ? "COOLING" : tiers[heat.tier()];
    const String timer = heat.mode != meltdown::HeatMode::Normal ? "  " + String::num(heat.remaining, 1) + "s" : "";
    status->set_text(String("MELTDOWN / ") + (practice ? "TRAINING" : "COMBAT PROTOTYPE") +
        "\nHP " + String::num(simulation.hp, 0) + "     HEAT " + String::num(heat.value, 0) + "  /  " + state + timer +
        "\nWAVE " + String::num_int64(simulation.wave) + "/5   KILLS " + String::num_int64(simulation.kills) + "   BREAKS " + String::num_int64(simulation.broken_bullets) +
        "   DODGE " + String::num(simulation.dash_cooldown, 1) + "   TIME " + String::num(simulation.elapsed, 0) + "s");
    message->set_text(String("WASD  MOVE   MOUSE  AIM   LMB  SLASH   RMB  HEAVY   SPACE  DODGE\nQ  VENT (40)   E  OVERDRIVE (80)   F1  TRAINING   ESC  PAUSE   M  SOUND ") + (muted ? "OFF" : "ON"));
    if (skills)
    {
        const String q = heat_spent_left > 0 ? "VENT!  -40 HEAT" : overdrive || cooling ? "LOCKED" :
            simulation.attack.pending ? "WAIT FOR STRIKE" : heat.value >= 40 ? "READY / COST 40" : "NEED 40 HEAT";
        const String e = overdrive ? "ACTIVE  " + String::num(heat.remaining, 1) + "s" : cooling ?
            "COOLING  " + String::num(heat.remaining, 1) + "s" : heat.value >= 80 ?
            "READY / " + String::num(heat.overdrive_duration, 0) + "s BOOST" : "NEED 80 HEAT";
        skills->set_text("[Q]  " + q + "\n[E]  " + e);
    }
    if (enemy_direction)
    {
        float nearest = 1e9f;
        String hint;
        const Vector2 size = get_viewport()->get_visible_rect().size;
        for (const auto &enemy : simulation.enemies)
        {
            if (enemy.hp <= 0)
                continue;
            const Vector2 screen = camera->unproject_position(world(enemy.position));
            const float distance = (enemy.position - simulation.player).length();
            if (distance < nearest && (screen.x < 0 || screen.x > size.x || screen.y < 0 || screen.y > size.y))
            {
                const Vector2 direction = screen - size * 0.5f;
                hint = std::abs(direction.x) > std::abs(direction.y) ? (direction.x > 0 ? "RIGHT" : "LEFT") :
                    (direction.y > 0 ? "BELOW" : "ABOVE");
                hint = "OFFSCREEN ENEMY  " + hint + "\n" + String::num(distance, 0) + "m";
                nearest = distance;
            }
        }
        enemy_direction->set_text(simulation.finished ? "" : hint);
    }
    if (heat_bar)
        heat_bar->set("value", heat.value);
    if (hp_bar)
        hp_bar->set("value", simulation.hp);
    if (center_message)
    {
        String text;
        if (simulation.finished)
            text = String(simulation.won ? "ARENA CLEARED" : "CORE OFFLINE") + "\nR  RESTART";
        else if (paused)
            text = "PAUSED\nESC  RESUME";
        else if (simulation.choosing_upgrade)
            text = "WAVE CLEARED / CHOOSE ONE (+20 HP)\n1  RANGE +15% (" + String::num_int64(simulation.range_upgrades) +
                "/3)   2  COOLING -0.5s (" + String::num_int64(simulation.cooling_upgrades) +
                "/4)\n3  OVERDRIVE +1s (" + String::num_int64(simulation.overdrive_upgrades) + "/3)";
        else if (heat.mode == meltdown::HeatMode::Cooling)
            text = "COOLING  /  KEEP MOVING";
        else if (heat.tier() == 4)
            text = "OVERDRIVE";
        else if (heat.tier() == 3)
            text = "CRITICAL  /  E OVERDRIVE OR Q VENT\n100 HEAT FORCES COOLING";
        center_message->set_text(text);
    }
}

Dictionary MeltdownArena::get_snapshot() const
{
    Dictionary result;
    result["player"] = world(simulation.player);
    result["hp"] = simulation.hp;
    result["hits"] = simulation.hits;
    result["dash_left"] = simulation.dash_left;
    result["enemy_count"] = static_cast<int>(simulation.enemies.size());
    result["heat"] = simulation.heat.value;
    result["heat_mode"] = static_cast<int>(simulation.heat.mode);
    result["kills"] = simulation.kills;
    result["projectiles"] = static_cast<int>(simulation.projectiles.size());
    result["fragments"] = static_cast<int>(simulation.fragments.size());
    result["finished"] = simulation.finished;
    result["won"] = simulation.won;
    result["training"] = practice;
    result["wave"] = simulation.wave;
    result["choosing_upgrade"] = simulation.choosing_upgrade;
    result["elapsed"] = simulation.elapsed;
    result["paused"] = paused;
    result["vent_flash"] = simulation.vent_flash;
    result["heat_remaining"] = simulation.heat.remaining;
    Array enemies;
    for (const auto &enemy : simulation.enemies)
    {
        Dictionary entry;
        entry["position"] = world(enemy.position);
        entry["hp"] = enemy.hp;
        entry["kind"] = enemy.kind;
        entry["warning"] = enemy.warning;
        enemies.push_back(entry);
    }
    result["enemies"] = enemies;
    Array objects;
    for (const auto &object : simulation.environment)
    {
        Dictionary entry;
        entry["id"] = object.id;
        entry["wall"] = object.kind == meltdown::EnvironmentKind::Wall;
        entry["pillar"] = object.kind == meltdown::EnvironmentKind::Pillar;
        entry["position"] = world(object.position);
        entry["direction"] = world(object.direction);
        entry["half_size"] = world(object.half_size);
        entry["hp"] = object.hp;
        entry["state"] = static_cast<int>(object.state);
        entry["remaining"] = object.remaining;
        entry["radius"] = object.radius;
        entry["angle"] = object.angle;
        entry["blocking"] = object.blocking();
        objects.push_back(entry);
    }
    result["environment"] = objects;
    Array blocks;
    for (const auto &block : simulation.falling_blocks)
    {
        Dictionary entry;
        entry["id"] = block.id;
        entry["owner_id"] = block.owner_id;
        entry["layer"] = block.layer;
        entry["position"] = world(block.position, block.height);
        entry["landing"] = world(block.landing, block.landing_height);
        entry["falling"] = block.falling;
        entry["settled"] = block.settled;
        entry["yaw"] = block.yaw;
        entry["radius"] = meltdown::block_radius;
        entry["half_height"] = meltdown::block_half_height;
        blocks.push_back(entry);
    }
    result["blocks"] = blocks;
    Array cells;
    for (const auto p : simulation.dungeon.occupied({-24, 0, -24}, {23, 7, 23}))
    {
        const auto cell = simulation.dungeon.get(p);
        Dictionary entry;
        entry["cell"] = Vector3i(p.x, p.y, p.z);
        entry["hp"] = cell.hp;
        entry["warning"] = cell.warning;
        entry["kind"] = static_cast<int>(cell.kind);
        cells.push_back(entry);
    }
    result["voxel_cells"] = cells;
    result["voxel_size"] = meltdown::voxel_size;
    result["chunk_size"] = meltdown::dungeon_chunk_size;
    result["chunk_count"] = static_cast<int>(simulation.dungeon.chunks.size());
    return result;
}

Node3D *MeltdownArena::instantiate_visual(const Ref<PackedScene> &scene)
{
    Node *instance = scene.is_valid() ? scene->instantiate() : nullptr;
    auto *visual = Object::cast_to<Node3D>(instance);
    if (visual)
        add_child(visual);
    else if (instance)
        memdelete(instance);
    return visual;
}

void MeltdownArena::present_environment()
{
    String hint;
    float nearest = 4;
    for (const auto &object : simulation.environment)
    {
        using State = meltdown::EnvironmentState;
        const bool is_pillar = object.kind == meltdown::EnvironmentKind::Pillar;
        const bool is_wall = object.kind == meltdown::EnvironmentKind::Wall;
        auto &visual = environment_visuals[object.id];
        if (!visual.body)
        {
            const Ref<PackedScene> scene = is_pillar ? Ref<PackedScene>() : is_wall ? wall_visual : pipe_visual;
            visual.body = instantiate_visual(scene);
            if (!visual.body)
            {
                visual.body = memnew(Node3D);
                add_child(visual.body);
                box(visual.body, Vector3(0, is_pillar ? 0.06 : 0.7, 0), is_pillar ? Vector3(1.2, 0.12, 1.2) :
                    is_wall ? Vector3(3, 1.4, 0.7) : Vector3(0.8, 1.3, 0.8), gold);
            }
            visual.body->set_name("Environment" + String::num_int64(object.id));
            visual.danger = memnew(MeshInstance3D);
            visual.danger->set_name("EnvironmentDanger" + String::num_int64(object.id));
            add_child(visual.danger);
            visual.health = box(this, {}, Vector3(is_wall ? 2.6 : 0.9, 0.07, 0.09), cyan);
            visual.direction = memnew(MeshInstance3D);
            add_child(visual.direction);
            visual.direction->set_material_override(material(Color(1, 0.72, 0.25), true));
        }
        const bool warning = object.state == State::Warning;
        const bool active = object.state == State::Active;
        const bool spent = object.state == State::Spent;
        if (!muted && visual.state != object.state && (warning || active))
        {
            const Ref<AudioStream> sound = warning ? environment_warning_sound : environment_burst_sound;
            hit_audio->set_stream(sound.is_valid() ? sound : Ref<AudioStream>(warning ? warning_sound : vent_sound));
            hit_audio->play();
        }
        visual.state = object.state;
        visual.body->set_position(world(object.position));
        visual.body->set_rotation(Vector3(0, std::atan2(-object.direction.x, -object.direction.z), 0));
        if (visual.body->has_method("set_environment_state"))
            visual.body->call("set_environment_state", static_cast<int>(object.state), object.hp / object.max_hp, object.remaining);
        else
            visual.body->set_visible(!active && !spent);
        visual.health->set_position(world(object.position, is_pillar ? 5.35f : 1.7f));
        visual.health->set_scale(Vector3(std::max(0.001f, object.hp / object.max_hp), 1, 1));
        visual.health->set_visible(object.hp > 0);
        visual.direction->set_visible(!spent && !active);
        draw_sector(visual.direction, object.position + object.direction * (is_wall ? 0.6f : 0.5f), object.direction, 0.65f, 48);
        visual.danger->set_visible(!is_pillar && (warning || active));
        if (!is_pillar && (warning || active))
        {
            visual.danger->set_material_override(active ? environment_active_material : warning_material);
            draw_sector(visual.danger, object.position, object.direction, object.radius, object.angle);
            visual.danger->set_position(world(object.position, 0.045f));
        }
        const float distance = (object.position - simulation.player).length();
        if (distance < nearest && !spent)
        {
            nearest = distance;
            hint = String(is_pillar ? "PILLAR" : is_wall ? "WALL" : "PRESSURE PIPE") + "  " +
                (warning ? "COLLAPSING / " + String::num(object.remaining, 1) + "s" : active ? "FALLING!" :
                "HP " + String::num(object.hp, 0) + " / ATTACK TO BREAK") +
                (is_pillar ? "\nFALLING BLOCKS HURT BOTH SIDES" : "\nGOLD ARROW: BURST / HURTS BOTH SIDES");
        }
    }
    if (environment_hint)
    {
        if (!simulation.collapses.empty())
            hint = "WALL COLLAPSING! / MOVE AWAY\nFALLING BLOCKS HURT BOTH SIDES";
        else if (hint.is_empty())
            hint = "1m BLOCKS / ATTACK STONE WALLS\nBREAK SUPPORTS TO DROP BLOCKS";
        environment_hint->set_text(simulation.finished || simulation.choosing_upgrade ? "" : hint);
    }
    present_blocks();
}

void MeltdownArena::present_blocks()
{
    for (const auto &block : simulation.falling_blocks)
    {
        const auto owner = std::find_if(simulation.environment.begin(), simulation.environment.end(),
            [&](const meltdown::EnvironmentObject &object)
            {
                return object.id == block.owner_id;
            });
        if (owner == simulation.environment.end() && block.owner_id != 0)
            continue;
        auto &visual = block_visuals[block.id];
        if (!visual.body)
        {
            visual.body = instantiate_visual(pillar_block_visual);
            if (!visual.body)
            {
                visual.body = box(this, {}, Vector3(1, 1, 1), gold);
            }
            visual.body->set_name("PillarBlock" + String::num_int64(block.id));
            visual.shadow = memnew(MeshInstance3D);
            add_child(visual.shadow);
            visual.shadow->set_material_override(material(Color(0.025, 0.025, 0.03), true));
            visual.landing = memnew(MeshInstance3D);
            visual.landing->set_name("BlockLanding" + String::num_int64(block.id));
            add_child(visual.landing);
            visual.landing->set_material_override(material(Color(1, 0.62, 0.15), true));
            visual.settled = block.settled;
        }
        if (!visual.settled && block.settled && !muted)
        {
            hit_audio->set_stream(environment_burst_sound.is_valid() ? environment_burst_sound : Ref<AudioStream>(hit_sound));
            hit_audio->play();
        }
        visual.settled = block.settled;
        visual.body->set_position(world(block.position, block.height));
        visual.body->set_rotation(Vector3(0, block.yaw, 0));
        if (visual.body->has_method("set_environment_state"))
            visual.body->call("set_environment_state", block.settled ? 4 :
                owner == simulation.environment.end() ? 3 : static_cast<int>(owner->state),
                owner == simulation.environment.end() ? 0.0f : owner->hp / owner->max_hp,
                owner == simulation.environment.end() ? 0.0f : owner->remaining);
        const bool warning = owner != simulation.environment.end() && owner->state == meltdown::EnvironmentState::Warning;
        visual.shadow->set_visible((warning || block.falling) && !block.settled);
        if (visual.shadow->is_visible())
            draw_sector(visual.shadow, block.position, {0, -1}, meltdown::block_radius, 360);
        visual.landing->set_visible((warning || block.falling) && !block.settled && block.layer > 0);
        if (visual.landing->is_visible())
        {
            draw_ring(visual.landing, block.landing, meltdown::block_radius + 0.35f, 0.06f, 1);
            visual.landing->set_position(world(block.landing, block.landing_height - meltdown::block_half_height + 0.09f));
        }
    }
}
