#include "healing.h"

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
#include <pe/effects/rising_spark/rising_spark.h>

PE2_EDITOR_LABEL("Aura")
/// How far above the player's root the aura sits, and its radius as it starts.
PE2_OPTION_DEF(AURA_HEIGHT, int, 0x400)
PE2_OPTION_DEF(START_RADIUS, int, 0x80)
/// Brightness gained each frame up to the level's, and lost each frame as the aura fades.
PE2_OPTION_DEF(BRIGHTNESS_STEP, int, 16)
/// Frames the aura holds, spinning, after it has faded.
PE2_OPTION_DEF(HOLD_FRAMES, int, 31)
/// The aura turns each frame by this many times its level's radius step.
PE2_OPTION_DEF(SPIN, int, 2)
/// How far out it sprays a sparkle each frame, in halves of its radius.
PE2_OPTION_DEF(SPARKLE_RADIUS_HALVES, int, 3)

PE2_EDITOR_LABEL("Glow")
/// How much dimmer red and green are than blue, as shifts.
PE2_OPTION_DEF(RED_SHIFT, int, 2)
PE2_OPTION_DEF(GREEN_SHIFT, int, 1)
/// The central disc's radius, as the aura's shifted down.
PE2_OPTION_DEF(DISC_RADIUS_SHIFT, int, 1)
/// The glow bands' width, how much dimmer each is than what it surrounds, as a shift, and how far out from the aura's
/// radius the outer band (from level 2) sits.
PE2_OPTION_DEF(BAND_WIDTH, int, 128)
PE2_OPTION_DEF(BAND_DIM_SHIFT, int, 1)
PE2_OPTION_DEF(OUTER_BAND_OFFSET, int, 512)

PE2_EDITOR_LABEL("Sparkles")
/// How fast a sparkle sinks, the frame it goes on, and the frame it starts fading from, losing its brightness shifted
/// down by the fade shift every other frame.
PE2_OPTION_DEF(SPARKLE_SINK_SPEED, int, 4)
PE2_OPTION_DEF(SPARKLE_FRAMES, int, 30)
PE2_OPTION_DEF(SPARKLE_FADE_FRAME, int, 16)
PE2_OPTION_DEF(SPARKLE_FADE_SHIFT, int, 4)
/// Every this many frames a sparkle sheds a rising spark. A power of two.
PE2_OPTION_DEF(SPARK_INTERVAL, int, 8)
/// The level from which sparkles are drawn as a core in a glow, the glow's size and brightness as the core's shifted
/// down.
PE2_OPTION_DEF(GLOW_LEVEL, int, 3)
PE2_OPTION_DEF(GLOW_SIZE_SHIFT, int, 1)
PE2_OPTION_DEF(GLOW_DIM_SHIFT, int, 1)
PE2_EDITOR_LABEL("")

STATIC_ASSERT(POWER_OF_TWO(PE2_OPTION(SPARK_INTERVAL)) && PE2_OPTION(SPARK_INTERVAL) > 1, spark_interval_is_a_mask);
STATIC_ASSERT(PE2_OPTION(BRIGHTNESS_STEP) > 0, the_aura_fades);

/// The aura's tuning at one Parasite Energy level, by the level digit of the attachment id less one. Brightness is the
/// blue channel's.
typedef struct {
    s16 unused;      // 8, 12 and 16; nothing reads it
    s16 brightness;  // Brightness the aura rises to, and its sparkles start at
    s16 radiusStep;  // Radius gained each frame, growing or fading; also sets the aura's spin
    s16 radiusLimit; // Radius that ends the growth; also each sparkle's and spark's size
} _LevelTuning;

/// Levels 1 to 3.
static _LevelTuning _gLevelTuning[] = {
    { 0x0008, 0x0080, 0x0040, 0x0400 },
    { 0x000C, 0x00B0, 0x0048, 0x0500 },
    { 0x0010, 0x00E0, 0x0050, 0x0600 },
};

