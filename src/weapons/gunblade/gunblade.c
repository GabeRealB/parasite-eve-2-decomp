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

/// Pose-bank selectors for the translation/rotation and rotation-only player tracks.
enum {
    GUNBLADE_ANIMATION_TRANSLATION_ROTATION_BANK = 1,
    GUNBLADE_ANIMATION_PACKED_ROTATION_BANK      = 4,
};

// Read-only clip payloads reside in the package's data section.

/// Translation/Euler keyframes for `_gGunbladeItemUseAnimation`'s tracks 0 and 1.
///
/// Translations use model units and angles use 1/4096 turns.
/// Record offsets address each twelve-byte pose in four-byte words.
static const AnimationPackedPose _gGunbladeItemUseAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/gunblade_animation_03920_bank1.inc"
};

/// Rotation-only keyframes for `_gGunbladeItemUseAnimation`'s tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gGunbladeItemUseAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/gunblade_animation_03920_bank4.inc"
};

/// Item-use keyframes and stop controls; durations count normal-rate frames and pose offsets count words.
static const AnimationRecord _gGunbladeItemUseAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/gunblade_animation_03920_records.inc"
};

/// Absolute record starts for `_gGunbladeItemUseAnimation`'s nineteen player tracks.
///
/// The serialized twentieth halfword is zero; native player playback uses entries 0..18.
static const u16 _gGunbladeItemUseAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/gunblade_animation_03920_indices.inc"
};

/// Player item-use presentation supplied by the Gunblade animation bank.
///
/// Native set 40 plays once before returning to aim locomotion.
/// Playback borrows this descriptor and its nineteen tracks; the Gunblade
/// package must remain loaded while any slot refers to them.
static AnimationSet _gGunbladeItemUseAnimation = {
    .records           = _gGunbladeItemUseAnimationRecords,
    .trackStartIndices = _gGunbladeItemUseAnimationIndices,
    .poseBanks         = {
        [GUNBLADE_ANIMATION_TRANSLATION_ROTATION_BANK] = _gGunbladeItemUseAnimationBank1,
        [GUNBLADE_ANIMATION_PACKED_ROTATION_BANK]      = _gGunbladeItemUseAnimationBank4,
    },
};

/// Translation/Euler keyframes for `_gGunbladeStairAscentAnimation`'s tracks 0 and 1.
///
/// Translations use model units and angles use 1/4096 turns.
/// Record offsets address each twelve-byte pose in four-byte words.
static const AnimationPackedPose _gGunbladeStairAscentAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/gunblade_animation_0407C_bank1.inc"
};

/// Rotation-only keyframes for `_gGunbladeStairAscentAnimation`'s tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gGunbladeStairAscentAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/gunblade_animation_0407C_bank4.inc"
};

/// Stair-ascent keyframes, footstep cues and loop controls, with durations in normal-rate frames.
///
/// Pose offsets count four-byte words; loop targets are absolute record indices.
static const AnimationRecord _gGunbladeStairAscentAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/gunblade_animation_0407C_records.inc"
};

/// Absolute record starts for `_gGunbladeStairAscentAnimation`'s nineteen player tracks.
///
/// The serialized twentieth halfword is zero; native player playback uses entries 0..18.
static const u16 _gGunbladeStairAscentAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/gunblade_animation_0407C_indices.inc"
};

/// Player stair-ascent loop supplied by the Gunblade animation bank.
///
/// Native set 36 drives scripted ascent; track 1's footstep cues gate strides.
/// All nineteen tracks loop. Playback borrows this descriptor and its data;
/// the Gunblade package must remain loaded while any slot refers to them.
static AnimationSet _gGunbladeStairAscentAnimation = {
    .records           = _gGunbladeStairAscentAnimationRecords,
    .trackStartIndices = _gGunbladeStairAscentAnimationIndices,
    .poseBanks         = {
        [GUNBLADE_ANIMATION_TRANSLATION_ROTATION_BANK] = _gGunbladeStairAscentAnimationBank1,
        [GUNBLADE_ANIMATION_PACKED_ROTATION_BANK]      = _gGunbladeStairAscentAnimationBank4,
    },
};

/// Translation/Euler keyframes for `_gGunbladeStairDescentAnimation`'s tracks 0 and 1.
///
/// Translations use model units and angles use 1/4096 turns.
/// Record offsets address each twelve-byte pose in four-byte words.
static const AnimationPackedPose _gGunbladeStairDescentAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/gunblade_animation_04804_bank1.inc"
};

/// Rotation-only keyframes for `_gGunbladeStairDescentAnimation`'s tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gGunbladeStairDescentAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/gunblade_animation_04804_bank4.inc"
};

/// Stair-descent keyframes, footstep cues and loop controls, with durations in normal-rate frames.
///
/// Pose offsets count four-byte words; loop targets are absolute record indices.
static const AnimationRecord _gGunbladeStairDescentAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/gunblade_animation_04804_records.inc"
};

