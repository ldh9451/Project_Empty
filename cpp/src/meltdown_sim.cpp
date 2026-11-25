#include "meltdown_sim.h"
#include <queue>

namespace meltdown
{
    namespace
    {
        void position_pillar_block(FallingBlock &block, const EnvironmentObject &pillar, float lean)
        {
            const float elevation = block.layer * pillar_layer_spacing;
            block.position = pillar.position + pillar.direction * (elevation * std::sin(lean));
            block.height = block_half_height + elevation * std::cos(lean);
        }

        float circle_entry(Vec start, Vec end, Vec center, float radius)
        {
            const Vec offset = start - center;
            const Vec direction = end - start;
            const float c = offset.dot(offset) - radius * radius;
            if (c <= 0)
                return 0;
            const float a = direction.dot(direction);
            const float b = offset.dot(direction);
            const float discriminant = b * b - a * c;
            if (a < 0.000001f || discriminant < 0)
                return 2;
            const float t = (-b - std::sqrt(discriminant)) / a;
            return t >= 0 && t <= 1 ? t : 2;
        }

        bool block_contact(Vec start, Vec end, float y_start, float y_end,
            Vec actor_start, Vec actor_end, float actor_radius, float actor_height)
        {
            float enter = 0;
            float leave = 1;
            const float dy = y_end - y_start;
            if (std::abs(dy) < 0.000001f)
            {
                if (y_start < -block_half_height || y_start > actor_height + block_half_height)
                    return false;
            }
            else
            {
                float a = (-block_half_height - y_start) / dy;
                float b = (actor_height + block_half_height - y_start) / dy;
                if (a > b)
                    std::swap(a, b);
                enter = std::max(enter, a);
                leave = std::min(leave, b);
                if (enter > leave)
                    return false;
            }
            const Vec relative_start = start - actor_start;
            const Vec relative_delta = end - actor_end - relative_start;
            const float origin[] = {relative_start.x, relative_start.z};
            const float movement[] = {relative_delta.x, relative_delta.z};
            const float extent = 0.5f + actor_radius;
            for (int axis = 0; axis < 2; ++axis)
            {
                if (std::abs(movement[axis]) < 0.000001f)
                {
                    if (std::abs(origin[axis]) > extent)
                        return false;
                    continue;
                }
                float a = (-extent - origin[axis]) / movement[axis];
                float b = (extent - origin[axis]) / movement[axis];
                if (a > b)
                    std::swap(a, b);
                enter = std::max(enter, a);
                leave = std::min(leave, b);
            }
            return enter <= leave;
        }

        // Earliest segment entry into an axis-aligned wall; 2 means no intersection.
        float wall_entry(Vec start, Vec end, const EnvironmentObject &wall, float padding)
        {
            float enter = 0;
            float leave = 1;
            const Vec a = start - wall.position;
            const Vec d = end - start;
            const float origin[] = {a.x, a.z};
            const float direction[] = {d.x, d.z};
            const float extent[] = {wall.half_size.x + padding, wall.half_size.z + padding};
            for (int axis = 0; axis < 2; ++axis)
            {
                if (std::abs(direction[axis]) < 0.000001f)
                {
                    if (std::abs(origin[axis]) > extent[axis])
                        return 2;
                    continue;
                }
                float near = (-extent[axis] - origin[axis]) / direction[axis];
                float far = (extent[axis] - origin[axis]) / direction[axis];
                if (near > far)
                    std::swap(near, far);
                enter = std::max(enter, near);
                leave = std::min(leave, far);
                if (enter > leave)
                    return 2;
            }
            return enter;
        }
    }

    bool Simulation::wall_between(Vec start, Vec end, float padding, int ignore_id) const
    {
        if (dungeon.sweep({start.x, 0.65f, start.z}, {end.x, 0.65f, end.z},
            {padding, padding > 0 ? 0.55f : 0.0f, padding}, ignore_id).fraction <= 1)
            return true;
        for (const auto &object : environment)
        {
            if (object.blocking() && object.id != ignore_id && wall_entry(start, end, object, padding) <= 1)
                return true;
        }
        return false;
    }

