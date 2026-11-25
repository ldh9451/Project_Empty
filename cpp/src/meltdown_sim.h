#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

namespace meltdown
{
    constexpr float pi = 3.14159265359f;
    constexpr float arena_half_size = 24.0f;
    constexpr float arena_limit = arena_half_size - 1.0f;
    constexpr float vent_radius = 3.2f;
    constexpr float vent_effect_duration = 0.4f;
    constexpr float block_gravity = 12.0f;
    constexpr float block_radius = 0.68f;
    constexpr float block_half_height = 0.48f;
    constexpr float block_damage = 18.0f;

    struct Vec
    {
        float x = 0;
        float z = 0;
        Vec operator+(Vec b) const
        {
            return {x + b.x, z + b.z};
        }
        Vec operator-(Vec b) const
        {
            return {x - b.x, z - b.z};
        }
        Vec operator*(float s) const
        {
            return {x * s, z * s};
        }
        float dot(Vec b) const
        {
            return x * b.x + z * b.z;
        }
        float length() const
        {
            return std::sqrt(dot(*this));
        }
        Vec normalized() const
        {
            const float n = length();
            return n > 0.00001f ? *this * (1.0f / n) : Vec{};
        }
    };

    inline bool in_sector(Vec offset, Vec direction, float radius, float degrees)
    {
        const float distance = offset.length();
        return distance <= radius && (distance < 0.001f ||
            offset.dot(direction) >= distance * std::cos(degrees * pi / 360.0f));
    }

    inline float segment_distance(Vec point, Vec start, Vec end)
    {
        const Vec segment = end - start;
        const float length_squared = segment.dot(segment);
        const float t = length_squared > 0.000001f ?
            std::clamp((point - start).dot(segment) / length_squared, 0.0f, 1.0f) : 0;
        return (point - (start + segment * t)).length();
    }

    enum class HeatMode
    {
        Normal, Overdrive, Cooling
    };

    struct Heat
    {
        float value = 0;
        float remaining = 0;
        float cooling_duration = 4;
        float overdrive_duration = 5;
        unsigned generation = 0;
        HeatMode mode = HeatMode::Normal;
        int tier() const;
        void gain(float amount, unsigned attack_generation);
        bool vent();
        bool overdrive();
        void tick(float delta);
    };

    struct Input
    {
        Vec move;
        Vec aim = {0, -1};
        bool light = false;
        bool heavy = false;
        bool dodge = false;
        bool vent = false;
        bool overdrive = false;
        int upgrade = 0;
    };

    struct Enemy
    {
        int id = 0;
        Vec position;
        float hp = 100;
        float flash = 0;
        int kind = 0;
        float max_hp = 100;
        float timer = 1;
        bool warning = false;
        Vec direction = {0, -1};
        float charge_left = 0;
        bool charge_hit = false;
        float radius = 0.4f;
        Vec previous_position;
    };

    struct AttackRecord
    {
        int id = 0;
        unsigned generation = 0;
        float heat = 0;
        bool charged = false;
        std::vector<int> hit_ids;
    };

    struct Attack
    {
        int id = 0;
        Vec direction;
        float radius = 0;
        float angle = 0;
        float damage = 0;
        float delay = 0;
        bool pending = false;
        int tier = 0;
        std::shared_ptr<AttackRecord> record;
    };

    struct Slash
    {
        Vec position;
        Vec direction;
        float radius = 0;
        float angle = 0;
        float life = 0;
    };

    struct Projectile
    {
        int id = 0;
        Vec position;
        Vec velocity;
        float life = 0;
        float radius = 0.16f;
        float damage = 8;
        std::shared_ptr<AttackRecord> record;
    };

    struct Fragment
    {
        int id = 0;
        Vec position;
        Vec velocity;
        float life = 0;
    };

    enum class EnvironmentState
    {
        Intact, Damaged, Warning, Active, Spent
    };

    struct EnvironmentObject
    {
        int id = 0;
        bool wall = false;
        bool pillar = false;
        Vec position;
        Vec direction = {0, -1};
        Vec half_size = {1.5f, 0.35f};
        float hp = 20;
        float max_hp = 20;
        float radius = 4.5f;
        float angle = 70;
        float damage = 30;
        float warning_duration = 0.85f;
        float remaining = 0;
        EnvironmentState state = EnvironmentState::Intact;
        bool blocking() const
        {
            return (wall || pillar) && state < EnvironmentState::Active;
        }
    };

    struct FallingBlock
    {
        int id = 0;
        int owner_id = 0;
        int layer = 0;
        Vec position;
        float height = block_half_height;
        Vec velocity;
        float vertical_speed = 0;
        Vec landing;
        float yaw = 0;
        bool falling = false;
        bool settled = false;
        std::vector<int> hit_ids;
    };

    class Simulation
    {
        int next_id = 1;
        Vec dash_direction;
        void move_player(Vec displacement);
        void start_attack(bool heavy);
        void resolve_attack();
        void damage_enemy(Enemy &enemy, float damage, const std::shared_ptr<AttackRecord> &record);
        void damage_player(float damage);
        void update_enemies(float delta);
        void update_projectiles(float delta, Vec old_player);
        void burst(Vec position, int count);
        void spawn_enemy(int kind, Vec position);
        void spawn_wave();
        void damage_environment(EnvironmentObject &object, float damage, const std::shared_ptr<AttackRecord> &record);
        void update_environment(float delta);
        void update_falling_blocks(float delta, Vec old_player);
        Vec constrain_walls(Vec position, float radius) const;
        Vec move_body(Vec position, Vec displacement, float radius) const;
        Vec steer(Vec position, Vec target, float radius) const;

    public:
        Vec player;
        Vec aim = {0, -1};
        float hp = 100;
        float cooldown = 0;
        float dash_left = 0;
        float dash_cooldown = 0;
        int hits = 0;
        int kills = 0;
        int broken_bullets = 0;
        float elapsed = 0;
        float hurt_left = 0;
        float vent_flash = 0;
        Vec vent_position;
        bool training = true;
        bool finished = false;
        bool won = false;
        bool dashing_this_tick = false;
        int wave = 1;
        bool choosing_upgrade = false;
        int range_upgrades = 0;
        int cooling_upgrades = 0;
        int overdrive_upgrades = 0;
        Heat heat;
        Attack attack;
        Slash slash;
        std::vector<Enemy> enemies;
        std::vector<Projectile> projectiles;
        std::vector<Fragment> fragments;
        std::vector<EnvironmentObject> environment;
        std::vector<FallingBlock> falling_blocks;

        void reset(bool practice = true);
        void tick(float delta, const Input &input);
        bool wall_between(Vec start, Vec end, float padding = 0, int ignore_id = 0) const;
    };
}
