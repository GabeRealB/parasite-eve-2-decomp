#include "weapons/gunblade.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

// Keep these first declarations before the cached handles to preserve BSS order.
static GfxCoord _gGunbladeTrailNearHistory[8];
static GfxCoord _gGunbladeTrailFarHistory[8];

#include "gunblade_private.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/scratch.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "../../shared/blade_trail.h"
#include "gameplay/animation.h"
#include "types.h"

/// The near end of the gunblade beam inside the muzzle frame, `(0, 0x60, 0x80)`;
/// the task's own coordinate starts there. The far end follows it directly, and
/// state 0 reaches that as element 1 of this array.
static SVECTOR D_gunblade_8011E704[1] = { { 0, 0x0060, 0x0080, 0 } };

/// The far end of that pair, immediately after it. Both forms appear in
/// the original: one path reaches it as `D_gunblade_8011E704[1]`, which compiles to the
/// array's address plus 8, and another names it directly, which compiles
/// to its own address - so it has to be a separate object, not element 1.
static SVECTOR D_gunblade_8011E70C = { 0, 0x0060, 0x0380, 0 };

/// Saves a Gunblade endpoint pose in a trail frame that follows the camera independently of the weapon.
///
/// `composedEndpoint->workm` must be current in view space, and
/// `gGfxViewCoord.workm` the current orthonormal world-to-view transform.
/// Copies the complete 32-byte cache, including its alignment bytes, then
/// removes the view into the destination's local matrix and borrows the
/// persistent view node as parent. Both rotation and translation are rebased;
/// the ribbon subsequently uses only the translation. Coefficients have twelve
/// fractional bits and translations are signed game-coordinate units.
///
/// Requires live word-aligned nodes disjoint from each other and the view node,
/// and an initialized scratch stack with 48 free bytes disjoint from them.
/// Scratch is released before return; GTE rotation, translation and arithmetic
/// state change. Leaves the destination's stamp and parameters untouched, so
/// the caller must mark it dirty before recomposing. Retains no source pointer.
static inline void _gunbladeStoreTrailFrame(GfxCoord* historyFrame, const GfxCoord* composedEndpoint)
{
    historyFrame->parent = &gGfxViewCoord;
    historyFrame->workm  = composedEndpoint->workm;
    gte_SetRotMatrix(&composedEndpoint->workm);
    gte_SetTransMatrix(&composedEndpoint->workm);
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &historyFrame->workm, &historyFrame->coord);
}

void gunbladeTrailTask(Task* task)
{
    enum {
        GUNBLADE_TRAIL_SEED               = 0,
        GUNBLADE_TRAIL_RECORD             = 1,
        GUNBLADE_TRAIL_CHARGE_START_TICKS = 9,
        GUNBLADE_TRAIL_LIFETIME_TICKS     = 13,
        GUNBLADE_TRAIL_CHARGE_PENDING     = 1,
    };
    GfxCoord       farEndpoint;
    GfxCoord*      nearCoord;
    GfxCoord*      historyFrame;
    EffectWork*    effectWork;
    EffectWork*    flashWork;
    s32            keepTask;
    const SVECTOR* farOffset;
    s32            historyIndex;

    effectWork = task->spawnArg2.pointer;
    nearCoord  = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        keepTask = gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN;
    } else {
        effectWork->age++;
        switch (task->state) {
            case GUNBLADE_TRAIL_SEED:
                // Seed both histories with the current endpoint poses before drawing any ribbon.
                nearCoord->parent       = effectWork->parent;
                nearCoord->coord.t[0]   = D_gunblade_8011E704[0].vx;
                D_gunblade_8012E244     = task;
                nearCoord->coord.t[1]   = D_gunblade_8011E704[0].vy;
                D_gunblade_8012E248     = effectWork;
                nearCoord->coord.t[2]   = D_gunblade_8011E704[0].vz;
                nearCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(nearCoord);
                task->state              = GUNBLADE_TRAIL_RECORD;
                farOffset                = &D_gunblade_8011E704[1];
                farEndpoint.parent       = effectWork->parent;
                farEndpoint.coord.t[0]   = farOffset->vx;
                farEndpoint.coord.t[1]   = farOffset->vy;
                farEndpoint.coord.t[2]   = farOffset->vz;
                farEndpoint.composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(&farEndpoint);
                for (historyIndex = 0; historyIndex < ARRAY_SIZE(_gGunbladeTrailNearHistory); historyIndex++) {
                    historyFrame = &_gGunbladeTrailNearHistory[historyIndex];
                    _gunbladeStoreTrailFrame(historyFrame, nearCoord);
                    historyFrame = &_gGunbladeTrailFarHistory[historyIndex];
                    _gunbladeStoreTrailFrame(historyFrame, &farEndpoint);
                }
                return;
            case GUNBLADE_TRAIL_RECORD:
                // Record world poses so old frames follow the camera, independently of the weapon.
                nearCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(nearCoord);
                farEndpoint.parent       = effectWork->parent;
                farEndpoint.coord.t[0]   = D_gunblade_8011E70C.vx;
                farEndpoint.coord.t[1]   = D_gunblade_8011E70C.vy;
                farEndpoint.coord.t[2]   = D_gunblade_8011E70C.vz;
                farEndpoint.composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(&farEndpoint);
                historyFrame = &_gGunbladeTrailNearHistory[effectWork->age & (ARRAY_SIZE(_gGunbladeTrailNearHistory) - 1)];
                _gunbladeStoreTrailFrame(historyFrame, nearCoord);
                historyFrame = &_gGunbladeTrailFarHistory[effectWork->age & (ARRAY_SIZE(_gGunbladeTrailNearHistory) - 1)];
                _gunbladeStoreTrailFrame(historyFrame, &farEndpoint);
                for (historyIndex = 0; historyIndex < ARRAY_SIZE(_gGunbladeTrailNearHistory); historyIndex++) {
                    historyFrame               = &_gGunbladeTrailNearHistory[historyIndex];
                    historyFrame->composeStamp = GRAPHICS_COORD_DIRTY;
                    actorRenderComposeCoord(historyFrame);
                    historyFrame               = &_gGunbladeTrailFarHistory[historyIndex];
                    historyFrame->composeStamp = GRAPHICS_COORD_DIRTY;
                    actorRenderComposeCoord(historyFrame);
                }
                if (effectWork->age < GUNBLADE_TRAIL_CHARGE_START_TICKS) {
                    _bladeTrailDraw(effectWork->age & (ARRAY_SIZE(_gGunbladeTrailNearHistory) - 1), BLADE_TRAIL_TINT_BLUE_WHITE);
                    return;
                }
                // Post a deferred charge once the ribbon changes tint; repeated requests remain a counter.
                if (effectWork->index == GUNBLADE_TRAIL_CHARGE_PENDING) {
                    effectWork->index++;
                    flashWork = effectSpawn(EFFECT_GUNBLADE_CHARGE_FLASH, nearCoord, task->spawnArg1.value, NULL);
                    if (flashWork != NULL) {
                        taskReparent(task, flashWork->task);
                    }
                }
                _bladeTrailDraw(effectWork->age & (ARRAY_SIZE(_gGunbladeTrailNearHistory) - 1), BLADE_TRAIL_TINT_YELLOW_WHITE);
                keepTask = effectWork->age < GUNBLADE_TRAIL_LIFETIME_TICKS;
                break;
            default:
                return;
        }
    }
    if (!keepTask) {
        D_gunblade_8012E248 = NULL;
        effectKillTask(effectWork, task);
    }
}

