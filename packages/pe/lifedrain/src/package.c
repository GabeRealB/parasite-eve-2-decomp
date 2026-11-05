#include "lifedrain.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include <core/pe2.h>
#include <math/gte.h>

#include <gameplay/display.h>
#include <gameplay/actor_render.h>
#include <gameplay/attachment_state.h>
#include <gameplay/attachments.h>
#include <gameplay/effects.h>
#include <gameplay/room_effects.h>
#include <gameplay/scene.h>
#include <gameplay/scene_combat.h>
#include <gameplay/world_coords.h>

#include <engine/coord.h>
#include <engine/display.h>
#include <engine/display_types.h>
#include <engine/random.h>
#include <engine/gfx.h>
#include <engine/gfx_types.h>
#include <engine/scratch.h>
#include <engine/session.h>
#include <engine/sound.h>
#include <engine/task.h>
#include <engine/task_types.h>
#include <engine/tmd_types.h>
#include <engine/wipsys.h>
#include <engine/wipsys_types.h>
#include <effects/glow/glow.h>
#include <pe/effects/rising_spark/rising_spark.h>

PE2_EDITOR_LABEL("Cast")
/// The frame the cue plays on, and the frame the drained health is given on, after which the funnel shows if any was.
PE2_OPTION_DEF(CUE_FRAME, int, 3)
PE2_OPTION_DEF(HEAL_FRAME, int, 30)
/// Brightness lost each frame of the opening flash, gained each frame of the funnel up to the level's, and lost each
/// frame as it fades.
PE2_OPTION_DEF(BRIGHTNESS_STEP, int, 16)
/// How much dimmer red and green are than blue, in the flash, the funnel and the rings, as a shift.
PE2_OPTION_DEF(RED_GREEN_SHIFT, int, 1)

PE2_EDITOR_LABEL("Funnel")
/// The funnel's radius as it starts.
PE2_OPTION_DEF(START_RADIUS, int, 0x80)
/// How far apart its wedges are, and the range of the random yaw added to each, a power of two, in a 4096-unit turn.
PE2_OPTION_DEF(WEDGE_SPACING, int, 1024)
PE2_OPTION_DEF(WEDGE_JITTER, int, 1024)
/// The central disc's radius, as the funnel's shifted down.
PE2_OPTION_DEF(DISC_RADIUS_SHIFT, int, 1)
/// The glow bands' width, and how much dimmer each is than what it surrounds, as a shift.
PE2_OPTION_DEF(GLOW_BAND_WIDTH, int, 128)
PE2_OPTION_DEF(GLOW_BAND_DIM_SHIFT, int, 1)

PE2_EDITOR_LABEL("Motes")
/// The range of a mote's drift on each horizontal axis, a power of two, and how fast it rises, plus a random part under
/// the jitter, a power of two.
PE2_OPTION_DEF(MOTE_DRIFT, int, 128)
PE2_OPTION_DEF(MOTE_RISE_SPEED, int, 32)
PE2_OPTION_DEF(MOTE_RISE_JITTER, int, 64)
/// The frame a mote turns for the player, and the frame it reaches the player and goes on.
PE2_OPTION_DEF(MOTE_AIM_FRAME, int, 15)
PE2_OPTION_DEF(MOTE_FRAMES, int, 30)
/// How strongly a mote homes, in 4096ths of its way to the player over the frames left, and the speed it turns by on
/// each axis every frame.
PE2_OPTION_DEF(MOTE_HOMING, int, 0x1200)
PE2_OPTION_DEF(MOTE_STEERING, int, 16)
/// How much smaller a mote is than its sparks.
PE2_OPTION_DEF(MOTE_SIZE_SHORTFALL, int, 0x100)
/// One drawn frame in this many, a mote sheds a spark. A power of two.
PE2_OPTION_DEF(MOTE_SPARK_ODDS, int, 4)

