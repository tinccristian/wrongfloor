#include "gameplay.h"
#include "collision_system.h"

static void load_level(GameState *state, int index, int screen_w, int screen_h)
{
    tilemap_unload(&state->tilemap);
    tilemap_load(&state->tilemap, assets_path(LEVELS[index]));

    Vector2 spawn = tilemap_get_spawn_point(&state->tilemap);
    player_init(&state->player, spawn);
    bullets_clear(&state->bullets);

    enemies_load_from_tilemap(&state->enemies, &state->tilemap);
    effects_clear(&state->effects);

    camera_init(&state->camera, player_center(&state->player), screen_w, screen_h);
    state->current_level = index;
}

static void update_audio(const PlayerSoundTriggers& triggers, GameState *state)
{
    if (triggers.footstep_walk) audio_play_footstep(&state->audio, state->audio.snd_walk);
    if (triggers.footstep_run)  audio_play_footstep(&state->audio, state->audio.snd_run);
    if (triggers.attacked)
    {
        audio_play_sfx_pitched(&state->audio, state->audio.snd_attack, 0.95f, 1.05f);
        bullets_spawn(&state->bullets, triggers.attack_origin, triggers.attack_direction);
    }

    if (state->player.state != PLAYER_WALKING) StopSound(state->audio.snd_walk);
    if (state->player.state != PLAYER_RUNNING) StopSound(state->audio.snd_run);
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
        load_level(state, next, screen_w, screen_h);
    }
}

void gameplay_init(GameState *state, int screen_w, int screen_h)
{
    audio_init(&state->audio);
    bullets_init(&state->bullets);
    enemies_init(&state->enemies);
    // EffectsSystem needs no init — its vectors are default-constructed.
    load_level(state, 0, screen_w, screen_h);
}

void gameplay_update(GameState *state, float dt, int screen_w, int screen_h, bool input_blocked)
{
    Vector2 mouse_world = GetScreenToWorld2D(GetMousePosition(), state->camera.cam);
    PlayerSoundTriggers triggers;

    // Update order: input/player → projectiles → collision → effects → enemies → camera
    player_update(&state->player, &state->tilemap, dt, mouse_world, &triggers, input_blocked);
    bullets_update(&state->bullets, dt);
    collision_bullets_vs_enemies(&state->bullets, &state->enemies, &state->effects);
    effects_update(&state->effects, &state->tilemap, dt);
    enemies_update(&state->enemies, dt);
    camera_update(&state->camera, player_center(&state->player), &state->tilemap, screen_w, screen_h, dt);

    update_audio(triggers, state);
    update_level_transition(state, screen_w, screen_h);
    // FUTURE: gate the above behind an AppState check for pause/menu/game-over.
}

void gameplay_prepare_draw(GameState *state)
{
    player_prepare_draw(&state->player);
}

void gameplay_draw_world(GameState *state)
{
    // Draw order: background → midground → ground effects → enemies →
    //             player → projectiles → particles → foreground → crosshair
    tilemap_draw_layers_prefixed(&state->tilemap, "background");
    tilemap_draw_layers_prefixed(&state->tilemap, "midground");
    effects_draw_stains(&state->effects);
    enemies_draw(&state->enemies);
    player_draw(&state->player);
    bullets_draw(&state->bullets);
    effects_draw_pixels(&state->effects);
    tilemap_draw_layers_prefixed(&state->tilemap, "foreground");
    player_draw_crosshair(&state->player);
}

void gameplay_cleanup(GameState *state)
{
    player_cleanup(&state->player);
    bullets_cleanup(&state->bullets);
    enemies_cleanup(&state->enemies);
    tilemap_unload(&state->tilemap);
    audio_cleanup(&state->audio);
    // EffectsSystem holds no GPU resources — vectors free themselves.
}