#define BLADE_TRAIL_BASE_HISTORY _gGunbladeTrailNearHistory
#define BLADE_TRAIL_TIP_HISTORY  _gGunbladeTrailFarHistory
#include "../../shared/blade_trail_draw.inc.c"
#undef BLADE_TRAIL_BASE_HISTORY
#undef BLADE_TRAIL_TIP_HISTORY

enum {
    GUNBLADE_FLASH_AMMUNITION_BUCKSHOT       = 13,
    GUNBLADE_FLASH_AMMUNITION_FIREFLY        = 14,
    GUNBLADE_FLASH_AMMUNITION_SLUG           = 15,
    GUNBLADE_FLASH_INIT                      = 0,
    GUNBLADE_FLASH_DRAW                      = 1,
    GUNBLADE_FLASH_INITIAL_BRIGHTNESS        = 0xE0,
    GUNBLADE_FLASH_INITIAL_HALF_RADIUS       = 0x80,
    GUNBLADE_FLASH_DISC_RADIUS_STEP          = 0x10,
    GUNBLADE_FLASH_BAND_RADIUS_STEP          = 0x40,
    GUNBLADE_FLASH_BAND_BRIGHTNESS_STEP      = 0x10,
    GUNBLADE_FLASH_DISC_BRIGHTNESS_STEP      = 0x20,
    GUNBLADE_FLASH_BAND_BRIGHTNESS_MIN       = 0x11,
    GUNBLADE_FLASH_DISC_BRIGHTNESS_MIN       = 0x20,
    GUNBLADE_FLASH_BAND_WIDTH                = 0x60,
    GUNBLADE_FLASH_IMPACT_RADIUS             = 0x600,
    GUNBLADE_FLASH_BAND_BUCKSHOT_ARGUMENT    = 0x10000,
    GUNBLADE_FLASH_BAND_FIREFLY_ARGUMENT     = 0x20000,
    GUNBLADE_FLASH_BAND_SLUG_ARGUMENT        = 0x30000,
    GUNBLADE_FLASH_BAND_SECOND_ANGLE         = 0x2AA,
    GUNBLADE_FLASH_BAND_THIRD_ANGLE          = 0x555,
    GUNBLADE_FLASH_SPARK_COUNT               = 8,
    GUNBLADE_FLASH_BOUNCING_SPARK_COUNT      = 4,
    GUNBLADE_FLASH_BOUNCING_SPARK_SPEED_MASK = 0x3F,
    GUNBLADE_FLASH_BOUNCING_SPARK_COLOR      = 0x100,
    GUNBLADE_FLASH_SLUG_SPARK_ARGUMENT       = 1,
};

/// Draws the bright charge bands and screen tint, then advances their fade and radius.
///
/// Borrows live effect work and a composed centre; RGB has three already
/// narrowed bytes. The odd-age band swaps inner radius and width deliberately.
/// Keeps signed-halfword radius narrowing and writes period/step after drawing.
/// Pointer arguments repeat and must have no side effects; captures no locals.
/// The block has no return/break and updates only this flash's period/step.
#define GUNBLADE_DRAW_CHARGE_BANDS(effectWork, coord, rgb)                                                     \
    {                                                                                                          \
        effectDrawOuterGlowBand(coord, (s16)((effectWork)->step * 3 / 2), GUNBLADE_FLASH_BAND_WIDTH, rgb);     \
        if ((effectWork)->age & 1) {                                                                           \
            effectDrawOuterGlowBand(coord, GUNBLADE_FLASH_BAND_WIDTH, (s16)((effectWork)->step * 3 / 2), rgb); \
        }                                                                                                      \
        effectDrawScreenTint(rgb, GPU_BLEND_ADD);                                                              \
        (effectWork)->period -= GUNBLADE_FLASH_BAND_BRIGHTNESS_STEP;                                           \
        (effectWork)->step   += GUNBLADE_FLASH_BAND_RADIUS_STEP;                                               \
    }

