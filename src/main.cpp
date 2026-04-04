#include "raylib.h"
#include "game.h"
#include "gameplay.h"
#include "pause_menu.h"
#include "replay.h"

#ifdef DEV_MODE
#include "debug.h"
#include <filesystem>
#include <algorithm>
#include <cctype>

// Scan assets/levels/ for .tmj files and return filenames (not full paths).
// Called at startup and on each loadLevel execution to pick up new files.
static std::vector<std::string> scan_level_files()
{
    std::vector<std::string> files;
    std::string levels_dir = assets_path("levels/");
    std::error_code ec;
    for (auto& entry : std::filesystem::directory_iterator(levels_dir, ec))
    {
        if (ec) break;
        if (entry.path().extension() == ".tmj")
            files.push_back(entry.path().filename().string());
    }
    std::sort(files.begin(), files.end(), [](const std::string& a, const std::string& b) {
        std::string la = a, lb = b;
        for (char& c : la) c = (char)std::tolower((unsigned char)c);
        for (char& c : lb) c = (char)std::tolower((unsigned char)c);
        return la < lb;
    });
    return files;
}

static void register_load_level(DebugState *debug, GameState *state,
                                 int screen_w, int screen_h)
{
    debug_register_command(debug, "loadLevel",
        "Load a level by filename from assets/levels/. Usage: loadLevel level_01.tmj",

        // ── Execute callback ─────────────────────────────────────────
        [state, screen_w, screen_h](DebugState *ds, const std::string& args) {
            if (args.empty()) {
                debug_print(ds, "Usage: loadLevel <filename.tmj>");
                return;
            }

            // Rescan so newly added levels are found without restart.
            std::vector<std::string> files = scan_level_files();

            // Case-insensitive match against scanned filenames.
            std::string arg_lower = args;
            for (char& c : arg_lower) c = (char)std::tolower((unsigned char)c);

            std::string matched;
            for (const auto& f : files) {
                std::string fl = f;
                for (char& c : fl) c = (char)std::tolower((unsigned char)c);
                if (fl == arg_lower) { matched = f; break; }
            }

            if (matched.empty()) {
                debug_print(ds, "Error: '" + args + "' not found in assets/levels/");
                return;
            }

            // Build the relative path and reload.
            std::string rel = "levels/" + matched;
            player_cleanup(&state->player);
            tilemap_unload(&state->tilemap);
            tilemap_load(&state->tilemap, assets_path(rel));
            Vector2 spawn = tilemap_get_spawn_point(&state->tilemap);
            player_init(&state->player, spawn);
            bullets_clear(&state->bullets);
            enemies_load_from_tilemap(&state->enemies, &state->tilemap);
            effects_clear(&state->effects);
            camera_init(&state->camera, player_center(&state->player), screen_w, screen_h);

            debug_print(ds, "Loaded: " + matched);
        },

        // ── Completion callback ──────────────────────────────────────
        [](const std::string& prefix) -> std::vector<std::string> {
            std::vector<std::string> files = scan_level_files();
            std::string prefix_lower = prefix;
            for (char& c : prefix_lower) c = (char)std::tolower((unsigned char)c);

            std::vector<std::string> matches;
            for (const auto& f : files) {
                std::string fl = f;
                for (char& c : fl) c = (char)std::tolower((unsigned char)c);
                if (fl.rfind(prefix_lower, 0) == 0)
                    matches.push_back(f);
            }
            return matches; // already sorted by scan_level_files
        }
    );
}
#endif

int main(void)
{
    const int screenWidth  = 1280;
    const int screenHeight = 720;

    InitWindow(screenWidth, screenHeight, "wrongfloor");
    SetTargetFPS(60);
    SetExitKey(0); // Escape is handled manually

    GameState state{};
    gameplay_init(&state, screenWidth, screenHeight);

    PauseMenu pause_menu{};
    pause_menu_init(&pause_menu);

#ifdef DEV_MODE
    DebugState debug{};
    debug_init(&debug);
    register_load_level(&debug, &state, screenWidth, screenHeight);
#endif

    while (!WindowShouldClose())
    {
        float dt = GetFrameTime();

        // ── Debug update (runs first, may consume Escape) ─────────────
        bool input_blocked   = false;
        bool escape_consumed = false;

#ifdef DEV_MODE
        bool was_console_open = debug.console_open;
        input_blocked = debug_update(&debug, dt);
        // Escape closes the console — don't also trigger pause this frame.
        if (was_console_open && !debug.console_open)
            escape_consumed = true;
#endif

        // ── Pause toggle ──────────────────────────────────────────────
        bool pause_pressed = !escape_consumed &&
            (IsKeyPressed(KEY_ESCAPE) ||
             IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_RIGHT));

        if (pause_pressed)
        {
            state.paused = !state.paused;
            if (state.paused)
                pause_menu_init(&pause_menu);
        }

        // ── Update ────────────────────────────────────────────────────
        if (state.paused)
        {
            PauseAction action = pause_menu_update(&pause_menu, &state.audio, dt, screenWidth, screenHeight);
            switch (action)
            {
                case PauseAction::Resume:
                    state.paused = false;
                    break;
                case PauseAction::Restart:
                    gameplay_reload_level(&state, screenWidth, screenHeight);
                    state.paused = false;
                    break;
                case PauseAction::MainMenu:
                    gameplay_load_level(&state, 0, screenWidth, screenHeight);
                    state.paused = false;
                    break;
                case PauseAction::Exit:
                    goto cleanup;
                case PauseAction::None:
                    break;
            }
        }
        else
        {
            gameplay_update(&state, dt, screenWidth, screenHeight,
                            input_blocked || state.paused);
        }

        gameplay_prepare_draw(&state);

        // ── Render gameplay into capture texture ──────────────────────
        BeginTextureMode(state.replay.capture_rt);
            ClearBackground(Color{30, 28, 36, 255});
            BeginMode2D(state.camera.cam);
                gameplay_draw_world(&state);
#ifdef DEV_MODE
                debug_draw_world(&debug, &state);
#endif
            EndMode2D();
            gameplay_draw_hud(&state, screenWidth, screenHeight);
        EndTextureMode();

        // Feed capture into replay buffer (only during live play).
        if (!state.player_dead && !replay_is_active(&state.replay))
            replay_capture_frame(&state.replay);

        // ── Draw to screen ────────────────────────────────────────────
        BeginDrawing();
            if (replay_is_active(&state.replay))
            {
                replay_draw(&state.replay, screenWidth, screenHeight);
            }
            else
            {
                // Blit capture_rt to the backbuffer; negate source height to
                // flip the render texture's Y-flipped storage to screen orientation.
                ClearBackground(Color{30, 28, 36, 255});
                Rectangle src = { 0.0f, 0.0f,
                    (float)screenWidth, -(float)screenHeight };
                Rectangle dst = { 0.0f, 0.0f,
                    (float)screenWidth, (float)screenHeight };
                DrawTexturePro(state.replay.capture_rt.texture,
                               src, dst, { 0.0f, 0.0f }, 0.0f, WHITE);
            }

            if (state.paused)
                pause_menu_draw(&pause_menu, screenWidth, screenHeight);

#ifdef DEV_MODE
            debug_draw_ui(&debug, &state, screenWidth, screenHeight);
#endif
        EndDrawing();
    }

cleanup:
    gameplay_cleanup(&state);
    CloseWindow();
    return 0;
}
