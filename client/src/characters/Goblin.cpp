
#include "Goblin.h"
#include "Student.h"
#include "../systems/NavigationGrid.h"
#include "../systems/Pathfinding.h"

#include "raymath.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace {

    constexpr float SPRITE_FRAMES = 4.0f;
    constexpr float SPRITE_ROWS = 20.0f;

    // Returns the center of a collision rectangle.
    Vector2 GetCenter(Rectangle bounds)
    {
        return {
            bounds.x + bounds.width / 2.0f,
            bounds.y + bounds.height / 2.0f
        };
    }

    // Checks whether a rectangle overlaps any obstacle.
    bool Collides(
        Rectangle bounds,
        const std::vector<Rectangle>& obstacles
    )
    {
        for (const Rectangle& obstacle : obstacles) {
            if (CheckCollisionRecs(bounds, obstacle)) {
                return true;
            }
        }

        return false;
    }

}

// Constructor
Goblin::Goblin(Vector2 spawnPosition, Texture2D texture, GameplayEventHandler onEvent)
    : position(spawnPosition),
      texture(texture),
      onEvent(std::move(onEvent))
{
    // Allow immediate path calculation.
    pathTimer = pathInterval;

    // Allow immediate attack when in range.
    attackTimer = attackCooldown;
}

// Main update
void Goblin::Update(
    float dt,
    Student& student,
    const NavigationGrid& grid,
    const std::vector<Rectangle>& obstacles
)
{
    if (state == State::Dead) {
        return;
    }

    // Continue death animation without moving or attacking.
    if (state == State::Dying) {
        UpdateAnimation(dt);
        return;
    }

    attackTimer += dt;
    pathTimer += dt;

    if (student.IsDead()) {
        state = State::Walking;
        UpdateAnimation(dt);
        return;
    }

    // Attack if the student is close enough.
    if (IsInAttackRange(student)) {
        if (state != State::Attacking) {
            state = State::Attacking;
            currentFrame = 0;
            frameTimer = 0.0f;
        }

        Attack(student);
    }
    else {
        if (state != State::Walking) {
            state = State::Walking;
            currentFrame = 0;
            frameTimer = 0.0f;
        }

        // Recalculate path periodically.
        if (pathTimer >= pathInterval) {
            UpdatePath(student, grid);
            pathTimer = 0.0f;
        }

        FollowPath(dt, obstacles);
    }

    UpdateAnimation(dt);
}

// Calculate path toward the student
void Goblin::UpdatePath(
    const Student& student,
    const NavigationGrid& grid
)
{
    Vector2 start = GetCenter(GetBounds());
    Vector2 goal = GetCenter(student.GetBounds());

    path = Pathfinding::FindPath(start, goal, grid);

    // The first waypoint usually represents the starting tile.
    pathIndex = path.size() > 1 ? 1 : 0;
}

// Move smoothly along the calculated path
void Goblin::FollowPath(
    float dt,
    const std::vector<Rectangle>& obstacles
)
{
    if (pathIndex >= path.size()) {
        return;
    }

    Vector2 target = path[pathIndex];
    Vector2 center = GetCenter(GetBounds());

    Vector2 delta = Vector2Subtract(target, center);
    float distance = Vector2Length(delta);

    // Advance when the current waypoint is reached.
    if (distance < 3.0f) {
        pathIndex++;
        return;
    }

    Vector2 movement = Vector2Scale(delta, 1.0f / distance);

    UpdateDirection(movement);

    float step = std::min(speed * dt, distance);

    Vector2 nextPosition = {
        position.x + movement.x * step,
        position.y + movement.y * step
    };

    Rectangle nextBounds = {
        nextPosition.x,
        nextPosition.y,
        size,
        size
    };

    // Only move if the destination is not blocked.
    if (!Collides(nextBounds, obstacles)) {
        position = nextPosition;
    }
    else {
        // Force a new path calculation on the next update.
        pathTimer = pathInterval;
    }
}

