#pragma once

#include "raylib.h"

// Circular-buffer death replay system.
//
// Normal flow: replay_capture_frame() is called every frame; it blit-copies the
// current game frame (from capture_rt) into a slot at half resolution, overwriting
// the oldest slot when the buffer is full.
//
// On player death: replay_trigger() snapshots the buffer state and starts the
// REPLAY phase. replay_update() advances the sequence; replay_draw() renders it
// with a VHS post-process shader. When replay_update() returns true, the caller
// should reload the level.
struct ReplaySystem {
    // Buffer holds ~3 s of footage at 30 captured fps (every 2nd frame at 60 fps).
    static constexpr int   BUFFER_SIZE  = 90;
    static constexpr int   CAPTURE_W    = 640;
    static constexpr int   CAPTURE_H    = 360;
    static constexpr float CAPTURE_FPS  = 30.0f;  // effective capture rate
    static constexpr float REPLAY_SPEED = 3.0f;   // playback rate multiplier
    static constexpr float BLACKOUT_DUR = 0.2f;   // black-screen duration after replay

    // Circular frame buffer — each slot is a render texture at CAPTURE_W x CAPTURE_H.
    RenderTexture2D frames[BUFFER_SIZE]{};
    int  write_head  = 0;  // next slot to write
    int  frame_count = 0;  // valid frames currently stored (0..BUFFER_SIZE)
    int  skip_count  = 0;  // ticks since last capture; write every 2nd frame

    // Full-resolution intermediate target. The game renders into this each frame;
    // it is then either blitted to screen or downscaled into the circular buffer.
    RenderTexture2D capture_rt{};

    // VHS post-process shader applied during glitch and replay phases.
    Shader vhs_shader{};
    int    time_loc           = -1;
    int    glitch_strength_loc = -1;

    // Replay sequence state.
    static constexpr float GLITCH_DUR = 0.35f; // glitch phase before playback starts
    enum class Phase { NONE, GLITCH, REPLAY, BLACKOUT } phase = Phase::NONE;
    float vhs_time   = 0.0f;  // cumulative time during replay (jitter seed)
    float read_pos   = 0.0f;  // fractional frame index into the play sequence
    int   play_start = 0;     // oldest frame slot index at trigger time
    int   play_count = 0;     // total frames to play back
};

// Allocate all render textures and load the VHS shader. Call once after InitWindow.
void replay_init(ReplaySystem *r, int screen_w, int screen_h);

// Downscale capture_rt into the circular buffer (every 2nd call is a no-op).
// Call once per frame during normal gameplay only.
void replay_capture_frame(ReplaySystem *r);

// Clear the circular buffer (e.g. after a level load).
void replay_reset_buffer(ReplaySystem *r);

// Snapshot the buffer state and begin the REPLAY → BLACKOUT sequence.
void replay_trigger(ReplaySystem *r);

// Advance replay state. Returns true when the full sequence has finished
// and the caller should reload the level.
bool replay_update(ReplaySystem *r, float dt);

// Draw the current replay frame with VHS shader, or solid black during BLACKOUT.
// Call inside BeginDrawing(), outside BeginMode2D().
void replay_draw(ReplaySystem *r, int screen_w, int screen_h);

// Returns true while any replay phase is active.
bool replay_is_active(const ReplaySystem *r);

// Release all GPU resources. Call before CloseWindow.
void replay_cleanup(ReplaySystem *r);