void gunbladeChargeFlashTask(Task* task)
{
    EffectWork* effectWork;
    GfxCoord*   coord;
    u8          rgb[3];
    s32         sparkIndex;

    coord      = task->extra.coordBody->coord;
    effectWork = task->spawnArg2.pointer;

    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            effectKillTask(effectWork, task);
        }
        return;
    }

    actorRenderComposeCoord(coord);
    effectWork->age++;

    // Ammunition changes the RGB channel ratios and the initial spark recipe.
    switch (task->spawnArg1.value) {
        case GUNBLADE_FLASH_AMMUNITION_BUCKSHOT:
            if (task->state == GUNBLADE_FLASH_INIT) {
                effectSpawn(EFFECT_IMPACT_FLASH, coord, GUNBLADE_FLASH_IMPACT_RADIUS, NULL);
                effectSpawn(EFFECT_EXPANDING_COLOR_BAND, coord, GUNBLADE_FLASH_BAND_BUCKSHOT_ARGUMENT, NULL);
                effectSpawn(EFFECT_EXPANDING_COLOR_BAND, coord, (GUNBLADE_FLASH_BAND_BUCKSHOT_ARGUMENT | GUNBLADE_FLASH_BAND_SECOND_ANGLE), NULL);
                effectSpawn(EFFECT_EXPANDING_COLOR_BAND, coord, (GUNBLADE_FLASH_BAND_BUCKSHOT_ARGUMENT | GUNBLADE_FLASH_BAND_THIRD_ANGLE), NULL);
                for (sparkIndex = 0; sparkIndex < GUNBLADE_FLASH_SPARK_COUNT; sparkIndex++) {
                    effectSpawn(EFFECT_SPARK_STREAK, coord, 0, NULL);
                }
                task->state       = GUNBLADE_FLASH_DRAW;
                effectWork->scale = effectWork->period = GUNBLADE_FLASH_INITIAL_BRIGHTNESS;
                effectWork->angle = effectWork->step = GUNBLADE_FLASH_INITIAL_HALF_RADIUS;
            }
            rgb[0] = rgb[1]    = effectWork->scale;
            rgb[2]             = effectWork->scale >> 2;
            effectWork->angle += GUNBLADE_FLASH_DISC_RADIUS_STEP;
            effectDrawGouraudDisc(coord, (s16)(effectWork->angle * 2), rgb);
            if (effectWork->period >= GUNBLADE_FLASH_BAND_BRIGHTNESS_MIN) {
                rgb[0] = rgb[1] = effectWork->period;
                rgb[2]          = effectWork->period >> 2;
                GUNBLADE_DRAW_CHARGE_BANDS(effectWork, coord, rgb);
                return;
            }
            effectWork->scale -= GUNBLADE_FLASH_DISC_BRIGHTNESS_STEP;
            if (effectWork->scale < GUNBLADE_FLASH_DISC_BRIGHTNESS_MIN) {
                effectKillTask(effectWork, task);
            }
            return;
        case GUNBLADE_FLASH_AMMUNITION_FIREFLY:
            if (task->state == GUNBLADE_FLASH_INIT) {
                effectSpawn(EFFECT_IMPACT_FLASH, coord, GUNBLADE_FLASH_IMPACT_RADIUS, NULL);
                effectSpawn(EFFECT_EXPANDING_COLOR_BAND, coord, GUNBLADE_FLASH_BAND_FIREFLY_ARGUMENT, NULL);
                effectSpawn(EFFECT_EXPANDING_COLOR_BAND, coord, (GUNBLADE_FLASH_BAND_FIREFLY_ARGUMENT | GUNBLADE_FLASH_BAND_SECOND_ANGLE), NULL);
                effectSpawn(EFFECT_EXPANDING_COLOR_BAND, coord, (GUNBLADE_FLASH_BAND_FIREFLY_ARGUMENT | GUNBLADE_FLASH_BAND_THIRD_ANGLE), NULL);
                for (sparkIndex = 0; sparkIndex < GUNBLADE_FLASH_BOUNCING_SPARK_COUNT; sparkIndex++) {
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    effectSpawn(EFFECT_BOUNCING_SPARK, coord, ((gRandomLcgState >> 16) & GUNBLADE_FLASH_BOUNCING_SPARK_SPEED_MASK) | GUNBLADE_FLASH_BOUNCING_SPARK_COLOR, NULL);
                }
                task->state       = GUNBLADE_FLASH_DRAW;
                effectWork->scale = effectWork->period = GUNBLADE_FLASH_INITIAL_BRIGHTNESS;
                effectWork->angle = effectWork->step = GUNBLADE_FLASH_INITIAL_HALF_RADIUS;
            }
            rgb[0]             = effectWork->scale;
            rgb[1]             = effectWork->scale >> 1;
            rgb[2]             = effectWork->scale >> 2;
            effectWork->angle += GUNBLADE_FLASH_DISC_RADIUS_STEP;
            effectDrawGouraudDisc(coord, (s16)(effectWork->angle * 2), rgb);
            if (effectWork->period >= GUNBLADE_FLASH_BAND_BRIGHTNESS_MIN) {
                rgb[0] = effectWork->period;
                rgb[1] = effectWork->period >> 1;
                rgb[2] = effectWork->period >> 2;
                GUNBLADE_DRAW_CHARGE_BANDS(effectWork, coord, rgb);
                return;
            }
            effectWork->scale -= GUNBLADE_FLASH_DISC_BRIGHTNESS_STEP;
            if (effectWork->scale < GUNBLADE_FLASH_DISC_BRIGHTNESS_MIN) {
                effectKillTask(effectWork, task);
            }
            return;
        case GUNBLADE_FLASH_AMMUNITION_SLUG:
            if (task->state == GUNBLADE_FLASH_INIT) {
                effectSpawn(EFFECT_IMPACT_FLASH, coord, GUNBLADE_FLASH_IMPACT_RADIUS, NULL);
                effectSpawn(EFFECT_EXPANDING_COLOR_BAND, coord, GUNBLADE_FLASH_BAND_SLUG_ARGUMENT, NULL);
                effectSpawn(EFFECT_EXPANDING_COLOR_BAND, coord, (GUNBLADE_FLASH_BAND_SLUG_ARGUMENT | GUNBLADE_FLASH_BAND_SECOND_ANGLE), NULL);
                effectSpawn(EFFECT_EXPANDING_COLOR_BAND, coord, (GUNBLADE_FLASH_BAND_SLUG_ARGUMENT | GUNBLADE_FLASH_BAND_THIRD_ANGLE), NULL);
                for (sparkIndex = 0; sparkIndex < GUNBLADE_FLASH_SPARK_COUNT; sparkIndex++) {
                    effectSpawn(EFFECT_SPARK_STREAK, coord, GUNBLADE_FLASH_SLUG_SPARK_ARGUMENT, NULL);
                }
                task->state       = GUNBLADE_FLASH_DRAW;
                effectWork->scale = effectWork->period = GUNBLADE_FLASH_INITIAL_BRIGHTNESS;
                effectWork->angle = effectWork->step = GUNBLADE_FLASH_INITIAL_HALF_RADIUS;
            }
            rgb[0]             = effectWork->scale >> 2;
            rgb[1]             = effectWork->scale >> 1;
            rgb[2]             = effectWork->scale;
            effectWork->angle += GUNBLADE_FLASH_DISC_RADIUS_STEP;
            effectDrawGouraudDisc(coord, (s16)(effectWork->angle * 2), rgb);
            if (effectWork->period >= GUNBLADE_FLASH_BAND_BRIGHTNESS_MIN) {
                rgb[0] = effectWork->period >> 2;
                rgb[1] = effectWork->period >> 1;
                rgb[2] = effectWork->period;
                GUNBLADE_DRAW_CHARGE_BANDS(effectWork, coord, rgb);
                return;
            }
            effectWork->scale -= GUNBLADE_FLASH_DISC_BRIGHTNESS_STEP;
            if (effectWork->scale < GUNBLADE_FLASH_DISC_BRIGHTNESS_MIN) {
                effectKillTask(effectWork, task);
            }
            return;
    }
}

#undef GUNBLADE_DRAW_CHARGE_BANDS

void gunbladeRequestChargeFlash(s32 ammunitionIndex)
{
    EffectWork* trailWork = D_gunblade_8012E248;

    if (trailWork != NULL) {
        D_gunblade_8012E244->spawnArg1.value = ammunitionIndex;
        trailWork->index++;
    }
}

/// Initial identity transform of the held Gunblade's single self-parented part.
static TmdBone _gGunbladeModelSkeleton[] = {
#include "assets/gunblade_model_017FC_skeleton.inc"
};

/// Vertex count of the held Gunblade's single part (41 vertices).
static u32 _gGunbladeModelPartVerts[] = {
#include "assets/gunblade_model_017FC_partVerts.inc"
};

/// Held Gunblade vertices in signed integer part-local coordinates.
static SVECTOR _gGunbladeModelVerts[] = {
#include "assets/gunblade_model_017FC_verts.inc"
};

/// Held Gunblade lighting normals, indexed separately from vertices.
static SVECTOR _gGunbladeModelNormals[] = {
#include "assets/gunblade_model_017FC_normals.inc"
};

/// Held Gunblade's 317-word textured-triangle/quad stream; creation patches draw slots.
static u32 _gGunbladeModelStream[] = {
#include "assets/gunblade_model_017FC_stream.inc"
};

enum {
    GUNBLADE_MODEL_HANDLERS_UNRESOLVED = 0,
    GUNBLADE_MODEL_BUFFER_HALF_BYTES   = 14 * sizeof(POLY_GT3) + 32 * sizeof(POLY_GT4)
};

TmdSource gGunbladeModel = {
    GUNBLADE_MODEL_HANDLERS_UNRESOLVED,
    GUNBLADE_MODEL_BUFFER_HALF_BYTES,
    0,
    ARRAY_SIZE(_gGunbladeModelSkeleton),
    _gGunbladeModelPartVerts,
    _gGunbladeModelVerts,
    _gGunbladeModelNormals,
    _gGunbladeModelSkeleton,
    _gGunbladeModelStream,
};

/// Two translation/Euler poses for the standing-idle clip; offsets count four-byte words.
static AnimationPackedPose _gGunbladeIdleAnimationBank1[] = {
#include "assets/gunblade_animation_01EA4_bank1.inc"
};

