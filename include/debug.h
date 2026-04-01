#pragma once

#ifdef DEV_MODE

#include "raylib.h"
#include "game.h"
#include <string>
#include <vector>
#include <deque>
#include <map>
#include <functional>

struct DebugState;
using DebugCallback = std::function<void(DebugState *)>;

struct DebugCommand {
    std::string  description;
    DebugCallback callback;
};

struct DebugState {
    // ── Console ───────────────────────────────────────────────────────
    bool        console_open   = false;
    std::string input;
    std::deque<std::string> output; // history lines

    float cursor_blink_timer = 0.0f;
    bool  cursor_visible     = true;

    // Tab completion state
    std::vector<std::string> tab_matches;
    int         tab_index      = -1;
    std::string last_tab_input; // input at time of last Tab press

    // ── Overlay toggles ───────────────────────────────────────────────
    bool show_colliders        = false;
    bool show_player_state     = false;
    bool show_player_collider  = false;
    bool show_fps              = false;
    bool show_enemy_colliders  = false;
    bool show_bullet_colliders = false;
    bool show_blood_count      = false;

    // ── Command registry ──────────────────────────────────────────────
    std::map<std::string, DebugCommand> commands;
};

// Set up built-in commands. Call once after all systems are initialised.
void debug_init(DebugState *d);

// Handle console input; call every frame before player_update.
// Returns true if the console is open (caller should pass input_blocked=true to player_update).
bool debug_update(DebugState *d, float dt);

// Register a custom command. The callback receives the DebugState* and may push output lines.
void debug_register_command(DebugState *d, const std::string& name,
                             const std::string& description, DebugCallback callback);

// Draw world-space debug overlays. Call inside BeginMode2D.
void debug_draw_world(const DebugState *d, const GameState *state);

// Draw the console UI and any screen-space overlays (FPS, blood count).
// Call outside BeginMode2D, last in the frame.
void debug_draw_ui(DebugState *d, const GameState *state, int screen_w, int screen_h);

#endif // DEV_MODE
