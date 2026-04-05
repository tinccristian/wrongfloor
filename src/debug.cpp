#ifdef DEV_MODE

#include "debug.h"
#include "raymath.h"
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

// Split "cmd arg..." into ("cmd", "arg..."). args is empty string if no space.
static void split_cmd(const std::string& input, std::string& cmd, std::string& args)
{
    size_t sp = input.find(' ');
    if (sp == std::string::npos) { cmd = input; args = ""; }
    else                         { cmd = input.substr(0, sp); args = input.substr(sp + 1); }
}

static void rebuild_tab_matches(DebugState *d)
{
    d->tab_matches.clear();
    d->tab_index = -1;

    size_t space = d->input.find(' ');

    if (space != std::string::npos)
    {
        // ── Argument completion mode ───────────────────────────────────
        std::string cmd_part  = d->input.substr(0, space);
        std::string arg_part  = d->input.substr(space + 1);
        std::string cmd_lower = to_lower(cmd_part);

        for (auto& [name, entry] : d->commands)
        {
            if (to_lower(name) == cmd_lower && entry.get_completions)
            {
                auto completions = entry.get_completions(arg_part);
                for (auto& c : completions)
                    d->tab_matches.push_back(cmd_part + " " + c);
                break;
            }
        }
        // Already sorted by get_completions contract.
    }
    else
    {
        // ── Command name completion mode (existing behaviour) ──────────
        std::string prefix_lower = to_lower(d->input);
        for (auto& [name, _] : d->commands)
            if (!d->input.empty() && to_lower(name).rfind(prefix_lower, 0) == 0)
                d->tab_matches.push_back(name);
        std::sort(d->tab_matches.begin(), d->tab_matches.end(),
                  [](const std::string& a, const std::string& b) {
                      return to_lower(a) < to_lower(b);
                  });
    }
}

static void execute(DebugState *d, const std::string& raw)
{
    // Trim leading/trailing whitespace.
    size_t a = raw.find_first_not_of(" \t");
    size_t b = raw.find_last_not_of(" \t");
    if (a == std::string::npos) return;
    std::string full = raw.substr(a, b - a + 1);
    if (full.empty()) return;

    push_output(d, "> " + full);

    // Split into command name + args.
    std::string cmd_name, cmd_args;
    split_cmd(full, cmd_name, cmd_args);

    std::string cmd_lower = to_lower(cmd_name);
    for (auto& [name, entry] : d->commands)
    {
        if (to_lower(name) == cmd_lower)
        {
            entry.callback(d, cmd_args);
            return;
        }
    }
    push_output(d, "Unknown command: " + cmd_name + ". Type 'help' for a list.");
}

// ── Public API ────────────────────────────────────────────────────────────────

void debug_register_command(DebugState *d, const std::string& name,
                             const std::string& description, DebugCallback callback,
                             CompletionCallback get_completions)
{
    d->commands[name] = { description, std::move(callback), std::move(get_completions) };
}

void debug_print(DebugState *d, const std::string& line)
{
    push_output(d, line);
}

