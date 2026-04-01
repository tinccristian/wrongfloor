#include "animation.h"
#include <cmath>

void animation_player_set(AnimationPlayer *ap, const Animation *anim)
{
    if (ap->current == anim) return;
    ap->current     = anim;
    ap->frame_index = 0;
    ap->elapsed     = 0.0f;
}

void animation_player_update(AnimationPlayer *ap, float dt)
{
    if (!ap->current || ap->current->frame_count == 0) return;

    ap->elapsed += dt;
    while (ap->elapsed >= ap->current->frame_duration)
    {
        ap->elapsed -= ap->current->frame_duration;
        ap->frame_index++;
        if (ap->frame_index >= ap->current->frame_count)
            ap->frame_index = ap->current->loops ? 0 : ap->current->frame_count - 1;
    }
}

void animation_player_draw(const AnimationPlayer *ap, Vector2 position, float scale)
{
    animation_player_draw_rotated(ap, position, scale, 0.0f, Vector2{0.0f, 0.0f});
}

void animation_player_draw_rotated(const AnimationPlayer *ap, Vector2 position, float scale,
                                   float rotation_deg, Vector2 origin)
{
    if (!ap->current || !ap->current->texture || ap->current->frame_count == 0) return;

    Rectangle source = ap->current->frames[ap->frame_index];
    if (ap->flip_h)
    {
        // Negate width only — DrawTexturePro swaps UVs internally for a clean horizontal flip.
        // Do NOT offset source.x; that shifts the sample region to the wrong column.
        source.width = -source.width;
    }

    float fw = ap->current->frames[ap->frame_index].width  * scale;
    float fh = ap->current->frames[ap->frame_index].height * scale;
    Rectangle dest = { position.x, position.y, fw, fh };

    DrawTexturePro(*ap->current->texture, source, dest, origin, rotation_deg, WHITE);
}