/// The cue each level's cast plays.
static s32 _gCastSoundScripts[ARRAY_SIZE(_gLevelTuning)] = { 0xE0200001, 0xE0230001, 0xE0260001 };

static void _drawSparkle(const GfxCoord* coord, s16 frame, s16 sizeFactor, s16 brightness);

/// Draws the aura: a disc, drawn twice to brighten it, inside a glow band that flickers, and from level 2 a fainter one.
static inline void _drawGlow(const GfxCoord* coord, const EffectWork* work)
{
    u8 rgb[3];

    rgb[0] = work->scale >> PE2_OPTION(RED_SHIFT);
    rgb[1] = work->scale >> PE2_OPTION(GREEN_SHIFT);
    rgb[2] = work->scale;
    // Draw the same additive disc twice to brighten the centre.
    effectDrawGouraudDisc(coord, work->angle >> PE2_OPTION(DISC_RADIUS_SHIFT), rgb);
    effectDrawGouraudDisc(coord, work->angle >> PE2_OPTION(DISC_RADIUS_SHIFT), rgb);
    rgb[0] >>= PE2_OPTION(BAND_DIM_SHIFT);
    rgb[1] >>= PE2_OPTION(BAND_DIM_SHIFT);
    rgb[2] >>= PE2_OPTION(BAND_DIM_SHIFT);
    effectDrawOuterGlowBand(coord, work->angle, PE2_OPTION(BAND_WIDTH), rgb);
    if (work->age & 1) {
        effectDrawOuterGlowBand(coord, PE2_OPTION(BAND_WIDTH), work->angle, rgb);
    }
    if (work->index != 0) {
        rgb[0] >>= PE2_OPTION(BAND_DIM_SHIFT);
        rgb[1] >>= PE2_OPTION(BAND_DIM_SHIFT);
        rgb[2] >>= PE2_OPTION(BAND_DIM_SHIFT);
        effectDrawOuterGlowBand(coord, (s16)(work->angle + PE2_OPTION(OUTER_BAND_OFFSET)), PE2_OPTION(BAND_WIDTH), rgb);
    }
}