/// Absolute record starts for `_gGunbladeStairDescentAnimation`'s nineteen player tracks.
///
/// The serialized twentieth halfword is zero; native player playback uses entries 0..18.
static const u16 _gGunbladeStairDescentAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/gunblade_animation_04804_indices.inc"
};

/// Player stair-descent loop supplied by the Gunblade animation bank.
///
/// Native set 37 drives scripted descent; track 1's footstep cues gate strides.
/// All nineteen tracks loop. Playback borrows this descriptor and its data;
/// the Gunblade package must remain loaded while any slot refers to them.
static AnimationSet _gGunbladeStairDescentAnimation = {
    .records           = _gGunbladeStairDescentAnimationRecords,
    .trackStartIndices = _gGunbladeStairDescentAnimationIndices,
    .poseBanks         = {
        [GUNBLADE_ANIMATION_TRANSLATION_ROTATION_BANK] = _gGunbladeStairDescentAnimationBank1,
        [GUNBLADE_ANIMATION_PACKED_ROTATION_BANK]      = _gGunbladeStairDescentAnimationBank4,
    },
};

/// Translation/Euler keyframes for `_gGunbladeStairAscentFinishOddAnimation`'s tracks 0 and 1.
///
/// Translations use model units and angles use 1/4096 turns.
/// Record offsets address each twelve-byte pose in four-byte words.
static const AnimationPackedPose _gGunbladeStairAscentFinishOddAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/gunblade_animation_04AD8_bank1.inc"
};

/// Rotation-only keyframes for `_gGunbladeStairAscentFinishOddAnimation`'s tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gGunbladeStairAscentFinishOddAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/gunblade_animation_04AD8_bank4.inc"
};

/// Odd-step ascent-finish keyframes, final footstep cue and stops, with durations in normal-rate frames.
static const AnimationRecord _gGunbladeStairAscentFinishOddAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/gunblade_animation_04AD8_records.inc"
};

/// Absolute record starts for `_gGunbladeStairAscentFinishOddAnimation`'s nineteen player tracks.
///
/// The serialized twentieth halfword is zero; native player playback uses entries 0..18.
static const u16 _gGunbladeStairAscentFinishOddAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/gunblade_animation_04AD8_indices.inc"
};

/// Player stair-ascent finish for an odd step count, supplied by the Gunblade animation bank.
///
/// Native set 39 uses the flight's original step-count parity, then stops.
/// Playback borrows this descriptor and its nineteen tracks; the Gunblade
/// package must remain loaded while any slot refers to them.
static AnimationSet _gGunbladeStairAscentFinishOddAnimation = {
    .records           = _gGunbladeStairAscentFinishOddAnimationRecords,
    .trackStartIndices = _gGunbladeStairAscentFinishOddAnimationIndices,
    .poseBanks         = {
        [GUNBLADE_ANIMATION_TRANSLATION_ROTATION_BANK] = _gGunbladeStairAscentFinishOddAnimationBank1,
        [GUNBLADE_ANIMATION_PACKED_ROTATION_BANK]      = _gGunbladeStairAscentFinishOddAnimationBank4,
    },
};

/// Translation/Euler keyframes for `_gGunbladeStairAscentFinishEvenAnimation`'s tracks 0 and 1.
///
/// Translations use model units and angles use 1/4096 turns.
/// Record offsets address each twelve-byte pose in four-byte words.
static const AnimationPackedPose _gGunbladeStairAscentFinishEvenAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/gunblade_animation_04E68_bank1.inc"
};

/// Rotation-only keyframes for `_gGunbladeStairAscentFinishEvenAnimation`'s tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gGunbladeStairAscentFinishEvenAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/gunblade_animation_04E68_bank4.inc"
};

/// Even-step ascent-finish keyframes, final footstep cue and stops, with durations in normal-rate frames.
static const AnimationRecord _gGunbladeStairAscentFinishEvenAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/gunblade_animation_04E68_records.inc"
};

/// Absolute record starts for `_gGunbladeStairAscentFinishEvenAnimation`'s nineteen player tracks.
///
/// The serialized twentieth halfword is zero; native player playback uses entries 0..18.
static const u16 _gGunbladeStairAscentFinishEvenAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/gunblade_animation_04E68_indices.inc"
};

/// Player stair-ascent finish for an even step count, supplied by the Gunblade animation bank.
///
/// Native set 38 uses the flight's original step-count parity, then stops.
/// Playback borrows this descriptor and its nineteen tracks; the Gunblade
/// package must remain loaded while any slot refers to them.
static AnimationSet _gGunbladeStairAscentFinishEvenAnimation = {
    .records           = _gGunbladeStairAscentFinishEvenAnimationRecords,
    .trackStartIndices = _gGunbladeStairAscentFinishEvenAnimationIndices,
    .poseBanks         = {
        [GUNBLADE_ANIMATION_TRANSLATION_ROTATION_BANK] = _gGunbladeStairAscentFinishEvenAnimationBank1,
        [GUNBLADE_ANIMATION_PACKED_ROTATION_BANK]      = _gGunbladeStairAscentFinishEvenAnimationBank4,
    },
};