    Vec Simulation::constrain_walls(Vec position, float radius) const
    {
        const auto lo = DungeonGrid::world_cell(position.x - radius, 0, position.z - radius);
        const auto hi = DungeonGrid::world_cell(position.x + radius, 1.2f, position.z + radius);
        for (const auto p : dungeon.occupied(lo, hi))
        {
            if (dungeon.get(p).kind != VoxelKind::Stone)
                continue;
            const Vec closest{std::clamp(position.x, float(p.x), p.x + 1.0f),
                std::clamp(position.z, float(p.z), p.z + 1.0f)};
            const Vec away = position - closest;
            const float distance = away.length();
            if (distance > 0.00001f && distance < radius)
                position = closest + away * (radius / distance);
            else if (distance <= 0.00001f)
            {
                const Vec local = position - Vec{p.x + 0.5f, p.z + 0.5f};
                if (std::abs(local.x) > std::abs(local.z))
                    position.x = p.x + 0.5f + (local.x < 0 ? -1 : 1) * (0.5f + radius);
                else
                    position.z = p.z + 0.5f + (local.z < 0 ? -1 : 1) * (0.5f + radius);
            }
        }
        for (const auto &object : environment)
        {
            if (!object.blocking())
                continue;
            const Vec local = position - object.position;
            const Vec closest = {std::clamp(local.x, -object.half_size.x, object.half_size.x),
                std::clamp(local.z, -object.half_size.z, object.half_size.z)};
            const Vec away = local - closest;
            const float distance = away.length();
            if (distance >= radius)
                continue;
            if (distance > 0.00001f)
                position = object.position + closest + away * (radius / distance);
            else if (object.half_size.x - std::abs(local.x) < object.half_size.z - std::abs(local.z))
                position.x = object.position.x + (local.x < 0 ? -1 : 1) * (object.half_size.x + radius);
            else
                position.z = object.position.z + (local.z < 0 ? -1 : 1) * (object.half_size.z + radius);
        }
        return {std::clamp(position.x, -arena_limit, arena_limit), std::clamp(position.z, -arena_limit, arena_limit)};
    }

    Vec Simulation::move_body(Vec position, Vec displacement, float radius) const
    {
        const int steps = std::max(1, static_cast<int>(std::ceil(displacement.length() / 0.15f)));
        for (int i = 0; i < steps; ++i)
            position = constrain_walls(position + displacement * (1.0f / steps), radius);
        return position;
    }

    Vec Simulation::steer(Enemy &enemy, Vec target) const
    {
        const Vec position = enemy.position;
        const float radius = enemy.radius;
        if (!wall_between(position, target, radius))
        {
            enemy.has_waypoint = false;
            return (target - position).normalized();
        }
        if (enemy.has_waypoint && (enemy.waypoint - position).length() > 0.08f &&
            !wall_between(position, enemy.waypoint, radius))
            return (enemy.waypoint - position).normalized();
        enemy.has_waypoint = false;
        // Bounded A* on the same 1m occupancy grid; destruction opens routes immediately.
        if (dungeon.sweep({position.x, 0.65f, position.z}, {target.x, 0.65f, target.z},
            {radius, 0.55f, radius}).fraction <= 1)
        {
            const auto start = DungeonGrid::world_cell(position.x, 0, position.z);
            const auto goal = DungeonGrid::world_cell(target.x, 0, target.z);
            auto inside = [](CellPosition p)
            {
                return p.x >= -23 && p.x < 23 && p.z >= -23 && p.z < 23;
            };
            std::map<CellPosition, float> costs;
            std::map<CellPosition, CellPosition> parent;
            using Visit = std::pair<float, CellPosition>;
            std::priority_queue<Visit, std::vector<Visit>, std::greater<Visit>> open;
            costs[start] = 0;
            open.push({0.0f, start});
            bool found = false;
            while (!open.empty())
            {
                const auto current = open.top().second;
                open.pop();
                if (current == goal)
                {
                    found = true;
                    break;
                }
                for (const auto direction : {CellPosition{1, 0, 0}, CellPosition{-1, 0, 0},
                    CellPosition{0, 0, 1}, CellPosition{0, 0, -1}})
                {
                    const auto next = current + direction;
                    if (!inside(next))
                        continue;
                    const Vec from = current == start ? position : Vec{current.x + 0.5f, current.z + 0.5f};
                    const Vec to = next == goal ? target : Vec{next.x + 0.5f, next.z + 0.5f};
                    if (wall_between(from, to, radius))
                        continue;
                    const float cost = costs[current] + (to - from).length();
                    const auto old = costs.find(next);
                    if (old != costs.end() && old->second <= cost)
                        continue;
                    costs[next] = cost;
                    parent[next] = current;
                    const float estimate = float(std::abs(next.x - goal.x) + std::abs(next.z - goal.z));
                    open.push({cost + estimate, next});
                }
            }
            if (found && !(start == goal))
            {
                auto next = goal;
                while (!(parent[next] == start))
                    next = parent[next];
                enemy.waypoint = next == goal ? target : Vec{next.x + 0.5f, next.z + 0.5f};
                enemy.has_waypoint = true;
                return (enemy.waypoint - position).normalized();
            }
            return {};
        }
        Vec waypoint = position;
        float best = 1e9f;
        // shortcut: local corner steering for separated short walls, use navigation for maze layouts.
        for (const auto &object : environment)
        {
            if (!object.blocking() || wall_entry(position, target, object, radius) > 1)
                continue;
            for (int x : {-1, 1})
            {
                for (int z : {-1, 1})
                {
                    const Vec corner = object.position + Vec{x * (object.half_size.x + radius + 0.2f),
                        z * (object.half_size.z + radius + 0.2f)};
                    const float distance = (corner - position).length();
                    float remaining = (target - corner).length();
                    if (wall_between(corner, target, radius))
                    {
                        remaining = 1e9f;
                        for (int next_x : {-1, 1})
                        {
                            for (int next_z : {-1, 1})
                            {
                                const Vec next = object.position + Vec{next_x * (object.half_size.x + radius + 0.2f),
                                    next_z * (object.half_size.z + radius + 0.2f)};
                                if (!wall_between(corner, next, radius) && !wall_between(next, target, radius))
                                    remaining = std::min(remaining, (next - corner).length() + (target - next).length());
                            }
                        }
                    }
                    const float cost = distance + remaining;
                    if (distance > 0.08f && cost < best && !wall_between(position, corner, radius))
                    {
                        best = cost;
                        waypoint = corner;
                    }
                }
            }
        }
        return (waypoint - position).normalized();
    }

