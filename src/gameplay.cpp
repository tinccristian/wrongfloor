#include "gameplay.h"
#include "collision_system.h"
#include "replay.h"

static constexpr float FOOTSTEP_SOUND_LIFETIME = 0.20f;

static void load_level(GameState *state, int index, int screen_w, int screen_h)
{
    tilemap_unload(&state->tilemap);
    tilemap_load(&state->tilemap, assets_path(LEVELS[index]));

    Vector2 spawn = tilemap_get_spawn_point(&state->tilemap);
    player_init(&state->player, spawn);
    bullets_clear(&state->bullets);

    enemies_load_from_tilemap(&state->enemies, &state->tilemap);
    effects_clear(&state->effects);

    weapons_clear(&state->weapons);
    weapons_load_from_tilemap(&state->weapons, &state->tilemap);

    camera_init(&state->camera, player_center(&state->player), screen_w, screen_h);
    state->current_level = index;

    // Fresh level footage — discard replay buffer so old frames can't leak in.
    replay_reset_buffer(&state->replay);
    sound_events_clear(&state->sound_events);
    state->time_scale         = 1.0f;
    state->death_slowmo_timer = 0.0f;
}

static void update_audio(const PlayerSoundTriggers& triggers, GameState *state)
{
    if (triggers.footstep_walk) audio_play_footstep(&state->audio, state->audio.snd_walk);
    if (triggers.footstep_run)  audio_play_footstep(&state->audio, state->audio.snd_run);
    // Shooting sounds are now handled by weapons_update.

    if (state->player.state != PLAYER_WALKING) StopSound(state->audio.snd_walk);
    if (state->player.state != PLAYER_RUNNING) StopSound(state->audio.snd_run);
}

static void emit_player_sound_events(const PlayerSoundTriggers& triggers, GameState *state)
{
    if (!triggers.footstep_run) return;
    sound_events_push(&state->sound_events, player_center(&state->player),
                      sound_events_get_footstep_radius(), FOOTSTEP_SOUND_LIFETIME,
                      SoundEventType::FOOTSTEP);
}

static void update_level_transition(GameState *state, int screen_w, int screen_h)
{
    const TileObject *exit = tilemap_get_object(&state->tilemap, "level_exit");
    if (!exit) return;

    Rectangle exit_rect   = { exit->x, exit->y, exit->width, exit->height };
    Rectangle player_rect = player_hitbox_rect(&state->player);

    if (CheckCollisionRecs(player_rect, exit_rect))
    {
        int next = (state->current_level + 1) % LEVEL_COUNT;
        player_cleanup(&state->player);
        weapons_save_held(&state->weapons);
        load_level(state, next, screen_w, screen_h);
        weapons_restore_held(&state->weapons);
    }
}

void gameplay_init(GameState *state, int screen_w, int screen_h)
{
    // audio_init is called once in main.cpp before the game loop, not here
    bullets_init(&state->bullets);
    enemies_init(&state->enemies);
    weapons_init(&state->weapons);
    replay_init(&state->replay, screen_w, screen_h);
    // EffectsSystem needs no init — its vectors are default-constructed.
    load_level(state, 0, screen_w, screen_h);
}