/// Translation/Euler keyframes for `_gGunbladeParasiteEnergyReleaseFamily0Animation`'s tracks 0 and 1.
///
/// Translations use model units and angles use 1/4096 turns.
/// Record offsets address each twelve-byte pose in four-byte words.
static const AnimationPackedPose _gGunbladeParasiteEnergyReleaseFamily0AnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/gunblade_animation_052F0_bank1.inc"
};

/// Rotation-only keyframes for `_gGunbladeParasiteEnergyReleaseFamily0Animation`'s tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gGunbladeParasiteEnergyReleaseFamily0AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/gunblade_animation_052F0_bank4.inc"
};

/// PE family-0 release keyframes and stop controls, with durations in normal-rate frames and pose offsets in words.
static const AnimationRecord _gGunbladeParasiteEnergyReleaseFamily0AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/gunblade_animation_052F0_records.inc"
};

/// Absolute record starts for `_gGunbladeParasiteEnergyReleaseFamily0Animation`'s nineteen player tracks.
///
/// The serialized twentieth halfword is zero; native player playback uses entries 0..18.
static const u16 _gGunbladeParasiteEnergyReleaseFamily0AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/gunblade_animation_052F0_indices.inc"
};

/// Player Parasite Energy release for attachment clip family 0 in the Gunblade animation bank.
///
/// Native set 26, selected before charge when the attachment uses family 0.
/// The release phase blends in over six normal-rate frames and waits for this
/// clip to stop. Playback borrows this descriptor and its nineteen tracks;
/// the Gunblade package must remain loaded while any slot refers to them.
static AnimationSet _gGunbladeParasiteEnergyReleaseFamily0Animation = {
    .records           = _gGunbladeParasiteEnergyReleaseFamily0AnimationRecords,
    .trackStartIndices = _gGunbladeParasiteEnergyReleaseFamily0AnimationIndices,
    .poseBanks         = {
        [GUNBLADE_ANIMATION_TRANSLATION_ROTATION_BANK] = _gGunbladeParasiteEnergyReleaseFamily0AnimationBank1,
        [GUNBLADE_ANIMATION_PACKED_ROTATION_BANK]      = _gGunbladeParasiteEnergyReleaseFamily0AnimationBank4,
    },
};

/// Translation/Euler keyframes for `_gGunbladeParasiteEnergyChargeFamily0Animation`'s tracks 0 and 1.
///
/// Translations use model units and angles use 1/4096 turns.
/// Record offsets address each twelve-byte pose in four-byte words.
static const AnimationPackedPose _gGunbladeParasiteEnergyChargeFamily0AnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/gunblade_animation_054EC_bank1.inc"
};

/// Rotation-only keyframes for `_gGunbladeParasiteEnergyChargeFamily0Animation`'s tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gGunbladeParasiteEnergyChargeFamily0AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/gunblade_animation_054EC_bank4.inc"
};

/// Family-0 PE charge keyframes and absolute per-track loop controls.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
/// Forward playback loops before each track's serialized trailing stop.
static const AnimationRecord _gGunbladeParasiteEnergyChargeFamily0AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/gunblade_animation_054EC_records.inc"
};

/// Absolute record starts for `_gGunbladeParasiteEnergyChargeFamily0Animation`'s nineteen player tracks.
///
/// The serialized twentieth halfword is zero; native player playback uses entries 0..18.
static const u16 _gGunbladeParasiteEnergyChargeFamily0AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/gunblade_animation_054EC_indices.inc"
};

/// Player Parasite Energy charge loop for attachment clip family 0 in the Gunblade bank.
///
/// Native set 27 loops while the attachment countdown is nonzero.
/// Charge starts without a blend after the release clip stops.
/// Playback borrows this descriptor and its nineteen tracks; the Gunblade
/// package must remain loaded while any slot or cue record refers to them.
static AnimationSet _gGunbladeParasiteEnergyChargeFamily0Animation = {
    .records           = _gGunbladeParasiteEnergyChargeFamily0AnimationRecords,
    .trackStartIndices = _gGunbladeParasiteEnergyChargeFamily0AnimationIndices,
    .poseBanks         = {
        [GUNBLADE_ANIMATION_TRANSLATION_ROTATION_BANK] = _gGunbladeParasiteEnergyChargeFamily0AnimationBank1,
        [GUNBLADE_ANIMATION_PACKED_ROTATION_BANK]      = _gGunbladeParasiteEnergyChargeFamily0AnimationBank4,
    },
};

