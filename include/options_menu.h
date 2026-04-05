#pragma once

#include "raylib.h"
#include "audio.h"
#include "settings.h"
#include <string>
#include <vector>

// Action triggered by options menu
enum class OptionsAction { None, Back };

// Options menu sections (used by init)
enum class OptionsSection { Display, Audio, Graphics, Controls };

struct OptionsItem {
    std::string label;
    std::string value;
    float anim_t = 0.0f;
    bool is_header = false;
    int section_idx = -1;  // -1 for blank lines, 0-3 for Display/Audio/Graphics/Controls
};

struct OptionsSectionView {
    std::vector<OptionsItem> items;
    int selected = 0;
};

struct OptionsMenu {
    OptionsSectionView current_section;
    int scroll_offset = 0;
    Settings *settings = nullptr;
    AudioState *audio = nullptr;
};

// Initialize the options menu (shows all sections)
void options_menu_init(OptionsMenu *menu, OptionsSection section, Settings *settings, AudioState *audio);

// Update navigation and value changes; updates *screen_w/*screen_h if resolution changes
OptionsAction options_menu_update(OptionsMenu *menu, float dt, int *screen_w, int *screen_h);

// Draw the options menu
void options_menu_draw(const OptionsMenu *menu, int screen_w, int screen_h);
