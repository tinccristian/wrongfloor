#include "tilemap.h"
#include <fstream>
#include <sstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

bool tilemap_load(Tilemap *tm, const std::string& path)
{
    std::ifstream f(path);
    if (!f.is_open())
    {
        TraceLog(LOG_ERROR, "TILEMAP: Cannot open '%s'", path.c_str());
        return false;
    }

    json j;
    try { j = json::parse(f); }
    catch (const json::parse_error& e)
    {
        TraceLog(LOG_ERROR, "TILEMAP: JSON parse error in '%s': %s", path.c_str(), e.what());
        return false;
    }

    tm->map_width   = j.value("width",       0);
    tm->map_height  = j.value("height",      0);
    tm->tile_width  = j.value("tilewidth",  TILE_SIZE);
    tm->tile_height = j.value("tileheight", TILE_SIZE);

    // ── Tileset ───────────────────────────────────────────────────────
    if (j.contains("tilesets") && !j["tilesets"].empty())
    {
        const auto& ts = j["tilesets"][0];
        tm->tileset_firstgid = ts.value("firstgid", 1);
        tm->tileset_columns  = ts.value("columns",  7);

        // Resolve the image path relative to the .tmj file's directory.
        std::string dir      = path.substr(0, path.find_last_of("/\\") + 1);
        std::string img      = ts.value("image", "DeepCaveTile.png");
        std::string img_base = img.substr(img.find_last_of("/\\") + 1);
        std::string img_path = dir + img_base;

        tm->tileset = LoadTexture(img_path.c_str());
        if (tm->tileset.id == 0)
            TraceLog(LOG_WARNING, "TILEMAP: Tileset texture not found: '%s'", img_path.c_str());
    }

    // ── Layers ────────────────────────────────────────────────────────
    if (!j.contains("layers")) return true;

    for (const auto& layer : j["layers"])
    {
        std::string type = layer.value("type", "");
        std::string name = layer.value("name", "");

        if (type == "tilelayer")
        {
            TileLayer tl;
            tl.name   = name;
            tl.width  = layer.value("width",  tm->map_width);
            tl.height = layer.value("height", tm->map_height);

            if (layer.contains("encoding") && layer["encoding"] == "csv")
            {
                // Tiled CSV encoding: the data field is a comma-separated string.
                std::string csv = layer["data"].get<std::string>();
                std::stringstream ss(csv);
                std::string token;
                while (std::getline(ss, token, ','))
                {
                    try { tl.data.push_back(std::stoi(token)); }
                    catch (...) { tl.data.push_back(0); }
                }
            }
            else if (layer.contains("data") && layer["data"].is_array())
            {
                tl.data = layer["data"].get<std::vector<int>>();
            }

            tm->tile_layers.push_back(std::move(tl));
        }
        else if (type == "objectgroup")
        {
            if (!layer.contains("objects")) continue;
            for (const auto& obj : layer["objects"])
            {
                TileObject to;
                to.name   = obj.value("name",   "");
                to.x      = obj.value("x",      0.0f);
                to.y      = obj.value("y",      0.0f);
                to.width  = obj.value("width",  0.0f);
                to.height = obj.value("height", 0.0f);
                tm->objects.push_back(std::move(to));
            }
        }
    }

    return true;
}

static void draw_layer(const Tilemap *tm, const TileLayer& layer)
{
    for (int ty = 0; ty < layer.height; ty++)
    for (int tx = 0; tx < layer.width;  tx++)
    {
        int id = layer.data[ty * layer.width + tx];
        if (id <= 0) continue;

        int local   = id - tm->tileset_firstgid;
        int src_col = local % tm->tileset_columns;
        int src_row = local / tm->tileset_columns;

        Rectangle src = {
            (float)(src_col * tm->tile_width),
            (float)(src_row * tm->tile_height),
            (float)tm->tile_width,
            (float)tm->tile_height
        };
        Vector2 dest = {
            (float)(tx * tm->tile_width),
            (float)(ty * tm->tile_height)
        };

        DrawTextureRec(tm->tileset, src, dest, WHITE);
    }
}

void tilemap_draw_layer(const Tilemap *tm, const std::string& name)
{
    if (!tm) return;
    for (const auto& layer : tm->tile_layers)
        if (layer.name == name) { draw_layer(tm, layer); break; }
}

void tilemap_draw_layers_prefixed(const Tilemap *tm, const std::string& prefix)
{
    if (!tm) return;
    // Tiled exports layers top-to-bottom (foreground first, deepest background last).
    // Draw in reverse so the bottommost layer in Tiled (last in file) renders behind.
    for (int i = (int)tm->tile_layers.size() - 1; i >= 0; i--)
    {
        const auto& layer = tm->tile_layers[i];
        if (layer.name.rfind(prefix, 0) == 0)
            draw_layer(tm, layer);
    }
}

bool tilemap_is_solid(const Tilemap *tm, int tile_x, int tile_y)
{
    if (!tm) return false;
    if (tile_x < 0 || tile_y < 0 || tile_x >= tm->map_width || tile_y >= tm->map_height)
        return false;

    for (const auto& layer : tm->tile_layers)
    {
        if (layer.name != "collision") continue;
        int idx = tile_y * layer.width + tile_x;
        if (idx < 0 || idx >= (int)layer.data.size()) return false;
        return layer.data[idx] != 0;
    }
    return false;
}

Vector2 tilemap_get_spawn_point(const Tilemap *tm)
{
    if (tm)
    {
        for (const auto& obj : tm->objects)
            if (obj.name == "player_spawn")
                return { obj.x, obj.y };
    }
    return { 64.0f, 64.0f };
}

const TileObject* tilemap_get_object(const Tilemap *tm, const std::string& name)
{
    if (!tm) return nullptr;
    for (const auto& obj : tm->objects)
        if (obj.name == name) return &obj;
    return nullptr;
}

void tilemap_unload(Tilemap *tm)
{
    if (!tm || tm->tileset.id == 0) return;
    UnloadTexture(tm->tileset);
    tm->tile_layers.clear();
    tm->objects.clear();
    tm->tileset    = Texture2D{};
    tm->map_width  = 0;
    tm->map_height = 0;
}