/// Translation/Euler keyframes for `_gGunbladeParasiteEnergyCastFamily0Animation`'s tracks 0 and 1.
///
/// Translations use model units and angles use 1/4096 turns.
/// Record offsets address each twelve-byte pose in four-byte words.
static const AnimationPackedPose _gGunbladeParasiteEnergyCastFamily0AnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/gunblade_animation_057C4_bank1.inc"
};

/// Rotation-only keyframes for `_gGunbladeParasiteEnergyCastFamily0Animation`'s tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gGunbladeParasiteEnergyCastFamily0AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/gunblade_animation_057C4_bank4.inc"
};

/// Family-0 PE cast keyframes and stop controls, with durations in normal-rate frames.
static const AnimationRecord _gGunbladeParasiteEnergyCastFamily0AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/gunblade_animation_057C4_records.inc"
};

/// Absolute record starts for `_gGunbladeParasiteEnergyCastFamily0Animation`'s nineteen player tracks.
///
/// The serialized twentieth halfword is zero; native player playback uses entries 0..18.
static const u16 _gGunbladeParasiteEnergyCastFamily0AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/gunblade_animation_057C4_indices.inc"
};

/// Player Parasite Energy cast for attachment clip family 0 in the Gunblade bank.
///
/// Native set 28 starts without a blend when the attachment countdown
/// reaches zero; the action waits for the clip to stop before resuming control.
/// Playback borrows this descriptor and its nineteen tracks; the Gunblade
/// package must remain loaded while any slot or cue record refers to them.
static AnimationSet _gGunbladeParasiteEnergyCastFamily0Animation = {
    .records           = _gGunbladeParasiteEnergyCastFamily0AnimationRecords,
    .trackStartIndices = _gGunbladeParasiteEnergyCastFamily0AnimationIndices,
    .poseBanks         = {
        [GUNBLADE_ANIMATION_TRANSLATION_ROTATION_BANK] = _gGunbladeParasiteEnergyCastFamily0AnimationBank1,
        [GUNBLADE_ANIMATION_PACKED_ROTATION_BANK]      = _gGunbladeParasiteEnergyCastFamily0AnimationBank4,
    },
};

/// Translation/Euler keyframes for `_gGunbladeParasiteEnergyReleaseFamily1Animation`'s tracks 0 and 1.
///
/// Translations use model units and angles use 1/4096 turns.
/// Record offsets address each twelve-byte pose in four-byte words.
static const AnimationPackedPose _gGunbladeParasiteEnergyReleaseFamily1AnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/gunblade_animation_05A68_bank1.inc"
};

/// Rotation-only keyframes for `_gGunbladeParasiteEnergyReleaseFamily1Animation`'s tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gGunbladeParasiteEnergyReleaseFamily1AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/gunblade_animation_05A68_bank4.inc"
};

/// Family-1 PE release keyframes and stop controls, with durations in normal-rate frames.
static const AnimationRecord _gGunbladeParasiteEnergyReleaseFamily1AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/gunblade_animation_05A68_records.inc"
};

/// Absolute record starts for `_gGunbladeParasiteEnergyReleaseFamily1Animation`'s nineteen player tracks.
///
/// The serialized twentieth halfword is zero; native player playback uses entries 0..18.
static const u16 _gGunbladeParasiteEnergyReleaseFamily1AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/gunblade_animation_05A68_indices.inc"
};

/// Player Parasite Energy release for attachment clip family 1 in the Gunblade bank.
///
/// Native set 29 blends in over six normal-rate frames while the attachment
/// countdown starts; the action waits for the clip to stop before charging.
/// Playback borrows this descriptor and its nineteen tracks; the Gunblade
/// package must remain loaded while any slot or cue record refers to them.
static AnimationSet _gGunbladeParasiteEnergyReleaseFamily1Animation = {
    .records           = _gGunbladeParasiteEnergyReleaseFamily1AnimationRecords,
    .trackStartIndices = _gGunbladeParasiteEnergyReleaseFamily1AnimationIndices,
    .poseBanks         = {
        [GUNBLADE_ANIMATION_TRANSLATION_ROTATION_BANK] = _gGunbladeParasiteEnergyReleaseFamily1AnimationBank1,
        [GUNBLADE_ANIMATION_PACKED_ROTATION_BANK]      = _gGunbladeParasiteEnergyReleaseFamily1AnimationBank4,
    },
};

/// Translation/Euler keyframes for `_gGunbladeParasiteEnergyChargeFamily1Animation`'s tracks 0 and 1.
///
/// Translations use model units and angles use 1/4096 turns.
/// Record offsets address each twelve-byte pose in four-byte words.
static const AnimationPackedPose _gGunbladeParasiteEnergyChargeFamily1AnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/gunblade_animation_05C68_bank1.inc"
};

/// Rotation-only keyframes for `_gGunbladeParasiteEnergyChargeFamily1Animation`'s tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gGunbladeParasiteEnergyChargeFamily1AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/gunblade_animation_05C68_bank4.inc"
};