    void Simulation::damage_environment(EnvironmentObject &object, float damage, const std::shared_ptr<AttackRecord> &record)
    {
        if (object.state >= EnvironmentState::Warning)
            return;
        if (record)
        {
            if (std::find(record->hit_ids.begin(), record->hit_ids.end(), object.id) != record->hit_ids.end())
                return;
            record->hit_ids.push_back(object.id);
        }
        object.hp = std::max(0.0f, object.hp - damage);
        object.state = object.hp > 0 ? EnvironmentState::Damaged : EnvironmentState::Warning;
        object.remaining = object.hp > 0 ? 0 : object.warning_duration;
        burst(object.position, 3);
    }

    void Simulation::update_environment(float delta)
    {
        // Open all walls whose warning ends this tick before resolving any environmental damage.
        std::vector<int> activated;
        for (auto &object : environment)
        {
            if (object.state != EnvironmentState::Warning && object.state != EnvironmentState::Active)
                continue;
            if (object.kind == EnvironmentKind::Pillar && object.state == EnvironmentState::Active)
                continue;
            object.remaining -= delta;
            if (object.remaining > 0)
                continue;
            if (object.state == EnvironmentState::Active)
                object.state = EnvironmentState::Spent;
            else
            {
                object.state = EnvironmentState::Active;
                if (object.kind == EnvironmentKind::Pillar)
                {
                    for (int y = 0; y < pillar_block_count; ++y)
                        dungeon.set(DungeonGrid::world_cell(object.position.x, float(y), object.position.z), {});
                    warn_unsupported();
                }
                object.remaining = object.kind == EnvironmentKind::Pillar ? 0.0f : 0.4f;
                if (object.kind != EnvironmentKind::Pillar)
                    activated.push_back(object.id);
            }
        }
        for (const auto &object : environment)
        {
            if (std::find(activated.begin(), activated.end(), object.id) == activated.end())
                continue;
            if (in_sector(player - object.position, object.direction, object.radius, object.angle) &&
                !wall_between(object.position, player))
                damage_player(object.damage);
            for (auto &enemy : enemies)
            {
                if (in_sector(enemy.position - object.position, object.direction, object.radius, object.angle) &&
                    !wall_between(object.position, enemy.position))
                    damage_enemy(enemy, object.damage, nullptr);
            }
            burst(object.position, 12);
        }
    }

