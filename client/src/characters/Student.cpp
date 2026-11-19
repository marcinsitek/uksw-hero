#include "Student.h"
#include "Goblin.h"
#include "raymath.h"

#include <utility>

namespace {
constexpr float SPRITE_SIZE = 64.0f;

Vector2 ReadMovement()
{
    Vector2 movement = {};
    if (IsKeyDown(KEY_W)) movement.y -= 1;
    if (IsKeyDown(KEY_S)) movement.y += 1;
    if (IsKeyDown(KEY_A)) movement.x -= 1;
    if (IsKeyDown(KEY_D)) movement.x += 1;
    return movement;
}
}

Student::Student(Vector2 startPosition, Texture2D texture, GameplayEventHandler onEvent)
    : position(startPosition), texture(texture), onEvent(std::move(onEvent))
{
}

void Student::Update(float dt, const std::vector<Rectangle>& obstacles, std::vector<Goblin>& goblins)
{
    HandleInput(goblins);
    Move(dt, obstacles);
    UpdateAnimation(dt);
}

void Student::HandleInput(std::vector<Goblin>& goblins)
{
    // Attacks, hurt, and death keep their starting direction and stop movement.
    if (animation == Animation::Idle || animation == Animation::Walk) {
        const Vector2 movement = ReadMovement();
        const Direction previousDirection = direction;
        const Animation previousAnimation = animation;
        const bool moving = Vector2LengthSqr(movement) > 0;
        if (moving) {
            // Horizontal facing takes priority for diagonal movement.
            if (movement.x < 0) direction = Direction::Left;
            else if (movement.x > 0) direction = Direction::Right;
            else if (movement.y < 0) direction = Direction::Up;
            else direction = Direction::Down;
        }
        animation = moving ? Animation::Walk : Animation::Idle;
        if (previousDirection != direction || previousAnimation != animation) {
            currentFrame = 0;
            frameTimer = 0.0f;
        }

        if (IsKeyPressed(KEY_SPACE)) {
            animation = Animation::Attack;
            currentFrame = 0;
            frameTimer = 0.0f;
            // One hit per swing; the attack animation prevents another swing.
            if (Goblin* target = FindAttackTarget(goblins)) {
                Attack(*target);
            }
        }
    }

    // Manual death trigger for debugging.
    if (IsKeyPressed(KEY_P)) TakeDamage(health, DamageSource::Debug);
}

Goblin* Student::FindAttackTarget(std::vector<Goblin>& goblins) const
{
    Vector2 facing = {};
    switch (direction) {
        case Direction::Down: facing.y = 1.0f; break;
        case Direction::Up: facing.y = -1.0f; break;
        case Direction::Left: facing.x = -1.0f; break;
        case Direction::Right: facing.x = 1.0f; break;
    }

    const Vector2 center = {position.x + size / 2, position.y + size / 2};
    Goblin* target = nullptr;
    float bestAlignment = -1.0f;
    float bestDistance = attackRange;
    for (Goblin& goblin : goblins) {
        if (!goblin.IsAlive()) continue;

        const Rectangle bounds = goblin.GetBounds();
        const Vector2 goblinCenter = {bounds.x + bounds.width / 2, bounds.y + bounds.height / 2};
        const Vector2 offset = Vector2Subtract(goblinCenter, center);
        const float distance = Vector2Length(offset);
        if (distance > attackRange) continue;

        // Require a target within 45 degrees of the facing direction.
        // A goblin at exactly the same center is reachable from any direction.
        const float alignment = (distance > 0.0f) ? Vector2DotProduct(facing, offset) / distance : 1.0f;
        if (alignment < 0.70710678f) continue;
        if (alignment > bestAlignment || (alignment == bestAlignment && distance < bestDistance)) {
            target = &goblin;
            bestAlignment = alignment;
            bestDistance = distance;
        }
    }
    return target;
}

void Student::Attack(Goblin& goblin)
{
    goblin.TakeDamage(attackDamage);
}

void Student::Move(float dt, const std::vector<Rectangle>& obstacles)
{
    if (animation != Animation::Walk) return;

    const Vector2 movement = Vector2Normalize(ReadMovement());
    const Vector2 nextPosition = {
        position.x + movement.x * speed * dt,
        position.y + movement.y * speed * dt
    };
    const Rectangle nextBounds = {nextPosition.x, nextPosition.y, size, size};
    for (const Rectangle& obstacle : obstacles) {
        if (CheckCollisionRecs(nextBounds, obstacle)) return;
    }
    position = nextPosition;
}

void Student::UpdateAnimation(float dt)
{
    if (animation != Animation::Walk && animation != Animation::Attack &&
        animation != Animation::Hurt && animation != Animation::Dying) return;

    const int frameCount = (animation == Animation::Dying) ? 2 : 4;
    const float framesPerSecond = (animation == Animation::Attack) ? 16.0f :
                                  (animation == Animation::Dying) ? 4.0f : 8.0f;
    const float frameDuration = 1.0f / framesPerSecond;
    frameTimer += dt;
    while (frameTimer >= frameDuration) {
        frameTimer -= frameDuration;
        currentFrame++;
        if (currentFrame >= frameCount) {
            if (animation == Animation::Dying) {
                currentFrame = frameCount - 1;
                animation = Animation::Dead;
            } else if (animation == Animation::Attack || animation == Animation::Hurt) {
                currentFrame = 0;
                animation = Animation::Idle;
            } else {
                currentFrame = 0;
                continue;
            }
            frameTimer = 0.0f;
            break;
        }
    }
}

void Student::Draw() const
{
    if (texture.id == 0) return;

    // Sheet rows: idle 0-3, walk 4-7, attack 8-11, hurt 12-15, die 16-19.
    int row = static_cast<int>(direction);
    if (animation == Animation::Walk) row += 4;
    else if (animation == Animation::Attack) row += 8;
    else if (animation == Animation::Hurt) row += 12;
    else if (animation == Animation::Dying || animation == Animation::Dead) row += 16;

    const float frameWidth = texture.width / 4.0f;
    const float frameHeight = texture.height / 20.0f;
    const Rectangle frameRec = {
        currentFrame * frameWidth, row * frameHeight, frameWidth, frameHeight
    };
    // Anchor the sprite's feet to the collision footprint.
    const Rectangle destination = {
        position.x + size / 2 - SPRITE_SIZE / 2,
        position.y + size - SPRITE_SIZE, SPRITE_SIZE, SPRITE_SIZE
    };
    DrawTexturePro(texture, frameRec, destination, {0, 0}, 0.0f, WHITE);
}

Vector2 Student::GetPosition() const
{
    return position;
}

Rectangle Student::GetBounds() const
{
    return {position.x, position.y, size, size};
}

void Student::TakeDamage(int damage, DamageSource source)
{
    if (damage <= 0 || IsDead()) return;

    health = (damage >= health) ? 0 : health - damage;
    animation = (health == 0) ? Animation::Dying : Animation::Hurt;
    currentFrame = 0;
    frameTimer = 0.0f;

    if (onEvent) {
        if (source == DamageSource::Attack) onEvent(GameplayEvent::PlayerAttacked);
        if (health == 0) onEvent(GameplayEvent::PlayerDied);
    }
}

bool Student::IsDead() const
{
    // Lethal damage counts as dead while the death animation finishes playing.
    return health == 0;
}

bool Student::HasDeathAnimationFinished() const
{
    return animation == Animation::Dead;
}

int Student::GetHealth() const
{
    return health;
}