PE2_EDITOR_LABEL("Rings")
/// A ring's radius as it starts, and the brightness it loses each frame.
PE2_OPTION_DEF(RING_RADIUS, int, 128)
PE2_OPTION_DEF(RING_FADE_STEP, int, 8)
PE2_EDITOR_LABEL("")

STATIC_ASSERT(POWER_OF_TWO(PE2_OPTION(WEDGE_JITTER)) && PE2_OPTION(WEDGE_JITTER) <= 4096, wedge_jitter_is_a_mask);
STATIC_ASSERT(POWER_OF_TWO(PE2_OPTION(MOTE_DRIFT)) && POWER_OF_TWO(PE2_OPTION(MOTE_RISE_JITTER)) &&
                  POWER_OF_TWO(PE2_OPTION(MOTE_SPARK_ODDS)),
              mote_masks);
STATIC_ASSERT(PE2_OPTION(CUE_FRAME) < PE2_OPTION(HEAL_FRAME), the_cue_plays_before_healing);
STATIC_ASSERT(PE2_OPTION(MOTE_AIM_FRAME) < PE2_OPTION(MOTE_FRAMES), motes_turn_before_they_go);
STATIC_ASSERT(PE2_OPTION(BRIGHTNESS_STEP) > 0 && PE2_OPTION(RING_FADE_STEP) > 0, fades);

/// The cast's tuning at one Parasite Energy level, by the level digit of the attachment id less one. Brightness is the
/// blue channel's.
typedef struct {
    s16 wedgeCount;  // Glow wedges fanned around the funnel
    s16 brightness;  // The flash's and the funnel's; a ring starts at it, growing by a third of it and widening by half
    s16 outerOffset; // A ring's first width; from level 2, how far out the funnel's dimmer band is
    s16 radiusLimit; // Radius that ends the funnel's growth; also its sparks' size (and a mote's, read at level 1)
    s16 radiusStep;  // Radius gained each frame, growing or fading
} _LevelTuning;
PE2_TYPE(_LevelTuning)

static void _drawMoteBillboards(const GfxCoord* coord, s16 animationFrame, s16 sizeFactor);

PE2_EDITOR_LABEL("Levels")
/// The cast at levels 1 to 3.
PE2_OPTION_DEF(LEVELS, _LevelTuning[3], {
    { 8, 0x0080, 0x0100, 0x0400, 0x0040 },
    { 12, 0x00B0, 0x0200, 0x0500, 0x0048 },
    { 16, 0x00E0, 0x0300, 0x0600, 0x0050 },
})
PE2_EDITOR_LABEL("")

/// The most any level has, which its table holds.
enum { MAX_CAST_WEDGES = LARGER(LARGER(PE2_OPTION_AT(LEVELS, 0, wedgeCount), PE2_OPTION_AT(LEVELS, 1, wedgeCount)), PE2_OPTION_AT(LEVELS, 2, wedgeCount)) };

static _LevelTuning _gLevelTuning[] = PE2_OPTION(LEVELS);

enum {
    EMPTY_SOUND_START     = 0,
    COLLECTED_SOUND_START = ARRAY_SIZE(_gLevelTuning),
    SOUND_SELECTOR_COUNT  = 2 * ARRAY_SIZE(_gLevelTuning),
};

/// Each level's cue when nothing was drained, then each level's when something was.
static s32 _gCastSoundScripts[SOUND_SELECTOR_COUNT] = {
    0xE0210001,
    0xE0240001,
    0xE0270001,
    0xE0210002,
    0xE0240002,
    0xE0270002,
};

/// The wedges' yaws, with a little jitter, chosen when a cast starts.
static s16 _gWedgeYaws[MAX_CAST_WEDGES] = { 0 };

/// The cast running, which its motes add their health to and are freed with.
static Task* _gCollectorTask = NULL;