void debug_init(DebugState *d)
{
    // Disable raylib's default Escape-to-quit so Escape can close the console instead.
    SetExitKey(0);

    debug_register_command(d, "quit", "Quit the game",
        [](DebugState *, const std::string&) { CloseWindow(); });

    debug_register_command(d, "help", "List all available commands",
        [](DebugState *ds, const std::string&) {
            push_output(ds, "Available commands:");
            for (auto& [name, cmd] : ds->commands)
                push_output(ds, "  " + name + " — " + cmd.description);
        });

    debug_register_command(d, "clear", "Clear console output",
        [](DebugState *ds, const std::string&) { ds->output.clear(); });

    debug_register_command(d, "showColliders", "Toggle collision layer overlay",
        [](DebugState *ds, const std::string&) {
            ds->show_colliders = !ds->show_colliders;
            push_output(ds, std::string("showColliders: ") + (ds->show_colliders ? "ON" : "OFF"));
        });

    debug_register_command(d, "showPlayerState", "Toggle player state info overlay",
        [](DebugState *ds, const std::string&) {
            ds->show_player_state = !ds->show_player_state;
            push_output(ds, std::string("showPlayerState: ") + (ds->show_player_state ? "ON" : "OFF"));
        });

    debug_register_command(d, "showPlayerCollider", "Toggle player hitbox overlay",
        [](DebugState *ds, const std::string&) {
            ds->show_player_collider = !ds->show_player_collider;
            push_output(ds, std::string("showPlayerCollider: ") + (ds->show_player_collider ? "ON" : "OFF"));
        });

    debug_register_command(d, "showFps", "Toggle FPS display",
        [](DebugState *ds, const std::string&) {
            ds->show_fps = !ds->show_fps;
            push_output(ds, std::string("showFps: ") + (ds->show_fps ? "ON" : "OFF"));
        });

    debug_register_command(d, "showEnemyColliders", "Toggle enemy collision radius overlay",
        [](DebugState *ds, const std::string&) {
            ds->show_enemy_colliders = !ds->show_enemy_colliders;
            push_output(ds, std::string("showEnemyColliders: ") + (ds->show_enemy_colliders ? "ON" : "OFF"));
        });

    debug_register_command(d, "showBulletColliders", "Toggle bullet collision radius overlay",
        [](DebugState *ds, const std::string&) {
            ds->show_bullet_colliders = !ds->show_bullet_colliders;
            push_output(ds, std::string("showBulletColliders: ") + (ds->show_bullet_colliders ? "ON" : "OFF"));
        });

    debug_register_command(d, "showBloodCount", "Toggle active/settled blood pixel count display",
        [](DebugState *ds, const std::string&) {
            ds->show_blood_count = !ds->show_blood_count;
            push_output(ds, std::string("showBloodCount: ") + (ds->show_blood_count ? "ON" : "OFF"));
        });

    debug_register_command(d, "showWeaponInfo", "Toggle weapon state overlays (ammo, state, cooldown)",
        [](DebugState *ds, const std::string&) {
            ds->show_weapon_info = !ds->show_weapon_info;
            push_output(ds, std::string("showWeaponInfo: ") + (ds->show_weapon_info ? "ON" : "OFF"));
        });

    debug_register_command(d, "showVisionCones", "Toggle enemy vision cone overlay (yellow=IDLE, red=alerted/searching)",
        [](DebugState *ds, const std::string&) {
            ds->show_vision_cones = !ds->show_vision_cones;
            push_output(ds, std::string("showVisionCones: ") + (ds->show_vision_cones ? "ON" : "OFF"));
        });

    debug_register_command(d, "showSoundEvents", "Toggle sound-event and investigation target overlay",
        [](DebugState *ds, const std::string&) {
            ds->show_sound_events = !ds->show_sound_events;
            push_output(ds, std::string("showSoundEvents: ") + (ds->show_sound_events ? "ON" : "OFF"));
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
        for (const auto& layer : tm->tile_layers)
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

        DrawText(line_state, (int)tx + 1, (int)ty + 1,          fs, BLACK);
        DrawText(line_vel,   (int)tx + 1, (int)ty + lh + 1,     fs, BLACK);
        DrawText(line_aim,   (int)tx + 1, (int)ty + lh * 2 + 1, fs, BLACK);
        DrawText(line_state, (int)tx, (int)ty,          fs, WHITE);
        DrawText(line_vel,   (int)tx, (int)ty + lh,     fs, WHITE);
        DrawText(line_aim,   (int)tx, (int)ty + lh * 2, fs, WHITE);
    }

    // ── Enemy collision rectangles ────────────────────────────────────
    if (d->show_enemy_colliders)
    {
        for (const auto& enemy : state->enemies.enemies)
        {
            if (!enemy.alive) continue;
            Rectangle hb = enemy_hitbox_rect(&enemy);
            DrawRectangleRec(hb, Color{255, 50, 50, 50});
            DrawRectangleLinesEx(hb, 1.5f, Color{255, 80, 80, 210});
        }
    }

    // ── Enemy vision cones ────────────────────────────────────────────
    if (d->show_vision_cones)
    {
        static constexpr float VISION_RANGE      = 250.0f;
        static constexpr float VISION_HALF_ANGLE = 45.0f;
        for (const auto& enemy : state->enemies.enemies)
        {
            if (!enemy.alive) continue;
            bool alert = (enemy.ai_state != EnemyAIState::IDLE);
            Color cone_color = alert ? Color{255, 50, 50, 60} : Color{255, 220, 50, 45};
            Color line_color = alert ? Color{255, 80, 80, 180} : Color{255, 220, 80, 140};

            // DrawCircleSector: centre, radius, startAngle, endAngle, segments, color.
            float start_deg = enemy.facing_angle - VISION_HALF_ANGLE;
            float end_deg   = enemy.facing_angle + VISION_HALF_ANGLE;
            DrawCircleSector(enemy.position, VISION_RANGE, start_deg, end_deg, 16, cone_color);
            DrawCircleSectorLines(enemy.position, VISION_RANGE, start_deg, end_deg, 16, line_color);
        }
    }

    // ── Sound events + AI memory targets ─────────────────────────────
    if (d->show_sound_events)
    {
        for (const auto& event : state->sound_events.events)
        {
            DrawCircleLines((int)event.position.x, (int)event.position.y, event.radius,
                            Color{80, 220, 255, 170});
            DrawCircleV(event.position, 3.0f, Color{80, 220, 255, 220});
        }

        for (const auto& enemy : state->enemies.enemies)
        {
            if (!enemy.alive) continue;

            if (enemy.has_investigation_target)
            {
                DrawLineV(enemy.position, enemy.investigation_target, Color{80, 220, 255, 180});
                DrawCircleV(enemy.investigation_target, 4.0f, Color{80, 220, 255, 220});
            }

            if (enemy.has_last_known_player_pos)
            {
                DrawLineV(enemy.position, enemy.last_known_player_pos, Color{255, 160, 80, 160});
                DrawCircleV(enemy.last_known_player_pos, 4.0f, Color{255, 160, 80, 220});
            }
        }
    }

    // ── Bullet positions ──────────────────────────────────────────────
    if (d->show_bullet_colliders)
    {
        static constexpr float BULLET_DEBUG_R = 3.0f;
        for (const auto& bullet : state->bullets.bullets)
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

    // ── Weapon info overlays ──────────────────────────────────────────
    if (d->show_weapon_info)
    {
        for (const auto& w_ptr : state->weapons.weapons)
        {
            const Weapon& w = *w_ptr;
            Vector2 pos = w.is_held ? w.render_pos : w.position;
            const char *status = w.is_held ? "HELD" : (w.is_thrown ? "THROWN" : "GROUND");
            char info[128];
            if (w.is_melee())
                snprintf(info, sizeof(info), "%.*s %s%s",
                         (int)w.type_name().size(), w.type_name().data(),
                         status, w.is_swinging ? " SWING" : "");
            else
                snprintf(info, sizeof(info), "%.*s %d/%d %s%s",
                         (int)w.type_name().size(), w.type_name().data(),
                         w.current_ammo, w.magazine_size(),
                         status, w.is_reloading ? " RELOAD" : "");

            DrawText(info, (int)pos.x - 30 + 1, (int)pos.y - 22 + 1, 10, BLACK);
            DrawText(info, (int)pos.x - 30,     (int)pos.y - 22,     10, Color{255, 255, 100, 220});

            if (w.is_held)
            {
                float bar_w = 30.0f;
                float fill  = (w.fire_rate() > 0.0f)
                    ? std::max(0.0f, 1.0f - w.fire_cooldown_timer * w.fire_rate())
                    : 1.0f;
                int bx = (int)pos.x - 15;
                int by = (int)pos.y + 10;
                DrawRectangle(bx, by, (int)bar_w, 3, Color{50, 50, 50, 200});
                DrawRectangle(bx, by, (int)(bar_w * fill), 3, Color{255, 200, 50, 220});

                // Melee hitbox: show cone or rectangle while swinging.
                if (w.is_melee() && w.is_swinging && w.melee_hit_triggered)
                {
                    const Vector2& aim = state->player.aim.direction;
                    float aim_deg  = atan2f(aim.y, aim.x) * RAD2DEG;
                    float range    = w.melee_range();
                    static constexpr float ORBIT_DIST_DBG = 22.0f;
                    Vector2 origin = Vector2Add(player_center(&state->player),
                        Vector2Scale(aim, ORBIT_DIST_DBG + w.melee_origin_offset()));

                    if (w.melee_cone_half_angle() > 0.0f)
                    {
                        float half = w.melee_cone_half_angle();
                        DrawCircleSector(origin, range,
                            aim_deg - half, aim_deg + half, 16,
                            Color{255, 80, 80, 55});
                        DrawCircleSectorLines(origin, range,
                            aim_deg - half, aim_deg + half, 16,
                            Color{255, 110, 110, 180});
                    }
                    else
                    {
                        Vector2 fwd  = aim;
                        Vector2 side = { -fwd.y, fwd.x };
                        float   half = w.melee_width() * 0.5f;
                        Vector2 corners[4] = {
                            Vector2Add(origin, Vector2Scale(side,  half)),
                            Vector2Add(origin, Vector2Scale(side, -half)),
                            Vector2Add(Vector2Add(origin, Vector2Scale(fwd, range)), Vector2Scale(side, -half)),
                            Vector2Add(Vector2Add(origin, Vector2Scale(fwd, range)), Vector2Scale(side,  half)),
                        };
                        DrawTriangle(corners[0], corners[1], corners[2], Color{255, 80, 80, 55});
                        DrawTriangle(corners[0], corners[2], corners[3], Color{255, 80, 80, 55});
                        DrawLineV(corners[0], corners[3], Color{255, 100, 100, 180});
                        DrawLineV(corners[1], corners[2], Color{255, 100, 100, 180});
                        DrawLineV(corners[0], corners[1], Color{255, 100, 100, 180});
                        DrawLineV(corners[3], corners[2], Color{255, 100, 100, 180});
                    }
                }
            }
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

    DrawRectangle(0, console_y, screen_w, console_h, Color{10, 10, 15, 210});
    DrawLine(0, console_y, screen_w, console_y, Color{80, 80, 100, 255});

    int input_line_y = screen_h - LINE_H - PADDING;
    int available_h  = input_line_y - console_y - PADDING;
    int max_lines    = available_h / LINE_H;

    int count = (int)d->output.size();
    int start = count > max_lines ? count - max_lines : 0;
    int y     = console_y + PADDING;
    for (int i = start; i < count; i++, y += LINE_H)
    {
        DrawText(d->output[i].c_str(), PADDING + 1, y + 1, FONT_SIZE, BLACK);
        DrawText(d->output[i].c_str(), PADDING,     y,     FONT_SIZE, Color{190, 190, 200, 255});
    }

    DrawLine(0, input_line_y - 2, screen_w, input_line_y - 2, Color{60, 60, 80, 200});

    std::string display = "> " + d->input;
    if (d->cursor_visible) display += "_";
    DrawText(display.c_str(), PADDING, input_line_y, FONT_SIZE, WHITE);
}

#endif // DEV_MODE
