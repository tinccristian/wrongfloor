#include "player.h"
#include <cmath>

#define GAMEPAD_ID 0

void player_init(Player *player, Vector2 start_pos)
{
    player->position = start_pos;
    player->speed    = 200.0f;
    player->size     = 32.0f;
    player->color    = BLUE;
}

void player_update(Player *player, float dt)
{
    Vector2 dir = { 0.0f, 0.0f };

    // Keyboard: WASD and arrow keys
    if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))    dir.y -= 1.0f;
    if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN))  dir.y += 1.0f;
    if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))  dir.x -= 1.0f;
    if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) dir.x += 1.0f;

    // Gamepad: left analog stick
    if (IsGamepadAvailable(GAMEPAD_ID))
    {
        float ax = GetGamepadAxisMovement(GAMEPAD_ID, GAMEPAD_AXIS_LEFT_X);
        float ay = GetGamepadAxisMovement(GAMEPAD_ID, GAMEPAD_AXIS_LEFT_Y);

        // Dead zone
        if (ax * ax + ay * ay > 0.1f * 0.1f)
        {
            dir.x += ax;
            dir.y += ay;
        }

        // D-pad
        if (IsGamepadButtonDown(GAMEPAD_ID, GAMEPAD_BUTTON_LEFT_FACE_UP))    dir.y -= 1.0f;
        if (IsGamepadButtonDown(GAMEPAD_ID, GAMEPAD_BUTTON_LEFT_FACE_DOWN))  dir.y += 1.0f;
        if (IsGamepadButtonDown(GAMEPAD_ID, GAMEPAD_BUTTON_LEFT_FACE_LEFT))  dir.x -= 1.0f;
        if (IsGamepadButtonDown(GAMEPAD_ID, GAMEPAD_BUTTON_LEFT_FACE_RIGHT)) dir.x += 1.0f;
    }

    // Normalize so diagonal movement isn't faster
    float len_sq = dir.x * dir.x + dir.y * dir.y;
    if (len_sq > 1.0f)
    {
        float len = sqrtf(len_sq);
        dir.x /= len;
        dir.y /= len;
    }

    player->position.x += dir.x * player->speed * dt;
    player->position.y += dir.y * player->speed * dt;
}

void player_draw(const Player *player)
{
    float half = player->size * 0.5f;
    DrawRectangleV(
        Vector2{ player->position.x - half, player->position.y - half },
        Vector2{ player->size, player->size },
        player->color
    );
}
