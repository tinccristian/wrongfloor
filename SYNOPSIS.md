# Wrongfloor — Codebase Synopsis

Top-down action game built with raylib 5.5 + C++17. Virtual resolution 1280×720, letterboxed to actual window.

---

## Modules / Systems

| File | Role |
|------|------|
| `main.cpp` | Entry point. Creates window, drives game loop, routes input to menus or gameplay, handles letterboxing and virtual mouse transform. |
| `game.h` | Central `GameState` struct (all subsystems), `GameStateMode` enum, `assets_path()`, virtual resolution constants, level file list. |
| `gameplay.h/cpp` | Initialises and drives in-game systems each frame. Houses `load_level`, level-transition check, and the draw pipeline split (world / HUD). |
| `player.h/cpp` | Player movement, top-down tile collision, mixed mouse+gamepad aim, 8-directional sprite animation (idle/run). |
| `enemy.h/cpp` | Enemy AI state machine (IDLE→ALERT→CHASE/SEARCH→ATTACK), vision cone, LOS raycasting, hearing via SoundEvents, animation, weapon orbit render. |
| `weapon.h` | Abstract `Weapon` base class with shared runtime state (ammo, timers, physics). |
| `assault_rifle.h/cpp` | Full-auto ranged weapon. 30-round mag, 10 rps, 4 sound aliases for rapid fire. |
| `deagle.h/cpp` | Semi-auto ranged weapon. 7-round mag, 3 rps, heavier recoil, 1.4× volume boost. |
| `weapons/saber.h/cpp` | Melee — wide 100° cone swing (80° arc, 0.3 s). Looping humming music stream while held; ignition on pickup. |
| `weapons/dagger.h/cpp` | Melee — fast stab (7 rps, 0.1 s, 10 px forward jab). Accumulates blood pixel decals on blade per hit; cleared on drop. |
| `weapon_manager.h/cpp` | Owns all live weapon instances (`vector<unique_ptr<Weapon>>`). Drives orbit, throw physics, pickup/throw/fire/reload input, ground bob, recoil decay, melee swing animation, HUD. Factory (`weapons_spawn`) maps type name → subclass. |
| `bullets.h/cpp` | Bullet pool. Each bullet has position, velocity, owner tag, lifetime (0.9 s), animated 4-frame sprite with additive glow. Wall collision via tilemap step-check each frame. |
| `collision_system.h/cpp` | `collision_bullets_vs_enemies`, `collision_bullets_vs_player`, `collision_melee_vs_enemies`. Enemy/player death on hit; spawns blood effects. |
| `effects.h/cpp` | Blood particle system: burst splatter, high-velocity spear pixels, fountain spray (0.3 s), ooze sources (2-5 s pool formation). Pixels settle into permanent stains. |
| `tilemap.h/cpp` | Loads Tiled `.tmj` maps (JSON). Parses tile layers, object layers, tileset. Exposes solid-tile queries, layer draw by name prefix, spawn point and named-object lookup. |
| `camera.h/cpp` | Raylib `Camera2D` wrapper. Smooth lerp follow, clamped to map bounds. |
| `audio.h/cpp` | Loads global sounds (walk, run, land, attack, menu). Volume controls (master × sfx). Footstep and one-shot playback helpers. |
| `sound_events.h/cpp` | Short-lived world-space sound cues read by enemy AI for hearing. Types: FOOTSTEP, GUNSHOT, IMPACT, GENERIC. Tunable radii (footstep 90, rifle 220, deagle 280, impact 150 px). |
| `replay.h/cpp` | Circular-buffer death replay. Captures game frames into 90-slot ring at 30 fps (3 s of footage). Sequence: GLITCH (0.2 s freeze) → REPLAY (forward playback at 3× speed ≈ 1 s) → BLACKOUT (0.2 s). VHS shader applied throughout. |
| `settings.h/cpp` | Persists display (resolution preset, window mode, vsync) and audio (master/sfx/music volumes) and show_fps to `%APPDATA%\wrongfloor\settings.json` via nlohmann/json. |
| `animation.h/cpp` | Frame-strip `Animation` struct + `AnimationPlayer` that advances frame index at fixed duration, with optional looping and horizontal flip. |
| `pause_menu.h/cpp` | In-game pause overlay (RESUME / RESTART / OPTIONS / MAIN MENU / EXIT). Keyboard, gamepad, and mouse input. Lerp-animated item selection. |
| `main_menu.h/cpp` | Title screen (PLAY / OPTIONS / QUIT) with splash background, same input and lerp system as pause menu. |
| `options_menu.h/cpp` | Tabbed options screen (Audio + Display). Adjusts volume sliders and display settings live; saves on Back. |
| `debug.h/cpp` | DEV_MODE-only: drop-down console (` `` ` to toggle), command registry, Tab autocomplete, 13 overlay toggles, world and UI draw pass. |

