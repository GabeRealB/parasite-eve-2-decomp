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
#include "../../shared/effects/blade_trail.h"
#include "types.h"

static Task*       _gGunbladeTrailTask;
static EffectWork* _gGunbladeTrailWork;

enum {
    GUNBLADE_TRAIL_ENDPOINT_NEAR = 0,
    GUNBLADE_TRAIL_ENDPOINT_FAR  = 1,
};

/// Near and far ribbon endpoints in integer coordinates of the Gunblade muzzle frame.
///
/// The task coordinate stays at endpoint 0; endpoint 1 is sampled every update.
static SVECTOR _gGunbladeTrailEndpointOffsets[] = {
    [GUNBLADE_TRAIL_ENDPOINT_NEAR] = { 0, 0x0060, 0x0080, 0 },
    [GUNBLADE_TRAIL_ENDPOINT_FAR]  = { 0, 0x0060, 0x0380, 0 },
};

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
                nearCoord->coord.t[0]   = _gGunbladeTrailEndpointOffsets[GUNBLADE_TRAIL_ENDPOINT_NEAR].vx;
                _gGunbladeTrailTask     = task;
                nearCoord->coord.t[1]   = _gGunbladeTrailEndpointOffsets[GUNBLADE_TRAIL_ENDPOINT_NEAR].vy;
                _gGunbladeTrailWork     = effectWork;
                nearCoord->coord.t[2]   = _gGunbladeTrailEndpointOffsets[GUNBLADE_TRAIL_ENDPOINT_NEAR].vz;
                nearCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(nearCoord);
                task->state              = GUNBLADE_TRAIL_RECORD;
                farOffset                = &_gGunbladeTrailEndpointOffsets[GUNBLADE_TRAIL_ENDPOINT_FAR];
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
            case GUNBLADE_TRAIL_RECORD: {
                const SVECTOR* recordOffset;

                // Record world poses so old frames follow the camera, independently of the weapon.
                nearCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(nearCoord);
                recordOffset             = &_gGunbladeTrailEndpointOffsets[GUNBLADE_TRAIL_ENDPOINT_FAR];
                farEndpoint.parent       = effectWork->parent;
                farEndpoint.coord.t[0]   = recordOffset->vx;
                farEndpoint.coord.t[1]   = recordOffset->vy;
                farEndpoint.coord.t[2]   = recordOffset->vz;
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
            }
            default:
                return;
        }
    }
    if (!keepTask) {
        _gGunbladeTrailWork = NULL;
        effectKillTask(effectWork, task);
    }
}

/// Binds the Gunblade's near-endpoint ring to the first side of the swing ribbon.
///
/// Supplies a side-effect-free `GfxCoord[8]` array identifier, paired by slot
/// with `BLADE_TRAIL_SECOND_ENDPOINT_HISTORY`. The drawer borrows composed view-space
/// `workm.t` translations in game-coordinate units for corners 0 and 2;
/// the task seeds every slot and refreshes the caches before drawing.
/// This binding exists only around the shared drawer inclusion.
#define BLADE_TRAIL_FIRST_ENDPOINT_HISTORY _gGunbladeTrailNearHistory

/// Binds the Gunblade's far-endpoint ring to the second side of the swing ribbon.
///
/// Supplies a side-effect-free, complete `GfxCoord[8]` array identifier, paired
/// by slot with `BLADE_TRAIL_FIRST_ENDPOINT_HISTORY`. Seed every slot and
/// recompose in the current view before drawing. The drawer borrows `workm.t`
/// translations in game-coordinate units for corners 1 and 3, narrowing them
/// to signed 16 bits and retaining no pointers. This binding exists only
/// around the shared drawer inclusion.
#define BLADE_TRAIL_SECOND_ENDPOINT_HISTORY _gGunbladeTrailFarHistory
#include "../../shared/effects/blade_trail_draw.inc.c"
#undef BLADE_TRAIL_FIRST_ENDPOINT_HISTORY
#undef BLADE_TRAIL_SECOND_ENDPOINT_HISTORY

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

/// Draws the charge flash's additive bands and screen tint, then steps its fade.
///
/// `effectWork` is live writable EffectWork; `coord` borrows a composed
/// view-space GfxCoord and `rgb` three readable tint bytes. The first band has
/// inner radius 3 * step / 2 and width 96 game units; odd ages draw a second
/// band with those dimensions swapped. Radius narrows to s16 before drawing.
/// After all draws, period loses 16 brightness units and step gains 64 game
/// units. The caller prepares RGB and checks period >= 17 before this phase.
/// Requires the drawers' scratch/GTE state and unchecked frame-arena space;
/// input storage must stay clear of their workspace. Retains no input pointer.
/// Pointer expressions repeat and must have no side effects. Captures no caller
/// locals. Expands to a block with no control transfer; use inside braces.
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
    EffectWork* trailWork = _gGunbladeTrailWork;

    if (trailWork != NULL) {
        _gGunbladeTrailTask->spawnArg1.value = ammunitionIndex;
        trailWork->index++;
    }
}

/// The model `gGunbladeModel`: generated from its asset (weapons/gunblade/model) by tools/buildsteps/gen_model_source.py.
#include "assets/weapons/gunblade/model.inc"

/// The player's clips this package carries, in the package's order, and its animation bank (weapons/gunblade/bank):
/// generated from the clip and bank assets by tools/buildsteps/gen_animation_source.py.
#include "assets/weapons/gunblade/clips.inc"
#include "assets/weapons/gunblade/bank.inc"

/// Borrowed trail task receiving the ammunition selector for a deferred charge flash.
///
/// Published when the histories are seeded. Explicit retirement leaves this
/// handle unchanged; requests dereference it only behind `_gGunbladeTrailWork`.
/// Parent teardown does not invalidate either cached handle.
static Task* _gGunbladeTrailTask = NULL;

/// Borrowed trail work whose index counts deferred charge-flash requests.
///
/// Published with `_gGunbladeTrailTask` when the histories are seeded and cleared
/// before normal or cancellation retirement. Parent teardown releases the work
/// through the generic effect exit without clearing this cache; a non-NULL
/// value alone does not establish liveness after that teardown.
static EffectWork* _gGunbladeTrailWork = NULL;

/// Two retained zero words with no code references; their original role is unproven.
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
