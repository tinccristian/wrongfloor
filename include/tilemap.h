#pragma once

#include "raylib.h"
#include <string>
#include <vector>

// World-space size of one tile in pixels. Matches the Tiled tile size.
inline constexpr int TILE_SIZE = 32;

struct TileObject {
    std::string name;
    float x, y, width, height;
};

struct TileLayer {
    std::string      name;
    int              width  = 0;
    int              height = 0;
    std::vector<int> data;  // tile IDs; 0 = empty
};

struct Tilemap {
    int map_width  = 0;  // in tiles
    int map_height = 0;
    int tile_width  = TILE_SIZE;
    int tile_height = TILE_SIZE;
    int tileset_columns  = 7;
    int tileset_firstgid = 1;

    Texture2D tileset{};

    std::vector<TileLayer>  tile_layers;
    std::vector<TileObject> objects;
};

// Parse a Tiled .tmj file and load the referenced tileset texture.
bool tilemap_load(Tilemap *tm, const std::string& path);

// Draw every non-empty tile in the named layer. No-op if layer is absent.
void tilemap_draw_layer(const Tilemap *tm, const std::string& name);

// Returns true if the tile at (tile_x, tile_y) in the "collision" layer is non-zero.
bool tilemap_is_solid(const Tilemap *tm, int tile_x, int tile_y);

// Returns the position of the "player_spawn" point object. Falls back to {64, 64}.
Vector2 tilemap_get_spawn_point(const Tilemap *tm);

// Returns a pointer to the named object, or nullptr if not found.
const TileObject* tilemap_get_object(const Tilemap *tm, const std::string& name);

// Unload the tileset texture and clear all layer data.
void tilemap_unload(Tilemap *tm);