/// Draws the funnel: its wedges, a disc drawn twice to brighten it, a glow band that flickers, and from level 2 a
/// fainter one further out, flickering too at level 3. Dims `rgb` as it goes.
#define DRAW_CAST_FUNNEL(coord, work, rgb, wedgeIndex)                                                                \
    {                                                                                                                 \
        enum { CAST_LEVEL_THREE_INDEX = 2 };                                                                          \
        const _LevelTuning* tuning;                                                                                   \
        const s16*          wedgeYaw;                                                                                 \
        (wedgeIndex) = 0;                                                                                             \
        if (_gLevelTuning[(work)->index].wedgeCount > 0) {                                                            \
            tuning   = _gLevelTuning;                                                                                 \
            wedgeYaw = _gWedgeYaws;                                                                                   \
            do {                                                                                                      \
                glowDrawWedge((coord), (work)->angle, *wedgeYaw, (rgb));                                              \
                wedgeYaw += 1;                                                                                        \
            } while (++(wedgeIndex) < tuning[(work)->index].wedgeCount);                                              \
        }                                                                                                             \
        effectDrawGouraudDisc((coord), (work)->angle >> PE2_OPTION(DISC_RADIUS_SHIFT), (rgb));                        \
        effectDrawGouraudDisc((coord), (work)->angle >> PE2_OPTION(DISC_RADIUS_SHIFT), (rgb));                        \
        (rgb)[0] >>= PE2_OPTION(GLOW_BAND_DIM_SHIFT);                                                                 \
        (rgb)[1] >>= PE2_OPTION(GLOW_BAND_DIM_SHIFT);                                                                 \
        (rgb)[2] >>= PE2_OPTION(GLOW_BAND_DIM_SHIFT);                                                                 \
        effectDrawOuterGlowBand((coord), (work)->angle, PE2_OPTION(GLOW_BAND_WIDTH), (rgb));                          \
        if ((work)->age & 1) {                                                                                        \
            effectDrawOuterGlowBand((coord), PE2_OPTION(GLOW_BAND_WIDTH), (work)->angle, (rgb));                      \
        }                                                                                                             \
        if ((work)->index != 0) {                                                                                     \
            (rgb)[0] >>= PE2_OPTION(GLOW_BAND_DIM_SHIFT);                                                             \
            (rgb)[1] >>= PE2_OPTION(GLOW_BAND_DIM_SHIFT);                                                             \
            (rgb)[2] >>= PE2_OPTION(GLOW_BAND_DIM_SHIFT);                                                             \
            effectDrawOuterGlowBand((coord),                                                                          \
                                    (s16)((work)->angle + _gLevelTuning[(work)->index].outerOffset),                  \
                                    PE2_OPTION(GLOW_BAND_WIDTH), (rgb));                                              \
            if ((work)->index == CAST_LEVEL_THREE_INDEX) {                                                            \
                if ((work)->age & 1) {                                                                                \
                    effectDrawOuterGlowBand((coord), PE2_OPTION(GLOW_BAND_WIDTH),                                     \
                                            (s16)((work)->angle + _gLevelTuning[CAST_LEVEL_THREE_INDEX].outerOffset), \
                                            (rgb));                                                                   \
                }                                                                                                     \
            }                                                                                                         \
        }                                                                                                             \
    }