/// Eight packed Euler rotations for the standing-idle clip, in eight-angle-unit steps.
static AnimationPackedRotation _gGunbladeIdleAnimationBank4[] = {
#include "assets/gunblade_animation_01EA4_bank4.inc"
};

/// Standing-idle track keyframes and loop/stop controls; durations count normal-rate frames.
static AnimationRecord _gGunbladeIdleAnimationRecords[] = {
#include "assets/gunblade_animation_01EA4_records.inc"
};

/// Standing-idle's 20-entry player track-start table; entry 19 repeats entry 0.
static u16 _gGunbladeIdleAnimationIndices[] = {
#include "assets/gunblade_animation_01EA4_indices.inc"
};

/// Player standing-idle pose with the Gunblade equipped.
///
/// Native bank slots 1, 21, 22 and 41 share this clip. Its 20 track starts
/// address player model parts, independently of the one-part weapon model.
/// Playback borrows these overlay-owned arrays while Gunblade remains loaded.
static AnimationSet _gGunbladeIdleAnimation = {
    _gGunbladeIdleAnimationRecords,
    _gGunbladeIdleAnimationIndices,
    { NULL, _gGunbladeIdleAnimationBank1, NULL, NULL, _gGunbladeIdleAnimationBank4, NULL, NULL, NULL },
};

/// Twelve translation/Euler poses for the forward-walk clip, addressed in four-byte words.
static AnimationPackedPose _gGunbladeWalkAnimationBank1[] = {
#include "assets/gunblade_animation_02548_bank1.inc"
};

/// 151 packed Euler rotations for the forward-walk clip, in eight-angle-unit steps.
static AnimationPackedRotation _gGunbladeWalkAnimationBank4[] = {
#include "assets/gunblade_animation_02548_bank4.inc"
};

/// Forward-walk track keyframes and loop/stop controls; durations count normal-rate frames.
static AnimationRecord _gGunbladeWalkAnimationRecords[] = {
#include "assets/gunblade_animation_02548_records.inc"
};

/// Forward-walk's 20-entry player track-start table; entry 19 repeats entry 0.
static u16 _gGunbladeWalkAnimationIndices[] = {
#include "assets/gunblade_animation_02548_indices.inc"
};

/// Player forward-walk clip supplied by the Gunblade animation bank.
///
/// Shared by native slots 2 (held weapon) and 19 (no held weapon task).
/// Player playback borrows these arrays for the loaded overlay's lifetime.
static AnimationSet _gGunbladeWalkAnimation = {
    _gGunbladeWalkAnimationRecords,
    _gGunbladeWalkAnimationIndices,
    { NULL, _gGunbladeWalkAnimationBank1, NULL, NULL, _gGunbladeWalkAnimationBank4, NULL, NULL, NULL },
};

/// Nineteen translation/Euler poses for negative turning, addressed in four-byte words.
static AnimationPackedPose _gGunbladeNegativeTurnAnimationBank1[] = {
#include "assets/gunblade_animation_02DA8_bank1.inc"
};

/// 169 packed Euler rotations for negative turning, in eight-angle-unit steps.
static AnimationPackedRotation _gGunbladeNegativeTurnAnimationBank4[] = {
#include "assets/gunblade_animation_02DA8_bank4.inc"
};

/// Negative-turn keyframes and loop/stop controls; durations count normal-rate frames.
static AnimationRecord _gGunbladeNegativeTurnAnimationRecords[] = {
#include "assets/gunblade_animation_02DA8_records.inc"
};

/// Negative-turn's 20-entry player track-start table; entry 19 repeats entry 0.
static u16 _gGunbladeNegativeTurnAnimationIndices[] = {
#include "assets/gunblade_animation_02DA8_indices.inc"
};

/// Player stationary negative-turn clip with the Gunblade equipped.
///
/// Native slot 5 is selected by left input (turn sign -1). Player playback
/// borrows these arrays for the loaded overlay's lifetime.
static AnimationSet _gGunbladeNegativeTurnAnimation = {
    _gGunbladeNegativeTurnAnimationRecords,
    _gGunbladeNegativeTurnAnimationIndices,
    { NULL, _gGunbladeNegativeTurnAnimationBank1, NULL, NULL, _gGunbladeNegativeTurnAnimationBank4, NULL, NULL, NULL },
};

/// Nineteen translation/Euler poses for positive turning, addressed in four-byte words.
static AnimationPackedPose _gGunbladePositiveTurnAnimationBank1[] = {
#include "assets/gunblade_animation_0360C_bank1.inc"
};

/// 170 packed Euler rotations for positive turning, in eight-angle-unit steps.
static AnimationPackedRotation _gGunbladePositiveTurnAnimationBank4[] = {
#include "assets/gunblade_animation_0360C_bank4.inc"
};

/// Positive-turn keyframes and loop/stop controls; durations count normal-rate frames.
static AnimationRecord _gGunbladePositiveTurnAnimationRecords[] = {
#include "assets/gunblade_animation_0360C_records.inc"
};

/// Positive-turn's 20-entry player track-start table; entry 19 repeats entry 0.
static u16 _gGunbladePositiveTurnAnimationIndices[] = {
#include "assets/gunblade_animation_0360C_indices.inc"
};

