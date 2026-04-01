#include "raylib.h"
#include "game.h"

#ifdef DEV_MODE
#include "debug.h"
#endif

static void load_level(GameState *gs, int index, int screen_w, int screen_h)
{
    tilemap_unload(&gs->tilemap);
    tilemap_load(&gs->tilemap, assets_path(LEVELS[index]));

    Vector2 spawn = tilemap_get_spawn_point(&gs->tilemap);
    player_init(&gs->player, spawn);

    Vector2 centre = player_center(&gs->player);
    camera_init(&gs->camera, centre, screen_w, screen_h);
    gs->current_level = index;
}

int main(void)
{
    const int screenWidth  = 1280;
    const int screenHeight = 720;

    InitWindow(screenWidth, screenHeight, "wrongfloor");
    SetTargetFPS(60);

    GameState state{};
    audio_init(&state.audio);
    load_level(&state, 0, screenWidth, screenHeight);

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

        // ── Update ────────────────────────────────────────────────────
        PlayerSoundTriggers triggers;
        player_update(&state.player, &state.tilemap, dt, &triggers, input_blocked);

        Vector2 centre = player_center(&state.player);
        camera_update(&state.camera, centre, &state.tilemap, screenWidth, screenHeight, dt);

        // ── Audio ─────────────────────────────────────────────────────
        PlayerState ps = state.player.state;

        if (triggers.footstep_walk) audio_play_footstep(&state.audio, state.audio.snd_walk);
        if (triggers.footstep_run)  audio_play_footstep(&state.audio, state.audio.snd_run);
        if (triggers.landed)        audio_play_sfx(&state.audio, state.audio.snd_land);
        if (triggers.attacked)      audio_play_sfx_pitched(&state.audio, state.audio.snd_attack, 0.85f, 1.15f);

        if (ps != PLAYER_WALKING)   StopSound(state.audio.snd_walk);
        if (ps != PLAYER_RUNNING)   StopSound(state.audio.snd_run);
        if (ps != PLAYER_ATTACKING) StopSound(state.audio.snd_attack);

        // ── Level transition ──────────────────────────────────────────
        const TileObject *exit = tilemap_get_object(&state.tilemap, "level_exit");
        if (exit)
        {
            Rectangle exit_rect   = { exit->x, exit->y, exit->width, exit->height };
            Rectangle player_rect = {
                state.player.position.x + HITBOX_OFFSET_X,
                state.player.position.y + HITBOX_OFFSET_Y,
                HITBOX_W, HITBOX_H
            };
            if (CheckCollisionRecs(player_rect, exit_rect))
            {
                int next = (state.current_level + 1) % LEVEL_COUNT;
                player_cleanup(&state.player);
                load_level(&state, next, screenWidth, screenHeight);
            }
        }

        // ── Draw ──────────────────────────────────────────────────────
        BeginDrawing();
            ClearBackground(Color{30, 28, 36, 255});

            BeginMode2D(state.camera.cam);
                tilemap_draw_layers_prefixed(&state.tilemap, "background");
                tilemap_draw_layers_prefixed(&state.tilemap, "midground");
                player_draw(&state.player);
                tilemap_draw_layers_prefixed(&state.tilemap, "foreground");
#ifdef DEV_MODE
                debug_draw_world(&debug, &state.tilemap, &state.player);
#endif
            EndMode2D();

#ifdef DEV_MODE
            debug_draw_ui(&debug, screenWidth, screenHeight);
#endif
        EndDrawing();
    }

    player_cleanup(&state.player);
    tilemap_unload(&state.tilemap);
    audio_cleanup(&state.audio);
    CloseWindow();
    return 0;
}