/// Family-1 PE charge keyframes and absolute per-track loop controls.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
/// Forward playback loops before each track's serialized trailing stop.
static const AnimationRecord _gGunbladeParasiteEnergyChargeFamily1AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/gunblade_animation_05C68_records.inc"
};

/// Absolute record starts for `_gGunbladeParasiteEnergyChargeFamily1Animation`'s nineteen player tracks.
///
/// The serialized twentieth halfword is zero; native player playback uses entries 0..18.
static const u16 _gGunbladeParasiteEnergyChargeFamily1AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/gunblade_animation_05C68_indices.inc"
};

/// Player Parasite Energy charge loop for attachment clip family 1 in the Gunblade bank.
///
/// Native set 30 loops while the attachment countdown is nonzero.
/// Charge starts without a blend after the release clip stops.
/// Playback borrows this descriptor and its nineteen tracks; the Gunblade
/// package must remain loaded while any slot or cue record refers to them.
static AnimationSet _gGunbladeParasiteEnergyChargeFamily1Animation = {
    .records           = _gGunbladeParasiteEnergyChargeFamily1AnimationRecords,
    .trackStartIndices = _gGunbladeParasiteEnergyChargeFamily1AnimationIndices,
    .poseBanks         = {
        [GUNBLADE_ANIMATION_TRANSLATION_ROTATION_BANK] = _gGunbladeParasiteEnergyChargeFamily1AnimationBank1,
        [GUNBLADE_ANIMATION_PACKED_ROTATION_BANK]      = _gGunbladeParasiteEnergyChargeFamily1AnimationBank4,
    },
};

/// Translation/Euler keyframes for `_gGunbladeParasiteEnergyCastFamily1Animation`'s tracks 0 and 1.
///
/// Translations use model units and angles use 1/4096 turns.
/// Record offsets address each twelve-byte pose in four-byte words.
static const AnimationPackedPose _gGunbladeParasiteEnergyCastFamily1AnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/gunblade_animation_05FBC_bank1.inc"
};

/// Rotation-only keyframes for `_gGunbladeParasiteEnergyCastFamily1Animation`'s tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gGunbladeParasiteEnergyCastFamily1AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/gunblade_animation_05FBC_bank4.inc"
};

/// Family-1 PE cast keyframes and stop controls, with durations in normal-rate frames.
static const AnimationRecord _gGunbladeParasiteEnergyCastFamily1AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/gunblade_animation_05FBC_records.inc"
};

/// Absolute record starts for `_gGunbladeParasiteEnergyCastFamily1Animation`'s nineteen player tracks.
///
/// The serialized twentieth halfword is zero; native player playback uses entries 0..18.
static const u16 _gGunbladeParasiteEnergyCastFamily1AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/gunblade_animation_05FBC_indices.inc"
};

/// Player Parasite Energy cast for attachment clip family 1 in the Gunblade bank.
///
/// Native set 31 starts without a blend when the attachment countdown
/// reaches zero; the action waits for the clip to stop before resuming control.
/// Playback borrows this descriptor and its nineteen tracks; the Gunblade
/// package must remain loaded while any slot or cue record refers to them.
static AnimationSet _gGunbladeParasiteEnergyCastFamily1Animation = {
    .records           = _gGunbladeParasiteEnergyCastFamily1AnimationRecords,
    .trackStartIndices = _gGunbladeParasiteEnergyCastFamily1AnimationIndices,
    .poseBanks         = {
        [GUNBLADE_ANIMATION_TRANSLATION_ROTATION_BANK] = _gGunbladeParasiteEnergyCastFamily1AnimationBank1,
        [GUNBLADE_ANIMATION_PACKED_ROTATION_BANK]      = _gGunbladeParasiteEnergyCastFamily1AnimationBank4,
    },
};

/// Translation/Euler keyframes for `_gGunbladePart4HitReactionAnimation`'s tracks 0 and 1.
///
/// Translations use model units and angles use 1/4096 turns.
/// Record offsets address each twelve-byte pose in four-byte words.
static const AnimationPackedPose _gGunbladePart4HitReactionAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/gunblade_animation_06270_bank1.inc"
};

/// Rotation-only keyframes for `_gGunbladePart4HitReactionAnimation`'s tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gGunbladePart4HitReactionAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/gunblade_animation_06270_bank4.inc"
};

/// Part-4 hit-reaction keyframes and stop controls, with durations in normal-rate frames.
static const AnimationRecord _gGunbladePart4HitReactionAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/gunblade_animation_06270_records.inc"
};

/// Absolute record starts for `_gGunbladePart4HitReactionAnimation`'s nineteen player tracks.
///
/// The serialized twentieth halfword is zero; native player playback uses entries 0..18.
static const u16 _gGunbladePart4HitReactionAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/gunblade_animation_06270_indices.inc"
};

