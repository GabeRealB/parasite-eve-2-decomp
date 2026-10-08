#include "pe/lifedrain.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/effects.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"
#include "../../shared/glow_draw.h"
#include "../../shared/rising_spark.h"

/// Visual tuning of the life drain cast for one Parasite Energy level.
///
/// The cast draws a funnel around the caster: a fan of glow wedges, two rings
/// and one or more gradient rings that all grow with one radius, shedding a
/// spark each frame, while three bands expand from the centre. The cast task
/// and each band select their row with the level digit of the attachment id,
/// less one. A mote selects its row with an `EffectWork` halfword it never
/// writes, so it reads the first row at every level.
///
/// Radii and widths are world units ahead of the perspective divide.
/// `brightness` is a blue channel value; red and green are half of it.
typedef struct {
    s16 wedgeCount;  // Glow wedges fanned around the funnel; the yaw table holds 16
    s16 brightness;  // Brightness the opening flash fades from and the funnel rises back to; a band starts at it and grows its inner radius by a third of it and its width by half of it each frame
    s16 outerOffset; // Width a band starts with; from level 2, also how far outside the funnel's gradient ring a second, dimmer one is drawn
    s16 radiusLimit; // Funnel radius that ends the growth, and the radius of each spark the funnel sheds; a mote's sparks take it too, and its own sprite is sized 0x100 less
    s16 radiusStep;  // Funnel radius gained per frame, while it grows and while it fades
} _LifedrainLevelTuning;
STATIC_ASSERT_SIZEOF(_LifedrainLevelTuning, 0xA);

static void _lifedrainDrawMoteBillboards(const GfxCoord* coord, s16 animationFrame, s16 sizeFactor);

/// Per-level tuning for the life drain, one row per PE level 1-3, weakest
/// first.
static _LifedrainLevelTuning D_lifedrain_80130AB4[] = {
    { 0x0008, 0x0080, 0x0100, 0x0400, 0x0040 },
    { 0x000C, 0x00B0, 0x0200, 0x0500, 0x0048 },
    { 0x0010, 0x00E0, 0x0300, 0x0600, 0x0050 },
};

/// Sound-script id of the drain's opening cue, indexed by `EffectWork.index`
/// when the cast has drained nothing yet and by `field_20 + 3` once there is
/// health banked in `gSceneCombatState.lifeDrainHp`.
static s32 D_lifedrain_80130AD4[] = {
    0xE0210001,
    0xE0240001,
    0xE0270001,
    0xE0210002,
    0xE0240002,
    0xE0270002,
};

/// One yaw per funnel wedge, `_LifedrainLevelTuning::wedgeCount` of them, re-rolled as a
/// block when the cast starts and replayed every frame by
/// `glowDrawWedge`.
static s16 D_lifedrain_80130AEC[16] = { 0 };
/// The cast's collector task, published by `lifedrainCastTask`. Every
/// drain mote reparents itself onto it and adds its own `spawnArg1` to the
/// running total there.
static struct Task* D_lifedrain_80130B0C = NULL;

