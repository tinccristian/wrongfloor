#pragma once

#include "raylib.h"
#include "audio.h"
#include <string>
#include <vector>

// Result returned by pause_menu_update each frame.
enum class PauseAction { None, Resume, Restart, Options, MainMenu, Exit };

struct PauseMenuItem {
    std::string label;
    float anim_t = 0.0f;  // smooth lerp state: 0 = unselected, 1 = fully selected
};

struct PauseMenu {
    std::vector<PauseMenuItem> items;
    int selected = 0;
};

// Populate menu items. Call once when the menu is first created.
void pause_menu_init(PauseMenu *menu);

// Handle navigation input, mouse hover/click, and advance lerp animations.
// Returns the action triggered this frame (PauseAction::None if nothing confirmed).
PauseAction pause_menu_update(PauseMenu *menu, AudioState *audio, float dt, int screen_w, int screen_h);

// Draw the translucent overlay and animated menu items in screen space.
// Call outside BeginMode2D.
void pause_menu_draw(const PauseMenu *menu, int screen_w, int screen_h);