void healingAuraTask(Task* task)
{
    enum {
        AURA_STATE_INITIALIZE   = 0,
        AURA_STATE_GROWING      = 1,
        AURA_STATE_FADING       = 2,
        AURA_STATE_HOLDING      = 3,
        AURA_ANGLE_MASK         = 0xFFF,
        AURA_TRIG_FRACTION_BITS = 12,
    };
    EffectWork*      work;
    GfxCoord*        coord;
    AttachmentState* attachment;
    EffectWork*      spawned;
    s32              pan;
    s32              brightness;
    s16              spawnAngle;
    s32              randomState;
    s32              verticalProduct;

    attachment = &gAttachmentState;
    work       = task->spawnArg2.pointer;
    coord      = task->extra.coordBody->coord;
    if ((attachment->effectPhase == ATTACHMENT_EFFECT_HELD) || (gRoomEffectState->peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN)) {
        if (task->state == AURA_STATE_INITIALIZE) {
            attachment->flags |= ATTACHMENT_FLAG_APPLY_STATS;
        }
        effectKillTask(work, task);
        return;
    }

    work->age = work->age + 1;
    switch (task->state) {
        case AURA_STATE_INITIALIZE:
            coord->parent = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
            gfxSetRotIdentity(&coord->coord);
            coord->coord.t[0]   = 0;
            coord->coord.t[1]   = -PE2_OPTION(AURA_HEIGHT);
            coord->coord.t[2]   = 0;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            task->state        = AURA_STATE_GROWING;
            work->index        = (gAttachmentState.attachId % 10) - 1;
            work->angle        = PE2_OPTION(START_RADIUS);
            attachment->flags |= ATTACHMENT_FLAG_APPLY_STATS;
            pan                = (s8)worldCoordGetOriginAudioPan(coord);
            sndEvtRequestScriptStart(_gCastSoundScripts[work->index], pan,
                                     (s8)worldCoordGetOriginAudioDepth(coord));
            /* fallthrough */
        case AURA_STATE_GROWING:
            brightness = work->scale;
            if (brightness < _gLevelTuning[work->index].brightness) {
                brightness += PE2_OPTION(BRIGHTNESS_STEP);
            }
            work->scale = brightness;
            work->angle = work->angle + _gLevelTuning[work->index].radiusStep;
            gfxRotMatrixY(&coord->coord, -(_gLevelTuning[work->index].radiusStep * PE2_OPTION(SPIN)), 0);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            // Scatter the next child in the spinning aura's local frame.
            randomState     = RANDOM_LCG_NEXT(gRandomLcgState);
            spawnAngle      = ((u32)randomState >> 16) & AURA_ANGLE_MASK;
            gRandomLcgState = randomState;
            work->step      = spawnAngle;
            work->move.vx   = (rcos(spawnAngle) * (work->angle * PE2_OPTION(SPARKLE_RADIUS_HALVES) / 2)) >> AURA_TRIG_FRACTION_BITS;
            verticalProduct = rsin(work->step) * (work->angle * PE2_OPTION(SPARKLE_RADIUS_HALVES) / 2);
            randomState     = RANDOM_LCG_NEXT(gRandomLcgState);
            gRandomLcgState = randomState;
            work->move.vy   = verticalProduct >> AURA_TRIG_FRACTION_BITS;
            work->move.vz   = (rsin(((u32)randomState >> 16) & AURA_ANGLE_MASK) * work->move.vx) >> AURA_TRIG_FRACTION_BITS;
            spawned         = effectSpawn(EFFECT_HEALING_SPARKLE, coord, (s32)_gLevelTuning[work->index].radiusLimit,
                                          &work->move);
            if (spawned != NULL) {
                taskReparent(task, spawned->task);
            }
            if (work->angle >= _gLevelTuning[work->index].radiusLimit) {
                task->state = AURA_STATE_FADING;
            }
            _drawGlow(coord, work);
            return;
        case AURA_STATE_FADING:
            gfxRotMatrixY(&coord->coord, -(_gLevelTuning[work->index].radiusStep * PE2_OPTION(SPIN)), 0);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            work->scale = work->scale - PE2_OPTION(BRIGHTNESS_STEP);
            work->angle = work->angle + _gLevelTuning[work->index].radiusStep;
            if (work->scale < (PE2_OPTION(BRIGHTNESS_STEP) + 1)) {
                task->state = AURA_STATE_HOLDING;
            }
            _drawGlow(coord, work);
            return;
        case AURA_STATE_HOLDING:
            gfxRotMatrixY(&coord->coord, -(_gLevelTuning[work->index].radiusStep * PE2_OPTION(SPIN)), 0);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            work->period = work->period + 1;
            if (work->period < PE2_OPTION(HOLD_FRAMES)) {
                return;
            }
            effectKillTask(work, task);
            return;
        default:
            return;
    }
}

#include <pe/effects/rising_spark/task.inc.c>

void healingRisingSparkTask(Task* task)
{
    _risingSparkTask(task);
}

/// Moves a sparkle by `yStep` and composes it.
static inline void _advanceSparkleCoord(GfxCoord* coord, s16 yStep)
{
    s32 nextY;

    nextY               = coord->coord.t[1] + yStep;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    coord->coord.t[1]   = nextY;
    actorRenderComposeCoord(coord);
}

