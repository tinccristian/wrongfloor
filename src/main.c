#include "raylib.h"

int main(void)
{
    const int screenWidth  = 1280;
    const int screenHeight = 720;

    InitWindow(screenWidth, screenHeight, "wrongfloor");
    SetTargetFPS(60);

    while (!WindowShouldClose())
    {
        BeginDrawing();
            ClearBackground(RAYWHITE);
            DrawText("wrongfloor is alive", 400, 340, 30, DARKGRAY);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}