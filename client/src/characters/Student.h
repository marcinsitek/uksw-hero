#pragma once

#include "raylib.h"
#include "../systems/GameplayEvent.h"
#include <vector>

class Goblin;

class Student {
public:
    // The caller owns the texture and keeps it loaded while Student uses it.
    Student(Vector2 startPosition, Texture2D texture, GameplayEventHandler onEvent = {});

    void Update(float dt, const std::vector<Rectangle>& obstacles, std::vector<Goblin>& goblins);
    void Draw() const;

    Vector2 GetPosition() const;
    Rectangle GetBounds() const;
    int GetHealth() const;
    enum class DamageSource { Attack, Debug };
    void TakeDamage(int damage, DamageSource source = DamageSource::Attack);
    bool IsDead() const;
    bool HasDeathAnimationFinished() const;

private:
    Vector2 position;
    Texture2D texture;
    GameplayEventHandler onEvent;

    float speed = 110.0f;
    float size = 24.0f;
    int health = 100;
    int attackDamage = 40;
    float attackRange = 32.0f;

    enum class Direction { Down, Up, Left, Right };
    enum class Animation { Idle, Walk, Attack, Hurt, Dying, Dead };

    Direction direction = Direction::Down;
    Animation animation = Animation::Idle;

    int currentFrame = 0;
    float frameTimer = 0.0f;

    void HandleInput(std::vector<Goblin>& goblins);
    Goblin* FindAttackTarget(std::vector<Goblin>& goblins) const;
    void Attack(Goblin& goblin);
    void UpdateAnimation(float dt);
    void Move(float dt, const std::vector<Rectangle>& obstacles);
};