    void Simulation::update_falling_blocks(float delta, Vec old_player)
    {
        for (auto &block : falling_blocks)
        {
            auto owner = std::find_if(environment.begin(), environment.end(), [&](const EnvironmentObject &object)
            {
                return object.id == block.owner_id && object.kind == EnvironmentKind::Pillar;
            });
            if ((owner == environment.end() && block.owner_id != 0) || block.settled)
                continue;
            if (owner != environment.end() && owner->state < EnvironmentState::Active)
            {
                const float progress = owner->state == EnvironmentState::Warning ?
                    std::clamp(1 - owner->remaining / owner->warning_duration, 0.0f, 1.0f) : 0;
                position_pillar_block(block, *owner, pillar_lean_angle * progress);
                if (progress > 0)
                    predict_landing(block);
                continue;
            }
            if (!block.falling && owner != environment.end())
            {
                position_pillar_block(block, *owner, pillar_lean_angle);
                block.falling = true;
            }
            // Ballistic blocks with cylinder proxies; upgrade to rigid bodies for block-to-block piles.
            const float impact_time = (block.vertical_speed + std::sqrt(block.vertical_speed * block.vertical_speed +
                2 * block_gravity * std::max(0.0f, block.height - block_half_height))) / block_gravity;
            float step = std::min(delta, impact_time);
            const Vec start = block.position;
            const float y_start = block.height;
            const float y_end = y_start + block.vertical_speed * step - 0.5f * block_gravity * step * step;
            const auto hit = dungeon.sweep({start.x, y_start, start.z},
                {start.x + block.velocity.x * step, y_end,
                    start.z + block.velocity.z * step}, {0.5f, 0.5f, 0.5f}, 0, true);
            const bool terrain_hit = hit.fraction <= 1;
            if (terrain_hit)
                step *= hit.fraction;
            block.position = block.position + block.velocity * step;
            block.height += block.vertical_speed * step - 0.5f * block_gravity * step * step;
            if (terrain_hit)
                block.height = y_start + (y_end - y_start) * hit.fraction;
            block.vertical_speed -= block_gravity * step;
            // Axis-aligned 1m rubble keeps the visual and static terrain sweep consistent.
            block.yaw = 0;
            const float fraction = step / delta;
            if (step > 0 && std::find(block.hit_ids.begin(), block.hit_ids.end(), 0) == block.hit_ids.end() &&
                block_contact(start, block.position, y_start, block.height, old_player,
                    old_player + (player - old_player) * fraction, 0.35f, 1.2f))
            {
                block.hit_ids.push_back(0);
                const float previous_hp = hp;
                damage_player(block_damage);
                if (hp < previous_hp)
                    move_player(block.velocity.normalized() * 0.7f);
            }
            for (auto &enemy : enemies)
            {
                if (step <= 0 || enemy.hp <= 0 ||
                    std::find(block.hit_ids.begin(), block.hit_ids.end(), enemy.id) != block.hit_ids.end())
                    continue;
                if (block_contact(start, block.position, y_start, block.height, enemy.previous_position,
                    enemy.previous_position + (enemy.position - enemy.previous_position) * fraction,
                    enemy.radius, enemy.kind == 1 ? 0.7f : 1.3f))
                {
                    block.hit_ids.push_back(enemy.id);
                    damage_enemy(enemy, block_damage, nullptr);
                    enemy.position = move_body(enemy.position, block.velocity.normalized() * 0.6f, enemy.radius);
                }
            }
            if (impact_time <= delta || terrain_hit)
            {
                block.height = std::max(block_half_height, block.height);
                block.settled = true;
                block.vertical_speed = 0;
                block.velocity = {};
                burst(block.position, 4);
            }
            else
                predict_landing(block);
        }
        for (auto &object : environment)
        {
            if (object.kind == EnvironmentKind::Pillar && object.state == EnvironmentState::Active &&
                std::none_of(falling_blocks.begin(), falling_blocks.end(), [&](const FallingBlock &block)
                {
                    return block.owner_id == object.id && !block.settled;
                }))
            {
                object.state = EnvironmentState::Spent;
                object.remaining = 0;
            }
        }
    }

    int Heat::tier() const
    {
        if (mode == HeatMode::Overdrive)
            return 4;
        if (mode == HeatMode::Cooling)
            return 0;
        return value >= 80 ? 3 : value >= 50 ? 2 : value >= 25 ? 1 : 0;
    }

    void Heat::gain(float amount, unsigned attack_generation)
    {
        if (mode != HeatMode::Normal || generation != attack_generation)
            return;
        value = std::min(100.0f, value + amount);
        if (value >= 100)
        {
            value = 0;
            mode = HeatMode::Cooling;
            remaining = cooling_duration;
            ++generation;
        }
    }

    bool Heat::vent()
    {
        if (mode != HeatMode::Normal || value < 40)
            return false;
        value -= 40;
        return true;
    }

    bool Heat::overdrive()
    {
        if (mode != HeatMode::Normal || value < 80 || value >= 100)
            return false;
        mode = HeatMode::Overdrive;
        remaining = overdrive_duration;
        ++generation;
        return true;
    }

    void Heat::tick(float delta)
    {
        while (mode != HeatMode::Normal && delta > 0)
        {
            const float step = std::min(delta, remaining);
            remaining -= step;
            delta -= step;
            if (remaining > 0.00001f)
                break;
            value = 0;
            mode = mode == HeatMode::Overdrive ? HeatMode::Cooling : HeatMode::Normal;
            remaining = mode == HeatMode::Cooling ? cooling_duration : 0.0f;
            ++generation;
        }
    }

    void Simulation::spawn_enemy(int kind, Vec position)
    {
        Enemy enemy;
        enemy.id = next_id++;
        enemy.position = position;
        enemy.kind = kind;
        enemy.radius = kind == 4 ? 0.65f : 0.4f;
        enemy.position = constrain_walls(position, enemy.radius + 0.1f);
        enemy.hp = enemy.max_hp = kind == 0 ? 100.0f : kind == 1 ? 20.0f :
            kind == 2 ? 35.0f : kind == 3 ? 30.0f : 120.0f;
        enemy.timer = 1.0f + (enemy.id % 5) * 0.2f;
        enemies.push_back(enemy);
    }

