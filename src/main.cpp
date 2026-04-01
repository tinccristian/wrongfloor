#include "raylib.h"
#include "game.h"
#include "gameplay.h"

#ifdef DEV_MODE
#include "debug.h"
#endif

int main(void)
{
    const int screenWidth  = 1280;
    const int screenHeight = 720;

    InitWindow(screenWidth, screenHeight, "wrongfloor");
    SetTargetFPS(60);

    GameState state{};
    gameplay_init(&state, screenWidth, screenHeight);

#ifdef DEV_MODE
    DebugState debug{};
    debug_init(&debug);
#endif

    while (!WindowShouldClose())
    {
        float dt = GetFrameTime();

        // ── Debug update (runs before player, may block input) ────────
        bool input_blocked = false;
#ifdef DEV_MODE
        input_blocked = debug_update(&debug, dt);
#endif

        gameplay_update(&state, dt, screenWidth, screenHeight, input_blocked);
        gameplay_prepare_draw(&state);

        BeginDrawing();
            ClearBackground(Color{30, 28, 36, 255});

            BeginMode2D(state.camera.cam);
                gameplay_draw_world(&state);
#ifdef DEV_MODE
                debug_draw_world(&debug, &state);
#endif
            EndMode2D();

#ifdef DEV_MODE
            debug_draw_ui(&debug, screenWidth, screenHeight);
#endif
        EndDrawing();
    }

    gameplay_cleanup(&state);
    CloseWindow();
    return 0;
}