/// Draws the cast funnel and attenuates the caller's writable RGB bytes.
///
/// Arguments are side-effect-free live coordinate/work pointers and a writable
/// three-byte colour array, used repeatedly. Work is read-only: index is PE
/// level 0..2, angle the radius, and age selects alternate broad bands.
/// Captures the cast's s32 phaseIndex loop counter and the package yaw/tuning
/// tables. The scoped tuning/yaw pointers are local to each expansion.
#define LIFEDRAIN_DRAW_CAST_FUNNEL(coord, work, rgb)                                                        \
    {                                                                                                       \
        enum { LIFEDRAIN_CAST_GLOW_BAND_WIDTH = 128 };                                                      \
        const _LifedrainLevelTuning* tuning;                                                                \
        const s16*                   wedgeYaw;                                                              \
        phaseIndex = 0;                                                                                     \
        if (D_lifedrain_80130AB4[(work)->index].wedgeCount > 0) {                                           \
            tuning   = D_lifedrain_80130AB4;                                                                \
            wedgeYaw = D_lifedrain_80130AEC;                                                                \
            do {                                                                                            \
                glowDrawWedge((coord), (work)->angle, *wedgeYaw, (rgb));                                    \
                wedgeYaw += 1;                                                                              \
            } while (++phaseIndex < tuning[(work)->index].wedgeCount);                                      \
        }                                                                                                   \
        effectDrawGouraudDisc((coord), (work)->angle >> 1, (rgb));                                          \
        effectDrawGouraudDisc((coord), (work)->angle >> 1, (rgb));                                          \
        (rgb)[0] >>= 1;                                                                                     \
        (rgb)[1] >>= 1;                                                                                     \
        (rgb)[2] >>= 1;                                                                                     \
        effectDrawOuterGlowBand((coord), (work)->angle, LIFEDRAIN_CAST_GLOW_BAND_WIDTH, (rgb));             \
        if ((work)->age & 1) {                                                                              \
            effectDrawOuterGlowBand((coord), LIFEDRAIN_CAST_GLOW_BAND_WIDTH, (work)->angle, (rgb));         \
        }                                                                                                   \
        if ((work)->index != 0) {                                                                           \
            (rgb)[0] >>= 1;                                                                                 \
            (rgb)[1] >>= 1;                                                                                 \
            (rgb)[2] >>= 1;                                                                                 \
            effectDrawOuterGlowBand((coord),                                                                \
                                    (s16)((work)->angle + D_lifedrain_80130AB4[(work)->index].outerOffset), \
                                    LIFEDRAIN_CAST_GLOW_BAND_WIDTH, (rgb));                                 \
            if ((work)->index == 2) {                                                                       \
                if ((work)->age & 1) {                                                                      \
                    effectDrawOuterGlowBand((coord), LIFEDRAIN_CAST_GLOW_BAND_WIDTH,                        \
                                            (s16)((work)->angle + D_lifedrain_80130AB4[2].outerOffset),     \
                                            (rgb));                                                         \
                }                                                                                           \
            }                                                                                               \
        }                                                                                                   \
    }

