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
// args = everything after the command name (trimmed), empty string if none.
using DebugCallback      = std::function<void(DebugState *, const std::string& args)>;
// Returns completions matching prefix, sorted. Called on Tab in arg position.
using CompletionCallback = std::function<std::vector<std::string>(const std::string& prefix)>;

struct DebugCommand {
    std::string        description;
    DebugCallback      callback;
    CompletionCallback get_completions; // optional; null if no argument completions
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
    bool show_weapon_info      = false;
    bool show_vision_cones     = false;
    bool show_sound_events     = false;

    // ── Command registry ──────────────────────────────────────────────
    std::map<std::string, DebugCommand> commands;
};

// Set up built-in commands. Call once after all systems are initialised.
void debug_init(DebugState *d);

// Handle console input; call every frame before player_update.
// Returns true if the console is open (caller should pass input_blocked=true to player_update).
bool debug_update(DebugState *d, float dt);

// Register a custom command. Optionally provide a completion callback for Tab on the argument.
void debug_register_command(DebugState *d, const std::string& name,
                             const std::string& description, DebugCallback callback,
                             CompletionCallback get_completions = nullptr);

// Append a line to the console output. Safe to call from registered command callbacks.
void debug_print(DebugState *d, const std::string& line);

// Draw world-space debug overlays. Call inside BeginMode2D.
void debug_draw_world(const DebugState *d, const GameState *state);

// Draw the console UI and any screen-space overlays (FPS, blood count).
// Call outside BeginMode2D, last in the frame.
void debug_draw_ui(DebugState *d, const GameState *state, int screen_w, int screen_h);

#endif // DEV_MODE
