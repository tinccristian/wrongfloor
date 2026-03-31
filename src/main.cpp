#include "raylib.h"
#include "game.h"

int main(void)
{
    const int screenWidth  = 1280;
    const int screenHeight = 720;

    InitWindow(screenWidth, screenHeight, "wrongfloor");
    SetTargetFPS(60);

    GameState state{};
    player_init(&state.player, Vector2{ screenWidth * 0.5f, screenHeight * 0.5f });

    while (!WindowShouldClose())
    {
        float dt = GetFrameTime();

        // Input + update
        player_update(&state.player, dt);

        // Draw
        BeginDrawing();
            ClearBackground(RAYWHITE);
            player_draw(&state.player);
        EndDrawing();
    }

    player_cleanup(&state.player);
    CloseWindow();
    return 0;
}