void lifedrainCastTask(Task* task)
{
    enum {
        LIFEDRAIN_CAST_STATE_INITIALIZE      = 0,
        LIFEDRAIN_CAST_STATE_COLLECTING      = 1,
        LIFEDRAIN_CAST_STATE_GROWING         = 2,
        LIFEDRAIN_CAST_STATE_FADING          = 3,
        LIFEDRAIN_CAST_STATE_RELEASE         = 4,
        LIFEDRAIN_CAST_INITIAL_RADIUS        = 0x80,
        LIFEDRAIN_CAST_BRIGHTNESS_STEP       = 16,
        LIFEDRAIN_CAST_FADE_END_BRIGHTNESS   = 17,
        LIFEDRAIN_CAST_COLLECTION_END_AGE    = 30,
        LIFEDRAIN_CAST_CUE_AGE               = 3,
        LIFEDRAIN_CAST_SUCCESS_CUE_OFFSET    = 3,
        LIFEDRAIN_CAST_WEDGE_YAW_SHIFT       = 10,
        LIFEDRAIN_CAST_WEDGE_YAW_JITTER_MASK = 0x3FF,
        LIFEDRAIN_CAST_BAND_YAW_STEP         = 0x2AA,
        LIFEDRAIN_CAST_BAND_YAW_END          = 0x556,
        LIFEDRAIN_CAST_ANGLE_MASK            = 0xFFF,
        LIFEDRAIN_CAST_TRIG_FRACTION_BITS    = 12,
    };
    EffectWork* work;
    GfxCoord*   coord;
    s32         phaseIndex;
    u8          rgb[3];

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if ((Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_HELD) || (gRoomEffectState->peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN)) {
        // Collected motes count immediately; cancellation can still grant their banked HP.
        if ((task->state < LIFEDRAIN_CAST_STATE_GROWING) && (task->spawnArg1.value != 0)) {
            gPlayerStatus.hp = (u16)gPlayerStatus.hp + gSceneCombatState.lifeDrainHp;
            if (gPlayerStatus.hp > gPlayerStatus.hpMax) {
                gPlayerStatus.hp = gPlayerStatus.hpMax;
            }
        }
        effectKillTask(work, task);
        return;
    }
    work->age = work->age + 1;
    switch (task->state) {
        case LIFEDRAIN_CAST_STATE_INITIALIZE: {
            EffectWork* spawned;
            s32         bandAngle;

            // Motes use this live collector as both their counter and teardown parent.
            D_lifedrain_80130B0C = task;
            coord->parent        = work->parent;
            gfxSetRotIdentity(&coord->coord);
            coord->coord.t[0]   = 0;
            coord->coord.t[1]   = 0;
            coord->coord.t[2]   = 0;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            task->state  = LIFEDRAIN_CAST_STATE_COLLECTING;
            work->index  = (Gp_StateC08.attachId % 10) - 1;
            work->scale  = D_lifedrain_80130AB4[work->index].brightness;
            work->angle  = LIFEDRAIN_CAST_INITIAL_RADIUS;
            work->period = D_lifedrain_80130AB4[work->index].brightness;
            phaseIndex   = 0;
            if (D_lifedrain_80130AB4[work->index].wedgeCount > 0) {
                do {
                    s32 yawRng;

                    yawRng                           = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    D_lifedrain_80130AEC[phaseIndex] = (phaseIndex << LIFEDRAIN_CAST_WEDGE_YAW_SHIFT) + (((u32)yawRng >> 16) & LIFEDRAIN_CAST_WEDGE_YAW_JITTER_MASK);
                    gRandomLcgState                  = yawRng;
                } while (++phaseIndex < D_lifedrain_80130AB4[work->index].wedgeCount);
            }
            bandAngle = 0;
            do {
                spawned = effectSpawn(EFFECT_LIFEDRAIN_RING, coord, bandAngle, NULL);
                if (spawned != NULL) {
                    taskReparent(task, spawned->task);
                }
                bandAngle += LIFEDRAIN_CAST_BAND_YAW_STEP;
            } while (bandAngle < LIFEDRAIN_CAST_BAND_YAW_END);
            Gp_StateC08.flags |= ATTACHMENT_FLAG_APPLY_STATS;
            return;
        }
        case LIFEDRAIN_CAST_STATE_COLLECTING:
            if (work->scale != 0) {
                work->scale = work->scale - LIFEDRAIN_CAST_BRIGHTNESS_STEP;
                rgb[0]      = work->scale >> 1;
                rgb[1]      = work->scale >> 1;
                rgb[2]      = (u8)work->scale;
                effectDrawScreenTint(rgb, GPU_BLEND_ADD);
            }
            if (work->age == LIFEDRAIN_CAST_COLLECTION_END_AGE) {
                if (task->spawnArg1.value != 0) {
                    gPlayerStatus.hp = (u16)gPlayerStatus.hp + gSceneCombatState.lifeDrainHp;
                    if (gPlayerStatus.hp > gPlayerStatus.hpMax) {
                        gPlayerStatus.hp = gPlayerStatus.hpMax;
                    }
                    task->state = LIFEDRAIN_CAST_STATE_GROWING;
                } else {
                    task->state = LIFEDRAIN_CAST_STATE_RELEASE;
                }
                return;
            }
            if (work->age != LIFEDRAIN_CAST_CUE_AGE) {
                return;
            }
            actorRenderComposeCoord(coord);
            if (task->spawnArg1.value != 0) {
                sndEvtRequestScriptStart(D_lifedrain_80130AD4[work->index + LIFEDRAIN_CAST_SUCCESS_CUE_OFFSET],
                                         (s8)worldCoordGetOriginAudioPan(coord),
                                         (s8)worldCoordGetOriginAudioDepth(coord));
            } else {
                sndEvtRequestScriptStart(D_lifedrain_80130AD4[work->index],
                                         (s8)worldCoordGetOriginAudioPan(coord),
                                         (s8)worldCoordGetOriginAudioDepth(coord));
            }
            return;
        case LIFEDRAIN_CAST_STATE_GROWING: {
            EffectWork* spawned;
            s32         brightness;

            actorRenderComposeCoord(coord);
            if (work->period != 0) {
                work->period = work->period - LIFEDRAIN_CAST_BRIGHTNESS_STEP;
                rgb[0]       = work->period >> 1;
                rgb[1]       = work->period >> 1;
                rgb[2]       = (u8)work->period;
                effectDrawScreenTint(rgb, GPU_BLEND_ADD);
            }
            brightness = work->scale;
            if (brightness < D_lifedrain_80130AB4[work->index].brightness) {
                brightness += LIFEDRAIN_CAST_BRIGHTNESS_STEP;
            }
            work->scale = brightness;
            work->angle = work->angle + D_lifedrain_80130AB4[work->index].radiusStep;
            rgb[0]      = work->scale >> 1;
            rgb[1]      = work->scale >> 1;
            rgb[2]      = (u8)work->scale;
            LIFEDRAIN_DRAW_CAST_FUNNEL(coord, work, rgb);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->step      = (gRandomLcgState >> 16) & LIFEDRAIN_CAST_ANGLE_MASK;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            gfxRotMatrixY(&coord->coord, (gRandomLcgState >> 16) & LIFEDRAIN_CAST_ANGLE_MASK, 0);
            gte_SetRotMatrix(&coord->coord);
            gte_ldv0(&work->move);
            gte_rtv0();
            gte_stsv(&work->move);
            work->move.vx = (rcos(work->step) * work->angle) >> LIFEDRAIN_CAST_TRIG_FRACTION_BITS;
            work->move.vy = (rsin(work->step) * work->angle) >> LIFEDRAIN_CAST_TRIG_FRACTION_BITS;
            work->move.vz = 0;
            spawned       = effectSpawn(EFFECT_LIFEDRAIN_SPARK, coord, (s32)D_lifedrain_80130AB4[work->index].radiusLimit,
                                        &work->move);
            if (spawned != NULL) {
                taskReparent(task, spawned->task);
            }
            if (work->angle >= D_lifedrain_80130AB4[work->index].radiusLimit) {
                task->state = LIFEDRAIN_CAST_STATE_FADING;
            }
            return;
        }
        case LIFEDRAIN_CAST_STATE_FADING: {

            actorRenderComposeCoord(coord);
            work->scale = work->scale - LIFEDRAIN_CAST_BRIGHTNESS_STEP;
            work->angle = work->angle + D_lifedrain_80130AB4[work->index].radiusStep;
            if (work->scale < LIFEDRAIN_CAST_FADE_END_BRIGHTNESS) {
                task->state = LIFEDRAIN_CAST_STATE_RELEASE;
            }
            rgb[0] = work->scale >> 1;
            rgb[1] = work->scale >> 1;
            rgb[2] = (u8)work->scale;
            LIFEDRAIN_DRAW_CAST_FUNNEL(coord, work, rgb);
            return;
        }
        case LIFEDRAIN_CAST_STATE_RELEASE:
            effectKillTask(work, task);
            return;
    }
}

