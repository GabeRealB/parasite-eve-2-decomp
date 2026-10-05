#include "gameplay/message.h"

#include "main/gfx.h"
#include "main/random.h"

/* Part of the water effects library; see water_effects.h. */

/// Seeds a new water ripple's size, brightness and local surface orientation.
///
/// `task` is borrowed read-only; `spawnArg1` bits 0..11 supply the initial
/// local half-side in game coordinate units (0..4095). `rippleWork` and
/// `surfaceCoord` must be live, writable objects. Stores that half-side in
/// `EffectWork::angle` and RGB brightness 64 in `EffectWork::scale`, where
/// 128 is neutral texture modulation.
///
/// Consumes one draw from `gRandomLcgState`, selecting a yaw in 0..4095
/// at 4096 units per turn. Replaces the local rotation with a pure Y rotation,
/// preserving translation, and marks the coordinate dirty. The cached
/// composition is retained until the caller rebuilds it. Requires the
/// initialized scratch stack to have room for the rotation helper's block;
/// all pointers are borrowed for this call, with none retained.
static inline void _waterInitializeRipple(const Task* task, EffectWork* rippleWork, GfxCoord* surfaceCoord)
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

/// Advances and draws one expanding, fading water-surface ripple.
///
/// `task` must be a live counted effect with an owned `EffectWork` in
/// `spawnArg2.pointer` and one coordinate body. Its initial state is zero;
/// `spawnArg1` bits 0..11 give the initial local half-side in game coordinate
/// units (0..4095); all higher bits are ignored. This task stores half-size
/// in `EffectWork::angle` and RGB brightness in `EffectWork::scale`.
///
/// Running updates compose the coordinate and increment `age`. The first
/// initializes brightness to 64 and replaces local rotation with a random Y
/// rotation. Each running update grows the half-side by 32, draws, then dims
/// by 2. A fresh ripple lasts 32 running updates, drawing half-sides 32..5119
/// and brightness 64..2; 128 is neutral texture modulation.
///
/// Every non-running control value redraws the retained size and brightness
/// without composing, initializing or aging. Cancellation draws once before
/// retirement. Cancellation and brightness below 2 release the work, decrement
/// the live-effect count and tear down the task and its coordinate body.
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
        // Compose before initialization: the first draw uses the pre-yaw matrix.
        actorRenderComposeCoord(surfaceCoord);
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