---

## Game State Machine

```
MAIN_MENU  ──Play──►  PLAYING  ◄──────────────────────────┐
    │                    │  │                              │
   Options            Escape (not during replay)    Back from options
    │                    ▼                                 │
OPTIONS_MAIN        PAUSED ──Options──► OPTIONS_PAUSE ─────┘
    │                 │  │
   Back            Restart / Main Menu / Exit
    ▼                 │
MAIN_MENU         PLAYING / MAIN_MENU / exit
```

**Death sub-sequence** (within PLAYING):
1. `player_dead = true`, `time_scale = 0.15` (slow-mo begins)
2. After **0.2 s** real time: `time_scale` restored, `replay_trigger()` called
3. Replay GLITCH phase: **0.2 s** freeze on death frame with heavy VHS distortion
4. Replay REPLAY phase: **~1 s** forward playback at 3× speed, VHS shader
5. Replay BLACKOUT phase: **0.2 s** solid black
6. `gameplay_reload_level()` → level restarts

Pause is **blocked** while any replay phase is active.

---

## Weapon Types

| Type | Mode | Rate | Mag | Reserve | Spread | Recoil | Notes |
|------|------|------|-----|---------|--------|--------|-------|
| `assault_riffle` | Auto | 10 rps | 30 | 90 | ±4° | 4 px | 4 sound aliases for rapid fire |
| `deagle` | Semi | 3 rps | 7 | 21 | ±1.5° | 7 px | 1.4× volume |
| `saber` | Melee | 2 swings/s | — | — | — | — | 100° cone, 95 px range; humming music stream |
| `dagger` | Melee | 7 stabs/s | — | — | — | — | 70 px rect hitbox, 50 px wide; blood decals accumulate on blade |

All weapons can be picked up (E / controller Y) and thrown (G / right bumper) except melee that sets `is_throwable()` — both melee types inherit the default `true`, so they are throwable. Thrown weapons travel at 950 px/s with 0.97/frame drag and kill enemies on contact (drops their weapon, spawns blood).

Enemy weapons: assigned by Tiled `weapon_type` property ("assault_riffle", "deagle", "saber", "dagger"); falls back to random 50/50 AssaultRifle or Deagle if absent. Enemies have infinite ammo (instant reload on empty).

---

## Enemy AI States & Behavior

| State | Trigger | Behavior |
|-------|---------|----------|
| **IDLE** | Default spawn state | Stands still, facing spawn direction. Watches for player in 250 px × 90° vision cone. Listens for sound events. |
| **ALERT** | Sees player (IDLE→ALERT) or hears sound | Reaction delay: 0.3–0.5 s visual, 0.12–0.22 s sound. Faces target. Transitions to CHASE/ATTACK/SEARCH when timer expires. |
| **CHASE** | Player visible and out of range, or has last-known pos | Moves toward player/LKP at 185 px/s with wall-slide. Fires weapon while chasing. If reaches LKP → SEARCH. |
| **ATTACK** | Player visible and within 200 px | Stands still, tracks and fires. If player moves >240 px → CHASE. |
| **SEARCH** | LKP expired (after 2.2 s memory) or investigation target | Moves to target pos, then spins in place for 1.6 s ± 0.45 s jitter. If nothing found → IDLE. |
| **DEAD** | Killed by bullet or melee | No updates. `alive = false`. |

Vision: 250 px range, 90° cone (±45°), LOS raycasting (half-tile steps). Hearing: picks nearest in-range SoundEvent; 0.18 s cooldown between alerts. Memory: 2.2 s after last sighting before transitioning to SEARCH.

---

## Debug Console Commands (DEV_MODE only)

