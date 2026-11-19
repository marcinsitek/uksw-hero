
#pragma once

#include "raylib.h"
#include "../systems/GameplayEvent.h"
#include <vector>
#include <cstddef>

class Student;
class NavigationGrid;

class Goblin {
public:
    // Create a goblin at a specified position.
    // The texture is shared and owned by the game.
    Goblin(Vector2 spawnPosition, Texture2D texture, GameplayEventHandler onEvent = {});

    // Update movement, pathfinding, attacks and animations.
    void Update(
        float dt,
        Student& student,
        const NavigationGrid& grid,
        const std::vector<Rectangle>& obstacles
    );

    // Draw the goblin and its current animation frame.
    void Draw() const;

    // Position and collision.
    Vector2 GetPosition() const;
    Rectangle GetBounds() const;

    // Combat.
    void TakeDamage(int damage);
    bool IsAlive() const;
    bool IsDead() const;

private:
    // Position is the top-left of the collision rectangle.
    Vector2 position;
    Texture2D texture;
    GameplayEventHandler onEvent;

    // Character properties.
    float speed = 65.0f;
    float size = 24.0f;
    float spriteSize = 64.0f;

    int health = 30;
    int attackDamage = 10;

    // Attack settings.
    float attackRange = 32.0f;
    float attackCooldown = 1.0f;
    float attackTimer = 0.0f;

    // Goblin behavior.
    enum class State {
        Walking,
        Attacking,
        Dying,
        Dead
    };

    State state = State::Walking;

    // Direction order matches the student's sprite layout.
    enum class Direction {
        Down,
        Up,
        Left,
        Right
    };

    Direction direction = Direction::Down;

    // Animation.
    int currentFrame = 0;
    float frameTimer = 0.0f;

    // A* pathfinding.
    std::vector<Vector2> path;
    std::size_t pathIndex = 0;

    // Recalculate the path periodically.
    float pathTimer = 0.0f;
    float pathInterval = 0.5f;

    // Behavior methods.
    void UpdatePath(
        const Student& student,
        const NavigationGrid& grid
    );

    void FollowPath(
        float dt,
        const std::vector<Rectangle>& obstacles
    );

    bool IsInAttackRange(const Student& student) const;

    void Attack(Student& student);

    // Animation methods.
    void UpdateAnimation(float dt);
    void UpdateDirection(Vector2 movement);
};
