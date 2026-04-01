#include "camera.h"

void camera_init(GameCamera *gc, Vector2 target, int screen_w, int screen_h)
{
    gc->cam.target   = target;
    gc->cam.offset   = { screen_w * 0.5f, screen_h * 0.5f };
    gc->cam.rotation = 0.0f;
    gc->cam.zoom     = 1.0f;
}

void camera_update(GameCamera *gc, Vector2 target, const Tilemap *tm,
                   int screen_w, int screen_h, float dt)
{
    (void)dt;
    gc->cam.target = target;

    // Clamp so the viewport never shows past map edges
    if (tm && tm->map_width > 0 && tm->map_height > 0)
    {
        float map_px_w = (float)(tm->map_width  * tm->tile_width);
        float map_px_h = (float)(tm->map_height * tm->tile_height);
        float half_w   = screen_w  * 0.5f / gc->cam.zoom;
        float half_h   = screen_h  * 0.5f / gc->cam.zoom;

        if (gc->cam.target.x < half_w)             gc->cam.target.x = half_w;
        if (gc->cam.target.x > map_px_w - half_w)  gc->cam.target.x = map_px_w - half_w;
        if (gc->cam.target.y < half_h)             gc->cam.target.y = half_h;
        if (gc->cam.target.y > map_px_h - half_h)  gc->cam.target.y = map_px_h - half_h;
    }
}