Toggle console with `` ` `` (backtick/grave). Escape closes it. Tab autocompletes commands and (for some) arguments.

| Command | Description |
|---------|-------------|
| `help` | List all commands |
| `clear` | Clear console output |
| `quit` | Quit the game |
| `showColliders` | Toggle collision tile overlay (red) |
| `showPlayerState` | Toggle player state/velocity/aim text overlay |
| `showPlayerCollider` | Toggle player hitbox rectangle |
| `showFps` | Toggle FPS counter |
| `showEnemyColliders` | Toggle enemy hitbox rectangles |
| `showBulletColliders` | Toggle bullet position crosshairs |
| `showBloodCount` | Toggle active/stain blood pixel count HUD |
| `showWeaponInfo` | Toggle per-weapon ammo/state/cooldown bar overlays |
| `showVisionCones` | Toggle enemy vision cones (yellow=IDLE, red=alerted) |
| `showSoundEvents` | Toggle sound-event radius circles |
| `showEnemyState` | Toggle AI state labels above enemies (with SEE/LKP/INV flags) |
| `showAITargets` | Toggle last-known-position and investigation target lines |
| `ai_state_labels` | Alias: toggle enemy AI state labels |
| `ai_target_debug` | Alias: toggle AI target lines |
| `sound_debug` | Alias: toggle sound event circles |
| `sound_clear` | Clear all active sound events |
| `sound_spawn [radius]` | Spawn a test sound event at player position |
| `sound_range [type] [value]` | Print or set sound radii (footstep/rifle/deagle/impact). Tab-completes type. |
| `ai_search_time [seconds]` | Print or set enemy SEARCH duration |
| `ai_memory_time [seconds]` | Print or set enemy LKP memory duration |
| `ai_reset` | Reset all enemies to IDLE and clear sound events |
| `loadLevel <filename.tmj>` | Load any .tmj from assets/levels/ by name. Tab-completes filenames. |

---

## Sound Files & When They Play

| File | System | Trigger |
|------|--------|---------|
| `sounds/walk.mp3` | AudioState.snd_walk | Player WALKING state (currently WALKING is unused — reserved) |
| `sounds/run.mp3` | AudioState.snd_run | Player RUNNING footstep frames (frames 0 and 3) |
| `sounds/land.mp3` | AudioState.snd_land | **Loaded but never played** (reserved for future jump/fall) |
| `sounds/attack.mp3` | AudioState.snd_attack | **Loaded but never played** (old system; weapons handle their own sounds now) |
| `sounds/menu.mp3` | AudioState.snd_menu | Menu navigation (pitch 0.9–1.1) and confirm (pitch 0.7–0.8, 1.2× volume) |
| `sounds/assault_riffle_burst.mp3` | AssaultRifle — 4 aliases | Each shot (random pitch 0.9–1.1, cycles through 4 aliases) |
| `sounds/assault_riffle_reload.mp3` | AssaultRifle | Reload start |
| `sounds/deagle_shot.mp3` | Deagle — 4 aliases | Each shot (random pitch, 1.4× volume) |
| `sounds/deagle_reload.mp3` | Deagle | Reload start |
| `sounds/saber_ignition.mp3` | Saber | On pickup |
| `sounds/saber_humming.mp3` | Saber (MusicStream, looping) | Continuously while held; stops on drop |
| `sounds/saber_attack.mp3` | Saber | Each swing (random pitch 0.95–1.05) |
| `sounds/dagger_draw.mp3` | Dagger | On pickup |
| `sounds/dagger_attack.mp3` | Dagger — 4 aliases | Each stab (random pitch 0.85–1.15) |
| `sounds/dagger_flesh.mp3` | Dagger | On successful melee hit (random pitch 0.9–1.1) |
| `sounds/attack_hit.mp3` | — | **Asset exists but is never loaded** |

---

## Settings System

Saved to `%APPDATA%\wrongfloor\settings.json` (Windows) or `~/.wrongfloor/settings.json` (Unix). Auto-created with defaults on first run.

| Key | Type | Default | Notes |
|-----|------|---------|-------|
| `res_idx` | int | 0 | Index into `RESOLUTIONS[]`: 1280×720, 1600×900, 1920×1080, 2560×1440 |
| `window_mode` | int | 0 | 0=Windowed, 1=Borderless, 2=Fullscreen |
| `vsync` | int | 1 | 1=on (no target fps cap), 0=off |
| `master_vol` | float | 0.8 | Applied to all sounds |
| `sfx_vol` | float | 0.8 | Multiplied with master for all SFX |
| `music_vol` | float | 0.8 | **Stored and applied to saber humming stream; no BGM system exists yet** |
| `show_fps` | int | 0 | Draws raylib FPS counter at top-right of virtual screen |

Applied via `settings_apply_display()` after `InitWindow`. Saved on: Back from options, exit to main menu, and app close.

---

## Update Order (main.cpp game loop)

```
1. GetFrameTime() → real_dt; dt = real_dt × time_scale
2. Compute virtual mouse (raw mouse → 1280×720 space)
3. [DEV_MODE] debug_update() — may consume Escape, returns input_blocked
4. State machine switch:
   ├─ MAIN_MENU: main_menu_update()
   ├─ PLAYING:
   │   ├─ Death slow-mo timer (real time) → replay_trigger() after 0.2 s
   │   ├─ Pause toggle (blocked during replay)
   │   ├─ if paused: pause_menu_update()
   │   └─ else: gameplay_update()
   │       ├─ if replay active: replay_update() → reload level on finish
   │       ├─ if player_dead (slow-mo): bullets_update, effects_update, sound_events_update only
   │       └─ normal: player → weapons → bullets → collision → effects → enemies → camera → audio → level_transition
   ├─ OPTIONS_MAIN: options_menu_update()
   └─ OPTIONS_PAUSE: options_menu_update()
