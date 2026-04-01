#include "enemy.h"
#include "game.h"

// These match the player's idle spritesheet layout — update if the sheet changes.
static constexpr int   ENEMY_FRAME_W      = 16;
static constexpr int   ENEMY_FRAME_H      = 32;
static constexpr float ENEMY_SPRITE_SCALE = 3.0f;
// Dark maroon tint to distinguish enemies from the player.
static const Color     ENEMY_TINT         = { 160, 30, 50, 255 };

// Map a facing string to the sprite-sheet row and horizontal flip flag.
static void facing_to_row(const std::string& facing, int *out_row, bool *out_flip)
{
    *out_flip = false;
    if (facing == "up")    { *out_row = 4; return; }
    if (facing == "right") { *out_row = 2; return; }
    if (facing == "left")  { *out_row = 2; *out_flip = true; return; }
    *out_row = 0; // "down" or unrecognised → front-facing
}

void enemies_init(EnemyManager *em)
{
    em->sprite_sheet = LoadTexture(assets_path("character/idle.png").c_str());
    em->enemies.clear();
}

void enemies_load_from_tilemap(EnemyManager *em, const Tilemap *tm)
{
    em->enemies.clear();
    if (!tm) return;

    for (const auto& obj : tm->objects)
    {
        if (obj.name != "enemy") continue;

        Enemy e;
        e.position = { obj.x, obj.y };
        e.alive    = true;

        std::string facing = "down";
        auto it = obj.properties.find("facing");
        if (it != obj.properties.end()) facing = it->second;

        facing_to_row(facing, &e.sprite_row, &e.sprite_flip);
        em->enemies.push_back(e);
    }
}

void enemies_clear(EnemyManager *em)
{
    em->enemies.clear();
}

void enemies_update(EnemyManager *em, float dt)
{
    (void)em;
    (void)dt;
    // FUTURE: pathfinding, attack logic, aggro range checks, etc.
}

void enemies_draw(const EnemyManager *em)
{
    if (em->sprite_sheet.id == 0) return;

    float fw = (float)ENEMY_FRAME_W * ENEMY_SPRITE_SCALE;
    float fh = (float)ENEMY_FRAME_H * ENEMY_SPRITE_SCALE;

    for (const auto& enemy : em->enemies)
    {
        if (!enemy.alive) continue;

        // Source: frame 0 of the facing row; negate width to flip horizontally.
        Rectangle src = {
            0.0f,
            (float)(enemy.sprite_row * ENEMY_FRAME_H),
            enemy.sprite_flip ? -(float)ENEMY_FRAME_W : (float)ENEMY_FRAME_W,
            (float)ENEMY_FRAME_H
        };
        // Destination: centred on enemy.position.
        Rectangle dst = {
            enemy.position.x - fw * 0.5f,
            enemy.position.y - fh * 0.5f,
            fw,
            fh
        };

        DrawTexturePro(em->sprite_sheet, src, dst, Vector2{ 0.0f, 0.0f }, 0.0f, ENEMY_TINT);
    }
}

void enemies_cleanup(EnemyManager *em)
{
    if (em->sprite_sheet.id != 0) UnloadTexture(em->sprite_sheet);
    em->sprite_sheet = Texture2D{};
    em->enemies.clear();
}