void healingSparkleTask(Task* task)
{
    enum {
        SPARKLE_STATE_INITIALIZE = 0,
        SPARKLE_STATE_RISING     = 1,
        SPARKLE_SIZE_MASK        = 0xFFF,
    };
    EffectWork* work;
    GfxCoord*   coord;
    s16         levelIndex;
    EffectWork* spawned;

    work      = task->spawnArg2.pointer;
    coord     = task->extra.coordBody->coord;
    work->age = work->age + 1;
    if (task->state == SPARKLE_STATE_INITIALIZE) {
        coord->parent       = work->parent;
        coord->coord.t[0]   = work->pos.vx;
        coord->coord.t[1]   = work->pos.vy;
        coord->coord.t[2]   = work->pos.vz;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(coord);
        work->move.vy = PE2_OPTION(SPARKLE_SINK_SPEED);
        work->move.vx = 0;
        work->move.vz = 0;
        task->state   = SPARKLE_STATE_RISING;
        levelIndex    = (gAttachmentState.attachId % 10U) - 1;
        work->step    = levelIndex;
        work->scale   = _gLevelTuning[levelIndex].brightness;
        work->angle   = (u16)task->spawnArg1.value & SPARKLE_SIZE_MASK;
    }
    // Initialization moves too; fading and child emission occur only on draw ticks.
    _advanceSparkleCoord(coord, work->move.vy);
    if (work->age < PE2_OPTION(SPARKLE_FRAMES)) {
        if (work->age & 1) {
            work->index = work->index + 1;
            if (work->age >= PE2_OPTION(SPARKLE_FADE_FRAME)) {
                work->scale = work->scale - (_gLevelTuning[work->step].brightness >> PE2_OPTION(SPARKLE_FADE_SHIFT));
            }
            if (work->step < (PE2_OPTION(GLOW_LEVEL) - 1)) {
                effectDrawModulatedBillboard(coord, work->index, work->angle,
                                             work->scale);
            } else {
                _drawSparkle(coord, work->index, work->angle, work->scale);
            }
            if ((work->age & (PE2_OPTION(SPARK_INTERVAL) - 1)) == 1) {
                spawned = effectSpawn(EFFECT_HEALING_SPARK, coord, (s32)(work->angle), 0);
                if (spawned != NULL) {
                    taskReparent(task, spawned->task);
                }
            }
        }
    } else {
        effectKillTask(work, task);
    }
}

/// Sets a square quad's corners around its projected center, `screenExtent` pixels out.
static inline void _setSparkleScreenBounds(POLY_FT4* quad, const EffectCentreScratch* scratch)
{
    quad->x0 = quad->x2 = scratch->screenX - scratch->screenExtent;
    quad->x1 = quad->x3 = scratch->screenX + scratch->screenExtent;
    quad->y0 = quad->y1 = scratch->screenY - scratch->screenExtent;
    quad->y2 = quad->y3 = scratch->screenY + scratch->screenExtent;
}

