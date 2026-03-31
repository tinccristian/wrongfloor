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
            ClearBackground(Color{185, 188, 192, 255});
            player_draw(&state.player);
            if constexpr (DEBUG_GROUND)
                DrawLineEx(Vector2{0.0f, GROUND_Y}, Vector2{(float)screenWidth, GROUND_Y}, 3.0f, Color{90, 90, 90, 220});
        EndDrawing();
    }

    player_cleanup(&state.player);
    CloseWindow();
    return 0;
}