void gameplay_update(GameState *state, float dt, int screen_w, int screen_h, bool input_blocked)
{
    // ── Replay sequence (GLITCH → REPLAY → BLACKOUT) — driven by real dt ──
    // (gameplay_update receives scaled dt from main.cpp, but replay is UI-only.
    //  During replay time_scale == 1.0 so dt == real_dt anyway.)
    if (replay_is_active(&state->replay))
    {
        if (replay_update(&state->replay, dt))
        {
            state->player_dead = false;
            gameplay_reload_level(state, screen_w, screen_h);
        }
        return;
    }

    // ── Slow-motion dead update: effects + bullets only, no input ─────
    if (state->player_dead)
    {
        bullets_update(&state->bullets, &state->tilemap, dt);  // dt is already scaled by main.cpp
        effects_update(&state->effects, &state->tilemap, dt);
        sound_events_update(&state->sound_events, dt);
        return;
    }

    Vector2 mouse_world = GetScreenToWorld2D(state->virtual_mouse, state->camera.cam);
    PlayerSoundTriggers triggers;

    // Update order: player → weapons (fire) → bullet physics → collision → effects → enemies → camera
    player_update(&state->player, &state->tilemap, dt, mouse_world, state->virtual_mouse,
                  &triggers, input_blocked);

    weapons_update(&state->weapons, &state->bullets, &state->audio,
                   &state->tilemap, &state->enemies, &state->effects, &state->sound_events,
                   player_center(&state->player),
                   state->player.aim.direction, input_blocked, dt);

    bullets_update(&state->bullets, &state->tilemap, dt);
    collision_bullets_vs_enemies(&state->bullets, &state->enemies, &state->effects,
                                 &state->weapons);

    if (collision_bullets_vs_player(&state->bullets, &state->player, &state->effects))
    {
        state->player_dead      = true;
        state->time_scale       = 0.15f;
        state->death_slowmo_timer = 0.0f;
        return;
    }

    emit_player_sound_events(triggers, state);
    effects_update(&state->effects, &state->tilemap, dt);

    if (enemies_update(&state->enemies, &state->bullets, &state->audio,
                       &state->tilemap, &state->sound_events,
                       player_center(&state->player), dt,
                       &state->player, &state->effects))
    {
        state->player_dead      = true;
        state->time_scale       = 0.15f;
        state->death_slowmo_timer = 0.0f;
        return;
    }

    sound_events_update(&state->sound_events, dt);
    camera_update(&state->camera, player_center(&state->player), &state->tilemap,
                  screen_w, screen_h, dt);

    update_audio(triggers, state);
    update_level_transition(state, screen_w, screen_h);
}

void gameplay_prepare_draw(GameState *state)
{
    player_prepare_draw(&state->player);
}

void gameplay_draw_world(GameState *state)
{
    // Draw order: background → midground → ground effects → enemies →
    //             ground weapons → held weapon (behind player) →
    //             player → held weapon (front) → bullets → particles →
    //             foreground → weapon HUD → crosshair
    tilemap_draw_layers_prefixed(&state->tilemap, "background");
    tilemap_draw_layers_prefixed(&state->tilemap, "midground");
    effects_draw_stains(&state->effects);
    enemies_draw(&state->enemies);

    bool controller_active = (state->player.aim.active_input_mode == AIM_GAMEPAD);
    weapons_draw_ground(&state->weapons, player_center(&state->player), controller_active);
    player_draw(&state->player);
    weapons_draw_held(&state->weapons);

    bullets_draw(&state->bullets);
    effects_draw_pixels(&state->effects);
    tilemap_draw_layers_prefixed(&state->tilemap, "foreground");

    weapons_draw_hud(&state->weapons, player_center(&state->player));
    player_draw_crosshair(&state->player);
}

void gameplay_draw_hud(GameState *state, int screen_w, int screen_h)
{
    weapons_draw_ammo_screen(&state->weapons, screen_w, screen_h);
}

void gameplay_cleanup(GameState *state)
{
    player_cleanup(&state->player);
    bullets_cleanup(&state->bullets);
    enemies_cleanup(&state->enemies);
    weapons_cleanup(&state->weapons);
    replay_cleanup(&state->replay);
    sound_events_clear(&state->sound_events);
    tilemap_unload(&state->tilemap);
    audio_cleanup(&state->audio);
    // EffectsSystem holds no GPU resources — vectors free themselves.
}

void gameplay_reload_level(GameState *state, int screen_w, int screen_h)
{
    player_cleanup(&state->player);
    load_level(state, state->current_level, screen_w, screen_h);
}

void gameplay_load_level(GameState *state, int index, int screen_w, int screen_h)
{
    player_cleanup(&state->player);
    load_level(state, index, screen_w, screen_h);
}