    void Simulation::spawn_wave()
    {
        const int counts[5][4] =
        {
            {6, 0, 0, 0},
            {6, 2, 0, 0},
            {4, 2, 2, 0},
            {5, 2, 2, 1},
            {6, 3, 3, 2}
        };
        int index = 0;
        for (int kind = 1; kind <= 4; ++kind)
        {
            for (int i = 0; i < counts[wave - 1][kind - 1]; ++i)
            {
                const float angle = index++ * 2.399963f + wave;
                Vec position;
                for (int attempt = 0; attempt < 64; ++attempt)
                {
                    const float candidate_angle = angle + attempt * 2.399963f;
                    position = player + Vec{std::sin(candidate_angle) * 8.5f, std::cos(candidate_angle) * 8.5f};
                    position.x = std::clamp(position.x, -arena_limit, arena_limit);
                    position.z = std::clamp(position.z, -arena_limit, arena_limit);
                    if ((position - player).length() < 5 ||
                        (constrain_walls(position, kind == 4 ? 0.75f : 0.5f) - position).length() > 0.001f)
                        continue;
                    const bool danger = std::any_of(environment.begin(), environment.end(),
                        [&](const EnvironmentObject &object)
                        {
                            return (object.state == EnvironmentState::Warning ||
                                (object.kind == EnvironmentKind::Pillar && object.state == EnvironmentState::Active)) &&
                                in_sector(position - object.position, object.direction, object.radius + 1, object.angle + 20);
                        });
                    if (!danger)
                        break;
                }
                spawn_enemy(kind, position);
            }
        }
    }

    void Simulation::create_environment()
    {
        const Vec positions[] = {{-3.5f, -1.5f}, {5.5f, 3.5f}, {-9.5f, -9.5f}, {12.5f, -7.5f}};
        for (int i = 0; i < 4; ++i)
        {
            EnvironmentObject object;
            object.id = next_id++;
            object.position = positions[i];
            object.direction = i % 2 ? Vec{0, -1} : Vec{0, 1};
            object.kind = EnvironmentKind::Pillar;
            object.hp = object.max_hp = 44;
            object.half_size = {0.5f, 0.5f};
            object.radius = 6;
            object.angle = 110;
            object.damage = 0;
            object.warning_duration = 1;
            environment.push_back(object);
            for (int layer = 0; layer < pillar_block_count; ++layer)
            {
                FallingBlock block;
                block.id = next_id++;
                block.owner_id = object.id;
                block.layer = layer;
                const Vec side = {-object.direction.z, object.direction.x};
                block.velocity = object.direction * (1.4f + layer * 0.55f) +
                    side * ((layer % 2 ? 1 : -1) * (0.2f + layer * 0.1f));
                position_pillar_block(block, object, pillar_lean_angle);
                block.landing = block.position + block.velocity *
                    std::sqrt(2 * (block.height - block_half_height) / block_gravity);
                position_pillar_block(block, object, 0);
                falling_blocks.push_back(block);
            }
        }
    }

    void Simulation::reset(bool practice)
    {
        *this = Simulation();
        training = practice;
        create_environment();
        create_dungeon();
        if (training)
        {
            spawn_enemy(0, {0, -2});
            spawn_enemy(0, {4, -3});
            spawn_enemy(0, {-3.5f, 1.3f});
        }
        else
        {
            spawn_wave();
        }
    }

    void Simulation::move_player(Vec displacement)
    {
        const int steps = std::max(1, static_cast<int>(std::ceil(displacement.length() / 0.15f)));
        for (int i = 0; i < steps; ++i)
        {
            Vec candidate = player + displacement * (1.0f / steps);
            candidate.x = std::clamp(candidate.x, -arena_limit, arena_limit);
            candidate.z = std::clamp(candidate.z, -arena_limit, arena_limit);
            for (const Enemy &enemy : enemies)
            {
                if (enemy.hp <= 0)
                    continue;
                Vec away = candidate - enemy.position;
                const float separation = enemy.radius + 0.45f;
                if (away.length() < separation)
                    candidate = enemy.position + (away.length() > 0.001f ? away.normalized() : Vec{1, 0}) * separation;
            }
            player = move_body(player, candidate - player, 0.45f);
        }
    }

    void Simulation::start_attack(bool heavy)
    {
        const int tier = heat.tier();
        const float range[] = {1, 1.25f, 1.4f, 1.6f, 2};
        attack = {};
        attack.id = next_id++;
        attack.direction = aim;
        attack.radius = (heavy ? 2.6f : 1.8f) * range[tier] * (1 + 0.15f * range_upgrades);
        attack.angle = heavy ? 130.0f : 100.0f;
        attack.damage = (heavy ? 22.0f : 10.0f) * (tier == 4 ? 1.5f : 1.0f);
        attack.delay = heavy ? 0.18f : 0.0f;
        attack.pending = true;
        attack.tier = tier;
        attack.record = std::make_shared<AttackRecord>();
        attack.record->id = attack.id;
        attack.record->generation = heat.generation;
        attack.record->heat = heavy ? 20.0f : 10.0f;
        cooldown = heavy ? 0.8f : 0.35f;
    }

    void Simulation::burst(Vec position, int count)
    {
        for (int i = 0; i < count && fragments.size() < 128; ++i)
        {
            const float angle = i * 2 * pi / count;
            fragments.push_back({next_id++, position, {std::sin(angle) * 2.5f, std::cos(angle) * 2.5f}, 0.45f});
        }
    }

