#include "../cpp/src/meltdown_sim.h"
#include <cstdlib>
#include <iostream>

int failures = 0;

void check(bool condition, const char *message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

#include "dungeon_grid_test.h"

void environment_tests()
{
    using namespace meltdown;
    Simulation game;
    auto fixture = [&](bool wall)
    {
        game.reset();
        game.enemies.clear();
        game.environment.resize(1);
        auto &object = game.environment[0];
        object = EnvironmentObject();
        object.id = 9000;
        object.kind = wall ? EnvironmentKind::Wall : EnvironmentKind::Pipe;
        object.position = {0, -1.8f};
        object.direction = {0, 1};
        object.hp = object.max_hp = wall ? 44.0f : 20.0f;
    };
    auto advance = [&](float seconds, Input command = Input())
    {
        for (int i = 0; i < static_cast<int>(seconds * 100 + 0.5f); ++i)
            game.tick(0.01f, command);
    };
    Input strike;
    strike.heavy = true;
    fixture(false);
    game.tick(0.01f, strike);
    advance(0.18f);
    check(game.environment[0].state == EnvironmentState::Warning && game.hp == 100 && game.heat.value == 0,
        "pipe attack arms telegraph without heat or immediate damage");
    Enemy target;
    target.id = 10000;
    target.position = {0.2f, 0.5f};
    game.enemies.push_back(target);
    advance(0.86f);
    check(game.hp == 70 && game.enemies[0].hp == 70, "one pipe burst damages both sides");
    advance(1);
    check(game.hp == 70 && game.enemies[0].hp == 70 && game.environment[0].state == EnvironmentState::Spent,
        "environment event hits each actor once and becomes spent");
    check(game.heat.value == 0, "environment kills/hits do not generate heat");

    for (int case_id = 0; case_id < 5; ++case_id)
    {
        fixture(false);
        auto &object = game.environment[0];
        object.state = EnvironmentState::Warning;
        object.remaining = 0.01f;
        Input command;
        if (case_id == 0)
            game.player = {4, 0};
        if (case_id == 1)
            game.player = {0, 3};
        if (case_id == 2)
            command.dodge = true;
        if (case_id == 3)
            game.hurt_left = 0.2f;
        if (case_id == 4)
        {
            game.heat.value = 80;
            game.heat.overdrive();
        }
        game.tick(0.01f, command);
        check(game.hp == (case_id == 4 ? 70 : 100), "sector boundaries, dodge, hurt protection and no E immunity");
    }

    fixture(true);
    Input move;
    move.move = {0, -1};
    move.dodge = true;
    game.tick(0.1f, move);
    move.dodge = false;
    advance(0.7f, move);
    check(game.player.z >= -1.001f, "intact wall blocks dash and movement without tunneling");
    target.position = {0, -2.7f};
    game.enemies.push_back(target);
    advance(0.2f, strike);
    check(game.enemies[0].hp == 100 && game.environment[0].hp == 22, "wall takes melee damage and shields enemy behind it");
    advance(0.8f, strike);
    check(game.environment[0].state == EnvironmentState::Warning && game.environment[0].blocking(),
        "wall stays blocking throughout destruction warning");
    game.player = {3, 0};
    advance(0.9f);
    check(!game.wall_between({0, 0}, {0, -3}), "wall destruction opens shared line of sight");
    game.player = {0, 0};
    game.enemies.clear();
    advance(0.7f, move);
    check(game.player.z < -3, "destroyed wall and visual debris permit passage");

    fixture(true);
    game.projectiles.push_back({10001, {0, -4}, {0, 60}, 1, 0.16f, 8, nullptr});
    game.tick(0.1f, {});
    check(game.hp == 100 && game.projectiles.empty() && game.environment[0].hp == 36,
        "fast enemy bullet strikes wall before player and damages cover");
    fixture(true);
    game.environment[0].position = {0, -3};
    game.projectiles.push_back({10001, {0, -1}, {0, 60}, 1, 0.16f, 8, nullptr});
    game.tick(0.1f, {});
    check(game.hp == 92 && game.environment[0].hp == 44, "actor in front of wall is hit first");

    fixture(true);
    target.position = {0, -3};
    game.enemies.push_back(target);
    auto record = std::make_shared<AttackRecord>();
    game.projectiles.push_back({10001, {}, {0, -60}, 1, 0.4f, 10, record});
    game.tick(0.1f, {});
    check(game.environment[0].hp == 34 && game.enemies[0].hp == 100 && game.projectiles.empty(),
        "blade hits wall first and cannot damage actor beyond it");
    fixture(true);
    game.heat.value = 50;
    advance(0.2f, strike);
    check(game.environment[0].hp == 22 && game.heat.value == 50,
        "shared melee/blade attack ID damages wall only once without charging heat");

    fixture(true);
    target.kind = 4;
    target.radius = 0.65f;
    target.position = {0, -4};
    target.charge_left = 0.6f;
    target.direction = {0, 1};
    game.enemies.push_back(target);
    advance(0.6f);
    check(game.enemies[0].position.z <= -2.79f && game.hp == 100 && game.environment[0].hp == 44,
        "Brute charge stops at cover without hitting through it or silently breaking it");
    fixture(true);
    target.kind = 1;
    target.radius = 0.4f;
    target.position = {0, -4};
    target.charge_left = 0;
    game.enemies.push_back(target);
    advance(5);
    check(game.enemies[0].position.z > -1, "pursuer steers around short wall");

    fixture(false);
    EnvironmentObject shield;
    shield.id = 9001;
    shield.kind = EnvironmentKind::Wall;
    shield.position = {0, -0.9f};
    game.environment.push_back(shield);
    game.environment[0].state = EnvironmentState::Warning;
    game.environment[0].remaining = 0.01f;
    game.tick(0.01f, {});
    check(game.hp == 100 && game.environment[1].hp == 20, "cover blocks environmental damage; no automatic chain");
    game.training = false;
    game.enemies.clear();
    game.tick(0.01f, {});
    const float remaining = game.environment[0].remaining;
    advance(1);
    check(game.environment[0].remaining == remaining, "upgrade pause freezes environment timer");
    Input upgrade;
    upgrade.upgrade = 1;
    game.tick(0.01f, upgrade);
    check(game.environment[0].state == EnvironmentState::Active, "environment state persists across waves");
    game.reset();
    check(game.environment.size() == 4 && std::all_of(game.environment.begin(), game.environment.end(),
        [](const EnvironmentObject &object)
        {
            return object.hp == object.max_hp && object.state == EnvironmentState::Intact && object.remaining == 0;
        }), "restart restores all environment objects and timers");
}

void pillar_tests()
{
    using namespace meltdown;
    Simulation game;
    game.reset();
    game.enemies.clear();
    check(game.falling_blocks.size() == 20 && game.environment[0].kind == EnvironmentKind::Pillar, "four columns built from five individual blocks");
    game.player = {-4, -0.6f};
    Input strike;
    strike.heavy = true;
    for (int i = 0; i < 100 && game.environment[0].state != EnvironmentState::Warning; ++i)
        game.tick(0.01f, strike);
    check(game.environment[0].state == EnvironmentState::Warning && game.hp == 100 && game.heat.value == 0,
        "pillar support breaks into a warning without area damage or heat");
    const float top_z = game.falling_blocks[4].position.z;
    game.player = {-4.45f, 1.3f};
    Enemy target;
    target.id = 10000;
    target.position = {-3.5f, 1.3f};
    game.enemies.push_back(target);
    for (int i = 0; i < 50; ++i)
        game.tick(0.01f, {});
    check(game.falling_blocks[4].position.z > top_z && !game.falling_blocks[4].falling && game.hp == 100,
        "warning leans real block positions without damage");
    for (int i = 0; i < 60 && game.environment[0].state == EnvironmentState::Warning; ++i)
        game.tick(0.01f, {});
    check(game.hp == 100 && game.enemies[0].hp == 100 && game.falling_blocks[4].height > 4,
        "collapse begins overhead; no instant cone damage at activation");
    for (int i = 0; i < 150; ++i)
        game.tick(0.01f, {});
    check(game.hp < 100 && game.enemies[0].hp < 100 && game.heat.value == 0,
        "falling blocks actually contact and damage both sides without heat");
    check(game.environment[0].state == EnvironmentState::Spent && !game.environment[0].blocking(),
        "column becomes spent only when all five blocks land");
    for (int i = 0; i < 5; ++i)
    {
        const auto &block = game.falling_blocks[i];
        check(block.settled && std::abs(block.height - block_half_height) < 0.001f &&
            (block.position - block.landing).length() < 0.01f, "gravity reaches predicted landing and stops");
    }

    auto contact_fixture = [&]()
    {
        game.reset();
        game.enemies.clear();
        game.environment.resize(1);
        game.environment[0].state = EnvironmentState::Active;
        game.falling_blocks.resize(1);
        auto &block = game.falling_blocks[0];
        block.position = {};
        block.height = 1;
        block.velocity = {};
        block.vertical_speed = 0;
        block.falling = true;
        block.settled = false;
        block.hit_ids.clear();
    };
    contact_fixture();
    game.falling_blocks[0].height = 5;
    game.tick(0.1f, {});
    check(game.hp == 100, "block above actor does not hit its XZ footprint");
    contact_fixture();
    game.player = {2, 0};
    game.tick(0.1f, {});
    check(game.hp == 100, "no damage outside block contact radius");
    contact_fixture();
    game.falling_blocks[0].height = 3;
    game.falling_blocks[0].vertical_speed = -50;
    game.tick(0.1f, {});
    check(game.hp == 82 && game.falling_blocks[0].settled, "swept vertical collision catches a fast block before landing");
    game.hurt_left = 0;
    game.tick(0.1f, {});
    check(game.hp == 82, "settled rubble never damages standing actors");
    contact_fixture();
    game.falling_blocks[0].position = {-4, 0};
    game.falling_blocks[0].velocity = {80, 0};
    game.tick(0.1f, {});
    check(game.hp == 82 && game.player.x > 0.5f, "fast lateral block sweeps actor and pushes along travel direction");
    contact_fixture();
    Input dodge;
    dodge.dodge = true;
    game.tick(0.01f, dodge);
    check(game.hp == 100, "dodge protects from block contact");
    game.dash_left = 0;
    game.player = {};
    game.tick(0.01f, {});
    check(game.hp == 100 && game.falling_blocks[0].hit_ids.size() == 1,
        "same block cannot retry an already dodged contact");
    contact_fixture();
    game.heat.value = 80;
    game.heat.overdrive();
    game.tick(0.01f, {});
    game.hurt_left = 0;
    game.tick(0.01f, {});
    check(game.hp == 82, "overdrive is not immunity; one block hits each actor only once");
    contact_fixture();
    game.choosing_upgrade = true;
    game.tick(0.1f, {});
    check(game.falling_blocks[0].height == 1, "upgrade selection freezes falling blocks");
    game.reset();
    check(std::none_of(game.falling_blocks.begin(), game.falling_blocks.end(), [](const FallingBlock &block)
        {
            return block.falling || block.settled || !block.hit_ids.empty();
        }), "restart restores block stack, velocities and collision history");
    for (float delta : {0.1f, 1.0f / 60, 1.0f / 120})
    {
        game.reset();
        game.enemies.clear();
        game.player = {10, 10};
        game.environment[0].state = EnvironmentState::Warning;
        game.environment[0].remaining = 0;
        for (int i = 0; i < static_cast<int>(2 / delta); ++i)
            game.tick(delta, {});
        for (int i = 0; i < 5; ++i)
            check((game.falling_blocks[i].position - game.falling_blocks[i].landing).length() < 0.001f,
                "ballistic landing is stable at 10, 60 and 120 physics steps per second");
    }
}

int main()
{
    dungeon_tests();
    pillar_tests();
    environment_tests();
    using namespace meltdown;
    Simulation game;
    game.reset();
    Input input;
    input.move = {1, 1};
    for (int i = 0; i < 60; ++i)
        game.tick(1.0f / 60, input);
    check(std::abs(game.player.length() - 5) < 0.01f, "normalized movement, 5 m/s");
    game.reset();
    input = {};
    input.heavy = true;
    game.tick(0.01f, input);
    check(game.hits == 0, "heavy windup precedes damage");
    input.heavy = false;
    input.aim = {1, 0};
    for (int i = 0; i < 30; ++i)
        game.tick(0.01f, input);
    check(game.hits == 1 && game.enemies[0].hp == 78, "direction snapshot and exactly one hit");
    game.reset();
    input = {};
    input.dodge = true;
    input.move = {1, 0};
    game.tick(0.01f, input);
    input.dodge = false;
    for (int i = 0; i < 19; ++i)
        game.tick(0.01f, input);
    check(std::abs(game.player.x - 3) < 0.01f, "dash travels 3m");
    game.tick(0.01f, input);
    check(game.dash_left == 0, "dash immunity ends");
    for (int i = 0; i < 1000; ++i)
        game.tick(0.01f, input);
    check(std::abs(game.player.x - arena_limit) < 0.001f && game.player.x > 20, "expanded arena boundary holds");
    game.reset();
    check(game.hits == 0 && game.cooldown == 0 && game.player.length() == 0, "reset clears run");
    Heat heat;
    for (const int value : {24, 25, 49, 50, 79, 80, 99})
    {
        heat.value = static_cast<float>(value);
        const int expected = value >= 80 ? 3 : value >= 50 ? 2 : value >= 25 ? 1 : 0;
        check(heat.tier() == expected, "heat stage boundaries");
    }
    heat.value = 39;
    check(!heat.vent() && heat.value == 39, "Q rejects insufficient heat");
    heat.value = 80;
    check(heat.vent() && heat.value == 40, "Q consumes exactly 40");
    heat.value = 79;
    check(!heat.overdrive(), "E rejects below 80");
    heat.value = 80;
    check(heat.overdrive() && heat.tier() == 4, "E enters overdrive");
    check(!heat.overdrive() && !heat.vent(), "Q/E locked during overdrive");
    heat.tick(4.99f);
    check(heat.mode == HeatMode::Overdrive, "overdrive lasts five seconds");
    heat.tick(0.01f);
    check(heat.mode == HeatMode::Cooling && heat.value == 0, "overdrive ends in cooling");
    heat.gain(20, heat.generation);
    check(heat.value == 0 && !heat.vent() && !heat.overdrive(), "cooling rejects heat and Q/E");
    heat.tick(4);
    check(heat.mode == HeatMode::Normal, "cooling lasts four seconds");
    heat.value = 90;
    const unsigned stale = heat.generation;
    heat.gain(10, stale);
    check(heat.mode == HeatMode::Cooling && heat.value == 0, "100 heat immediately cools");
    heat.tick(4);
    heat.gain(20, stale);
    check(heat.value == 0, "stale attacks cannot charge a later heat generation");
    game.reset();
    game.enemies[0].position = {0, -1};
    game.enemies[1].position = {0.5f, -1};
    input = {};
    input.light = true;
    game.tick(0.01f, input);
    check(game.hits == 2 && game.heat.value == 10, "multi-target hit gains heat once");
    game.reset();
    input.aim = {0, 1};
    game.tick(0.01f, input);
    check(game.heat.value == 0, "miss gains no heat");
    game.reset();
    game.enemies[0].position = {0, -1};
    game.heat.value = 50;
    input = {};
    input.light = true;
    game.tick(0.01f, input);
    input.light = false;
    for (int i = 0; i < 30; ++i)
        game.tick(0.01f, input);
    check(game.enemies[0].hp == 90 && game.heat.value == 60, "blade shares melee hit ID and heat grant");
    game.reset();
    game.enemies[0].position = {0, -4};
    game.heat.value = 50;
    input.light = true;
    game.tick(0.01f, input);
    input.light = false;
    for (int i = 0; i < 40; ++i)
        game.tick(0.01f, input);
    check(game.enemies[0].hp == 93 && game.heat.value == 60, "blade-only hit charges once");
    game.reset();
    game.heat.value = 80;
    input = {};
    input.heavy = true;
    game.tick(0.01f, input);
    input.heavy = false;
    for (int i = 0; i < 15; ++i)
        game.tick(0.01f, input);
    game.projectiles.push_back({500, {0, -0.5f}, {0, 10}, 1, 0.16f, 8, nullptr});
    game.tick(0.02f, input);
    check(game.broken_bullets == 1 && game.hp == 100, "critical destruction precedes same-frame bullet hit");
    check(game.heat.mode == HeatMode::Cooling, "80 plus heavy 20 causes cooling");
    game.reset();
    game.projectiles.push_back({500, {-3, 0}, {100, 0}, 1, 0.16f, 8, nullptr});
    input = {};
    game.tick(0.06f, input);
    check(game.hp == 92 && game.projectiles.empty(), "swept bullet hits once at high speed");
    game.reset();
    game.projectiles.push_back({500, {-3, 0}, {100, 0}, 1, 0.16f, 8, nullptr});
    input.dodge = true;
    input.move = {0, 1};
    game.tick(0.06f, input);
    check(game.hp == 100, "dash protects against bullets");
    game.reset();
    game.heat.value = 40;
    game.projectiles.push_back({500, {0, -1}, {0, 0}, 1, 0.16f, 8, nullptr});
    input = {};
    input.vent = true;
    game.tick(0.01f, input);
    check(game.heat.value == 0 && game.enemies[0].hp == 72 && game.projectiles.size() == 1,
        "Q damages in radius without generating heat or clearing bullets");
    for (int tier_heat : {0, 25, 50, 80})
    {
        game.reset();
        game.heat.value = static_cast<float>(tier_heat);
        game.enemies.clear();
        game.projectiles.push_back({500, {0, -1}, {}, 1, 0.16f, 8, nullptr});
        input = {};
        input.light = true;
        game.tick(0.01f, input);
        check(game.broken_bullets == 0, "light attacks never clear enemy bullets");
        const float ranges[] = {1.8f, 2.25f, 2.52f, 2.88f};
        const int tier = tier_heat == 0 ? 0 : tier_heat == 25 ? 1 : tier_heat == 50 ? 2 : 3;
        check(std::abs(game.attack.radius - ranges[tier]) < 0.001f, "stage changes real attack radius");
    }
    game.reset();
    game.heat.value = 90;
    game.heat.gain(10, game.heat.generation);
    game.enemies[0].position = {0, -1};
    input = {};
    input.light = true;
    game.tick(0.01f, input);
    check(game.enemies[0].hp == 90 && game.heat.value == 0 && game.attack.tier == 0,
        "cooling permits ordinary attacks without heat bonuses");
    input = {};
    input.move = {1, 0};
    input.dodge = true;
    game.tick(0.01f, input);
    check(game.dash_left > 0, "cooling permits dodge during attack recovery");
    input = {};
    for (int i = 0; i < 30; ++i)
        game.tick(0.01f, input);
    game.projectiles.push_back({501, game.player, {}, 1, 0.16f, 8, nullptr});
    game.tick(0.01f, input);
    check(game.hp == 92, "damage resumes after dash immunity ends");
    game.reset();
    input = {};
    input.dodge = true;
    game.tick(0.1f, input);
    input.dodge = false;
    game.tick(0.1f, input);
    check((game.player - game.enemies[0].position).length() >= 0.84f,
        "dash cannot pass through a target body");
    check(game.player.z > -2, "dash stays on near side of target");
    for (float x : {-arena_limit, 0.0f, arena_limit})
    {
        for (float z : {-arena_limit, 0.0f, arena_limit})
        {
            game.reset(false);
            game.enemies.clear();
            game.player = {x, z};
            game.choosing_upgrade = true;
            input = {};
            input.upgrade = 1;
            game.tick(0.01f, input);
            for (const auto &enemy : game.enemies)
            {
                check(std::abs(enemy.position.x) <= arena_limit && std::abs(enemy.position.z) <= arena_limit,
                    "player-relative spawning stays inside enlarged arena at edges and corners");
                check((enemy.position - game.player).length() >= 5 && (enemy.position - game.player).length() <= 8.51f,
                    "spawn remains nearby but safely separated from player");
            }
        }
    }
    game.reset();
    game.player = {20, 0};
    game.projectiles.push_back({500, {19, 0}, {10, 0}, 1, 0.16f, 8, nullptr});
    input = {};
    game.tick(0.1f, input);
    check(game.hp == 92, "bullets hit beyond old 12m map boundary");
    game.projectiles.push_back({501, {arena_half_size - 0.01f, 0}, {10, 0}, 1, 0.16f, 8, nullptr});
    game.tick(0.1f, input);
    check(game.projectiles.empty(), "bullets expire beyond expanded floor");

    for (int run = 0; run < 3; ++run)
    {
        game.reset(false);
        input = {};
        for (int i = 0; i < 18000 && !game.finished; ++i)
            game.tick(1.0f / 60, input);
        check(game.finished && game.hp == 0, "live enemies can finish an idle run");
        const float elapsed = game.elapsed;
        game.tick(0.1f, input);
        check(game.elapsed == elapsed, "death freezes simulation timers");
        game.reset(false);
        check(game.hp == 100 && game.heat.value == 0 && game.projectiles.empty(), "death/restart clears all combat state");
    }
    game.reset(false);
    for (int wave = 1; wave <= 5; ++wave)
    {
        check(game.wave == wave, "waves advance in order");
        for (const auto &enemy : game.enemies)
            check((enemy.position - game.player).length() >= 5, "spawn respects safety distance");
        for (auto &enemy : game.enemies)
            enemy.hp = 0;
        input = {};
        game.tick(0.01f, input);
        if (wave < 5)
        {
            check(game.choosing_upgrade && game.projectiles.empty(), "wave clears projectiles and offers upgrade");
            const float elapsed = game.elapsed;
            game.tick(0.1f, input);
            check(game.elapsed == elapsed, "upgrade choice freezes combat");
            input.upgrade = wave <= 3 ? 1 : 2;
            game.tick(0.01f, input);
        }
    }
    check(game.won && game.finished && game.range_upgrades == 3, "five waves end in victory");
    game.reset(false);
    check(game.wave == 1 && game.range_upgrades == 0 && game.cooling_upgrades == 0 &&
        game.overdrive_upgrades == 0 && !game.finished, "victory restart resets upgrades and wave");
    game.choosing_upgrade = true;
    game.range_upgrades = 3;
    input.upgrade = 1;
    game.tick(0.01f, input);
    check(game.choosing_upgrade && game.wave == 1, "maxed upgrade cannot consume a choice");

    for (int run = 0; run < 3; ++run)
    {
        game.reset(false);
        int peak_bullets = 0;
        int peak_fragments = 0;
        for (int i = 0; i < 36000 && !game.finished; ++i)
        {
            input = {};
            input.upgrade = game.range_upgrades < 3 ? 1 : 2;
            float nearest = 1e9f;
            const Enemy *target = nullptr;
            for (const auto &enemy : game.enemies)
            {
                const float distance = (enemy.position - game.player).length();
                if (enemy.hp > 0 && distance < nearest)
                {
                    target = &enemy;
                    nearest = distance;
                }
            }
            if (target)
            {
                input.aim = (target->position - game.player).normalized();
                if (nearest > 1.9f)
                    input.move = input.aim;
                input.heavy = nearest < (game.heat.tier() >= 2 ? 4.0f : 2.5f);
                input.overdrive = game.heat.value >= 80;
                input.vent = run == 1 && game.heat.value >= 40 && nearest < 2.5f;
                if (target->warning && nearest < 2 && !game.attack.pending)
                {
                    input.dodge = true;
                    input.move = input.aim * -1;
                }
            }
            game.tick(1.0f / 60, input);
            peak_bullets = std::max(peak_bullets, static_cast<int>(game.projectiles.size()));
            peak_fragments = std::max(peak_fragments, static_cast<int>(game.fragments.size()));
        }
        check(game.finished, "combat bot reaches a result without external state edits");
        check(peak_bullets <= 256 && peak_fragments <= 128, "bounded projectiles and fragments");
        std::cout << "BOT_RUN " << run + 1 << ": " << (game.won ? "WIN" : "DEATH")
            << " wave=" << game.wave << " time=" << game.elapsed << " kills=" << game.kills
            << " hp=" << game.hp << " breaks=" << game.broken_bullets
            << " peak_bullets=" << peak_bullets << " peak_fragments=" << peak_fragments << '\n';
    }
    std::cout << "MELTDOWN_SIM_TEST: " << failures << " failures\n";
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
