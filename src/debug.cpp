#ifdef DEV_MODE

#include "debug.h"
#include <algorithm>
#include <cstdio>
#include <cctype>

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

static std::string to_lower(const std::string& s)
{
    std::string r = s;
    for (char &c : r) c = (char)std::tolower((unsigned char)c);
    return r;
}

static void rebuild_tab_matches(DebugState *d)
{
    d->tab_matches.clear();
    d->tab_index = -1;
    std::string prefix_lower = to_lower(d->input);
    for (auto &[name, _] : d->commands)
        if (!d->input.empty() && to_lower(name).rfind(prefix_lower, 0) == 0)
            d->tab_matches.push_back(name); // store original casing
    std::sort(d->tab_matches.begin(), d->tab_matches.end(),
              [](const std::string &a, const std::string &b) {
                  return to_lower(a) < to_lower(b);
              });
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

    // Case-insensitive lookup: find the command whose lowercased name matches
    std::string cmd_lower = to_lower(cmd);
    for (auto &[name, entry] : d->commands)
    {
        if (to_lower(name) == cmd_lower)
        {
            entry.callback(d);
            return;
        }
    }
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
    // Disable raylib's default Escape-to-quit so Escape can close the console instead.
    SetExitKey(0);

    debug_register_command(d, "quit", "Quit the game",
        [](DebugState *) { CloseWindow(); });

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

    debug_register_command(d, "showFps", "Toggle FPS display",
        [](DebugState *ds) {
            ds->show_fps = !ds->show_fps;
            push_output(ds, std::string("showFps: ") + (ds->show_fps ? "ON" : "OFF"));
        });

    debug_register_command(d, "showEnemyColliders", "Toggle enemy collision radius overlay",
        [](DebugState *ds) {
            ds->show_enemy_colliders = !ds->show_enemy_colliders;
            push_output(ds, std::string("showEnemyColliders: ") + (ds->show_enemy_colliders ? "ON" : "OFF"));
        });

    debug_register_command(d, "showBulletColliders", "Toggle bullet collision radius overlay",
        [](DebugState *ds) {
            ds->show_bullet_colliders = !ds->show_bullet_colliders;
            push_output(ds, std::string("showBulletColliders: ") + (ds->show_bullet_colliders ? "ON" : "OFF"));
        });

    debug_register_command(d, "showBloodCount", "Toggle active/settled blood pixel count display",
        [](DebugState *ds) {
            ds->show_blood_count = !ds->show_blood_count;
            push_output(ds, std::string("showBloodCount: ") + (ds->show_blood_count ? "ON" : "OFF"));
        });
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

void debug_draw_world(const DebugState *d, const GameState *state)
{
    const Tilemap *tm     = &state->tilemap;
    const Player  *player = &state->player;

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

    // ── Player hitbox ─────────────────────────────────────────────────
    if (d->show_player_collider)
    {
        Rectangle hb = player_hitbox_rect(player);
        DrawRectangleRec(hb, Color{50, 255, 50, 50});
        DrawRectangleLinesEx(hb, 1.5f, Color{80, 255, 80, 220});
    }

    // ── Player state text ─────────────────────────────────────────────
    if (d->show_player_state)
    {
        char line_state[32];
        char line_vel[48];
        char line_aim[32];
        snprintf(line_state, sizeof(line_state), "%s", player_state_name(player->state));
        snprintf(line_vel,   sizeof(line_vel),   "vel: (%.1f, %.1f)",
                 player->velocity_x, player->velocity_y);
        snprintf(line_aim,   sizeof(line_aim),   "aim: %.1f deg", player->aim.angle_deg);

        int fs = 14;
        int lh = 16;
        Rectangle hb = player_hitbox_rect(player);
        float tx = hb.x;
        float ty = hb.y - 3 * lh - 4.0f;

        // Shadow
        DrawText(line_state, (int)tx + 1, (int)ty + 1,          fs, BLACK);
        DrawText(line_vel,   (int)tx + 1, (int)ty + lh + 1,     fs, BLACK);
        DrawText(line_aim,   (int)tx + 1, (int)ty + lh * 2 + 1, fs, BLACK);
        // Text
        DrawText(line_state, (int)tx, (int)ty,          fs, WHITE);
        DrawText(line_vel,   (int)tx, (int)ty + lh,     fs, WHITE);
        DrawText(line_aim,   (int)tx, (int)ty + lh * 2, fs, WHITE);
    }

    // ── Enemy collision rectangles ────────────────────────────────────
    if (d->show_enemy_colliders)
    {
        for (const auto &enemy : state->enemies.enemies)
        {
            if (!enemy.alive) continue;
            Rectangle hb = enemy_hitbox_rect(&enemy);
            DrawRectangleRec(hb, Color{255, 50, 50, 50});
            DrawRectangleLinesEx(hb, 1.5f, Color{255, 80, 80, 210});
        }
    }

    // ── Bullet positions ──────────────────────────────────────────────
    // Bullets use point collision (CheckCollisionPointRec), so draw a small
    // crosshair to show the exact point being tested.
    if (d->show_bullet_colliders)
    {
        static constexpr float BULLET_DEBUG_R = 3.0f;
        for (const auto &bullet : state->bullets.bullets)
        {
            if (bullet.dead) continue;
            DrawCircleV(bullet.position, BULLET_DEBUG_R, Color{255, 255, 50, 180});
            DrawLineV({ bullet.position.x - BULLET_DEBUG_R * 2, bullet.position.y },
                      { bullet.position.x + BULLET_DEBUG_R * 2, bullet.position.y },
                      Color{255, 230, 60, 255});
            DrawLineV({ bullet.position.x, bullet.position.y - BULLET_DEBUG_R * 2 },
                      { bullet.position.x, bullet.position.y + BULLET_DEBUG_R * 2 },
                      Color{255, 230, 60, 255});
        }
    }
}

// ── Draw console UI ───────────────────────────────────────────────────────────

void debug_draw_ui(DebugState *d, const GameState *state, int screen_w, int screen_h)
{
    int hud_y = 8;

    if (d->show_fps)
    {
        const char *fps_text = TextFormat("FPS: %d", GetFPS());
        DrawText(fps_text, 9, hud_y + 1, FONT_SIZE, BLACK);
        DrawText(fps_text, 8, hud_y,     FONT_SIZE, Color{210, 255, 210, 255});
        hud_y += FONT_SIZE + 4;
    }

    if (d->show_blood_count)
    {
        const char *bc_text = TextFormat("blood  active:%d  stains:%d",
                                         effects_active_count(&state->effects),
                                         effects_stain_count(&state->effects));
        DrawText(bc_text, 9, hud_y + 1, FONT_SIZE, BLACK);
        DrawText(bc_text, 8, hud_y,     FONT_SIZE, Color{255, 100, 100, 255});
        hud_y += FONT_SIZE + 4;
    }
    (void)hud_y;

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