    void Simulation::damage_enemy(Enemy &enemy, float damage, const std::shared_ptr<AttackRecord> &record)
    {
        if (enemy.hp <= 0)
            return;
        if (record)
        {
            if (std::find(record->hit_ids.begin(), record->hit_ids.end(), enemy.id) != record->hit_ids.end())
                return;
            record->hit_ids.push_back(enemy.id);
            if (!record->charged)
            {
                record->charged = true;
                heat.gain(record->heat, record->generation);
            }
        }
        enemy.hp = std::max(0.0f, enemy.hp - damage);
        enemy.flash = 0.14f;
        ++hits;
        burst(enemy.position, enemy.hp <= 0 ? 8 : 3);
        if (enemy.hp <= 0)
            ++kills;
    }

    void Simulation::damage_player(float damage)
    {
        if (dashing_this_tick || hurt_left > 0 || finished)
            return;
        hp = std::max(0.0f, hp - damage);
        hurt_left = 0.45f;
        if (hp <= 0)
            finished = true;
    }

    void Simulation::resolve_attack()
    {
        slash = {player, attack.direction, attack.radius, attack.angle, 0.14f};
        for (Enemy &enemy : enemies)
        {
            if (enemy.hp > 0 && in_sector(enemy.position - player, attack.direction, attack.radius, attack.angle) &&
                !wall_between(player, enemy.position))
            {
                damage_enemy(enemy, attack.damage, attack.record);
            }
        }
        for (auto &object : environment)
        {
            const Vec target = (object.kind != EnvironmentKind::Pipe) ? object.position + Vec{
                std::clamp(player.x - object.position.x, -object.half_size.x, object.half_size.x),
                std::clamp(player.z - object.position.z, -object.half_size.z, object.half_size.z)} : object.position;
            if (in_sector(target - player, attack.direction, attack.radius, attack.angle) &&
                !wall_between(player, target, 0, object.id))
                damage_environment(object, attack.damage, attack.record);
        }
        std::vector<CellPosition> struck;
        const auto lo = DungeonGrid::world_cell(player.x - attack.radius, 0, player.z - attack.radius);
        const auto hi = DungeonGrid::world_cell(player.x + attack.radius, 1.2f, player.z + attack.radius);
        for (const auto p : dungeon.occupied(lo, hi))
        {
            if (dungeon.get(p).kind != VoxelKind::Stone)
                continue;
            const Vec target{p.x + 0.5f, p.z + 0.5f};
            const auto hit = dungeon.sweep({player.x, 0.65f, player.z}, {target.x, 0.65f, target.z}, {0, 0, 0});
            if (in_sector(target - player, attack.direction, attack.radius + 0.5f, attack.angle) &&
                (hit.fraction > 1 || (hit.cell.x == p.x && hit.cell.z == p.z)))
                struck.push_back(p);
        }
        // Collect before applying damage: one strike cannot tunnel through a newly opened front layer.
        for (const auto p : struck)
            damage_voxel(p, attack.damage, attack.record);
        if (!struck.empty())
            warn_unsupported();
        if (attack.tier >= 3 && attack.record->heat == 20)
        {
            for (auto &projectile : projectiles)
            {
                if (!projectile.record && projectile.life > 0 &&
                    in_sector(projectile.position - player, attack.direction, attack.radius, attack.angle) &&
                    !wall_between(player, projectile.position))
                {
                    projectile.life = 0;
                    ++broken_bullets;
                    burst(projectile.position, 3);
                }
            }
        }
        if (attack.tier >= 2 && projectiles.size() < 256)
        {
            projectiles.push_back({next_id++, player, attack.direction * 12, 0.5f,
                attack.tier == 4 ? 0.65f : 0.4f, attack.damage * 0.7f, attack.record});
        }
        attack.pending = false;
    }

