#include <gameplay/message.h>

#include <engine/gfx.h>
#include <engine/random.h>

/* Part of the water effects library; see water.h. */

/// Starts a ripple: its half-size from `spawnArg1` bits 0..11, brightness 64 and a random yaw on its coordinate.
static inline void _waterInitializeRipple(const Task* task, EffectWork* rippleWork, GfxCoord* surfaceCoord)
{
    enum {
        WATER_RIPPLE_INITIAL_BRIGHTNESS = 0x40,
        WATER_RIPPLE_SPAWN_SIZE_MASK    = 0xFFF,
    };
    u32 surfaceYaw;

    rippleWork->scale = WATER_RIPPLE_INITIAL_BRIGHTNESS;
    rippleWork->angle = task->spawnArg1.halves.low & WATER_RIPPLE_SPAWN_SIZE_MASK;
    RANDOM_LCG_ADVANCE();
    surfaceYaw = RANDOM_LCG_DRAW() & ACTOR_TRANSFORM_ANGLE_MASK;
    gfxRotMatrixY(&surfaceCoord->coord, surfaceYaw, GRAPHICS_ROTATION_REPLACE);
    surfaceCoord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Draws a splash whose half-size grows by 32 and brightness drops by 2 each update, retiring after 32 updates. While
/// effects are paused it redraws unchanged; cancellation draws it once more and retires it. With
/// `WATER_RIPPLE_CACHED_COORD` defined it draws the coordinate as cached, which the model-list pass composes, so a new
/// yaw shows a frame later.
static inline void _waterRippleTask(Task* task)
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
#ifndef WATER_RIPPLE_CACHED_COORD
        // Compose before initialization: the first draw uses the pre-yaw matrix.
        actorRenderComposeCoord(surfaceCoord);
#endif
        rippleWork->age++;
        if (task->state == WATER_RIPPLE_STATE_NEW) {
            _waterInitializeRipple(task, rippleWork, surfaceCoord);
            task->state = WATER_RIPPLE_STATE_ACTIVE;
        }
        rippleWork->angle += WATER_RIPPLE_HALF_SIZE_PER_UPDATE;
        _waterDrawSplash(surfaceCoord, rippleWork->angle, rippleWork->scale);
        rippleWork->scale -= WATER_RIPPLE_FADE_PER_UPDATE;
        if (rippleWork->scale < WATER_RIPPLE_MIN_DRAW_BRIGHTNESS) {
            effectKillTask(rippleWork, task);
        }
    }
}