/// Player hit reaction for the motion sphere on model part 4, supplied by the Gunblade bank.
///
/// Native set 16 blends in over three normal-rate frames for a pending or
/// contact hit on body index 1 (hit region 1), then stops.
/// Playback borrows this descriptor and its nineteen tracks; the Gunblade
/// package must remain loaded while any slot or cue record refers to them.
static AnimationSet _gGunbladePart4HitReactionAnimation = {
    .records           = _gGunbladePart4HitReactionAnimationRecords,
    .trackStartIndices = _gGunbladePart4HitReactionAnimationIndices,
    .poseBanks         = {
        [GUNBLADE_ANIMATION_TRANSLATION_ROTATION_BANK] = _gGunbladePart4HitReactionAnimationBank1,
        [GUNBLADE_ANIMATION_PACKED_ROTATION_BANK]      = _gGunbladePart4HitReactionAnimationBank4,
    },
};

/// Translation/Euler keyframes for `_gGunbladeOtherBodyHitReactionAnimation`'s tracks 0 and 1.
///
/// Translations use model units and angles use 1/4096 turns.
/// Record offsets address each twelve-byte pose in four-byte words.
static const AnimationPackedPose _gGunbladeOtherBodyHitReactionAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/gunblade_animation_06590_bank1.inc"
};

/// Rotation-only keyframes for `_gGunbladeOtherBodyHitReactionAnimation`'s tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gGunbladeOtherBodyHitReactionAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/gunblade_animation_06590_bank4.inc"
};

/// Other-body hit-reaction keyframes and stop controls, with durations in normal-rate frames.
static const AnimationRecord _gGunbladeOtherBodyHitReactionAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/gunblade_animation_06590_records.inc"
};

/// Absolute record starts for `_gGunbladeOtherBodyHitReactionAnimation`'s nineteen player tracks.
///
/// Valid track indices are 0..18; native child playback uses 1..18.
/// The serialized twentieth halfword is zero and is retained in this array.
static const u16 _gGunbladeOtherBodyHitReactionAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/gunblade_animation_06590_indices.inc"
};

/// Player hit reaction outside the part-4 motion sphere, supplied by the Gunblade bank.
///
/// Native set 17 blends in over three normal-rate frames for a pending or
/// contact hit on the root or part-1 motion body, then stops.
/// Playback borrows this descriptor and its nineteen tracks; the Gunblade
/// package must remain loaded while any slot or cue record refers to them.
static AnimationSet _gGunbladeOtherBodyHitReactionAnimation = {
    .records           = _gGunbladeOtherBodyHitReactionAnimationRecords,
    .trackStartIndices = _gGunbladeOtherBodyHitReactionAnimationIndices,
    .poseBanks         = {
        [GUNBLADE_ANIMATION_TRANSLATION_ROTATION_BANK] = _gGunbladeOtherBodyHitReactionAnimationBank1,
        [GUNBLADE_ANIMATION_PACKED_ROTATION_BANK]      = _gGunbladeOtherBodyHitReactionAnimationBank4,
    },
};

/// Translation/Euler keyframes for `_gGunbladeStoppedPoseAnimation`'s tracks 0 and 1.
///
/// Translations use model units and angles use 1/4096 turns.
/// Record offsets address each twelve-byte pose in four-byte words.
static const AnimationPackedPose _gGunbladeStoppedPoseAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/gunblade_animation_06D54_bank1.inc"
};

/// Rotation-only keyframes for `_gGunbladeStoppedPoseAnimation`'s tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gGunbladeStoppedPoseAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/gunblade_animation_06D54_bank4.inc"
};

/// Stopped-pose keyframes and stop controls, with durations in normal-rate frames.
static const AnimationRecord _gGunbladeStoppedPoseAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/gunblade_animation_06D54_records.inc"
};

/// Absolute record starts for `_gGunbladeStoppedPoseAnimation`'s nineteen player tracks.
///
/// Valid track indices are 0..18; native child playback uses 1..18.
/// The serialized twentieth halfword is zero and is retained in this array.
static const u16 _gGunbladeStoppedPoseAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/gunblade_animation_06D54_indices.inc"
};

/// Player stopped/death clip supplied by the Gunblade bank.
///
/// Native set 18 plays with movement and turning disabled, then holds its final
/// pose. Zero-HP entry blends in over four normal-rate frames; a stopped-pose request
/// can reset the clip or choose its own blend duration.
/// Playback borrows this descriptor and its nineteen tracks; the Gunblade
/// package must remain loaded while any slot or cue record refers to them.
static AnimationSet _gGunbladeStoppedPoseAnimation = {
    .records           = _gGunbladeStoppedPoseAnimationRecords,
    .trackStartIndices = _gGunbladeStoppedPoseAnimationIndices,
    .poseBanks         = {
        [GUNBLADE_ANIMATION_TRANSLATION_ROTATION_BANK] = _gGunbladeStoppedPoseAnimationBank1,
        [GUNBLADE_ANIMATION_PACKED_ROTATION_BANK]      = _gGunbladeStoppedPoseAnimationBank4,
    },
};