```

## Draw Order (main.cpp, per frame)

```
BeginDrawing() → ClearBackground(BLACK)
│
├─ [PLAYING] BeginTextureMode(capture_rt)          ← full-res game world
│   ├─ BeginMode2D(camera)
│   │   ├─ tilemap background layers
│   │   ├─ tilemap midground layers
│   │   ├─ effects_draw_stains (blood pools)
│   │   ├─ enemies_draw (enemy sprites + weapons)
│   │   ├─ weapons_draw_ground (ground/thrown weapons)
│   │   ├─ player_draw (animated character sprite)
│   │   ├─ weapons_draw_held (held weapon over player)
│   │   ├─ bullets_draw (animated glow bullets)
│   │   ├─ effects_draw_pixels (flying blood pixels)
│   │   ├─ tilemap foreground layers
│   │   ├─ weapons_draw_hud (reload bar, world-space)
│   │   ├─ player_draw_crosshair
│   │   └─ [DEV_MODE] debug_draw_world (colliders, cones, etc.)
│   └─ EndMode2D()
│   └─ gameplay_draw_hud (ammo counter, screen-space)
│   └─ EndTextureMode()
│   └─ replay_capture_frame (if allowed)
│
├─ BeginTextureMode(virtual_rt)                    ← 1280×720 composition
│   ├─ if replay active: replay_draw() (VHS shader or BLACK)
│   └─ else: blit capture_rt (flipped) → virtual_rt
│   ├─ if paused: pause_menu_draw (overlay)
│   ├─ [MAIN_MENU]: main_menu_draw
│   ├─ [OPTIONS_*]: options_menu_draw
│   ├─ [DEV_MODE] debug_draw_ui (console, FPS, blood count)
│   └─ show_fps → DrawFPS
│   └─ EndTextureMode()
│
└─ DrawTexturePro(virtual_rt → screen, letterboxed)
EndDrawing()
```

---

## TODOs, Known Issues & Incomplete Features

- **`PLAYER_WALKING` state** — declared and commented as "reserved for future use"; never set in code. `snd_walk` is loaded and stop-called for this state but never played.
- **`snd_land` and `snd_attack`** — loaded in `audio_init` but never played. Remnants of an older system.
- **`attack_hit.mp3`** — asset exists in `assets/sounds/` but is never loaded or played.
- **`music_volume`** — stored in settings and respected by the saber hum stream, but no background music system exists.
- **No BGM** — `audio.h` notes `music_volume` is "reserved for future BGM."
- **Enemy melee weapons** — enemies default to random AssaultRifle or Deagle. Saber and dagger can now be assigned via the Tiled `weapon_type` property, but no levels currently use this.
- **Enemy instant reload** — when an enemy runs out of ammo, its magazine is silently reset to full (`current_ammo = magazine_size()`) with no delay or sound.
- **Only 2 levels** — `LEVEL_COUNT = 2`; level cycling wraps back to level_01 after level_02.
- **`weapons_save_held` / `weapons_restore_held`** — used for cross-level weapon persistence when hitting the level exit, but the `Restart` path in the pause menu calls `gameplay_reload_level` directly, which calls `weapons_clear` without saving, so the player loses their weapon on manual restart (intentional reset behavior).
- **`effects.cpp` static vectors** — `s_fountains` and `s_ooze_sources` are module-level statics. They're cleared by `effects_clear` (called on level load) but not cleared on program exit, which is fine since they contain no GPU resources.
- **Duplicate `sound_debug` / `showSoundEvents` commands** — both `sound_debug` (registered in `main.cpp`) and `showSoundEvents` (registered in `debug.cpp`) toggle the same `show_sound_events` flag. Both work; just slightly redundant.
- **`player_prepare_draw`** — currently a no-op (comment: "attack shader removed; nothing to prepare"). Called every frame before drawing; safe to remove if never given a purpose.
- **Gamepad right-trigger throw** — `GAMEPAD_BUTTON_RIGHT_TRIGGER_1` is used for throw; on most controllers this is the right bumper (RB/R1), not the analog trigger. The analog trigger is `GAMEPAD_AXIS_RIGHT_TRIGGER` (already used for fire). This may not match player expectations on all controllers.