void lifedrainCastTask(Task* task)
{
    enum {
        CAST_STATE_INITIALIZE   = 0,
        CAST_STATE_COLLECTING   = 1,
        CAST_STATE_GROWING      = 2,
        CAST_STATE_FADING       = 3,
        CAST_STATE_RELEASE      = 4,
        CAST_BAND_YAW_STEP      = 0x2AA,
        CAST_BAND_YAW_END       = 0x556,
        CAST_ANGLE_MASK         = 0xFFF,
        CAST_TRIG_FRACTION_BITS = 12,
    };
    EffectWork* work;
    GfxCoord*   coord;
    s32         phaseIndex;
    u8          rgb[3];

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if ((gAttachmentState.effectPhase == ATTACHMENT_EFFECT_HELD) || (gRoomEffectState->peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN)) {
        // Collected motes count immediately; cancellation can still grant their banked HP.
        if ((task->state < CAST_STATE_GROWING) && (task->spawnArg1.value != 0)) {
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
        case CAST_STATE_INITIALIZE: {
            EffectWork* spawned;
            s32         bandAngle;

            // Motes use this live collector as both their counter and teardown parent.
            _gCollectorTask = task;
            coord->parent   = work->parent;
            gfxSetRotIdentity(&coord->coord);
            coord->coord.t[0]   = 0;
            coord->coord.t[1]   = 0;
            coord->coord.t[2]   = 0;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            task->state  = CAST_STATE_COLLECTING;
            work->index  = (gAttachmentState.attachId % 10) - 1;
            work->scale  = _gLevelTuning[work->index].brightness;
            work->angle  = PE2_OPTION(START_RADIUS);
            work->period = _gLevelTuning[work->index].brightness;
            phaseIndex   = 0;
            if (_gLevelTuning[work->index].wedgeCount > 0) {
                do {
                    s32 yawRng;

                    yawRng                  = RANDOM_LCG_NEXT(gRandomLcgState);
                    _gWedgeYaws[phaseIndex] = (phaseIndex * PE2_OPTION(WEDGE_SPACING)) + (((u32)yawRng >> 16) & (PE2_OPTION(WEDGE_JITTER) - 1));
                    gRandomLcgState         = yawRng;
                } while (++phaseIndex < _gLevelTuning[work->index].wedgeCount);
            }
            bandAngle = 0;
            do {
                spawned = effectSpawn(EFFECT_LIFEDRAIN_RING, coord, bandAngle, NULL);
                if (spawned != NULL) {
                    taskReparent(task, spawned->task);
                }
                bandAngle += CAST_BAND_YAW_STEP;
            } while (bandAngle < CAST_BAND_YAW_END);
            gAttachmentState.flags |= ATTACHMENT_FLAG_APPLY_STATS;
            return;
        }
        case CAST_STATE_COLLECTING:
            if (work->scale != 0) {
                work->scale = work->scale - PE2_OPTION(BRIGHTNESS_STEP);
                rgb[0]      = work->scale >> PE2_OPTION(RED_GREEN_SHIFT);
                rgb[1]      = work->scale >> PE2_OPTION(RED_GREEN_SHIFT);
                rgb[2]      = (u8)work->scale;
                effectDrawScreenTint(rgb, GPU_BLEND_ADD);
            }
            if (work->age == PE2_OPTION(HEAL_FRAME)) {
                if (task->spawnArg1.value != 0) {
                    gPlayerStatus.hp = (u16)gPlayerStatus.hp + gSceneCombatState.lifeDrainHp;
                    if (gPlayerStatus.hp > gPlayerStatus.hpMax) {
                        gPlayerStatus.hp = gPlayerStatus.hpMax;
                    }
                    task->state = CAST_STATE_GROWING;
                } else {
                    task->state = CAST_STATE_RELEASE;
                }
                return;
            }
            if (work->age != PE2_OPTION(CUE_FRAME)) {
                return;
            }
            actorRenderComposeCoord(coord);
            if (task->spawnArg1.value != 0) {
                sndEvtRequestScriptStart(_gCastSoundScripts[work->index + COLLECTED_SOUND_START],
                                         (s8)worldCoordGetOriginAudioPan(coord),
                                         (s8)worldCoordGetOriginAudioDepth(coord));
            } else {
                sndEvtRequestScriptStart(_gCastSoundScripts[work->index + EMPTY_SOUND_START],
                                         (s8)worldCoordGetOriginAudioPan(coord),
                                         (s8)worldCoordGetOriginAudioDepth(coord));
            }
            return;
        case CAST_STATE_GROWING: {
            EffectWork* spawned;
            s32         brightness;

            actorRenderComposeCoord(coord);
            if (work->period != 0) {
                work->period = work->period - PE2_OPTION(BRIGHTNESS_STEP);
                rgb[0]       = work->period >> PE2_OPTION(RED_GREEN_SHIFT);
                rgb[1]       = work->period >> PE2_OPTION(RED_GREEN_SHIFT);
                rgb[2]       = (u8)work->period;
                effectDrawScreenTint(rgb, GPU_BLEND_ADD);
            }
            brightness = work->scale;
            if (brightness < _gLevelTuning[work->index].brightness) {
                brightness += PE2_OPTION(BRIGHTNESS_STEP);
            }
            work->scale = brightness;
            work->angle = work->angle + _gLevelTuning[work->index].radiusStep;
            rgb[0]      = work->scale >> PE2_OPTION(RED_GREEN_SHIFT);
            rgb[1]      = work->scale >> PE2_OPTION(RED_GREEN_SHIFT);
            rgb[2]      = (u8)work->scale;
            DRAW_CAST_FUNNEL(coord, work, rgb, phaseIndex);
            RANDOM_LCG_ADVANCE();
            work->step = RANDOM_LCG_DRAW() & CAST_ANGLE_MASK;
            RANDOM_LCG_ADVANCE();
            gfxRotMatrixY(&coord->coord, RANDOM_LCG_DRAW() & CAST_ANGLE_MASK, 0);
            gte_SetRotMatrix(&coord->coord);
            gte_ldv0(&work->move);
            gte_rtv0();
            gte_stsv(&work->move);
            work->move.vx = (rcos(work->step) * work->angle) >> CAST_TRIG_FRACTION_BITS;
            work->move.vy = (rsin(work->step) * work->angle) >> CAST_TRIG_FRACTION_BITS;
            work->move.vz = 0;
            spawned       = effectSpawn(EFFECT_LIFEDRAIN_SPARK, coord, (s32)_gLevelTuning[work->index].radiusLimit,
                                        &work->move);
            if (spawned != NULL) {
                taskReparent(task, spawned->task);
            }
            if (work->angle >= _gLevelTuning[work->index].radiusLimit) {
                task->state = CAST_STATE_FADING;
            }
            return;
        }
        case CAST_STATE_FADING: {

            actorRenderComposeCoord(coord);
            work->scale = work->scale - PE2_OPTION(BRIGHTNESS_STEP);
            work->angle = work->angle + _gLevelTuning[work->index].radiusStep;
            if (work->scale < (PE2_OPTION(BRIGHTNESS_STEP) + 1)) {
                task->state = CAST_STATE_RELEASE;
            }
            rgb[0] = work->scale >> PE2_OPTION(RED_GREEN_SHIFT);
            rgb[1] = work->scale >> PE2_OPTION(RED_GREEN_SHIFT);
            rgb[2] = (u8)work->scale;
            DRAW_CAST_FUNNEL(coord, work, rgb, phaseIndex);
            return;
        }
        case CAST_STATE_RELEASE:
            effectKillTask(work, task);
            return;
    }
}

#undef DRAW_CAST_FUNNEL

#include <pe/effects/rising_spark/task.inc.c>

void lifedrainRisingSparkTask(Task* task)
{
    _risingSparkTask(task);
}

/// Aims a mote at the player's second model coordinate, to arrive as it goes (`pos`, a velocity in the mote's frame).
#define AIM_MOTE(moteCoord, work, playerCoord, playerOffset)                                                \
    do {                                                                                                    \
        enum { MOTE_PLAYER_PART = 1 };                                                                      \
        (playerCoord)     = &(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords[MOTE_PLAYER_PART]; \
        (playerOffset).vx = (playerCoord)->workm.t[0] - (moteCoord)->workm.t[0];                            \
        (playerOffset).vy = (playerCoord)->workm.t[1] - (moteCoord)->workm.t[1];                            \
        (playerOffset).vz = (playerCoord)->workm.t[2] - (moteCoord)->workm.t[2];                            \
        ApplyTransposeMatrixLV(&(moteCoord)->workm, &(playerOffset), &(playerOffset));                      \
        (work)->pos.vx = (playerOffset).vx;                                                                 \
        (work)->pos.vy = (playerOffset).vy;                                                                 \
        (work)->pos.vz = (playerOffset).vz;                                                                 \
        gte_SetRotMatrix(&(moteCoord)->coord);                                                              \
        gte_ldv0(&(work)->pos);                                                                             \
        gte_rtv0();                                                                                         \
        gte_stsv(&(work)->pos);                                                                             \
        gte_lddp(PE2_OPTION(MOTE_HOMING) / (PE2_OPTION(MOTE_FRAMES) - (work)->age));                        \
        gte_ldsv(&(work)->pos);                                                                             \
        gte_gpf12();                                                                                        \
        gte_stsv(&(work)->pos);                                                                             \
    } while (0)

void lifedrainMoteTask(Task* task)
{
    enum {
        MOTE_STATE_INITIALIZE = 0,
        MOTE_STATE_DRIFTING   = 1,
        MOTE_STATE_HOMING     = 2,
        MOTE_FRAME_MASK       = 3,
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
    if ((gAttachmentState.effectPhase != ATTACHMENT_EFFECT_HELD) && (gRoomEffectState->peEffectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN)) {
        work->age = work->age + 1;
        switch (task->state) {
            case MOTE_STATE_INITIALIZE:
                taskReparent(_gCollectorTask, task);
                _gCollectorTask->spawnArg1.value += task->spawnArg1.value;
                RANDOM_LCG_ADVANCE();
                work->move.vx = (PE2_OPTION(MOTE_DRIFT) / 2) - (RANDOM_LCG_DRAW() & (PE2_OPTION(MOTE_DRIFT) - 1));
                RANDOM_LCG_ADVANCE();
                work->move.vy = (u16)-PE2_OPTION(MOTE_RISE_SPEED) - (RANDOM_LCG_DRAW() & (PE2_OPTION(MOTE_RISE_JITTER) - 1));
                RANDOM_LCG_ADVANCE();
                work->move.vz = (PE2_OPTION(MOTE_DRIFT) / 2) - (RANDOM_LCG_DRAW() & (PE2_OPTION(MOTE_DRIFT) - 1));
                task->state   = MOTE_STATE_DRIFTING;
                // Keep the binary's sizing selector: step stays at its cleared zero value.
                work->scale  = (gAttachmentState.attachId % 10) - 1;
                sparkRadius  = _gLevelTuning[work->step].radiusLimit;
                work->angle  = sparkRadius;
                work->period = sparkRadius - PE2_OPTION(MOTE_SIZE_SHORTFALL);
                /* fallthrough */
            case MOTE_STATE_DRIFTING:
                coord->coord.t[0]  += work->move.vx;
                coord->coord.t[1]  += work->move.vy;
                coord->coord.t[2]  += work->move.vz;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(coord);
                if (work->age & 1) {
                    work->index = work->index + 1;
                    _drawMoteBillboards(coord, work->index, work->period);
                    RANDOM_LCG_ADVANCE();
                    if ((RANDOM_LCG_DRAW() & (PE2_OPTION(MOTE_SPARK_ODDS) - 1)) == 0) {
                        spawned = effectSpawn(EFFECT_LIFEDRAIN_SPARK, coord, (s32)(work->angle), NULL);
                        if (spawned != NULL) {
                            taskReparent(task, spawned->task);
                        }
                    }
                }
                if (work->age == PE2_OPTION(MOTE_AIM_FRAME)) {
                    AIM_MOTE(coord, work, playerCoord, playerOffset);
                    task->state = MOTE_STATE_HOMING;
                }
                return;
            case MOTE_STATE_HOMING:
                // Approach the scaled displacement component by component, without clamping.
                velocityComponent = work->move.vx;
                work->move.vx     = (velocityComponent < work->pos.vx) ? velocityComponent + PE2_OPTION(MOTE_STEERING) : velocityComponent - PE2_OPTION(MOTE_STEERING);
                velocityComponent = work->move.vy;
                work->move.vy     = (velocityComponent < work->pos.vy) ? velocityComponent + PE2_OPTION(MOTE_STEERING) : velocityComponent - PE2_OPTION(MOTE_STEERING);
                velocityComponent = work->move.vz;
                work->move.vz     = (velocityComponent < work->pos.vz) ? velocityComponent + PE2_OPTION(MOTE_STEERING) : velocityComponent - PE2_OPTION(MOTE_STEERING);

                coord->coord.t[0]  += work->move.vx;
                coord->coord.t[1]  += work->move.vy;
                coord->coord.t[2]  += work->move.vz;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(coord);
                if (work->age >= PE2_OPTION(MOTE_FRAMES)) {
                    break;
                }
                if (work->age & 1) {
                    work->index = (work->index + 1) & MOTE_FRAME_MASK;
                    _drawMoteBillboards(coord, work->index, work->period);
                    RANDOM_LCG_ADVANCE();
                    if ((RANDOM_LCG_DRAW() & (PE2_OPTION(MOTE_SPARK_ODDS) - 1)) == 0) {
                        spawned = effectSpawn(EFFECT_LIFEDRAIN_SPARK, coord, (s32)(work->angle), NULL);
                        if (spawned != NULL) {
                            taskReparent(task, spawned->task);
                        }
                    }
                }
                AIM_MOTE(coord, work, playerCoord, playerOffset);
                return;
            default:
                return;
        }
    }
    effectKillTask(work, task);
}

#undef AIM_MOTE

/// Sets a square quad's corners around its projected center, `screenExtent` pixels out.
static inline void _setMoteBillboardBounds(POLY_FT4* quad, const EffectCentreScratch* scratch)
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

/// Draws a mote: a core cycling four cells in a halo alternating two palettes, both additive and sized by `sizeFactor`
/// in perspective; a point failing to project draws nothing.
static void _drawMoteBillboards(const GfxCoord* coord, s16 animationFrame, s16 sizeFactor)
{
    enum {
        MOTE_CORE_FRAME_COUNT      = 4,
        MOTE_CORE_CELL_WIDTH       = 24,
        MOTE_CORE_UV_SPAN          = MOTE_CORE_CELL_WIDTH - 1,
        MOTE_CORE_TPAGE            = getTPage(0, GPU_BLEND_ADD, 640, 0),
        MOTE_CORE_CLUT             = getClut(80, 267),
        MOTE_HALO_TPAGE            = getTPage(0, GPU_BLEND_ADD, 576, 0),
        MOTE_HALO_PALETTE_COUNT    = 2,
        MOTE_HALO_PALETTE_X        = 256,
        MOTE_HALO_PALETTE_X_STRIDE = 16,
        MOTE_HALO_CLUT_ROW         = getClut(0, 268),
        MOTE_HALO_LEFT_U           = 56,
        MOTE_HALO_TOP_V            = 200,
        MOTE_HALO_UV_SPAN          = 55,
        MOTE_DEPTH_BIAS            = 1,
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
        scratch->depth += MOTE_DEPTH_BIAS;
        quad            = gGpuPrimCursor;
        gGpuPrimCursor  = quad + 1;
        quad->tpage     = MOTE_CORE_TPAGE;
        quad->clut      = MOTE_CORE_CLUT;
        setPolyFT4(quad);
        setSemiTrans(quad, 1);
        setShadeTex(quad, 1);
        leftU                 = (animationFrame & (MOTE_CORE_FRAME_COUNT - 1)) * MOTE_CORE_CELL_WIDTH;
        rightU                = leftU + MOTE_CORE_UV_SPAN;
        quad->u1              = rightU;
        quad->u0              = leftU;
        quad->u2              = leftU;
        quad->u3              = rightU;
        quad->v2              = MOTE_CORE_UV_SPAN;
        quad->v3              = MOTE_CORE_UV_SPAN;
        quad->v0              = 0;
        quad->v1              = 0;
        scratch->screenExtent = (sizeFactor * MOTE_CORE_UV_SPAN) / scratch->depth;
        _setMoteBillboardBounds(quad, scratch);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratch->depth),
                quad);

        // The halo alternates palettes while retaining a fixed texture cell.
        quad           = gGpuPrimCursor;
        gGpuPrimCursor = quad + 1;
        quad->tpage    = MOTE_HALO_TPAGE;
        quad->clut     = ((u32)(((animationFrame & (MOTE_HALO_PALETTE_COUNT - 1)) * MOTE_HALO_PALETTE_X_STRIDE) + MOTE_HALO_PALETTE_X) >> 4) | MOTE_HALO_CLUT_ROW;
        setPolyFT4(quad);
        setSemiTrans(quad, 1);
        setShadeTex(quad, 1);
        quad->u0              = MOTE_HALO_LEFT_U;
        quad->v0              = MOTE_HALO_TOP_V;
        quad->u1              = MOTE_HALO_LEFT_U + MOTE_HALO_UV_SPAN;
        quad->v1              = MOTE_HALO_TOP_V;
        quad->v2              = MOTE_HALO_TOP_V + MOTE_HALO_UV_SPAN;
        quad->v3              = MOTE_HALO_TOP_V + MOTE_HALO_UV_SPAN;
        quad->u2              = MOTE_HALO_LEFT_U;
        quad->u3              = MOTE_HALO_LEFT_U + MOTE_HALO_UV_SPAN;
        scratch->screenExtent = ((s16)((sizeFactor * 2) / 3) * MOTE_HALO_UV_SPAN) / scratch->depth;
        _setMoteBillboardBounds(quad, scratch);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratch->depth),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectCentreScratch);
}