/// Translation/Euler keyframes for `_gGunbladeHighHealthIdleAnimation`'s tracks 0 and 1.
///
/// Translations use model units and angles use 1/4096 turns.
/// Record offsets address each twelve-byte pose in four-byte words.
static const AnimationPackedPose _gGunbladeHighHealthIdleAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/gunblade_animation_07FEC_bank1.inc"
};

/// Rotation-only keyframes for `_gGunbladeHighHealthIdleAnimation`'s tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gGunbladeHighHealthIdleAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/gunblade_animation_07FEC_bank4.inc"
};

/// High-health idle keyframes, loop jumps and stop controls, with durations in normal-rate frames.
static const AnimationRecord _gGunbladeHighHealthIdleAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/gunblade_animation_07FEC_records.inc"
};

/// Absolute record starts for `_gGunbladeHighHealthIdleAnimation`'s nineteen player tracks.
///
/// Valid track indices are 0..18; native child playback uses 1..18.
/// The serialized twentieth halfword is zero and is retained in this array.
static const u16 _gGunbladeHighHealthIdleAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/gunblade_animation_07FEC_indices.inc"
};

/// Player idle loop above half maximum HP, supplied by the Gunblade bank.
///
/// Native set 23 starts after 300 direction-free idle ticks, blending in
/// over five normal-rate frames before looping its final section.
/// Playback borrows this descriptor and its nineteen tracks; the Gunblade
/// package must remain loaded while any slot or cue record refers to them.
static AnimationSet _gGunbladeHighHealthIdleAnimation = {
    .records           = _gGunbladeHighHealthIdleAnimationRecords,
    .trackStartIndices = _gGunbladeHighHealthIdleAnimationIndices,
    .poseBanks         = {
        [GUNBLADE_ANIMATION_TRANSLATION_ROTATION_BANK] = _gGunbladeHighHealthIdleAnimationBank1,
        [GUNBLADE_ANIMATION_PACKED_ROTATION_BANK]      = _gGunbladeHighHealthIdleAnimationBank4,
    },
};

/// Translation/Euler keyframes for `_gGunbladeLowHealthIdleAnimation`'s tracks 0 and 1.
///
/// Translations use model units and angles use 1/4096 turns.
/// Record offsets address each twelve-byte pose in four-byte words.
static const AnimationPackedPose _gGunbladeLowHealthIdleAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/gunblade_animation_08B64_bank1.inc"
};

/// Rotation-only keyframes for `_gGunbladeLowHealthIdleAnimation`'s tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gGunbladeLowHealthIdleAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/gunblade_animation_08B64_bank4.inc"
};

/// Low-health idle/paralysis keyframes, loop jumps and stop controls, with durations in normal-rate frames.
static const AnimationRecord _gGunbladeLowHealthIdleAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/gunblade_animation_08B64_records.inc"
};

/// Absolute record starts for `_gGunbladeLowHealthIdleAnimation`'s nineteen player tracks.
///
/// Valid track indices are 0..18; native child playback uses 1..18.
/// The serialized twentieth halfword is zero and is retained in this array.
static const u16 _gGunbladeLowHealthIdleAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/gunblade_animation_08B64_indices.inc"
};

/// Player idle loop at or below a quarter maximum HP, also used for paralysis.
///
/// Native set 25 starts after 300 direction-free idle ticks with a five-frame
/// blend. Paralysis uses the same clip with a six-frame blend on all active
/// child slots. Both paths loop the final section.
/// Playback borrows this descriptor and its nineteen tracks; the Gunblade
/// package must remain loaded while any slot or cue record refers to them.
static AnimationSet _gGunbladeLowHealthIdleAnimation = {
    .records           = _gGunbladeLowHealthIdleAnimationRecords,
    .trackStartIndices = _gGunbladeLowHealthIdleAnimationIndices,
    .poseBanks         = {
        [GUNBLADE_ANIMATION_TRANSLATION_ROTATION_BANK] = _gGunbladeLowHealthIdleAnimationBank1,
        [GUNBLADE_ANIMATION_PACKED_ROTATION_BANK]      = _gGunbladeLowHealthIdleAnimationBank4,
    },
};

/// Translation/Euler keyframes for `_gGunbladeMidHealthIdleAnimation`'s tracks 0 and 1.
///
/// Translations use model units and angles use 1/4096 turns.
/// Record offsets address each twelve-byte pose in four-byte words.
static const AnimationPackedPose _gGunbladeMidHealthIdleAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/gunblade_animation_09280_bank1.inc"
};

/// Rotation-only keyframes for `_gGunbladeMidHealthIdleAnimation`'s tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gGunbladeMidHealthIdleAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/gunblade_animation_09280_bank4.inc"
};

