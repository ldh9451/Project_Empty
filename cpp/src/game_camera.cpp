#include "game_camera.h"

#include <algorithm>
#include <cmath>

using namespace godot;

void GameCamera::Init()
{
    if (initialized || !input)
        return;
    pitch = get_rotation().x;
    yaw = get_rotation().y;
    make_current();
    initialized = true;
}

void GameCamera::Update(double delta)
{
    if (!initialized || !input || !input->is_looking() || delta < 0.0 || !std::isfinite(delta))
        return;

    constexpr double sensitivity = 0.0025;
    constexpr double pitch_limit = 1.553343; // 89 degrees: avoid flipping at the poles.
    const Vector2 look = input->get_look_delta();
    yaw = std::remainder(yaw - look.x * sensitivity, Math::TAU);
    pitch = std::clamp(pitch - look.y * sensitivity, -pitch_limit, pitch_limit);
    set_rotation(Vector3(pitch, yaw, 0.0));

    const double speed = input->is_fast() ? 24.0 : 8.0;
    set_position(get_position() + get_basis().xform(input->get_movement()) * speed * delta);
}

void GameCamera::Release()
{
    initialized = false;
    input = nullptr;
}