/// Draws a glowing sparkle: a core cycling four cells, in a glow alternating two palettes, both additive and sized by
/// `sizeFactor` in perspective; a point failing to project draws nothing.
static void _drawSparkle(const GfxCoord* coord, s16 frame, s16 sizeFactor, s16 brightness)
{
    enum {
        SPARKLE_FRAME_COUNT         = 4,
        SPARKLE_CELL_SIZE           = 24,
        SPARKLE_UV_SPAN             = SPARKLE_CELL_SIZE - 1,
        SPARKLE_TEXTURE_DEPTH_4BIT  = 0,
        SPARKLE_TEXTURE_PAGE        = getTPage(SPARKLE_TEXTURE_DEPTH_4BIT, GPU_BLEND_ADD, 640, 0),
        SPARKLE_CLUT                = getClut(80, 267),
        SPARKLE_GLOW_TEXTURE_PAGE   = getTPage(SPARKLE_TEXTURE_DEPTH_4BIT, GPU_BLEND_ADD, 576, 0),
        SPARKLE_GLOW_PALETTE_COUNT  = 2,
        SPARKLE_GLOW_CLUT_X         = 256,
        SPARKLE_GLOW_CLUT_Y         = 268,
        SPARKLE_GLOW_PALETTE_X_STEP = 16,
        SPARKLE_GLOW_LEFT_U         = 56,
        SPARKLE_GLOW_TOP_V          = 200,
        SPARKLE_GLOW_UV_SPAN        = 55,
        SPARKLE_DEPTH_BIAS          = 1,
    };
    EffectCentreScratch* scratch;
    POLY_FT4*            quad;
    s32                  leftU;
    s32                  rightU;

    // Project one composed centre for both screen-aligned layers.
    scratch                = SCRATCH_STACK_RESERVE_BLOCK(EffectCentreScratch);
    scratch->worldPoint.vx = coord->workm.t[0];
    scratch->worldPoint.vy = coord->workm.t[1];
    scratch->worldPoint.vz = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&scratch->worldPoint);
    gte_rtps();
    gte_stsxy(&scratch->screenX);
    gte_stflg(&scratch->projectionFlags);
    if (scratch->projectionFlags >= 0) {
        gte_stszotz(&scratch->depth);
        // Bias the shared depth so neither perspective divisor can be zero.
        scratch->depth += SPARKLE_DEPTH_BIAS;
        quad            = gGpuPrimCursor;
        gGpuPrimCursor  = quad + 1;
        setPolyFT4(quad);
        setSemiTrans(quad, true);
        quad->tpage = SPARKLE_TEXTURE_PAGE;
        quad->clut  = SPARKLE_CLUT;
        leftU       = (frame & (SPARKLE_FRAME_COUNT - 1)) * SPARKLE_CELL_SIZE;
        rightU      = leftU + SPARKLE_UV_SPAN;
        quad->u0    = leftU;
        quad->u1    = rightU;
        quad->u2    = leftU;
        quad->u3    = rightU;
        quad->v2    = SPARKLE_UV_SPAN;
        quad->v3    = SPARKLE_UV_SPAN;
        setRGB0(quad, brightness, brightness, brightness);
        quad->v0              = 0;
        quad->v1              = 0;
        scratch->screenExtent = (sizeFactor * SPARKLE_UV_SPAN) / scratch->depth;
        _setSparkleScreenBounds(quad, scratch);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratch->depth),
                quad);

        // The glow shares the centre and depth, but halves size and modulation.
        quad           = gGpuPrimCursor;
        gGpuPrimCursor = quad + 1;
        quad->tpage    = SPARKLE_GLOW_TEXTURE_PAGE;
        quad->clut     = getClut((frame & (SPARKLE_GLOW_PALETTE_COUNT - 1)) * SPARKLE_GLOW_PALETTE_X_STEP + SPARKLE_GLOW_CLUT_X, SPARKLE_GLOW_CLUT_Y);
        setPolyFT4(quad);
        setSemiTrans(quad, true);
        brightness = brightness >> PE2_OPTION(GLOW_DIM_SHIFT);
        setRGB0(quad, brightness, brightness, brightness);
        setUV4(quad, SPARKLE_GLOW_LEFT_U, SPARKLE_GLOW_TOP_V,
               SPARKLE_GLOW_LEFT_U + SPARKLE_GLOW_UV_SPAN, SPARKLE_GLOW_TOP_V,
               SPARKLE_GLOW_LEFT_U, SPARKLE_GLOW_TOP_V + SPARKLE_GLOW_UV_SPAN,
               SPARKLE_GLOW_LEFT_U + SPARKLE_GLOW_UV_SPAN, SPARKLE_GLOW_TOP_V + SPARKLE_GLOW_UV_SPAN);
        scratch->screenExtent = ((sizeFactor >> PE2_OPTION(GLOW_SIZE_SHIFT)) * SPARKLE_GLOW_UV_SPAN) / scratch->depth;
        _setSparkleScreenBounds(quad, scratch);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratch->depth),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectCentreScratch);
}