#undef LIFEDRAIN_DRAW_CAST_FUNNEL

#include "../../shared/rising_spark_task.inc.c"

void lifedrainRisingSparkTask(Task* task)
{
    _risingSparkTask(task);
}

/// Stores the player's transformed and Q12-scaled displacement in the mote's pos.
///
/// Arguments are side-effect-free live pointers, used repeatedly; work is writable.
/// Requires composed coordinates in one frame and work age 15..29, making 30-age
/// positive. Narrows the transposed displacement to s16 before the local rotation;
/// there is no normalization. Captures the mote's GfxCoord* playerCoord and VECTOR
/// playerOffset scratch locals, and uses the live player and GTE working registers.
#define LIFEDRAIN_AIM_MOTE(coord, work)                                                        \
    {                                                                                          \
        enum { LIFEDRAIN_MOTE_RELEASE_AGE     = 30,                                            \
               LIFEDRAIN_MOTE_HOMING_GAIN_Q12 = 0x1200 };                                      \
        playerCoord     = &(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords[1];     \
        playerOffset.vx = playerCoord->workm.t[0] - (coord)->workm.t[0];                       \
        playerOffset.vy = playerCoord->workm.t[1] - (coord)->workm.t[1];                       \
        playerOffset.vz = playerCoord->workm.t[2] - (coord)->workm.t[2];                       \
        ApplyTransposeMatrixLV(&(coord)->workm, &playerOffset, &playerOffset);                 \
        (work)->pos.vx = playerOffset.vx;                                                      \
        (work)->pos.vy = playerOffset.vy;                                                      \
        (work)->pos.vz = playerOffset.vz;                                                      \
        gte_SetRotMatrix(&(coord)->coord);                                                     \
        gte_ldv0(&(work)->pos);                                                                \
        gte_rtv0();                                                                            \
        gte_stsv(&(work)->pos);                                                                \
        gte_lddp(LIFEDRAIN_MOTE_HOMING_GAIN_Q12 / (LIFEDRAIN_MOTE_RELEASE_AGE - (work)->age)); \
        gte_ldsv(&(work)->pos);                                                                \
        gte_gpf12();                                                                           \
        gte_stsv(&(work)->pos);                                                                \
    }

void lifedrainMoteTask(Task* task)
{
    enum {
        LIFEDRAIN_MOTE_STATE_INITIALIZE        = 0,
        LIFEDRAIN_MOTE_STATE_DRIFTING          = 1,
        LIFEDRAIN_MOTE_STATE_HOMING            = 2,
        LIFEDRAIN_MOTE_HORIZONTAL_DRIFT_BASE   = 0x40,
        LIFEDRAIN_MOTE_HORIZONTAL_DRIFT_MASK   = 0x7F,
        LIFEDRAIN_MOTE_VERTICAL_DRIFT_ENCODING = 0xFFE0,
        LIFEDRAIN_MOTE_VERTICAL_DRIFT_MASK     = 0x3F,
        LIFEDRAIN_MOTE_SIZE_SHORTFALL          = 0x100,
        LIFEDRAIN_MOTE_AIM_START_AGE           = 15,
        LIFEDRAIN_MOTE_RELEASE_AGE             = 30,
        LIFEDRAIN_MOTE_STEERING_STEP           = 16,
        LIFEDRAIN_MOTE_SPARK_CHANCE_MASK       = 3,
        LIFEDRAIN_MOTE_FRAME_MASK              = 3,
    };
    EffectWork* work;
    GfxCoord*   coord;
    GfxCoord*   playerCoord;
    VECTOR      playerOffset;
    EffectWork* spawned;
    s16         sparkRadius;
    s32         velocityComponent;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if ((Gp_StateC08.effectPhase != ATTACHMENT_EFFECT_HELD) && (gRoomEffectState->peEffectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN)) {
        work->age = work->age + 1;
        switch (task->state) {
            case LIFEDRAIN_MOTE_STATE_INITIALIZE:
                taskReparent(D_lifedrain_80130B0C, task);
                D_lifedrain_80130B0C->spawnArg1.value += task->spawnArg1.value;
                gRandomLcgState                        = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->move.vx                          = LIFEDRAIN_MOTE_HORIZONTAL_DRIFT_BASE - ((gRandomLcgState >> 16) & LIFEDRAIN_MOTE_HORIZONTAL_DRIFT_MASK);
                gRandomLcgState                        = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->move.vy                          = LIFEDRAIN_MOTE_VERTICAL_DRIFT_ENCODING - ((gRandomLcgState >> 16) & LIFEDRAIN_MOTE_VERTICAL_DRIFT_MASK);
                gRandomLcgState                        = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->move.vz                          = LIFEDRAIN_MOTE_HORIZONTAL_DRIFT_BASE - ((gRandomLcgState >> 16) & LIFEDRAIN_MOTE_HORIZONTAL_DRIFT_MASK);
                task->state                            = LIFEDRAIN_MOTE_STATE_DRIFTING;
                // Keep the binary's sizing selector: step stays at its cleared zero value.
                work->scale  = (Gp_StateC08.attachId % 10) - 1;
                sparkRadius  = D_lifedrain_80130AB4[work->step].radiusLimit;
                work->angle  = sparkRadius;
                work->period = sparkRadius - LIFEDRAIN_MOTE_SIZE_SHORTFALL;
                /* fallthrough */
            case LIFEDRAIN_MOTE_STATE_DRIFTING:
                coord->coord.t[0]  += work->move.vx;
                coord->coord.t[1]  += work->move.vy;
                coord->coord.t[2]  += work->move.vz;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(coord);
                if (work->age & 1) {
                    work->index = work->index + 1;
                    _lifedrainDrawMoteBillboards(coord, work->index, work->period);
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    if (((gRandomLcgState >> 16) & LIFEDRAIN_MOTE_SPARK_CHANCE_MASK) == 0) {
                        spawned = effectSpawn(EFFECT_LIFEDRAIN_SPARK, coord, (s32)(work->angle), NULL);
                        if (spawned != NULL) {
                            taskReparent(task, spawned->task);
                        }
                    }
                }
                if (work->age == LIFEDRAIN_MOTE_AIM_START_AGE) {
                    LIFEDRAIN_AIM_MOTE(coord, work);
                    task->state = LIFEDRAIN_MOTE_STATE_HOMING;
                }
                return;
            case LIFEDRAIN_MOTE_STATE_HOMING:
                // Approach the scaled displacement component by component, without clamping.
                velocityComponent = work->move.vx;
                work->move.vx     = (velocityComponent < work->pos.vx) ? velocityComponent + LIFEDRAIN_MOTE_STEERING_STEP : velocityComponent - LIFEDRAIN_MOTE_STEERING_STEP;
                velocityComponent = work->move.vy;
                work->move.vy     = (velocityComponent < work->pos.vy) ? velocityComponent + LIFEDRAIN_MOTE_STEERING_STEP : velocityComponent - LIFEDRAIN_MOTE_STEERING_STEP;
                velocityComponent = work->move.vz;
                work->move.vz     = (velocityComponent < work->pos.vz) ? velocityComponent + LIFEDRAIN_MOTE_STEERING_STEP : velocityComponent - LIFEDRAIN_MOTE_STEERING_STEP;

                coord->coord.t[0]  += work->move.vx;
                coord->coord.t[1]  += work->move.vy;
                coord->coord.t[2]  += work->move.vz;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(coord);
                if (work->age >= LIFEDRAIN_MOTE_RELEASE_AGE) {
                    break;
                }
                if (work->age & 1) {
                    work->index = (work->index + 1) & LIFEDRAIN_MOTE_FRAME_MASK;
                    _lifedrainDrawMoteBillboards(coord, work->index, work->period);
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    if (((gRandomLcgState >> 16) & LIFEDRAIN_MOTE_SPARK_CHANCE_MASK) == 0) {
                        spawned = effectSpawn(EFFECT_LIFEDRAIN_SPARK, coord, (s32)(work->angle), NULL);
                        if (spawned != NULL) {
                            taskReparent(task, spawned->task);
                        }
                    }
                }
                LIFEDRAIN_AIM_MOTE(coord, work);
                return;
            default:
                return;
        }
    }
    effectKillTask(work, task);
}

#undef LIFEDRAIN_AIM_MOTE

/// Sets a mote billboard's corners around its projected centre.
///
/// Borrows a live scratch block and writable quad. Centre and extent are
/// used in signed word edge arithmetic, which must fit s32; packet stores
/// retain the low 16 bits. Only the eight XY halfwords change.
static inline void _lifedrainSetMoteBillboardBounds(POLY_FT4* quad, const EffectCentreScratch* scratch)
{
    s16 edgeX;
    s16 edgeY;

    edgeX    = scratch->screenX - scratch->screenExtent;
    quad->x2 = edgeX;
    quad->x0 = edgeX;
    edgeX    = scratch->screenX + scratch->screenExtent;
    quad->x3 = edgeX;
    quad->x1 = edgeX;
    edgeY    = scratch->screenY - scratch->screenExtent;
    quad->y1 = edgeY;
    quad->y0 = edgeY;
    edgeY    = scratch->screenY + scratch->screenExtent;
    quad->y3 = edgeY;
    quad->y2 = edgeY;
}

/// Queues the animated core and alternating-palette halo of a Life Drain mote.
///
/// Borrows `coord`'s composed translation in the input space of `GsWSMATRIX`,
/// narrowing each component to signed 16 bits. Both quads stay aligned to the
/// screen axes and use additive, unmodulated texture colours. The low two
/// bits of `animationFrame` select a 24-by-24 core cell; its low bit selects
/// one of two palettes for the fixed 56-by-56 halo cell.
///
/// `sizeFactor` is a signed perspective-sizing numerator, not a pixel radius:
/// the core's screen half-extent is sizeFactor * 23 / depth, and the halo's is
/// (s16)(sizeFactor * 2 / 3) * 55 / depth, truncated toward zero. Depth is
/// SZ3 / 4 + 1, shared by sizing and ordering. Negative GTE FLAG rejects both.
/// Requires initialized projection settings, room for one `EffectCentreScratch`
/// on the word-aligned scratch stack and two `POLY_FT4`s in the frame arena.
/// Scratch is released on both paths; queued packets remain live until drawing.
static void _lifedrainDrawMoteBillboards(const GfxCoord* coord, s16 animationFrame, s16 sizeFactor)
{
    enum {
        LIFEDRAIN_MOTE_CORE_FRAME_COUNT      = 4,
        LIFEDRAIN_MOTE_CORE_CELL_WIDTH       = 24,
        LIFEDRAIN_MOTE_CORE_UV_SPAN          = LIFEDRAIN_MOTE_CORE_CELL_WIDTH - 1,
        LIFEDRAIN_MOTE_CORE_TPAGE            = getTPage(0, GPU_BLEND_ADD, 640, 0),
        LIFEDRAIN_MOTE_CORE_CLUT             = getClut(80, 267),
        LIFEDRAIN_MOTE_HALO_TPAGE            = getTPage(0, GPU_BLEND_ADD, 576, 0),
        LIFEDRAIN_MOTE_HALO_PALETTE_COUNT    = 2,
        LIFEDRAIN_MOTE_HALO_PALETTE_X        = 256,
        LIFEDRAIN_MOTE_HALO_PALETTE_X_STRIDE = 16,
        LIFEDRAIN_MOTE_HALO_CLUT_ROW         = getClut(0, 268),
        LIFEDRAIN_MOTE_HALO_LEFT_U           = 56,
        LIFEDRAIN_MOTE_HALO_TOP_V            = 200,
        LIFEDRAIN_MOTE_HALO_UV_SPAN          = 55,
        LIFEDRAIN_MOTE_DEPTH_BIAS            = 1,
    };
    EffectCentreScratch* scratchEnd;
    EffectCentreScratch* scratch;
    POLY_FT4*            quad;
    SVECTOR*             worldPoint;
    s32                  leftU;
    s32                  rightU;
    u16                  worldZBits;

    // Stage the composed centre before reserving the block; both sprites share one projection.
    scratchEnd                                = SCRATCH_STACK_CURSOR(EffectCentreScratch);
    scratchEnd[-1].worldPoint.vx              = (u16)coord->workm.t[0];
    scratch                                   = scratchEnd - 1;
    scratch->worldPoint.vy                    = (u16)coord->workm.t[1];
    worldZBits                                = (u16)coord->workm.t[2];
    SCRATCH_STACK_CURSOR(EffectCentreScratch) = scratch;
    scratch->worldPoint.vz                    = worldZBits;
    worldPoint                                = &scratch->worldPoint;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(worldPoint);
    gte_rtps();
    gte_stsxy(&scratchEnd[-1].screenX);
    gte_stflg(&scratchEnd[-1].projectionFlags);
    if (scratch->projectionFlags >= 0) {
        gte_stszotz(&scratchEnd[-1].depth);
        scratch->depth += LIFEDRAIN_MOTE_DEPTH_BIAS;
        quad            = gGpuPrimCursor;
        gGpuPrimCursor  = quad + 1;
        quad->tpage     = LIFEDRAIN_MOTE_CORE_TPAGE;
        quad->clut      = LIFEDRAIN_MOTE_CORE_CLUT;
        setPolyFT4(quad);
        setSemiTrans(quad, 1);
        setShadeTex(quad, 1);
        leftU                 = (animationFrame & (LIFEDRAIN_MOTE_CORE_FRAME_COUNT - 1)) * LIFEDRAIN_MOTE_CORE_CELL_WIDTH;
        rightU                = leftU + LIFEDRAIN_MOTE_CORE_UV_SPAN;
        quad->u1              = rightU;
        quad->u0              = leftU;
        quad->u2              = leftU;
        quad->u3              = rightU;
        quad->v2              = LIFEDRAIN_MOTE_CORE_UV_SPAN;
        quad->v3              = LIFEDRAIN_MOTE_CORE_UV_SPAN;
        quad->v0              = 0;
        quad->v1              = 0;
        scratch->screenExtent = (sizeFactor * LIFEDRAIN_MOTE_CORE_UV_SPAN) / scratch->depth;
        _lifedrainSetMoteBillboardBounds(quad, scratch);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                quad);

        // The halo alternates palettes while retaining a fixed texture cell.
        quad           = gGpuPrimCursor;
        gGpuPrimCursor = quad + 1;
        quad->tpage    = LIFEDRAIN_MOTE_HALO_TPAGE;
        quad->clut     = ((u32)(((animationFrame & (LIFEDRAIN_MOTE_HALO_PALETTE_COUNT - 1)) * LIFEDRAIN_MOTE_HALO_PALETTE_X_STRIDE) + LIFEDRAIN_MOTE_HALO_PALETTE_X) >> 4) | LIFEDRAIN_MOTE_HALO_CLUT_ROW;
        setPolyFT4(quad);
        setSemiTrans(quad, 1);
        setShadeTex(quad, 1);
        quad->u0              = LIFEDRAIN_MOTE_HALO_LEFT_U;
        quad->v0              = LIFEDRAIN_MOTE_HALO_TOP_V;
        quad->u1              = LIFEDRAIN_MOTE_HALO_LEFT_U + LIFEDRAIN_MOTE_HALO_UV_SPAN;
        quad->v1              = LIFEDRAIN_MOTE_HALO_TOP_V;
        quad->v2              = LIFEDRAIN_MOTE_HALO_TOP_V + LIFEDRAIN_MOTE_HALO_UV_SPAN;
        quad->v3              = LIFEDRAIN_MOTE_HALO_TOP_V + LIFEDRAIN_MOTE_HALO_UV_SPAN;
        quad->u2              = LIFEDRAIN_MOTE_HALO_LEFT_U;
        quad->u3              = LIFEDRAIN_MOTE_HALO_LEFT_U + LIFEDRAIN_MOTE_HALO_UV_SPAN;
        scratch->screenExtent = ((s16)((sizeFactor * 2) / 3) * LIFEDRAIN_MOTE_HALO_UV_SPAN) / scratch->depth;
        _lifedrainSetMoteBillboardBounds(quad, scratch);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectCentreScratch);
}

