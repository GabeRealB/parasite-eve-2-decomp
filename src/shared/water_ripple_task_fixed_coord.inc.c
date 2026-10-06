#include "gameplay/message.h"

#include "main/gfx.h"
#include "main/random.h"

/* Part of the water effects library; see water_effects.h. */

#ifndef WATER_RIPPLE_CACHED_COORD_TASK
#error "Define WATER_RIPPLE_CACHED_COORD_TASK to the package's declared void (Task*) callback"
#endif

/// Initializes a ripple's half-side, brightness and local yaw, retaining its draw cache.
///
/// Borrows a live task, writable effect work and coordinate. Spawn bits 0..11
/// supply a half-side in local coordinate units (0..4095). Brightness 64 is
/// half of neutral texture modulation. Consumes one LCG draw for a yaw in
/// 4096 units per turn, replaces local rotation without changing translation,
/// and marks the cache dirty for a later composition pass. No pointer is retained.
static inline void _waterInitializeCachedRipple(const Task* task, EffectWork* rippleWork, GfxCoord* surfaceCoord)
{
    enum {
        WATER_RIPPLE_INITIAL_BRIGHTNESS = 0x40,
        WATER_RIPPLE_SPAWN_SIZE_MASK    = 0xFFF,
    };

    rippleWork->scale = WATER_RIPPLE_INITIAL_BRIGHTNESS;
    rippleWork->angle = task->spawnArg1.halves.low & WATER_RIPPLE_SPAWN_SIZE_MASK;
    gRandomLcgState   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    gfxRotMatrixY(&surfaceCoord->coord, (gRandomLcgState >> 16) & ACTOR_TRANSFORM_ANGLE_MASK, GRAPHICS_ROTATION_REPLACE);
    surfaceCoord->composeStamp = GRAPHICS_COORD_DIRTY;
}

void WATER_RIPPLE_CACHED_COORD_TASK(Task* task)
{
    enum {
        WATER_RIPPLE_STATE_NEW            = 0,
        WATER_RIPPLE_STATE_ACTIVE         = 1,
        WATER_RIPPLE_HALF_SIZE_PER_UPDATE = 0x20,
        WATER_RIPPLE_FADE_PER_UPDATE      = 2,
        WATER_RIPPLE_MIN_DRAW_BRIGHTNESS  = 2,
    };
    EffectWork* rippleWork;
    GfxCoord*   surfaceCoord;

    rippleWork   = task->spawnArg2.pointer;
    surfaceCoord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        // Frozen effects still redraw, including the cancellation update.
        _waterDrawSplash(surfaceCoord, rippleWork->angle, rippleWork->scale);
        if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            effectKillTask(rippleWork, task);
        }
    } else {
        rippleWork->age++;
        if (task->state == WATER_RIPPLE_STATE_NEW) {
            _waterInitializeCachedRipple(task, rippleWork, surfaceCoord);
            task->state = WATER_RIPPLE_STATE_ACTIVE;
        }
        // Draw the existing cache; the model-list pass later picks up the new yaw.
        rippleWork->angle += WATER_RIPPLE_HALF_SIZE_PER_UPDATE;
        _waterDrawSplash(surfaceCoord, rippleWork->angle, rippleWork->scale);
        rippleWork->scale -= WATER_RIPPLE_FADE_PER_UPDATE;
        if (rippleWork->scale < WATER_RIPPLE_MIN_DRAW_BRIGHTNESS) {
            effectKillTask(rippleWork, task);
        }
    }
}
