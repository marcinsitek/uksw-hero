
#pragma once

#include "raylib.h"
#include "../characters/Goblin.h"

#include <vector>

class GoblinSpawner {
public:
    // spawnPoints: positions in front of building entrances
    // goblinsPerWave: number of goblins spawned each wave
    // texture: shared goblin sprite texture (not owned by spawner)
    GoblinSpawner(
        const std::vector<Vector2>& spawnPoints,
        int goblinsPerWave,
        Texture2D texture,
        GameplayEventHandler onEvent = {}
    );

    // Update the spawning timer and create goblins when needed.
    void Update(
        float dt,
        std::vector<Goblin>& goblins
    );

    // Change the number of goblins spawned per wave.
    void SetGoblinsPerWave(int count);

private:
    // Possible building entrance positions.
    std::vector<Vector2> spawnPoints;

    // Shared texture loaded by the game.
    Texture2D texture;
    GameplayEventHandler onEvent;

    // Number of goblins spawned per wave.
    int goblinsPerWave;

    // Spawning timer.
    float spawnTimer = 0.0f;

    // Time until the next wave (10-15 seconds).
    float spawnInterval = 10.0f;

    // Create one wave of goblins.
    void SpawnWave(std::vector<Goblin>& goblins);

    // Generate a random interval between 10 and 15 seconds.
    float RandomSpawnInterval() const;
};
