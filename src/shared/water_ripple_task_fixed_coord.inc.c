#include "gameplay/message.h"

#include "main/gfx.h"
#include "main/random.h"

/* Part of the water effects library; see water_effects.h. */

#ifndef WATER_RIPPLE_CACHED_COORD_TASK
#error "Define WATER_RIPPLE_CACHED_COORD_TASK to the package's declared void (Task*) callback"
#endif

/// Seeds a water ripple's half-side, brightness and yaw without rebuilding its draw matrix.
///
/// `task` is borrowed read-only; bits 0..11 of `spawnArg1` supply the initial
/// half-side in local game coordinate units (0..4095), with higher bits ignored.
/// `rippleWork` and `surfaceCoord` must be live, writable objects. Stores the
/// half-side in `EffectWork::angle` and RGB brightness 64 in `EffectWork::scale`;
/// 128 is neutral texture modulation. Other work fields and task state stay intact.
///
/// Advances `gRandomLcgState` once with unsigned 32-bit wraparound and selects
/// a yaw in 0..4095, at 4096 units per turn. Replaces the local-to-parent
/// rotation with a pure Y rotation, preserving translation, parent and stored
/// Euler angles. Marks the coordinate dirty but leaves `workm` intact for the
/// current draw; the caller must arrange a later composition to apply the yaw.
///
/// Requires an initialized, word-aligned scratch stack with 0x24 free bytes,
/// reserved only until the rotation helper returns. Caller objects must be
/// disjoint from that reservation. Ownership is unchanged; no pointer is retained.
static inline void _waterInitializeCachedRipple(const Task* task, EffectWork* rippleWork, GfxCoord* surfaceCoord)
{
    enum {
        WATER_RIPPLE_INITIAL_BRIGHTNESS = 0x40,
        WATER_RIPPLE_SPAWN_SIZE_MASK    = 0xFFF,
    };
    u32 surfaceYaw;

    rippleWork->scale = WATER_RIPPLE_INITIAL_BRIGHTNESS;
    rippleWork->angle = task->spawnArg1.halves.low & WATER_RIPPLE_SPAWN_SIZE_MASK;
    gRandomLcgState   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    surfaceYaw        = (gRandomLcgState >> 16) & ACTOR_TRANSFORM_ANGLE_MASK;
    gfxRotMatrixY(&surfaceCoord->coord, surfaceYaw, GRAPHICS_ROTATION_REPLACE);
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
