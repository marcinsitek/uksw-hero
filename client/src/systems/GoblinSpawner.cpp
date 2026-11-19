
#include "GoblinSpawner.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

// Constructor
GoblinSpawner::GoblinSpawner(
    const std::vector<Vector2>& spawnPoints,
    int goblinsPerWave,
    Texture2D texture,
    GameplayEventHandler onEvent
)
    : spawnPoints(spawnPoints),
      texture(texture),
      onEvent(std::move(onEvent)),
      goblinsPerWave(std::max(0, goblinsPerWave))
{
    if (spawnPoints.empty()) {
        throw std::invalid_argument(
            "GoblinSpawner requires at least one spawn point"
        );
    }

    // Randomize the time until the first wave.
    spawnInterval = RandomSpawnInterval();
}

// Update the spawning timer
void GoblinSpawner::Update(
    float dt,
    std::vector<Goblin>& goblins
)
{
    if (dt <= 0.0f) {
        return;
    }

    spawnTimer += dt;

    // Handle elapsed time, including long frames.
    while (spawnTimer >= spawnInterval) {
        spawnTimer -= spawnInterval;

        SpawnWave(goblins);

        // Choose a new interval for the next wave.
        spawnInterval = RandomSpawnInterval();
    }
}

// Spawn a wave of goblins
void GoblinSpawner::SpawnWave(
    std::vector<Goblin>& goblins
)
{
    for (int i = 0; i < goblinsPerWave; i++) {

        // Select a random building entrance.
        int index = GetRandomValue(
            0,
            static_cast<int>(spawnPoints.size()) - 1
        );

        Vector2 spawnPosition = spawnPoints[index];

        // Construct the goblin directly in the vector.
        goblins.emplace_back(
            spawnPosition,
            texture,
            onEvent
        );
    }
}

// Change the number of goblins per wave
void GoblinSpawner::SetGoblinsPerWave(int count)
{
    goblinsPerWave = std::max(0, count);
}

// Generate a random interval between 10 and 15 seconds
float GoblinSpawner::RandomSpawnInterval() const
{
    // Random interval in increments of 0.1 seconds.
    return static_cast<float>(
        GetRandomValue(100, 150)
    ) / 10.0f;
}
