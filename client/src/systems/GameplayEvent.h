#pragma once

#include <functional>

enum class GameplayEvent {
    PlayerAttacked,
    PlayerDied,
    GoblinKilled
};

// Characters report gameplay facts without depending on HTTP or session IDs.
using GameplayEventHandler = std::function<void(GameplayEvent)>;
