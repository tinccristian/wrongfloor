#include "replay.h"
#include "game.h"  // assets_path()

// ── Init / cleanup ────────────────────────────────────────────────────────────

void replay_init(ReplaySystem *r, int screen_w, int screen_h)
{
    r->capture_rt = LoadRenderTexture(screen_w, screen_h);
    for (int i = 0; i < ReplaySystem::BUFFER_SIZE; ++i)
        r->frames[i] = LoadRenderTexture(ReplaySystem::CAPTURE_W, ReplaySystem::CAPTURE_H);

    // NULL vertex shader → raylib's built-in default (passes fragTexCoord, fragColor).
    r->vhs_shader = LoadShader(nullptr, assets_path("shaders/vhs.fs").c_str());
    r->time_loc   = GetShaderLocation(r->vhs_shader, "time");
}

void replay_cleanup(ReplaySystem *r)
{
    for (int i = 0; i < ReplaySystem::BUFFER_SIZE; ++i)
        if (r->frames[i].id != 0) UnloadRenderTexture(r->frames[i]);
    if (r->capture_rt.id != 0)  UnloadRenderTexture(r->capture_rt);
    if (r->vhs_shader.id != 0)  UnloadShader(r->vhs_shader);
    *r = ReplaySystem{};
}

// ── Capture ───────────────────────────────────────────────────────────────────

void replay_capture_frame(ReplaySystem *r)
{
    r->skip_count++;
    if (r->skip_count < 2) return;  // capture every 2nd frame → 30 fps effective
    r->skip_count = 0;

    // Blit capture_rt → current buffer slot at reduced resolution.
    // Both are render textures (Y-flipped storage), so using positive source height
    // preserves the same orientation — no flip needed here.
    BeginTextureMode(r->frames[r->write_head]);
        Rectangle src = { 0.0f, 0.0f,
            (float)r->capture_rt.texture.width,
            (float)r->capture_rt.texture.height };
        Rectangle dst = { 0.0f, 0.0f,
            (float)ReplaySystem::CAPTURE_W,
            (float)ReplaySystem::CAPTURE_H };
        DrawTexturePro(r->capture_rt.texture, src, dst, { 0.0f, 0.0f }, 0.0f, WHITE);
    EndTextureMode();

    r->write_head = (r->write_head + 1) % ReplaySystem::BUFFER_SIZE;
    if (r->frame_count < ReplaySystem::BUFFER_SIZE) r->frame_count++;
}

void replay_reset_buffer(ReplaySystem *r)
{
    r->write_head  = 0;
    r->frame_count = 0;
    r->skip_count  = 0;
    r->phase       = ReplaySystem::Phase::NONE;
}

// ── Sequence control ──────────────────────────────────────────────────────────

void replay_trigger(ReplaySystem *r)
{
    // Snapshot which frames are valid and where playback should start.
    r->play_count = r->frame_count;
    r->play_start = (r->frame_count < ReplaySystem::BUFFER_SIZE)
        ? 0                 // buffer not yet full: oldest = slot 0
        : r->write_head;    // buffer full: oldest = the slot we'd write next
    r->read_pos  = 0.0f;
    r->vhs_time  = 0.0f;
    r->phase     = ReplaySystem::Phase::REPLAY;
}

bool replay_update(ReplaySystem *r, float dt)
{
    switch (r->phase)
    {
    case ReplaySystem::Phase::REPLAY:
        r->vhs_time += dt;
        r->read_pos += ReplaySystem::REPLAY_SPEED * ReplaySystem::CAPTURE_FPS * dt;
        if (r->play_count == 0 || r->read_pos >= (float)r->play_count)
        {
            r->phase    = ReplaySystem::Phase::BLACKOUT;
            r->vhs_time = 0.0f;
        }
        break;

    case ReplaySystem::Phase::BLACKOUT:
        r->vhs_time += dt;
        if (r->vhs_time >= ReplaySystem::BLACKOUT_DUR)
        {
            r->phase = ReplaySystem::Phase::NONE;
            return true;  // sequence complete — caller should reload level
        }
        break;

    case ReplaySystem::Phase::NONE:
        break;
    }
    return false;
}

// ── Draw ──────────────────────────────────────────────────────────────────────

void replay_draw(ReplaySystem *r, int screen_w, int screen_h)
{
    if (r->phase == ReplaySystem::Phase::BLACKOUT || r->play_count == 0)
    {
        ClearBackground(BLACK);
        return;
    }

    // Clamp read position to valid range.
    int frame_idx = (int)r->read_pos;
    if (frame_idx >= r->play_count) frame_idx = r->play_count - 1;
    int slot = (r->play_start + frame_idx) % ReplaySystem::BUFFER_SIZE;

    const RenderTexture2D& frame = r->frames[slot];

    // The frame slots were written via DrawTexturePro into a render texture, which
    // already compensates for OpenGL's Y-flip. Drawing them to screen therefore uses
    // positive source height (no extra flip needed — negative H would invert them).
    Rectangle src = { 0.0f, 0.0f,
        (float)ReplaySystem::CAPTURE_W,
        (float)ReplaySystem::CAPTURE_H };
    Rectangle dst = { 0.0f, 0.0f, (float)screen_w, (float)screen_h };

    ClearBackground(BLACK);

    if (r->vhs_shader.id != 0 && r->time_loc >= 0)
    {
        SetShaderValue(r->vhs_shader, r->time_loc, &r->vhs_time, SHADER_UNIFORM_FLOAT);
        BeginShaderMode(r->vhs_shader);
            DrawTexturePro(frame.texture, src, dst, { 0.0f, 0.0f }, 0.0f, WHITE);
        EndShaderMode();
    }
    else
    {
        // Shader failed to load — draw without effect.
        DrawTexturePro(frame.texture, src, dst, { 0.0f, 0.0f }, 0.0f, WHITE);
    }
}

bool replay_is_active(const ReplaySystem *r)
{
    return r->phase != ReplaySystem::Phase::NONE;
}
