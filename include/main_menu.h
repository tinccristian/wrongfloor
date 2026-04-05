#pragma once

#include "raylib.h"
#include "audio.h"
#include <string>
#include <vector>

// Action triggered by main menu selection
enum class MainMenuAction { None, Play, Options, Quit };

struct MainMenuItem {
    std::string label;
    float anim_t = 0.0f;  // smooth lerp state: 0 = unselected, 1 = fully selected
};

struct MainMenu {
    std::vector<MainMenuItem> items;
    int selected = 0;
    Texture2D splash = {};  // assets/splash.png, loaded at init
};

// Initialize the main menu and load splash background
void main_menu_init(MainMenu *menu);

// Handle navigation input, mouse hover/click, and advance lerp animations
// Returns the action triggered this frame (MainMenuAction::None if nothing confirmed)
MainMenuAction main_menu_update(MainMenu *menu, AudioState *audio, float dt, int screen_w, int screen_h, Vector2 virtual_mouse);

// Draw the splash background and animated menu items
void main_menu_draw(const MainMenu *menu, int screen_w, int screen_h);

// Clean up resources (splash texture)
void main_menu_cleanup(MainMenu *menu);