#include <effects/glow/wedge.inc.c>

void lifedrainExpandingGlowBandTask(Task* task)
{
    enum {
        BAND_STATE_INITIALIZE = 0,
        BAND_STATE_EXPAND     = 1,
        BAND_ROTATION_MASK    = 0xFFF,
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
    if (task->state == BAND_STATE_INITIALIZE) {
        gfxRotMatrixZ(&coord->coord, task->spawnArg1.value & BAND_ROTATION_MASK, GRAPHICS_ROTATION_COMPOSE);
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        levelIndex          = (gAttachmentState.attachId % 10U) - 1;
        work->index         = levelIndex;
        work->scale         = _gLevelTuning[levelIndex].brightness;
        work->angle         = PE2_OPTION(RING_RADIUS);
        work->period        = _gLevelTuning[work->index].outerOffset;
        task->state         = BAND_STATE_EXPAND;
    }

    actorRenderComposeCoord(coord);
    // angle and period hold the inner radius and width; scale is the fading blue channel.
    work->angle  = work->angle + (_gLevelTuning[work->index].brightness / 3);
    work->period = work->period + (_gLevelTuning[work->index].brightness >> 1);
    rgb[0]       = work->scale >> PE2_OPTION(RED_GREEN_SHIFT);
    rgb[1]       = work->scale >> PE2_OPTION(RED_GREEN_SHIFT);
    rgb[2]       = (u8)work->scale;
    effectDrawInnerGlowBand(coord, work->angle, work->period, rgb);

    // Draw before fading, retaining the halfword wrap and signed release test.
    fadedBrightness  = (u16)work->scale;
    fadedBrightness -= PE2_OPTION(RING_FADE_STEP);
    work->scale      = fadedBrightness;
    if ((s16)fadedBrightness < (PE2_OPTION(RING_FADE_STEP) + 1)) {
        effectKillTask(work, task);
    }
}