    void Simulation::update_enemies(float delta)
    {
        for (auto &enemy : enemies)
        {
            if (enemy.kind == 0 || enemy.hp <= 0)
                continue;
            enemy.timer -= delta;
            const Vec toward = player - enemy.position;
            if (enemy.charge_left > 0)
            {
                const Vec start = enemy.position;
                const Vec displacement = enemy.direction * (10 * std::min(delta, enemy.charge_left));
                enemy.position = move_body(enemy.position, displacement, enemy.radius);
                if ((enemy.position - start - displacement).length() > 0.01f)
                    enemy.charge_left = 0;
                if (!enemy.charge_hit && segment_distance(player, start, enemy.position) <= 1.05f &&
                    !wall_between(start, player))
                {
                    damage_player(20);
                    enemy.charge_hit = true;
                }
                enemy.charge_left = std::max(0.0f, enemy.charge_left - delta);
                if (enemy.charge_left == 0)
                    enemy.timer = 1.4f;
            }
            else if (enemy.warning)
            {
                if (enemy.timer <= 0)
                {
                    if (enemy.kind == 1)
                    {
                        if (in_sector(player - enemy.position, enemy.direction, 1.6f, 110) &&
                            !wall_between(enemy.position, player))
                            damage_player(8);
                    }
                    else if (enemy.kind == 4)
                    {
                        enemy.charge_left = 0.6f;
                        enemy.charge_hit = false;
                    }
                    else
                    {
                        const int count = enemy.kind == 2 ? 8 : 5;
                        for (int i = 0; i < count && projectiles.size() < 256; ++i)
                        {
                            const float angle = enemy.kind == 2 ? i * pi / 4 :
                                std::atan2(enemy.direction.x, enemy.direction.z) + (i - 2) * 0.18f;
                            const float speed = enemy.kind == 2 ? 4.5f : 6.0f;
                            projectiles.push_back({next_id++, enemy.position,
                                {std::sin(angle) * speed, std::cos(angle) * speed}, 5, 0.16f, 8, nullptr});
                        }
                    }
                    enemy.warning = false;
                    enemy.timer = enemy.kind == 1 ? 0.9f : enemy.kind == 3 ? 1.55f : 2.0f;
                }
            }
            else if (enemy.timer <= 0 && (enemy.kind == 2 || enemy.kind == 3 ||
                (toward.length() < (enemy.kind == 4 ? 7.0f : 1.5f) && !wall_between(enemy.position, player))))
            {
                enemy.warning = true;
                enemy.direction = toward.normalized();
                enemy.timer = enemy.kind == 1 ? 0.25f : enemy.kind == 2 ? 0.5f : enemy.kind == 3 ? 0.45f : 0.7f;
            }
            else if ((enemy.kind == 1 || enemy.kind == 4) && toward.length() > 1.05f)
            {
                enemy.position = move_body(enemy.position, steer(enemy, player) *
                    ((enemy.kind == 4 ? 1.2f : 2.1f) * delta), enemy.radius);
                enemy.direction = toward.normalized();
            }
            else if (enemy.kind == 3)
            {
                const float speed = toward.length() > 6 ? 1.4f : toward.length() < 4 ? -1.4f : 0;
                enemy.position = move_body(enemy.position, steer(enemy, player) * (speed * delta), enemy.radius);
                enemy.direction = toward.normalized();
            }
        }
        for (size_t i = 0; i < enemies.size(); ++i)
        {
            auto &a = enemies[i];
            if (a.hp <= 0 || a.kind == 0)
                continue;
            for (size_t j = i + 1; j < enemies.size(); ++j)
            {
                auto &b = enemies[j];
                Vec apart = a.position - b.position;
                const float distance = apart.length();
                const float separation = a.radius + b.radius + 0.15f;
                if (b.hp > 0 && distance < separation)
                {
                    const Vec offset = (distance > 0.001f ? apart.normalized() : Vec{1, 0}) * ((separation - distance) * 0.5f);
                    if (!a.warning && a.charge_left == 0)
                        a.position = move_body(a.position, offset, a.radius);
                    if (!b.warning && b.charge_left == 0)
                        b.position = move_body(b.position, offset * -1, b.radius);
                }
            }
            a.position = constrain_walls(a.position, a.radius);
        }
    }

    void Simulation::update_projectiles(float delta, Vec old_player)
    {
        for (auto &projectile : projectiles)
        {
            if (projectile.life <= 0)
                continue;
            const Vec start = projectile.position;
            const Vec end = start + projectile.velocity * std::min(delta, projectile.life);
            float nearest = 2;
            EnvironmentObject *obstacle = nullptr;
            Enemy *first = nullptr;
            const auto voxel_hit = dungeon.sweep({start.x, 0.65f, start.z}, {end.x, 0.65f, end.z},
                {projectile.radius, projectile.radius, projectile.radius});
            bool hit_voxel = false;
            for (auto &object : environment)
            {
                float t = 2;
                if (object.blocking())
                    t = wall_entry(start, end, object, projectile.radius);
                else if (object.kind == EnvironmentKind::Pipe && object.state < EnvironmentState::Warning && projectile.record)
                    t = circle_entry(start, end, object.position, projectile.radius + 0.4f);
                if (t < nearest)
                {
                    nearest = t;
                    obstacle = &object;
                }
            }
            if (voxel_hit.fraction < nearest)
            {
                nearest = voxel_hit.fraction;
                hit_voxel = true;
                obstacle = nullptr;
            }
            if (projectile.record)
            {
                for (auto &enemy : enemies)
                {
                    const auto &ids = projectile.record->hit_ids;
                    if (enemy.hp <= 0 || std::find(ids.begin(), ids.end(), enemy.id) != ids.end())
                        continue;
                    const float t = circle_entry(start, end, enemy.position, projectile.radius + enemy.radius);
                    if (t < nearest)
                    {
                        first = &enemy;
                        obstacle = nullptr;
                        hit_voxel = false;
                        nearest = t;
                    }
                }
                if (first)
                {
                    damage_enemy(*first, projectile.damage, projectile.record);
                    projectile.life = 0;
                }
            }
            else if (circle_entry(start - old_player, end - player, {}, projectile.radius + 0.35f) < nearest)
            {
                damage_player(projectile.damage);
                projectile.life = 0;
                obstacle = nullptr;
                hit_voxel = false;
            }
            if (obstacle)
            {
                damage_environment(*obstacle, projectile.damage, projectile.record);
                projectile.life = 0;
            }
            if (hit_voxel)
            {
                damage_voxel(voxel_hit.cell, projectile.damage, projectile.record);
                warn_unsupported();
                projectile.life = 0;
            }
            projectile.position = end;
            projectile.life -= delta;
            if (std::abs(end.x) > arena_half_size || std::abs(end.z) > arena_half_size)
                projectile.life = 0;
        }
        projectiles.erase(std::remove_if(projectiles.begin(), projectiles.end(),
            [](const Projectile &p)
            {
                return p.life <= 0;
            }), projectiles.end());
    }

