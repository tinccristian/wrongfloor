#include "weapon.h"
#include <algorithm>
#include <cmath>

// Weapon is an abstract base class. All concrete behavior lives in subclasses
// (assault_rifle.cpp, etc.) and shared update logic lives in weapon_manager.cpp.

bool weapon_swing_update(Weapon *w, float dt)
{
    if (!w || !w->is_swinging) return false;

    w->swing_timer += dt;
    float dur = w->swing_duration();
    float t   = (dur > 0.0f) ? std::min(w->swing_timer / dur, 1.0f) : 1.0f;

    float env = sinf(t * 3.14159f);
    w->swing_rotation_offset = w->swing_peak_angle() * env;
    w->melee_forward_offset  = w->swing_peak_fwd()   * env;

    bool hit_frame = false;
    if (!w->melee_hit_triggered && t >= 0.40f)
    {
        w->melee_hit_triggered = true;
        hit_frame = true;
    }

    if (t >= 1.0f)
    {
        w->is_swinging           = false;
        w->swing_rotation_offset = 0.0f;
        w->melee_forward_offset  = 0.0f;
    }

    return hit_frame;
}
