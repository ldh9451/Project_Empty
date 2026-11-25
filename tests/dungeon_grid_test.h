#pragma once
#include "../cpp/src/meltdown_sim.h"

void dungeon_tests()
{
    using namespace meltdown;
    check(voxel_size == 1 && dungeon_chunk_size == 8, "1m blocks / 8m chunks");
    check(DungeonGrid::world_cell(-0.01f, -0.01f, -8.01f) == CellPosition{-1, -1, -9},
        "World coordinates floor correctly on all negative axes");
    check(DungeonGrid::chunk_of({-1, -8, -9}) == CellPosition{-1, -1, -2} &&
        DungeonGrid::local_index({-1, -1, -1}) == 511, "Negative chunk/local coordinates");
    DungeonGrid grid;
    grid.set({7, -1, 0}, {VoxelKind::Floor});
    grid.set({7, 0, 0}, {VoxelKind::Stone});
    grid.set({7, 1, 0}, {VoxelKind::Stone});
    grid.set({8, 1, 0}, {VoxelKind::Stone});
    grid.set({20, 0, 20}, {VoxelKind::Floor});
    check(grid.unsupported().empty(), "Support connectivity crosses chunk boundary");
    const auto neighbor_revision = grid.chunks.at({1, 0, 0}).revision;
    const auto distant_revision = grid.chunks.at({2, 0, 2}).revision;
    grid.set({7, 0, 0}, {});
    check(grid.unsupported().size() == 2, "Removing support detaches group across chunks");
    check(grid.chunks.at({1, 0, 0}).revision > neighbor_revision &&
        grid.chunks.at({2, 0, 2}).revision == distant_revision, "Only changed and face-neighbor chunks become dirty");
    check(grid.occupied({7, 1, 0}, {8, 1, 0}).size() == 2, "Bounds query returns both sides of seam");
    auto hit = grid.sweep({-10, 1.5f, 0.5f}, {20, 1.5f, 0.5f}, {0.1f, 0.1f, 0.1f});
    check(hit.cell == CellPosition{7, 1, 0} && hit.fraction < 1, "Fast sweep finds first voxel across multiple chunks");
    hit = grid.sweep({7.5f, 5, 0.5f}, {7.5f, -2, 0.5f}, {0.5f, 0.5f, 0.5f}, 0, true);
    check(hit.cell == CellPosition{7, 1, 0} && std::abs(hit.fraction - 2.5f / 7) < 0.0001f,
        "Falling cube sweep lands on top of terrain, not through it");

    Simulation game;
    auto fixture = [&]()
    {
        game.reset();
        game.environment.clear();
        game.falling_blocks.clear();
        game.enemies.clear();
        game.dungeon.create_floor();
        game.player = {7.5f, 2.2f};
        game.dungeon.set({7, 0, 0}, {VoxelKind::Stone});
        game.dungeon.set({7, 1, 0}, {VoxelKind::Stone});
        game.dungeon.set({7, 2, 0}, {VoxelKind::Stone});
        game.dungeon.set({8, 2, 0}, {VoxelKind::Stone});
    };
    auto advance = [&](float time, Input command = Input{})
    {
        for (int i = 0; i < static_cast<int>(time * 100); ++i)
            game.tick(0.01f, command);
    };
    fixture();
    Input heavy;
    heavy.heavy = true;
    heavy.aim = {0, -1};
    advance(0.25f, heavy);
    check(game.dungeon.get({7, 0, 0}).hp == 22 && game.dungeon.get({7, 1, 0}).hp == 22,
        "Heavy strike damages reachable 1m cells once each");
    check(game.heat.value == 0, "Voxel damage does not charge heat");
    advance(0.8f, heavy);
    check(!game.dungeon.get({7, 0, 0}).solid() && !game.dungeon.get({7, 1, 0}).solid(),
        "Two heavy strikes open wall cells");
    check(game.collapses.size() == 1 && game.dungeon.get({8, 2, 0}).warning,
        "Disconnected upper wall warns across chunk boundary");
    check(game.falling_blocks.empty(), "Warning precedes physical debris");
    game.player = {7.5f, 0.5f};
    advance(1.7f);
    check(game.hp == 82 && game.heat.value == 0, "Detached wall block contacts player without heat");
    check(game.falling_blocks.size() == 2 && game.falling_blocks[0].settled,
        "Disconnected cells become settled 1m blocks");
    const float hp = game.hp;
    advance(0.6f);
    check(game.hp == hp && !game.wall_between({7.5f, -1}, {7.5f, 2}), "Settled rubble is harmless and passage stays open");
    game.reset();
    check(game.collapses.empty() && game.falling_blocks.size() == 20 &&
        game.dungeon.occupied({-24, 0, -24}, {23, 7, 23}).size() == 74, "Reset restores dungeon / four pillars / 54 wall cells");

    fixture();
    Input move;
    move.move = {0, -1};
    move.dodge = true;
    advance(0.5f, move);
    check(game.player.z >= 1.34f, "Dash cannot cross solid 1m wall");
    fixture();
    game.player = {7.5f, 2.2f};
    game.projectiles.push_back({9000, {7.5f, -5}, {0, 100}, 1, 0.1f, 8, nullptr});
    game.tick(0.1f, {});
    check(game.hp == 100 && game.projectiles.empty(), "Wall wins earliest hit against fast enemy projectile");

    fixture();
    game.dungeon.set({7, 0, 0}, {});
    check(game.wall_between({7.5f, -1}, {7.5f, 2}, 0.45f), "Body clearance rejects a one-metre-high tunnel");
    fixture();
    game.dungeon.set({7, 1, 0}, {});
    game.dungeon.set({7, 2, 0}, {});
    game.dungeon.set({8, 2, 0}, {});
    FallingBlock debris;
    debris.id = 9500;
    debris.position = {7.5f, 0.5f};
    debris.height = 5;
    debris.falling = true;
    game.falling_blocks.push_back(debris);
    advance(1.5f);
    check(game.falling_blocks[0].settled && std::abs(game.falling_blocks[0].height - 1.5f) < 0.001f,
        "Detached block rests on solid terrain top with correct half-metre offset");

    fixture();
    Enemy enemy;
    enemy.id = 8000;
    enemy.kind = 1;
    enemy.position = {7.5f, -2};
    enemy.timer = 100;
    game.enemies.push_back(enemy);
    advance(5);
    std::cout << "DUNGEON_ROUTE: " << game.enemies[0].position.x << ", " << game.enemies[0].position.z << '\n';
    check(game.enemies[0].position.z > 1.0f, "Enemy navigates around voxel wall using clear body routes");
}