    void Simulation::tick(float delta, const Input &input)
    {
        if (!std::isfinite(delta) || delta <= 0 || delta > 0.1f || finished)
            return;
        if (choosing_upgrade)
        {
            if (input.upgrade < 1 || input.upgrade > 3)
                return;
            if ((input.upgrade == 1 && range_upgrades == 3) ||
                (input.upgrade == 2 && cooling_upgrades == 4) ||
                (input.upgrade == 3 && overdrive_upgrades == 3))
                return;
            if (input.upgrade == 1)
                range_upgrades = std::min(3, range_upgrades + 1);
            else if (input.upgrade == 2)
                cooling_upgrades = std::min(4, cooling_upgrades + 1);
            else
                overdrive_upgrades = std::min(3, overdrive_upgrades + 1);
            heat.cooling_duration = 4.0f - cooling_upgrades * 0.5f;
            heat.overdrive_duration = 5.0f + overdrive_upgrades;
            hp = std::min(100.0f, hp + 20);
            ++wave;
            choosing_upgrade = false;
            spawn_wave();
            return;
        }
        elapsed += delta;
        heat.tick(delta);
        hurt_left = std::max(0.0f, hurt_left - delta);
        vent_flash = std::max(0.0f, vent_flash - delta);
        cooldown = std::max(0.0f, cooldown - delta);
        dash_cooldown = std::max(0.0f, dash_cooldown - delta);
        slash.life = std::max(0.0f, slash.life - delta);
        for (Enemy &enemy : enemies)
        {
            enemy.previous_position = enemy.position;
            enemy.flash = std::max(0.0f, enemy.flash - delta);
        }
        for (auto &fragment : fragments)
        {
            fragment.life -= delta;
            fragment.position = fragment.position + fragment.velocity * delta;
        }
        fragments.erase(std::remove_if(fragments.begin(), fragments.end(),
            [](const Fragment &f)
            {
                return f.life <= 0;
            }), fragments.end());
        if (input.aim.length() > 0.01f)
            aim = input.aim.normalized();
        if (input.overdrive)
            heat.overdrive();
        if (input.vent && !attack.pending && heat.vent())
        {
            vent_position = player;
            vent_flash = vent_effect_duration;
            cooldown = std::max(cooldown, 0.35f);
            for (auto &enemy : enemies)
            {
                if ((enemy.position - player).length() <= vent_radius && !wall_between(player, enemy.position))
                    damage_enemy(enemy, 28, nullptr);
            }
        }
        if (input.dodge && dash_cooldown <= 0 && !attack.pending)
        {
            dash_direction = input.move.length() > 0.01f ? input.move.normalized() : aim;
            dash_left = 0.2f;
            dash_cooldown = 0.9f;
        }
        const float dash_time = std::min(dash_left, delta);
        dashing_this_tick = dash_time > 0;
        const Vec old_player = player;
        move_player(dash_direction * (15.0f * dash_time) +
            input.move.normalized() * (5.0f * (delta - dash_time)));
        dash_left = std::max(0.0f, dash_left - delta);
        update_environment(delta);
        update_dungeon(delta);
        if (cooldown <= 0 && dash_time == 0 && !finished && (input.light || input.heavy))
            start_attack(input.heavy);
        if (attack.pending)
        {
            attack.delay -= delta;
            if (attack.delay <= 0)
                resolve_attack();
        }
        update_enemies(delta);
        update_projectiles(delta, old_player);
        update_falling_blocks(delta, old_player);
        if (!training)
        {
            enemies.erase(std::remove_if(enemies.begin(), enemies.end(),
                [](const Enemy &enemy)
                {
                    return enemy.hp <= 0;
                }), enemies.end());
            if (enemies.empty() && !finished)
            {
                if (wave == 5)
                {
                    won = true;
                    finished = true;
                }
                else
                {
                    choosing_upgrade = true;
                    projectiles.clear();
                    fragments.clear();
                    attack = {};
                    slash = {};
                    cooldown = 0;
                    dash_left = 0;
                    dash_cooldown = 0;
                    hurt_left = 0;
                    vent_flash = 0;
                }
            }
        }
    }
}