/// Player stationary positive-turn clip with the Gunblade equipped.
///
/// Native slot 6 is selected by right input (turn sign +1). Player playback
/// borrows these arrays for the loaded overlay's lifetime.
static AnimationSet _gGunbladePositiveTurnAnimation = {
    _gGunbladePositiveTurnAnimationRecords,
    _gGunbladePositiveTurnAnimationIndices,
    { NULL, _gGunbladePositiveTurnAnimationBank1, NULL, NULL, _gGunbladePositiveTurnAnimationBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGunbladeAnimation03920Bank1[3] = {
#include "assets/gunblade_animation_03920_bank1.inc"
};

static AnimationPackedRotation _gGunbladeAnimation03920Bank4[69] = {
#include "assets/gunblade_animation_03920_bank4.inc"
};

static AnimationRecord _gGunbladeAnimation03920Records[99] = {
#include "assets/gunblade_animation_03920_records.inc"
};

static u16 _gGunbladeAnimation03920Indices[20] = {
#include "assets/gunblade_animation_03920_indices.inc"
};

static AnimationSet _gGunbladeAnimation03920 = {
    _gGunbladeAnimation03920Records,
    _gGunbladeAnimation03920Indices,
    { NULL, _gGunbladeAnimation03920Bank1, NULL, NULL, _gGunbladeAnimation03920Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGunbladeAnimation0407CBank1[14] = {
#include "assets/gunblade_animation_0407C_bank1.inc"
};

static AnimationPackedRotation _gGunbladeAnimation0407CBank4[156] = {
#include "assets/gunblade_animation_0407C_bank4.inc"
};

static AnimationRecord _gGunbladeAnimation0407CRecords[253] = {
#include "assets/gunblade_animation_0407C_records.inc"
};

static u16 _gGunbladeAnimation0407CIndices[20] = {
#include "assets/gunblade_animation_0407C_indices.inc"
};

static AnimationSet _gGunbladeAnimation0407C = {
    _gGunbladeAnimation0407CRecords,
    _gGunbladeAnimation0407CIndices,
    { NULL, _gGunbladeAnimation0407CBank1, NULL, NULL, _gGunbladeAnimation0407CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGunbladeAnimation04804Bank1[16] = {
#include "assets/gunblade_animation_04804_bank1.inc"
};

static AnimationPackedRotation _gGunbladeAnimation04804Bank4[167] = {
#include "assets/gunblade_animation_04804_bank4.inc"
};

static AnimationRecord _gGunbladeAnimation04804Records[247] = {
#include "assets/gunblade_animation_04804_records.inc"
};

static u16 _gGunbladeAnimation04804Indices[20] = {
#include "assets/gunblade_animation_04804_indices.inc"
};

static AnimationSet _gGunbladeAnimation04804 = {
    _gGunbladeAnimation04804Records,
    _gGunbladeAnimation04804Indices,
    { NULL, _gGunbladeAnimation04804Bank1, NULL, NULL, _gGunbladeAnimation04804Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGunbladeAnimation04AD8Bank1[6] = {
#include "assets/gunblade_animation_04AD8_bank1.inc"
};

static AnimationPackedRotation _gGunbladeAnimation04AD8Bank4[52] = {
#include "assets/gunblade_animation_04AD8_bank4.inc"
};

static AnimationRecord _gGunbladeAnimation04AD8Records[91] = {
#include "assets/gunblade_animation_04AD8_records.inc"
};

static u16 _gGunbladeAnimation04AD8Indices[20] = {
#include "assets/gunblade_animation_04AD8_indices.inc"
};

static AnimationSet _gGunbladeAnimation04AD8 = {
    _gGunbladeAnimation04AD8Records,
    _gGunbladeAnimation04AD8Indices,
    { NULL, _gGunbladeAnimation04AD8Bank1, NULL, NULL, _gGunbladeAnimation04AD8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGunbladeAnimation04E68Bank1[7] = {
#include "assets/gunblade_animation_04E68_bank1.inc"
};

static AnimationPackedRotation _gGunbladeAnimation04E68Bank4[73] = {
#include "assets/gunblade_animation_04E68_bank4.inc"
};

static AnimationRecord _gGunbladeAnimation04E68Records[114] = {
#include "assets/gunblade_animation_04E68_records.inc"
};

static u16 _gGunbladeAnimation04E68Indices[20] = {
#include "assets/gunblade_animation_04E68_indices.inc"
};

static AnimationSet _gGunbladeAnimation04E68 = {
    _gGunbladeAnimation04E68Records,
    _gGunbladeAnimation04E68Indices,
    { NULL, _gGunbladeAnimation04E68Bank1, NULL, NULL, _gGunbladeAnimation04E68Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGunbladeAnimation052F0Bank1[9] = {
#include "assets/gunblade_animation_052F0_bank1.inc"
};

static AnimationPackedRotation _gGunbladeAnimation052F0Bank4[104] = {
#include "assets/gunblade_animation_052F0_bank4.inc"
};

static AnimationRecord _gGunbladeAnimation052F0Records[139] = {
#include "assets/gunblade_animation_052F0_records.inc"
};

static u16 _gGunbladeAnimation052F0Indices[20] = {
#include "assets/gunblade_animation_052F0_indices.inc"
};

static AnimationSet _gGunbladeAnimation052F0 = {
    _gGunbladeAnimation052F0Records,
    _gGunbladeAnimation052F0Indices,
    { NULL, _gGunbladeAnimation052F0Bank1, NULL, NULL, _gGunbladeAnimation052F0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGunbladeAnimation054ECBank1[3] = {
#include "assets/gunblade_animation_054EC_bank1.inc"
};

static AnimationPackedRotation _gGunbladeAnimation054ECBank4[22] = {
#include "assets/gunblade_animation_054EC_bank4.inc"
};

static AnimationRecord _gGunbladeAnimation054ECRecords[76] = {
#include "assets/gunblade_animation_054EC_records.inc"
};

static u16 _gGunbladeAnimation054ECIndices[20] = {
#include "assets/gunblade_animation_054EC_indices.inc"
};

static AnimationSet _gGunbladeAnimation054EC = {
    _gGunbladeAnimation054ECRecords,
    _gGunbladeAnimation054ECIndices,
    { NULL, _gGunbladeAnimation054ECBank1, NULL, NULL, _gGunbladeAnimation054ECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGunbladeAnimation057C4Bank1[6] = {
#include "assets/gunblade_animation_057C4_bank1.inc"
};

static AnimationPackedRotation _gGunbladeAnimation057C4Bank4[57] = {
#include "assets/gunblade_animation_057C4_bank4.inc"
};

static AnimationRecord _gGunbladeAnimation057C4Records[87] = {
#include "assets/gunblade_animation_057C4_records.inc"
};

static u16 _gGunbladeAnimation057C4Indices[20] = {
#include "assets/gunblade_animation_057C4_indices.inc"
};

static AnimationSet _gGunbladeAnimation057C4 = {
    _gGunbladeAnimation057C4Records,
    _gGunbladeAnimation057C4Indices,
    { NULL, _gGunbladeAnimation057C4Bank1, NULL, NULL, _gGunbladeAnimation057C4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGunbladeAnimation05A68Bank1[4] = {
#include "assets/gunblade_animation_05A68_bank1.inc"
};

static AnimationPackedRotation _gGunbladeAnimation05A68Bank4[55] = {
#include "assets/gunblade_animation_05A68_bank4.inc"
};

static AnimationRecord _gGunbladeAnimation05A68Records[82] = {
#include "assets/gunblade_animation_05A68_records.inc"
};

static u16 _gGunbladeAnimation05A68Indices[20] = {
#include "assets/gunblade_animation_05A68_indices.inc"
};

static AnimationSet _gGunbladeAnimation05A68 = {
    _gGunbladeAnimation05A68Records,
    _gGunbladeAnimation05A68Indices,
    { NULL, _gGunbladeAnimation05A68Bank1, NULL, NULL, _gGunbladeAnimation05A68Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGunbladeAnimation05C68Bank1[3] = {
#include "assets/gunblade_animation_05C68_bank1.inc"
};

static AnimationPackedRotation _gGunbladeAnimation05C68Bank4[23] = {
#include "assets/gunblade_animation_05C68_bank4.inc"
};

static AnimationRecord _gGunbladeAnimation05C68Records[76] = {
#include "assets/gunblade_animation_05C68_records.inc"
};

static u16 _gGunbladeAnimation05C68Indices[20] = {
#include "assets/gunblade_animation_05C68_indices.inc"
};

static AnimationSet _gGunbladeAnimation05C68 = {
    _gGunbladeAnimation05C68Records,
    _gGunbladeAnimation05C68Indices,
    { NULL, _gGunbladeAnimation05C68Bank1, NULL, NULL, _gGunbladeAnimation05C68Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGunbladeAnimation05FBCBank1[8] = {
#include "assets/gunblade_animation_05FBC_bank1.inc"
};

static AnimationPackedRotation _gGunbladeAnimation05FBCBank4[68] = {
#include "assets/gunblade_animation_05FBC_bank4.inc"
};

static AnimationRecord _gGunbladeAnimation05FBCRecords[101] = {
#include "assets/gunblade_animation_05FBC_records.inc"
};

static u16 _gGunbladeAnimation05FBCIndices[20] = {
#include "assets/gunblade_animation_05FBC_indices.inc"
};

static AnimationSet _gGunbladeAnimation05FBC = {
    _gGunbladeAnimation05FBCRecords,
    _gGunbladeAnimation05FBCIndices,
    { NULL, _gGunbladeAnimation05FBCBank1, NULL, NULL, _gGunbladeAnimation05FBCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGunbladeAnimation06270Bank1[5] = {
#include "assets/gunblade_animation_06270_bank1.inc"
};

static AnimationPackedRotation _gGunbladeAnimation06270Bank4[55] = {
#include "assets/gunblade_animation_06270_bank4.inc"
};

static AnimationRecord _gGunbladeAnimation06270Records[83] = {
#include "assets/gunblade_animation_06270_records.inc"
};

static u16 _gGunbladeAnimation06270Indices[20] = {
#include "assets/gunblade_animation_06270_indices.inc"
};

static AnimationSet _gGunbladeAnimation06270 = {
    _gGunbladeAnimation06270Records,
    _gGunbladeAnimation06270Indices,
    { NULL, _gGunbladeAnimation06270Bank1, NULL, NULL, _gGunbladeAnimation06270Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGunbladeAnimation06590Bank1[6] = {
#include "assets/gunblade_animation_06590_bank1.inc"
};

static AnimationPackedRotation _gGunbladeAnimation06590Bank4[66] = {
#include "assets/gunblade_animation_06590_bank4.inc"
};

static AnimationRecord _gGunbladeAnimation06590Records[96] = {
#include "assets/gunblade_animation_06590_records.inc"
};

static u16 _gGunbladeAnimation06590Indices[20] = {
#include "assets/gunblade_animation_06590_indices.inc"
};

static AnimationSet _gGunbladeAnimation06590 = {
    _gGunbladeAnimation06590Records,
    _gGunbladeAnimation06590Indices,
    { NULL, _gGunbladeAnimation06590Bank1, NULL, NULL, _gGunbladeAnimation06590Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGunbladeAnimation06D54Bank1[18] = {
#include "assets/gunblade_animation_06D54_bank1.inc"
};

static AnimationPackedRotation _gGunbladeAnimation06D54Bank4[184] = {
#include "assets/gunblade_animation_06D54_bank4.inc"
};

static AnimationRecord _gGunbladeAnimation06D54Records[239] = {
#include "assets/gunblade_animation_06D54_records.inc"
};

static u16 _gGunbladeAnimation06D54Indices[20] = {
#include "assets/gunblade_animation_06D54_indices.inc"
};

static AnimationSet _gGunbladeAnimation06D54 = {
    _gGunbladeAnimation06D54Records,
    _gGunbladeAnimation06D54Indices,
    { NULL, _gGunbladeAnimation06D54Bank1, NULL, NULL, _gGunbladeAnimation06D54Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGunbladeAnimation07FECBank1[29] = {
#include "assets/gunblade_animation_07FEC_bank1.inc"
};

static AnimationPackedRotation _gGunbladeAnimation07FECBank4[450] = {
#include "assets/gunblade_animation_07FEC_bank4.inc"
};

static AnimationRecord _gGunbladeAnimation07FECRecords[633] = {
#include "assets/gunblade_animation_07FEC_records.inc"
};

static u16 _gGunbladeAnimation07FECIndices[20] = {
#include "assets/gunblade_animation_07FEC_indices.inc"
};

static AnimationSet _gGunbladeAnimation07FEC = {
    _gGunbladeAnimation07FECRecords,
    _gGunbladeAnimation07FECIndices,
    { NULL, _gGunbladeAnimation07FECBank1, NULL, NULL, _gGunbladeAnimation07FECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGunbladeAnimation08B64Bank1[12] = {
#include "assets/gunblade_animation_08B64_bank1.inc"
};

static AnimationPackedRotation _gGunbladeAnimation08B64Bank4[266] = {
#include "assets/gunblade_animation_08B64_bank4.inc"
};

static AnimationRecord _gGunbladeAnimation08B64Records[412] = {
#include "assets/gunblade_animation_08B64_records.inc"
};

static u16 _gGunbladeAnimation08B64Indices[20] = {
#include "assets/gunblade_animation_08B64_indices.inc"
};

static AnimationSet _gGunbladeAnimation08B64 = {
    _gGunbladeAnimation08B64Records,
    _gGunbladeAnimation08B64Indices,
    { NULL, _gGunbladeAnimation08B64Bank1, NULL, NULL, _gGunbladeAnimation08B64Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGunbladeAnimation09280Bank1[9] = {
#include "assets/gunblade_animation_09280_bank1.inc"
};

static AnimationPackedRotation _gGunbladeAnimation09280Bank4[144] = {
#include "assets/gunblade_animation_09280_bank4.inc"
};

static AnimationRecord _gGunbladeAnimation09280Records[264] = {
#include "assets/gunblade_animation_09280_records.inc"
};

static u16 _gGunbladeAnimation09280Indices[20] = {
#include "assets/gunblade_animation_09280_indices.inc"
};

static AnimationSet _gGunbladeAnimation09280 = {
    _gGunbladeAnimation09280Records,
    _gGunbladeAnimation09280Indices,
    { NULL, _gGunbladeAnimation09280Bank1, NULL, NULL, _gGunbladeAnimation09280Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGunbladeAnimation09700Bank1[6] = {
#include "assets/gunblade_animation_09700_bank1.inc"
};

static AnimationPackedRotation _gGunbladeAnimation09700Bank4[107] = {
#include "assets/gunblade_animation_09700_bank4.inc"
};

static AnimationRecord _gGunbladeAnimation09700Records[143] = {
#include "assets/gunblade_animation_09700_records.inc"
};

static u16 _gGunbladeAnimation09700Indices[20] = {
#include "assets/gunblade_animation_09700_indices.inc"
};

static AnimationSet _gGunbladeAnimation09700 = {
    _gGunbladeAnimation09700Records,
    _gGunbladeAnimation09700Indices,
    { NULL, _gGunbladeAnimation09700Bank1, NULL, NULL, _gGunbladeAnimation09700Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGunbladeAnimation098D8Bank1[3] = {
#include "assets/gunblade_animation_098D8_bank1.inc"
};

static AnimationPackedRotation _gGunbladeAnimation098D8Bank4[32] = {
#include "assets/gunblade_animation_098D8_bank4.inc"
};

static AnimationRecord _gGunbladeAnimation098D8Records[57] = {
#include "assets/gunblade_animation_098D8_records.inc"
};

static u16 _gGunbladeAnimation098D8Indices[20] = {
#include "assets/gunblade_animation_098D8_indices.inc"
};

static AnimationSet _gGunbladeAnimation098D8 = {
    _gGunbladeAnimation098D8Records,
    _gGunbladeAnimation098D8Indices,
    { NULL, _gGunbladeAnimation098D8Bank1, NULL, NULL, _gGunbladeAnimation098D8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGunbladeAnimation09E3CBank1[11] = {
#include "assets/gunblade_animation_09E3C_bank1.inc"
};

static AnimationPackedRotation _gGunbladeAnimation09E3CBank4[125] = {
#include "assets/gunblade_animation_09E3C_bank4.inc"
};

static AnimationRecord _gGunbladeAnimation09E3CRecords[167] = {
#include "assets/gunblade_animation_09E3C_records.inc"
};

static u16 _gGunbladeAnimation09E3CIndices[20] = {
#include "assets/gunblade_animation_09E3C_indices.inc"
};

static AnimationSet _gGunbladeAnimation09E3C = {
    _gGunbladeAnimation09E3CRecords,
    _gGunbladeAnimation09E3CIndices,
    { NULL, _gGunbladeAnimation09E3CBank1, NULL, NULL, _gGunbladeAnimation09E3CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGunbladeAnimation0A030Bank1[3] = {
#include "assets/gunblade_animation_0A030_bank1.inc"
};

static AnimationPackedRotation _gGunbladeAnimation0A030Bank4[20] = {
#include "assets/gunblade_animation_0A030_bank4.inc"
};

static AnimationRecord _gGunbladeAnimation0A030Records[76] = {
#include "assets/gunblade_animation_0A030_records.inc"
};

static u16 _gGunbladeAnimation0A030Indices[20] = {
#include "assets/gunblade_animation_0A030_indices.inc"
};

static AnimationSet _gGunbladeAnimation0A030 = {
    _gGunbladeAnimation0A030Records,
    _gGunbladeAnimation0A030Indices,
    { NULL, _gGunbladeAnimation0A030Bank1, NULL, NULL, _gGunbladeAnimation0A030Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGunbladeAnimation0A4B0Bank1[8] = {
#include "assets/gunblade_animation_0A4B0_bank1.inc"
};

static AnimationPackedRotation _gGunbladeAnimation0A4B0Bank4[105] = {
#include "assets/gunblade_animation_0A4B0_bank4.inc"
};

static AnimationRecord _gGunbladeAnimation0A4B0Records[139] = {
#include "assets/gunblade_animation_0A4B0_records.inc"
};

static u16 _gGunbladeAnimation0A4B0Indices[20] = {
#include "assets/gunblade_animation_0A4B0_indices.inc"
};

static AnimationSet _gGunbladeAnimation0A4B0 = {
    _gGunbladeAnimation0A4B0Records,
    _gGunbladeAnimation0A4B0Indices,
    { NULL, _gGunbladeAnimation0A4B0Bank1, NULL, NULL, _gGunbladeAnimation0A4B0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGunbladeAnimation0A68CBank1[2] = {
#include "assets/gunblade_animation_0A68C_bank1.inc"
};

static AnimationPackedRotation _gGunbladeAnimation0A68CBank4[17] = {
#include "assets/gunblade_animation_0A68C_bank4.inc"
};

static AnimationRecord _gGunbladeAnimation0A68CRecords[76] = {
#include "assets/gunblade_animation_0A68C_records.inc"
};

static u16 _gGunbladeAnimation0A68CIndices[20] = {
#include "assets/gunblade_animation_0A68C_indices.inc"
};

static AnimationSet _gGunbladeAnimation0A68C = {
    _gGunbladeAnimation0A68CRecords,
    _gGunbladeAnimation0A68CIndices,
    { NULL, _gGunbladeAnimation0A68CBank1, NULL, NULL, _gGunbladeAnimation0A68CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGunbladeAnimation0B1B0Bank1[21] = {
#include "assets/gunblade_animation_0B1B0_bank1.inc"
};

static AnimationPackedRotation _gGunbladeAnimation0B1B0Bank4[278] = {
#include "assets/gunblade_animation_0B1B0_bank4.inc"
};

static AnimationRecord _gGunbladeAnimation0B1B0Records[352] = {
#include "assets/gunblade_animation_0B1B0_records.inc"
};

static u16 _gGunbladeAnimation0B1B0Indices[20] = {
#include "assets/gunblade_animation_0B1B0_indices.inc"
};

static AnimationSet _gGunbladeAnimation0B1B0 = {
    _gGunbladeAnimation0B1B0Records,
    _gGunbladeAnimation0B1B0Indices,
    { NULL, _gGunbladeAnimation0B1B0Bank1, NULL, NULL, _gGunbladeAnimation0B1B0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGunbladeAnimation0BCB4Bank1[21] = {
#include "assets/gunblade_animation_0BCB4_bank1.inc"
};

static AnimationPackedRotation _gGunbladeAnimation0BCB4Bank4[264] = {
#include "assets/gunblade_animation_0BCB4_bank4.inc"
};

static AnimationRecord _gGunbladeAnimation0BCB4Records[358] = {
#include "assets/gunblade_animation_0BCB4_records.inc"
};

static u16 _gGunbladeAnimation0BCB4Indices[20] = {
#include "assets/gunblade_animation_0BCB4_indices.inc"
};

static AnimationSet _gGunbladeAnimation0BCB4 = {
    _gGunbladeAnimation0BCB4Records,
    _gGunbladeAnimation0BCB4Indices,
    { NULL, _gGunbladeAnimation0BCB4Bank1, NULL, NULL, _gGunbladeAnimation0BCB4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGunbladeAnimation0C9C0Bank1[24] = {
#include "assets/gunblade_animation_0C9C0_bank1.inc"
};

static AnimationPackedRotation _gGunbladeAnimation0C9C0Bank4[330] = {
#include "assets/gunblade_animation_0C9C0_bank4.inc"
};

static AnimationRecord _gGunbladeAnimation0C9C0Records[413] = {
#include "assets/gunblade_animation_0C9C0_records.inc"
};

static u16 _gGunbladeAnimation0C9C0Indices[20] = {
#include "assets/gunblade_animation_0C9C0_indices.inc"
};

static AnimationSet _gGunbladeAnimation0C9C0 = {
    _gGunbladeAnimation0C9C0Records,
    _gGunbladeAnimation0C9C0Indices,
    { NULL, _gGunbladeAnimation0C9C0Bank1, NULL, NULL, _gGunbladeAnimation0C9C0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGunbladeAnimation0E7C4Bank1[55] = {
#include "assets/gunblade_animation_0E7C4_bank1.inc"
};

static AnimationPackedRotation _gGunbladeAnimation0E7C4Bank4[803] = {
#include "assets/gunblade_animation_0E7C4_bank4.inc"
};

static AnimationRecord _gGunbladeAnimation0E7C4Records[933] = {
#include "assets/gunblade_animation_0E7C4_records.inc"
};

static u16 _gGunbladeAnimation0E7C4Indices[20] = {
#include "assets/gunblade_animation_0E7C4_indices.inc"
};

static AnimationSet _gGunbladeAnimation0E7C4 = {
    _gGunbladeAnimation0E7C4Records,
    _gGunbladeAnimation0E7C4Indices,
    { NULL, _gGunbladeAnimation0E7C4Bank1, NULL, NULL, _gGunbladeAnimation0E7C4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGunbladeAnimation0EDA4Bank1[8] = {
#include "assets/gunblade_animation_0EDA4_bank1.inc"
};

static AnimationPackedRotation _gGunbladeAnimation0EDA4Bank4[142] = {
#include "assets/gunblade_animation_0EDA4_bank4.inc"
};

static AnimationRecord _gGunbladeAnimation0EDA4Records[190] = {
#include "assets/gunblade_animation_0EDA4_records.inc"
};

static u16 _gGunbladeAnimation0EDA4Indices[20] = {
#include "assets/gunblade_animation_0EDA4_indices.inc"
};

static AnimationSet _gGunbladeAnimation0EDA4 = {
    _gGunbladeAnimation0EDA4Records,
    _gGunbladeAnimation0EDA4Indices,
    { NULL, _gGunbladeAnimation0EDA4Bank1, NULL, NULL, _gGunbladeAnimation0EDA4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGunbladeAnimation0F6F0Bank1[15] = {
#include "assets/gunblade_animation_0F6F0_bank1.inc"
};

static AnimationPackedRotation _gGunbladeAnimation0F6F0Bank4[238] = {
#include "assets/gunblade_animation_0F6F0_bank4.inc"
};

static AnimationRecord _gGunbladeAnimation0F6F0Records[292] = {
#include "assets/gunblade_animation_0F6F0_records.inc"
};

static u16 _gGunbladeAnimation0F6F0Indices[20] = {
#include "assets/gunblade_animation_0F6F0_indices.inc"
};

static AnimationSet _gGunbladeAnimation0F6F0 = {
    _gGunbladeAnimation0F6F0Records,
    _gGunbladeAnimation0F6F0Indices,
    { NULL, _gGunbladeAnimation0F6F0Bank1, NULL, NULL, _gGunbladeAnimation0F6F0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGunbladeAnimation1004CBank1[15] = {
#include "assets/gunblade_animation_1004C_bank1.inc"
};

static AnimationPackedRotation _gGunbladeAnimation1004CBank4[242] = {
#include "assets/gunblade_animation_1004C_bank4.inc"
};

static AnimationRecord _gGunbladeAnimation1004CRecords[292] = {
#include "assets/gunblade_animation_1004C_records.inc"
};

static u16 _gGunbladeAnimation1004CIndices[20] = {
#include "assets/gunblade_animation_1004C_indices.inc"
};

static AnimationSet _gGunbladeAnimation1004C = {
    _gGunbladeAnimation1004CRecords,
    _gGunbladeAnimation1004CIndices,
    { NULL, _gGunbladeAnimation1004CBank1, NULL, NULL, _gGunbladeAnimation1004CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGunbladeAnimation107ECBank1[16] = {
#include "assets/gunblade_animation_107EC_bank1.inc"
};

static AnimationPackedRotation _gGunbladeAnimation107ECBank4[157] = {
#include "assets/gunblade_animation_107EC_bank4.inc"
};

static AnimationRecord _gGunbladeAnimation107ECRecords[263] = {
#include "assets/gunblade_animation_107EC_records.inc"
};

static u16 _gGunbladeAnimation107ECIndices[20] = {
#include "assets/gunblade_animation_107EC_indices.inc"
};

static AnimationSet _gGunbladeAnimation107EC = {
    _gGunbladeAnimation107ECRecords,
    _gGunbladeAnimation107ECIndices,
    { NULL, _gGunbladeAnimation107ECBank1, NULL, NULL, _gGunbladeAnimation107ECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGunbladeAnimation10F20Bank1[13] = {
#include "assets/gunblade_animation_10F20_bank1.inc"
};

static AnimationPackedRotation _gGunbladeAnimation10F20Bank4[167] = {
#include "assets/gunblade_animation_10F20_bank4.inc"
};

static AnimationRecord _gGunbladeAnimation10F20Records[235] = {
#include "assets/gunblade_animation_10F20_records.inc"
};

static u16 _gGunbladeAnimation10F20Indices[20] = {
#include "assets/gunblade_animation_10F20_indices.inc"
};

static AnimationSet _gGunbladeAnimation10F20 = {
    _gGunbladeAnimation10F20Records,
    _gGunbladeAnimation10F20Indices,
    { NULL, _gGunbladeAnimation10F20Bank1, NULL, NULL, _gGunbladeAnimation10F20Bank4, NULL, NULL, NULL },
};

AnimationBank D_gunblade_8012E108 = { { {
    NULL,
    &_gGunbladeIdleAnimation,
    &_gGunbladeWalkAnimation,
    &_gGunbladeAnimation107EC,
    &_gGunbladeAnimation10F20,
    &_gGunbladeNegativeTurnAnimation,
    &_gGunbladePositiveTurnAnimation,
    &_gGunbladeAnimation0F6F0,
    &_gGunbladeAnimation1004C,
    &_gGunbladeAnimation0A68C,
    &_gGunbladeAnimation0E7C4,
    &_gGunbladeAnimation0EDA4,
    &_gGunbladeAnimation0BCB4,
    &_gGunbladeAnimation0B1B0,
    &_gGunbladeAnimation0C9C0,
    &_gGunbladeAnimation0C9C0,
    &_gGunbladeAnimation06270,
    &_gGunbladeAnimation06590,
    &_gGunbladeAnimation06D54,
    &_gGunbladeWalkAnimation,
    &_gGunbladeAnimation0C9C0,
    &_gGunbladeIdleAnimation,
    &_gGunbladeIdleAnimation,
    &_gGunbladeAnimation07FEC,
    &_gGunbladeAnimation09280,
    &_gGunbladeAnimation08B64,
    &_gGunbladeAnimation052F0,
    &_gGunbladeAnimation054EC,
    &_gGunbladeAnimation057C4,
    &_gGunbladeAnimation05A68,
    &_gGunbladeAnimation05C68,
    &_gGunbladeAnimation05FBC,
    &_gGunbladeAnimation09700,
    &_gGunbladeAnimation098D8,
    &_gGunbladeAnimation09700,
    &_gGunbladeAnimation098D8,
    &_gGunbladeAnimation0407C,
    &_gGunbladeAnimation04804,
    &_gGunbladeAnimation04E68,
    &_gGunbladeAnimation04AD8,
    &_gGunbladeAnimation03920,
    &_gGunbladeIdleAnimation,
    &_gGunbladeAnimation09E3C,
    &_gGunbladeAnimation0A030,
    &_gGunbladeAnimation0A4B0,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
} } };

/// The running beam task and its `EffectWork`, cached on entry to state 0 so
/// `gunbladeRequestChargeFlash` can reach them from outside the task.
Task*       D_gunblade_8012E244 = NULL;
EffectWork* D_gunblade_8012E248 = NULL;

/// Nothing reads the two words after the work pointer; they keep the offset of
/// the trails that follow.
static s32 s_unused_8012E24C[2] = { 0, 0 };

/// Eight retained world-coordinate snapshots of the Gunblade ribbon's near endpoint.
///
/// Seeded at local (0, 96, 128), then overwritten at age modulo eight.
/// Each local matrix retains the endpoint under the persistent view parent;
/// recomposition refreshes its view-space cache before drawing seven segments.
/// The ribbon reads only the cache's translation, in game-coordinate units.
/// Shared by all trail tasks in this loaded Gunblade overlay; reseeded on entry.
static GfxCoord _gGunbladeTrailNearHistory[8] = { 0 };

/// Eight retained world-coordinate snapshots of the Gunblade ribbon's far endpoint.
///
/// Local (0, 96, 896), paired slot-for-slot with the near-end history.
/// Retained under the view parent and recomposed for drawing; only translation
/// is meaningful, since the temporary endpoint's rotation is not initialized.
/// All trail tasks share this overlay-owned ring and reseed it on entry.
static GfxCoord _gGunbladeTrailFarHistory[8] = { 0 };
