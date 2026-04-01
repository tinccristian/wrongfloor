#ifdef DEV_MODE

#include "debug.h"
#include <algorithm>
#include <cstdio>

static constexpr int   MAX_OUTPUT_LINES = 200;
static constexpr int   MAX_INPUT_LEN    = 256;
static constexpr int   FONT_SIZE        = 16;
static constexpr int   LINE_H           = 20;
static constexpr int   PADDING          = 8;

// ── Helpers ───────────────────────────────────────────────────────────────────

static const char *player_state_name(PlayerState s)
{
    switch (s)
    {
        case PLAYER_IDLE:      return "IDLE";
        case PLAYER_WALKING:   return "WALKING";
        case PLAYER_RUNNING:   return "RUNNING";
        case PLAYER_JUMPING:   return "JUMPING";
        case PLAYER_PEAK:      return "PEAK";
        case PLAYER_FALLING:   return "FALLING";
        case PLAYER_LANDING:   return "LANDING";
        case PLAYER_ATTACKING: return "ATTACKING";
        default:               return "UNKNOWN";
    }
}

static void push_output(DebugState *d, const std::string& line)
{
    d->output.push_back(line);
    while ((int)d->output.size() > MAX_OUTPUT_LINES)
        d->output.pop_front();
}

static void rebuild_tab_matches(DebugState *d)
{
    d->tab_matches.clear();
    d->tab_index = -1;
    for (auto &[name, _] : d->commands)
        if (!d->input.empty() && name.rfind(d->input, 0) == 0)
            d->tab_matches.push_back(name);
    std::sort(d->tab_matches.begin(), d->tab_matches.end());
}

static void execute(DebugState *d, const std::string& raw)
{
    // trim leading/trailing whitespace
    size_t a = raw.find_first_not_of(" \t");
    size_t b = raw.find_last_not_of(" \t");
    if (a == std::string::npos) return;
    std::string cmd = raw.substr(a, b - a + 1);
    if (cmd.empty()) return;

    push_output(d, "> " + cmd);

    auto it = d->commands.find(cmd);
    if (it != d->commands.end())
        it->second.callback(d);
    else
        push_output(d, "Unknown command: " + cmd + ". Type 'help' for a list.");
}

// ── Public API ────────────────────────────────────────────────────────────────

void debug_register_command(DebugState *d, const std::string& name,
                             const std::string& description, DebugCallback callback)
{
    d->commands[name] = { description, std::move(callback) };
}

void debug_init(DebugState *d)
{
    debug_register_command(d, "help", "List all available commands",
        [](DebugState *ds) {
            push_output(ds, "Available commands:");
            for (auto &[name, cmd] : ds->commands)
                push_output(ds, "  " + name + " — " + cmd.description);
        });

    debug_register_command(d, "clear", "Clear console output",
        [](DebugState *ds) { ds->output.clear(); });

    debug_register_command(d, "showColliders", "Toggle collision layer overlay",
        [](DebugState *ds) {
            ds->show_colliders = !ds->show_colliders;
            push_output(ds, std::string("showColliders: ") + (ds->show_colliders ? "ON" : "OFF"));
        });

    debug_register_command(d, "showPlayerState", "Toggle player state info overlay",
        [](DebugState *ds) {
            ds->show_player_state = !ds->show_player_state;
            push_output(ds, std::string("showPlayerState: ") + (ds->show_player_state ? "ON" : "OFF"));
        });

    debug_register_command(d, "showPlayerCollider", "Toggle player hitbox overlay",
        [](DebugState *ds) {
            ds->show_player_collider = !ds->show_player_collider;
            push_output(ds, std::string("showPlayerCollider: ") + (ds->show_player_collider ? "ON" : "OFF"));
        });

    push_output(d, "DEV_MODE active. Type 'help' for commands.");
}

bool debug_update(DebugState *d, float dt)
{
    // Toggle console with backtick/tilde
    if (IsKeyPressed(KEY_GRAVE))
    {
        d->console_open = !d->console_open;
        d->input.clear();
        d->tab_matches.clear();
        d->tab_index      = -1;
        d->last_tab_input = "";
        return d->console_open;
    }

    if (!d->console_open) return false;

    // Cursor blink
    d->cursor_blink_timer += dt;
    if (d->cursor_blink_timer >= 0.5f)
    {
        d->cursor_blink_timer -= 0.5f;
        d->cursor_visible = !d->cursor_visible;
    }

    // Escape closes the console
    if (IsKeyPressed(KEY_ESCAPE))
    {
        d->console_open = false;
        d->input.clear();
        return false;
    }

    // Enter executes the command
    if (IsKeyPressed(KEY_ENTER))
    {
        execute(d, d->input);
        d->input.clear();
        d->tab_matches.clear();
        d->tab_index      = -1;
        d->last_tab_input = "";
        return true;
    }

    // Backspace
    if (IsKeyPressed(KEY_BACKSPACE) && !d->input.empty())
    {
        d->input.pop_back();
        d->tab_matches.clear();
        d->tab_index      = -1;
        d->last_tab_input = "";
    }

    // Tab autocomplete
    if (IsKeyPressed(KEY_TAB) && !d->input.empty())
    {
        // Rebuild match list if input changed since last Tab
        if (d->input != d->last_tab_input)
            rebuild_tab_matches(d);

        if (!d->tab_matches.empty())
        {
            d->tab_index = (d->tab_index + 1) % (int)d->tab_matches.size();
            d->input          = d->tab_matches[d->tab_index];
            d->last_tab_input = d->input;
        }
    }

    // Printable character input
    int ch;
    while ((ch = GetCharPressed()) != 0)
    {
        if (ch >= 32 && (int)d->input.size() < MAX_INPUT_LEN)
        {
            d->input += (char)ch;
            // Any typed character invalidates the tab state
            d->tab_matches.clear();
            d->tab_index      = -1;
            d->last_tab_input = "";
        }
    }

    return true; // console is open — block player input
}