#include "../../shared/glow_draw_wedge.inc.c"

void lifedrainExpandingGlowBandTask(Task* task)
{
    enum {
        LIFEDRAIN_BAND_STATE_INITIALIZE   = 0,
        LIFEDRAIN_BAND_STATE_EXPAND       = 1,
        LIFEDRAIN_BAND_ROTATION_MASK      = 0xFFF,
        LIFEDRAIN_BAND_INITIAL_RADIUS     = 128,
        LIFEDRAIN_BAND_FADE_STEP          = 8,
        LIFEDRAIN_BAND_RELEASE_BRIGHTNESS = LIFEDRAIN_BAND_FADE_STEP + 1,
    };
    EffectWork* work;
    GfxCoord*   coord;
    s16         effectControl;
    s16         levelIndex;
    u8          rgb[3];
    s32         fadedBrightness;

    work          = task->spawnArg2.pointer;
    effectControl = gRoomEffectState->peEffectControl;
    coord         = task->extra.coordBody->coord;
    if (effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            effectKillTask(work, task);
        }
        return;
    }

    // Tilt each band's local XZ plane and capture its PE level on the first running tick.
    if (task->state == LIFEDRAIN_BAND_STATE_INITIALIZE) {
        gfxRotMatrixZ(&coord->coord, task->spawnArg1.value & LIFEDRAIN_BAND_ROTATION_MASK, GRAPHICS_ROTATION_COMPOSE);
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        levelIndex          = (Gp_StateC08.attachId % 10U) - 1;
        work->index         = levelIndex;
        work->scale         = D_lifedrain_80130AB4[levelIndex].brightness;
        work->angle         = LIFEDRAIN_BAND_INITIAL_RADIUS;
        work->period        = D_lifedrain_80130AB4[work->index].outerOffset;
        task->state         = LIFEDRAIN_BAND_STATE_EXPAND;
    }

    actorRenderComposeCoord(coord);
    // angle and period hold the inner radius and width; scale is the fading blue channel.
    work->angle  = work->angle + (D_lifedrain_80130AB4[work->index].brightness / 3);
    work->period = work->period + (D_lifedrain_80130AB4[work->index].brightness >> 1);
    rgb[0]       = work->scale >> 1;
    rgb[1]       = work->scale >> 1;
    rgb[2]       = (u8)work->scale;
    effectDrawInnerGlowBand(coord, work->angle, work->period, rgb);

    // Draw before fading, retaining the halfword wrap and signed release test.
    fadedBrightness  = (u16)work->scale;
    fadedBrightness -= LIFEDRAIN_BAND_FADE_STEP;
    work->scale      = fadedBrightness;
    if ((s16)fadedBrightness < LIFEDRAIN_BAND_RELEASE_BRIGHTNESS) {
        effectKillTask(work, task);
    }
}