// Check whether the student is within attack range
bool Goblin::IsInAttackRange(const Student& student) const
{
    Vector2 goblinCenter = GetCenter(GetBounds());
    Vector2 studentCenter = GetCenter(student.GetBounds());

    float distance = Vector2Distance(
        goblinCenter,
        studentCenter
    );

    return distance <= attackRange;
}

// Attack the student
void Goblin::Attack(Student& student)
{
    if (attackTimer < attackCooldown) {
        return;
    }

    student.TakeDamage(attackDamage);
    attackTimer = 0.0f;
}

// Receive damage
void Goblin::TakeDamage(int damage)
{
    if (state == State::Dying || state == State::Dead) {
        return;
    }

    if (damage <= 0) {
        return;
    }

    health -= damage;

    if (health <= 0) {
        health = 0;

        state = State::Dying;
        currentFrame = 0;
        frameTimer = 0.0f;

        path.clear();
        pathIndex = 0;

        if (onEvent) onEvent(GameplayEvent::GoblinKilled);
    }
}

bool Goblin::IsAlive() const
{
    return health > 0;
}

// Returns true after the death animation finishes
bool Goblin::IsDead() const
{
    return state == State::Dead;
}

// Current world position
Vector2 Goblin::GetPosition() const
{
    return position;
}

// Collision rectangle
Rectangle Goblin::GetBounds() const
{
    return {
        position.x,
        position.y,
        size,
        size
    };
}

// Update facing direction based on movement
void Goblin::UpdateDirection(Vector2 movement)
{
    // Horizontal direction takes priority.
    if (std::fabs(movement.x) > std::fabs(movement.y)) {
        direction = movement.x < 0
            ? Direction::Left
            : Direction::Right;
    }
    else if (movement.y != 0.0f) {
        direction = movement.y < 0
            ? Direction::Up
            : Direction::Down;
    }
}

// Update animation frames
void Goblin::UpdateAnimation(float dt)
{
    if (state == State::Dead) {
        return;
    }

    float framesPerSecond = 8.0f;
    int frameCount = 4;

    switch (state) {
        case State::Walking:
            framesPerSecond = 8.0f;
            frameCount = 4;
            break;

        case State::Attacking:
            framesPerSecond = 16.0f;
            frameCount = 4;
            break;

        case State::Dying:
            framesPerSecond = 4.0f;
            frameCount = 2;
            break;

        case State::Dead:
            return;
    }

    frameTimer += dt;

    float frameDuration = 1.0f / framesPerSecond;

    while (frameTimer >= frameDuration) {
        frameTimer -= frameDuration;
        currentFrame++;

        if (currentFrame >= frameCount) {
            if (state == State::Dying) {
                state = State::Dead;
                currentFrame = frameCount - 1;
                frameTimer = 0.0f;
                return;
            }

            currentFrame = 0;
        }
    }
}

// Render the goblin
void Goblin::Draw() const
{
    if (state == State::Dead) {
        return;
    }

    // Fallback if the texture could not be loaded.
    if (texture.id == 0) {
        DrawRectangleRec(GetBounds(), GREEN);
        return;
    }

    float frameWidth = texture.width / SPRITE_FRAMES;
    float frameHeight = texture.height / SPRITE_ROWS;

    int row = static_cast<int>(direction);

    switch (state) {
        case State::Walking:
            row += 4;
            break;

        case State::Attacking:
            row += 8;
            break;

        case State::Dying:
        case State::Dead:
            row += 16;
            break;
    }

    Rectangle source = {
        currentFrame * frameWidth,
        row * frameHeight,
        frameWidth,
        frameHeight
    };

    // Anchor the sprite's feet to the collision footprint.
    Rectangle destination = {
        position.x + size / 2.0f - spriteSize / 2.0f,
        position.y + size - spriteSize,
        spriteSize,
        spriteSize
    };

    DrawTexturePro(
        texture,
        source,
        destination,
        {0.0f, 0.0f},
        0.0f,
        WHITE
    );
};
