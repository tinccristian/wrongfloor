#include "raylib.h"
#include "game.h"
#include "gameplay.h"
#include "pause_menu.h"
#include "main_menu.h"
#include "options_menu.h"
#include "replay.h"
#include "settings.h"

#ifdef DEV_MODE
#include "debug.h"
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <sstream>

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

static std::string trim_copy(const std::string& s)
{
    size_t a = s.find_first_not_of(" \t");
    size_t b = s.find_last_not_of(" \t");
    if (a == std::string::npos) return "";
    return s.substr(a, b - a + 1);
}

static bool try_parse_float_arg(const std::string& s, float *out_value)
{
    std::string trimmed = trim_copy(s);
    if (trimmed.empty()) return false;

    char *end = nullptr;
    float value = std::strtof(trimmed.c_str(), &end);
    if (end == trimmed.c_str() || *end != '\0')
        return false;

    if (out_value) *out_value = value;
    return true;
}

static void register_ai_debug_commands(DebugState *debug, GameState *state)
{
    debug_register_command(debug, "ai_state_labels",
        "Toggle enemy AI state labels above enemies",
        [](DebugState *ds, const std::string&) {
            ds->show_enemy_state_labels = !ds->show_enemy_state_labels;
            debug_print(ds, std::string("ai_state_labels: ") +
                              (ds->show_enemy_state_labels ? "ON" : "OFF"));
        });

    debug_register_command(debug, "ai_target_debug",
        "Toggle enemy investigation/last-known target lines and markers",
        [](DebugState *ds, const std::string&) {
            ds->show_ai_targets = !ds->show_ai_targets;
            debug_print(ds, std::string("ai_target_debug: ") +
                              (ds->show_ai_targets ? "ON" : "OFF"));
        });

    debug_register_command(debug, "sound_debug",
        "Toggle active sound-event circle rendering",
        [](DebugState *ds, const std::string&) {
            ds->show_sound_events = !ds->show_sound_events;
            debug_print(ds, std::string("sound_debug: ") +
                              (ds->show_sound_events ? "ON" : "OFF"));
        });

    debug_register_command(debug, "sound_clear",
        "Clear all active sound events",
        [state](DebugState *ds, const std::string&) {
            sound_events_clear(&state->sound_events);
            debug_print(ds, "sound_clear: cleared");
        });

    debug_register_command(debug, "sound_spawn",
        "Spawn a test sound at the player. Usage: sound_spawn [radius]",
        [state](DebugState *ds, const std::string& args) {
            if (state->mode != GameStateMode::PLAYING)
            {
                debug_print(ds, "sound_spawn: only available during gameplay");
                return;
            }

            float radius = sound_events_get_impact_radius();
            float parsed = 0.0f;
            if (!trim_copy(args).empty())
            {
                if (!try_parse_float_arg(args, &parsed))
                {
                    debug_print(ds, "Usage: sound_spawn [radius]");
                    return;
                }
                radius = parsed;
            }

            Vector2 pos = player_center(&state->player);
            sound_events_push(&state->sound_events, pos, radius, 0.30f, SoundEventType::GENERIC);

            char buf[96];
            std::snprintf(buf, sizeof(buf), "sound_spawn: radius %.1f at player", radius);
            debug_print(ds, buf);
        });

    debug_register_command(debug, "ai_search_time",
        "Print or set enemy search duration. Usage: ai_search_time [seconds]",
        [](DebugState *ds, const std::string& args) {
            float parsed = 0.0f;
            if (trim_copy(args).empty())
            {
                char buf[96];
                std::snprintf(buf, sizeof(buf), "ai_search_time: %.2f", enemies_get_search_time());
                debug_print(ds, buf);
                return;
            }

            if (!try_parse_float_arg(args, &parsed))
            {
                debug_print(ds, "Usage: ai_search_time [seconds]");
                return;
            }

            enemies_set_search_time(parsed);
            char buf[96];
            std::snprintf(buf, sizeof(buf), "ai_search_time set to %.2f", enemies_get_search_time());
            debug_print(ds, buf);
        });

    debug_register_command(debug, "ai_memory_time",
        "Print or set enemy memory duration. Usage: ai_memory_time [seconds]",
        [](DebugState *ds, const std::string& args) {
            float parsed = 0.0f;
            if (trim_copy(args).empty())
            {
                char buf[96];
                std::snprintf(buf, sizeof(buf), "ai_memory_time: %.2f", enemies_get_memory_time());
                debug_print(ds, buf);
                return;
            }

            if (!try_parse_float_arg(args, &parsed))
            {
                debug_print(ds, "Usage: ai_memory_time [seconds]");
                return;
            }

            enemies_set_memory_time(parsed);
            char buf[96];
            std::snprintf(buf, sizeof(buf), "ai_memory_time set to %.2f", enemies_get_memory_time());
            debug_print(ds, buf);
        });

    debug_register_command(debug, "sound_range",
        "Print or set sound radii. Usage: sound_range [footstep|rifle|deagle|impact] [value]",
        [](DebugState *ds, const std::string& args) {
            std::istringstream ss(args);
            std::string type;
            std::string value_token;
            ss >> type >> value_token;

            if (type.empty())
            {
                char buf[160];
                std::snprintf(buf, sizeof(buf),
                    "sound_range: footstep=%.1f rifle=%.1f deagle=%.1f impact=%.1f",
                    sound_events_get_footstep_radius(),
                    sound_events_get_rifle_radius(),
                    sound_events_get_deagle_radius(),
                    sound_events_get_impact_radius());
                debug_print(ds, buf);
                return;
            }

            auto print_single = [&](const char *name, float value) {
                char buf[96];
                std::snprintf(buf, sizeof(buf), "sound_range %s: %.1f", name, value);
                debug_print(ds, buf);
            };

            auto set_single = [&](const char *name, float *current,
                                  void (*setter)(float), float parsed_value) {
                (void)current;
                setter(parsed_value);
                char buf[96];
                std::snprintf(buf, sizeof(buf), "sound_range %s set to %.1f", name, parsed_value);
                debug_print(ds, buf);
            };

            float parsed = 0.0f;
            bool has_value = !value_token.empty();
            if (has_value && !try_parse_float_arg(value_token, &parsed))
            {
                debug_print(ds, "Usage: sound_range [footstep|rifle|deagle|impact] [value]");
                return;
            }

            if (type == "footstep")
            {
                if (has_value) set_single("footstep", nullptr, sound_events_set_footstep_radius, parsed);
                else print_single("footstep", sound_events_get_footstep_radius());
            }
            else if (type == "rifle")
            {
                if (has_value) set_single("rifle", nullptr, sound_events_set_rifle_radius, parsed);
                else print_single("rifle", sound_events_get_rifle_radius());
            }
            else if (type == "deagle")
            {
                if (has_value) set_single("deagle", nullptr, sound_events_set_deagle_radius, parsed);
                else print_single("deagle", sound_events_get_deagle_radius());
            }
            else if (type == "impact")
            {
                if (has_value) set_single("impact", nullptr, sound_events_set_impact_radius, parsed);
                else print_single("impact", sound_events_get_impact_radius());
            }
            else
            {
                debug_print(ds, "Usage: sound_range [footstep|rifle|deagle|impact] [value]");
            }
        },
        [](const std::string& prefix) -> std::vector<std::string> {
            const std::vector<std::string> types = { "footstep", "rifle", "deagle", "impact" };
            std::vector<std::string> matches;
            std::string lower = prefix;
            for (char& c : lower) c = (char)std::tolower((unsigned char)c);
            for (const auto& type : types)
            {
                std::string type_lower = type;
                for (char& c : type_lower) c = (char)std::tolower((unsigned char)c);
                if (type_lower.rfind(lower, 0) == 0)
                    matches.push_back(type);
            }
            return matches;
        });

    debug_register_command(debug, "ai_reset",
        "Reset enemy investigation state and clear active sound events",
        [state](DebugState *ds, const std::string&) {
            enemies_reset_perception(&state->enemies);
            sound_events_clear(&state->sound_events);
            debug_print(ds, "ai_reset: enemies set to IDLE and sound events cleared");
        });
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
            sound_events_clear(&state->sound_events);
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
    int screenWidth  = 1280;
    int screenHeight = 720;

    InitWindow(screenWidth, screenHeight, "wrongfloor");
    SetExitKey(0); // Escape is handled manually
    SetTargetFPS(60);

    GameState state{};
    settings_load(&state.settings);

    // Apply display settings from saved config (updates screenWidth/screenHeight)
    settings_apply_display(&state.settings, &screenWidth, &screenHeight);

    // Virtual resolution render target — all game drawing goes here at 1280×720,
    // then this is scaled/letterboxed onto the actual window each frame.
    RenderTexture2D virtual_rt = LoadRenderTexture(VIRTUAL_W, VIRTUAL_H);

    // Initialize audio (needed for menu sounds)
    audio_init(&state.audio);

    // Apply loaded settings to audio
    audio_set_master_volume(&state.audio, state.settings.master_volume);
    audio_set_sfx_volume(&state.audio, state.settings.sfx_volume);
    audio_set_music_volume(&state.audio, state.settings.music_volume);

    MainMenu main_menu{};
    main_menu_init(&main_menu);

    PauseMenu pause_menu{};
    OptionsMenu options_menu{};

#ifdef DEV_MODE
    DebugState debug{};
    debug_init(&debug);
    register_ai_debug_commands(&debug, &state);
    register_load_level(&debug, &state, VIRTUAL_W, VIRTUAL_H);
#endif

    // Start at main menu
    state.mode = GameStateMode::MAIN_MENU;

    while (!WindowShouldClose())
    {
        float real_dt = GetFrameTime();
        float dt = real_dt * state.time_scale;

        // ── Virtual screen transform (reused for mouse and final blit) ─
        float vscale    = std::min(screenWidth  / (float)VIRTUAL_W,
                                   screenHeight / (float)VIRTUAL_H);
        float voffset_x = (screenWidth  - VIRTUAL_W * vscale) * 0.5f;
        float voffset_y = (screenHeight - VIRTUAL_H * vscale) * 0.5f;

        // Transform actual mouse into virtual 1280×720 space
        Vector2 raw_mouse = GetMousePosition();
        state.virtual_mouse = {
            (raw_mouse.x - voffset_x) / vscale,
            (raw_mouse.y - voffset_y) / vscale
        };

        // ── Debug update (runs first, may consume Escape) ─────────────
        bool input_blocked   = false;
        bool escape_consumed = false;

#ifdef DEV_MODE
        bool was_console_open = debug.console_open;
        input_blocked = debug_update(&debug, dt);
        if (was_console_open && !debug.console_open)
            escape_consumed = true;
#endif

        // ── Update based on game state ────────────────────────────────
        switch (state.mode)
        {
            case GameStateMode::MAIN_MENU:
            {
                MainMenuAction action = main_menu_update(&main_menu, &state.audio, dt,
                                                         VIRTUAL_W, VIRTUAL_H,
                                                         state.virtual_mouse);
                switch (action)
                {
                    case MainMenuAction::Play:
                        gameplay_init(&state, VIRTUAL_W, VIRTUAL_H);
                        state.mode = GameStateMode::PLAYING;
                        break;
                    case MainMenuAction::Options:
                        options_menu_init(&options_menu, OptionsSection::Audio, &state.settings, &state.audio);
                        state.mode = GameStateMode::OPTIONS_MAIN;
                        break;
                    case MainMenuAction::Quit:
                        goto cleanup;
                    case MainMenuAction::None:
                        break;
                }
                break;
            }

            case GameStateMode::PLAYING:
            {
                // Drive death slow-mo timer in real time; trigger replay once 1s has elapsed.
                static constexpr float SLOWMO_DURATION = 0.2f;
                if (state.player_dead && !replay_is_active(&state.replay))
                {
                    state.death_slowmo_timer += real_dt;
                    if (state.death_slowmo_timer >= SLOWMO_DURATION)
                    {
                        state.time_scale = 1.0f;
                        replay_trigger(&state.replay);
                    }
                }

                // Pause toggle — blocked while replay is playing
                bool pause_pressed = !escape_consumed &&
                    !replay_is_active(&state.replay) &&
                    (IsKeyPressed(KEY_ESCAPE) ||
                     IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_RIGHT));

                if (pause_pressed)
                {
                    state.paused = !state.paused;
                    if (state.paused)
                        pause_menu_init(&pause_menu);
                }

                // Update gameplay or pause menu
                if (state.paused)
                {
                    PauseAction action = pause_menu_update(&pause_menu, &state.audio, dt,
                                                           VIRTUAL_W, VIRTUAL_H,
                                                           state.virtual_mouse);
                    switch (action)
                    {
                        case PauseAction::Resume:
                            state.paused = false;
                            break;
                        case PauseAction::Restart:
                            gameplay_reload_level(&state, VIRTUAL_W, VIRTUAL_H);
                            state.paused = false;
                            break;
                        case PauseAction::Options:
                            options_menu_init(&options_menu, OptionsSection::Audio, &state.settings, &state.audio);
                            state.mode = GameStateMode::OPTIONS_PAUSE;
                            break;
                        case PauseAction::MainMenu:
                            gameplay_cleanup(&state);
                            main_menu_init(&main_menu);
                            state.mode = GameStateMode::MAIN_MENU;
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
                    gameplay_update(&state, dt, VIRTUAL_W, VIRTUAL_H,
                                    input_blocked || state.paused);
                }

                break;
            }

            case GameStateMode::OPTIONS_MAIN:
            {
                // options_menu still receives actual screen size pointers so it can resize the window
                OptionsAction action = options_menu_update(&options_menu, dt, &screenWidth, &screenHeight);
                if (action == OptionsAction::Back)
                {
                    settings_save(&state.settings);
                    audio_set_master_volume(&state.audio, state.settings.master_volume);
                    audio_set_sfx_volume(&state.audio, state.settings.sfx_volume);
                    audio_set_music_volume(&state.audio, state.settings.music_volume);
                    main_menu_init(&main_menu);
                    state.mode = GameStateMode::MAIN_MENU;
                }
                break;
            }

            case GameStateMode::OPTIONS_PAUSE:
            {
                OptionsAction action = options_menu_update(&options_menu, dt, &screenWidth, &screenHeight);
                if (action == OptionsAction::Back)
                {
                    settings_save(&state.settings);
                    audio_set_master_volume(&state.audio, state.settings.master_volume);
                    audio_set_sfx_volume(&state.audio, state.settings.sfx_volume);
                    audio_set_music_volume(&state.audio, state.settings.music_volume);
                    state.mode = GameStateMode::PLAYING;
                    // state.paused remains true, so pause menu will show again
                }
                break;
            }
        }

        BeginDrawing();
        ClearBackground(BLACK);

        // ── Step 1: render world into capture_rt (sequential, not nested) ──
        if (state.mode == GameStateMode::PLAYING)
        {
            gameplay_prepare_draw(&state);

            BeginTextureMode(state.replay.capture_rt);
                ClearBackground(Color{30, 28, 36, 255});
                BeginMode2D(state.camera.cam);
                    gameplay_draw_world(&state);
#ifdef DEV_MODE
                    debug_draw_world(&debug, &state);
#endif
                EndMode2D();
                gameplay_draw_hud(&state, VIRTUAL_W, VIRTUAL_H);
            EndTextureMode();  // back to screen FBO

            const bool fatal_frame =
                state.player_dead &&
                state.death_slowmo_timer <= 0.0f &&
                !state.replay.snapshot_valid;

            const bool allow_replay_capture =
                !replay_is_active(&state.replay) &&
                (!state.player_dead || fatal_frame);

            if (allow_replay_capture)
                replay_capture_frame(&state.replay, real_dt, fatal_frame);

            if (fatal_frame)
                replay_snapshot(&state.replay);
        }

        // ── Step 2: compose into virtual_rt (sequential, not nested) ───
        BeginTextureMode(virtual_rt);

        if (state.mode == GameStateMode::PLAYING)
        {
            if (replay_is_active(&state.replay))
            {
                replay_draw(&state.replay, VIRTUAL_W, VIRTUAL_H);
            }
            else
            {
                ClearBackground(Color{30, 28, 36, 255});
                Rectangle src = { 0.0f, 0.0f,
                    (float)VIRTUAL_W, -(float)VIRTUAL_H };
                Rectangle dst = { 0.0f, 0.0f,
                    (float)VIRTUAL_W, (float)VIRTUAL_H };
                DrawTexturePro(state.replay.capture_rt.texture,
                               src, dst, { 0.0f, 0.0f }, 0.0f, WHITE);
            }

            if (state.paused)
                pause_menu_draw(&pause_menu, VIRTUAL_W, VIRTUAL_H);
        }
        else if (state.mode == GameStateMode::MAIN_MENU)
        {
            ClearBackground(Color{30, 28, 36, 255});
            main_menu_draw(&main_menu, VIRTUAL_W, VIRTUAL_H);
        }
        else if (state.mode == GameStateMode::OPTIONS_MAIN || state.mode == GameStateMode::OPTIONS_PAUSE)
        {
            ClearBackground(Color{30, 28, 36, 255});
            options_menu_draw(&options_menu, VIRTUAL_W, VIRTUAL_H);
        }

#ifdef DEV_MODE
        debug_draw_ui(&debug, &state, VIRTUAL_W, VIRTUAL_H);
#endif

        if (state.settings.show_fps)
            DrawFPS(VIRTUAL_W - 90, 10);

        EndTextureMode();  // back to screen FBO

        // ── Step 3: letterbox virtual_rt onto the actual window ─────────
        Rectangle src = { 0.0f, 0.0f, (float)VIRTUAL_W, -(float)VIRTUAL_H };
        Rectangle dst = { voffset_x, voffset_y, VIRTUAL_W * vscale, VIRTUAL_H * vscale };
        DrawTexturePro(virtual_rt.texture, src, dst, { 0.0f, 0.0f }, 0.0f, WHITE);

        EndDrawing();
    }

cleanup:
    settings_save(&state.settings);
    main_menu_cleanup(&main_menu);
    if (state.mode == GameStateMode::PLAYING)
        gameplay_cleanup(&state);
    audio_cleanup(&state.audio);
    UnloadRenderTexture(virtual_rt);
    CloseWindow();
    return 0;
}