// ── Draw world overlays ───────────────────────────────────────────────────────

void debug_draw_world(const DebugState *d, const Tilemap *tm, const Player *player)
{
    // ── Collision layer overlay ───────────────────────────────────────
    if (d->show_colliders && tm)
    {
        for (const auto &layer : tm->tile_layers)
        {
            if (layer.name != "collision") continue;
            for (int ty = 0; ty < layer.height; ty++)
            for (int tx = 0; tx < layer.width;  tx++)
            {
                if (layer.data[ty * layer.width + tx] == 0) continue;
                int wx = tx * tm->tile_width;
                int wy = ty * tm->tile_height;
                DrawRectangle(wx, wy, tm->tile_width, tm->tile_height,
                              Color{255, 50, 50, 60});
                DrawRectangleLines(wx, wy, tm->tile_width, tm->tile_height,
                                   Color{255, 80, 80, 180});
            }
            break;
        }
    }

    if (!player) return;

    // ── Player hitbox ─────────────────────────────────────────────────
    if (d->show_player_collider)
    {
        Rectangle hb = {
            player->position.x + HITBOX_OFFSET_X,
            player->position.y + HITBOX_OFFSET_Y,
            HITBOX_W, HITBOX_H
        };
        DrawRectangleRec(hb, Color{50, 255, 50, 50});
        DrawRectangleLinesEx(hb, 1.5f, Color{80, 255, 80, 220});
    }

    // ── Player state text ─────────────────────────────────────────────
    if (d->show_player_state)
    {
        // Build the three lines
        char line_state[32];
        char line_vel[48];
        char line_grnd[24];
        snprintf(line_state, sizeof(line_state), "%s", player_state_name(player->state));
        snprintf(line_vel,   sizeof(line_vel),   "vel: (%.1f, %.1f)",
                 player->velocity_x, player->velocity_y);
        snprintf(line_grnd,  sizeof(line_grnd),  "grounded: %s",
                 player->grounded ? "true" : "false");

        int fs  = 14;
        int lh  = 16;
        float tx = player->position.x + HITBOX_OFFSET_X;
        float ty = player->position.y - 3 * lh - 4.0f;

        // Shadow
        DrawText(line_state, (int)tx + 1, (int)ty + 1,         fs, BLACK);
        DrawText(line_vel,   (int)tx + 1, (int)ty + lh + 1,    fs, BLACK);
        DrawText(line_grnd,  (int)tx + 1, (int)ty + lh * 2 + 1, fs, BLACK);
        // Text
        DrawText(line_state, (int)tx, (int)ty,         fs, WHITE);
        DrawText(line_vel,   (int)tx, (int)ty + lh,    fs, WHITE);
        DrawText(line_grnd,  (int)tx, (int)ty + lh * 2, fs, WHITE);
    }
}

// ── Draw console UI ───────────────────────────────────────────────────────────

void debug_draw_ui(DebugState *d, int screen_w, int screen_h)
{
    if (!d->console_open) return;

    int console_h = screen_h / 3;
    int console_y = screen_h - console_h;

    // Background
    DrawRectangle(0, console_y, screen_w, console_h, Color{10, 10, 15, 210});
    DrawLine(0, console_y, screen_w, console_y, Color{80, 80, 100, 255});

    // How many history lines fit above the input row
    int input_line_y = screen_h - LINE_H - PADDING;
    int available_h  = input_line_y - console_y - PADDING;
    int max_lines    = available_h / LINE_H;

    // Draw history (most recent lines fill upward)
    int count = (int)d->output.size();
    int start = count > max_lines ? count - max_lines : 0;
    int y     = console_y + PADDING;
    for (int i = start; i < count; i++, y += LINE_H)
    {
        // Shadow
        DrawText(d->output[i].c_str(), PADDING + 1, y + 1, FONT_SIZE, BLACK);
        DrawText(d->output[i].c_str(), PADDING,     y,     FONT_SIZE, Color{190, 190, 200, 255});
    }

    // Separator above input
    DrawLine(0, input_line_y - 2, screen_w, input_line_y - 2, Color{60, 60, 80, 200});

    // Input line with blinking cursor
    std::string display = "> " + d->input;
    if (d->cursor_visible) display += "_";
    DrawText(display.c_str(), PADDING, input_line_y, FONT_SIZE, WHITE);
}

#endif // DEV_MODE
