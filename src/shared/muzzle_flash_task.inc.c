#include "main/random.h"

/* Part of the muzzle flash library; see muzzle_flash.h. */

/// Activates a transient white point light at the effect's initial position.
///
/// `spawnCoord` supplies its local translation in the light's parent frame;
/// the flash calls this while both coordinates are parented to the view,
/// before attaching the effect to the weapon's muzzle. The slot must already
/// be initialized with that borrowed parent. Full Q12 white intensity extends
/// to 4000 world units and falls to zero at 4800; the slot expires after four
/// unpaused light updates. Placement invalidates its cached transform for the
/// next composition pass. The slot is shared storage that another effect can
/// overwrite; no pointer is retained by this helper.
static inline void _muzzleFlashInitializeLight(const GfxCoord* spawnCoord, WorldCoordTransientPointLight* transientLight)
{
    enum {
        MUZZLE_FLASH_LIGHT_INNER_RADIUS = 0xFA0,
        MUZZLE_FLASH_LIGHT_OUTER_RADIUS = 0x12C0,
        MUZZLE_FLASH_LIGHT_FRAME_COUNT  = 4
    };
    WorldCoordPointLight* pointLight = &transientLight->light;

    pointLight->head.transform.coord.coord.t[0]   = spawnCoord->coord.t[0];
    pointLight->head.transform.coord.coord.t[1]   = spawnCoord->coord.t[1];
    pointLight->head.transform.coord.coord.t[2]   = spawnCoord->coord.t[2];
    pointLight->head.transform.coord.composeStamp = GRAPHICS_COORD_DIRTY;
    pointLight->head.color.r                      = ONE;
    pointLight->head.color.g                      = ONE;
    pointLight->head.color.b                      = ONE;
    pointLight->inner                             = MUZZLE_FLASH_LIGHT_INNER_RADIUS;
    pointLight->outer                             = MUZZLE_FLASH_LIGHT_OUTER_RADIUS;
    transientLight->framesLeft                    = MUZZLE_FLASH_LIGHT_FRAME_COUNT;
}

/// Advances the seven-update flash, its additive screen tint and its point light.
///
/// `task` owns the live EffectWork in spawnArg2.pointer and a coordinate body.
/// Its borrowed spawn coordinate and ancestors must survive the flash. The
/// first visible update attaches the body at the carrier's muzzle offset,
/// initializes size 1536..2559, spin 0..4095 and brightness 192, and retains
/// one random streak angle in each quadrant. Later visible updates halve size
/// and brightness; EffectWork.scale, angle and period hold those values.
/// Hidden effects suspend this task completely, including age and cleanup.
/// Each update draws before freeing the work and task at age seven. Transient
/// light slot zero is shared and may be overwritten by another effect; its
/// four-frame expiry is maintained separately from the task's lifetime.
static inline void _muzzleFlashTask(Task* task)
{
    enum {
        MUZZLE_FLASH_STATE_INITIALIZE   = 0,
        MUZZLE_FLASH_STATE_FADE         = 1,
        MUZZLE_FLASH_LIGHT_SLOT         = 0,
        MUZZLE_FLASH_LIGHT_RADIUS_STEP  = 0x190,
        MUZZLE_FLASH_FRAME_COUNT        = 7,
        MUZZLE_FLASH_INITIAL_BRIGHTNESS = 0xC0,
        MUZZLE_FLASH_MIN_WORLD_SIZE     = 0x600,
        MUZZLE_FLASH_QUADRANT_SHIFT     = 10 // 1024 angle units per streak quadrant
    };
    EffectWork*                    work;
    GfxCoord*                      muzzleCoord;
    WorldCoordTransientPointLight* transientLight;
    WorldCoordPointLight*          pointLight;
    u8                             tintRgb[3];
    s32                            streakIndex;

    work           = task->spawnArg2.pointer;
    muzzleCoord    = task->extra.coordBody->coord;
    transientLight = &gWorldCoordTransientPointLights[MUZZLE_FLASH_LIGHT_SLOT];
    pointLight     = &transientLight->light;

    if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_HIDDEN) {
        return;
    }

    work->age++;
    switch (task->state) {
        case MUZZLE_FLASH_STATE_INITIALIZE:
            // Start the light at the spawn position before applying the muzzle offset.
            _muzzleFlashInitializeLight(muzzleCoord, transientLight);

            // Attach and compose once; normal coordinate updates refresh it thereafter.
            muzzleCoord->parent       = work->parent;
            muzzleCoord->coord.t[0]   = _gMuzzleOffset.vx;
            muzzleCoord->coord.t[1]   = _gMuzzleOffset.vy;
            muzzleCoord->coord.t[2]   = _gMuzzleOffset.vz;
            muzzleCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(muzzleCoord);

            work->period    = MUZZLE_FLASH_INITIAL_BRIGHTNESS;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->scale     = ((gRandomLcgState >> 16) & (MUZZLE_FLASH_ANGLE_QUARTER_TURN - 1)) + MUZZLE_FLASH_MIN_WORLD_SIZE;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->angle     = (gRandomLcgState >> 16) & MUZZLE_FLASH_ANGLE_MASK;
            task->state     = MUZZLE_FLASH_STATE_FADE;
            for (streakIndex = 0; streakIndex < ARRAY_SIZE(gMuzzleFlashAngles); streakIndex++) {
                gRandomLcgState                 = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                gMuzzleFlashAngles[streakIndex] = ((streakIndex & (ARRAY_SIZE(gMuzzleFlashAngles) - 1)) << MUZZLE_FLASH_QUADRANT_SHIFT) + ((gRandomLcgState >> 16) & (MUZZLE_FLASH_ANGLE_QUARTER_TURN - 1));
            }
            break;
        case MUZZLE_FLASH_STATE_FADE:
            work->scale  = work->scale >> 1;
            work->period = work->period >> 1;
            break;
    }

    _muzzleFlashDrawCore(muzzleCoord, work->scale, work->angle);
    // One byte supplies all tint channels; preserve the single shared load.
    tintRgb[0] = tintRgb[1] = tintRgb[2] = work->period;
    effectDrawScreenTint(tintRgb, GPU_BLEND_ADD);
    for (streakIndex = 0; streakIndex < ARRAY_SIZE(gMuzzleFlashAngles); streakIndex++) {
        _muzzleFlashDrawStreak(muzzleCoord, gMuzzleFlashAngles[streakIndex], work->period);
    }
    // Shrink only the full-strength radius; the zero-contribution radius stays fixed.
    if (pointLight->inner >= MUZZLE_FLASH_LIGHT_RADIUS_STEP + 1) {
        pointLight->inner -= MUZZLE_FLASH_LIGHT_RADIUS_STEP;
    }
    if (work->age >= MUZZLE_FLASH_FRAME_COUNT) {
        effectKillTask(work, task);
    }
}