/// Middle-health idle keyframes, loop jumps and stop controls, with durations in normal-rate frames.
static const AnimationRecord _gGunbladeMidHealthIdleAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/gunblade_animation_09280_records.inc"
};

/// Absolute record starts for `_gGunbladeMidHealthIdleAnimation`'s nineteen player tracks.
///
/// Valid track indices are 0..18; native child playback uses 1..18.
/// The serialized twentieth halfword is zero and is retained in this array.
static const u16 _gGunbladeMidHealthIdleAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/gunblade_animation_09280_indices.inc"
};

/// Player idle loop above a quarter and at or below half maximum HP.
///
/// Native set 24 in the Gunblade bank starts after 300 direction-free idle
/// ticks, blending in over five normal-rate frames before looping its final
/// section.
/// Playback borrows this descriptor and its nineteen tracks; the Gunblade
/// package must remain loaded while any slot or cue record refers to them.
static AnimationSet _gGunbladeMidHealthIdleAnimation = {
    .records           = _gGunbladeMidHealthIdleAnimationRecords,
    .trackStartIndices = _gGunbladeMidHealthIdleAnimationIndices,
    .poseBanks         = {
        [GUNBLADE_ANIMATION_TRANSLATION_ROTATION_BANK] = _gGunbladeMidHealthIdleAnimationBank1,
        [GUNBLADE_ANIMATION_PACKED_ROTATION_BANK]      = _gGunbladeMidHealthIdleAnimationBank4,
    },
};

/// Translation/Euler keyframes for `_gGunbladeEventEntryAnimation`'s tracks 0 and 1.
///
/// Translations use model units and angles use 1/4096 turns.
/// Record offsets address each twelve-byte pose in four-byte words.
static const AnimationPackedPose _gGunbladeEventEntryAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/gunblade_animation_09700_bank1.inc"
};

/// Rotation-only keyframes for `_gGunbladeEventEntryAnimation`'s tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gGunbladeEventEntryAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/gunblade_animation_09700_bank4.inc"
};

/// Event-entry keyframes and stop controls, with durations in normal-rate frames.
static const AnimationRecord _gGunbladeEventEntryAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/gunblade_animation_09700_records.inc"
};

/// Absolute record starts for `_gGunbladeEventEntryAnimation`'s nineteen player tracks.
///
/// Valid track indices are 0..18; native child playback uses 1..18.
/// The serialized twentieth halfword is zero and is retained in this array.
static const u16 _gGunbladeEventEntryAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/gunblade_animation_09700_indices.inc"
};

/// Player event-entry presentation supplied by the Gunblade bank.
///
/// Native set 32 plays without a timed blend when scripted presentation
/// receives the zero selector, then stops before the event begins. Native
/// set 34 points to this same clip.
/// Playback borrows this descriptor and its nineteen tracks; the Gunblade
/// package must remain loaded while any slot or cue record refers to them.
static AnimationSet _gGunbladeEventEntryAnimation = {
    .records           = _gGunbladeEventEntryAnimationRecords,
    .trackStartIndices = _gGunbladeEventEntryAnimationIndices,
    .poseBanks         = {
        [GUNBLADE_ANIMATION_TRANSLATION_ROTATION_BANK] = _gGunbladeEventEntryAnimationBank1,
        [GUNBLADE_ANIMATION_PACKED_ROTATION_BANK]      = _gGunbladeEventEntryAnimationBank4,
    },
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
    &_gGunbladePart4HitReactionAnimation,
    &_gGunbladeOtherBodyHitReactionAnimation,
    &_gGunbladeStoppedPoseAnimation,
    &_gGunbladeWalkAnimation,
    &_gGunbladeAnimation0C9C0,
    &_gGunbladeIdleAnimation,
    &_gGunbladeIdleAnimation,
    &_gGunbladeHighHealthIdleAnimation,
    &_gGunbladeMidHealthIdleAnimation,
    &_gGunbladeLowHealthIdleAnimation,
    &_gGunbladeParasiteEnergyReleaseFamily0Animation,
    &_gGunbladeParasiteEnergyChargeFamily0Animation,
    &_gGunbladeParasiteEnergyCastFamily0Animation,
    &_gGunbladeParasiteEnergyReleaseFamily1Animation,
    &_gGunbladeParasiteEnergyChargeFamily1Animation,
    &_gGunbladeParasiteEnergyCastFamily1Animation,
    &_gGunbladeEventEntryAnimation,
    &_gGunbladeAnimation098D8,
    &_gGunbladeEventEntryAnimation,
    &_gGunbladeAnimation098D8,
    &_gGunbladeStairAscentAnimation,
    &_gGunbladeStairDescentAnimation,
    &_gGunbladeStairAscentFinishEvenAnimation,
    &_gGunbladeStairAscentFinishOddAnimation,
    &_gGunbladeItemUseAnimation,
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
