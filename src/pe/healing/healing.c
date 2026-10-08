#include "pe/healing.h"

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
#include "../../shared/rising_spark.h"

/// Visual tuning of the healing cast for one Parasite Energy level.
///
/// The cast is an aura around the caster: two rings and one or more glow arcs
/// that brighten and grow with one radius while the aura sprays a sparkle each
/// frame, then fade while still growing. The aura task and each sparkle select
/// their row with the level digit of the attachment id, less one.
///
/// `brightness` is a blue channel value; green is half of it and red a
/// quarter. Radii are world units ahead of the perspective divide; the rings
/// are drawn at half the aura's radius and the sparkles sprayed from about
/// one and a half times it out.
typedef struct {
    s16 field_0;     // Never read by the cast, so its role is unproven
    s16 brightness;  // Brightness the aura rises to, 0x10 a frame; a sparkle starts at it and, from frame 0x10 of its 0x1E, loses a sixteenth of it every other frame
    s16 radiusStep;  // Aura radius gained per frame, while it grows and while it fades; the aura also turns about its own Y axis by twice this angle each frame, 0x1000 to the turn
    s16 radiusLimit; // Aura radius that ends the growth; also the size of each sparkle, and of the sparks a sparkle sheds
} _HealingLevelTuning;
STATIC_ASSERT_SIZEOF(_HealingLevelTuning, 8);

/// Per-level tuning for the healing aura: rows are PE levels 1-3, selected by
/// `index`. `brightness` is the brightness ceiling, `radiusStep` the per-tick
/// growth and spin, `radiusLimit` the radius the ring grows to before it fades.
static _HealingLevelTuning D_healing_8012FC1C[] = {
    { 0x0008, 0x0080, 0x0040, 0x0400 },
    { 0x000C, 0x00B0, 0x0048, 0x0500 },
    { 0x0010, 0x00E0, 0x0050, 0x0600 },
};

/// The `sndEvtRequestScriptStart` id for each `D_healing_8012FC1C` row.
static s32 D_healing_8012FC34[] = { 0xE0200001, 0xE0230001, 0xE0260001 };

static void _healingDrawSparkle(const GfxCoord* coord, s16 frame, s16 sizeFactor, s16 brightness);

/// Draws the Healing aura's doubled centre disc and surrounding blue glow bands.
///
/// Borrows the composed centre coordinate and aura work without changing them.
/// `angle` is the radius in game-coordinate units, `scale` the blue brightness
/// (red is a quarter, green a half), `index` the zero-based PE level (0..2),
/// and `age` the tick count. The disc uses half the signed radius; odd ages add
/// a broad band and levels two and three add a fainter band farther out.
/// Uses the radial drawers' scratch storage and unchecked frame primitive arena;
/// all emitted packets must remain live until GPU drawing completes.
static inline void _healingDrawGlow(const GfxCoord* coord, const EffectWork* work)
{
    enum {
        HEALING_AURA_BAND_WIDTH          = 128,
        HEALING_AURA_OUTER_RADIUS_OFFSET = 512,
    };
    u8 rgb[3];

    rgb[0] = work->scale >> 2;
    rgb[1] = work->scale >> 1;
    rgb[2] = work->scale;
    // Draw the same additive disc twice to brighten the centre.
    effectDrawGouraudDisc(coord, work->angle >> 1, rgb);
    effectDrawGouraudDisc(coord, work->angle >> 1, rgb);
    rgb[0] >>= 1;
    rgb[1] >>= 1;
    rgb[2] >>= 1;
    effectDrawOuterGlowBand(coord, work->angle, HEALING_AURA_BAND_WIDTH, rgb);
    if (work->age & 1) {
        effectDrawOuterGlowBand(coord, HEALING_AURA_BAND_WIDTH, work->angle, rgb);
    }
    if (work->index != 0) {
        rgb[0] >>= 1;
        rgb[1] >>= 1;
        rgb[2] >>= 1;
        effectDrawOuterGlowBand(coord, (s16)(work->angle + HEALING_AURA_OUTER_RADIUS_OFFSET), HEALING_AURA_BAND_WIDTH, rgb);
    }
}

void healingAuraTask(Task* task)
{
    enum {
        HEALING_AURA_STATE_INITIALIZE    = 0,
        HEALING_AURA_STATE_GROWING       = 1,
        HEALING_AURA_STATE_FADING        = 2,
        HEALING_AURA_STATE_HOLDING       = 3,
        HEALING_AURA_LOCAL_Y             = -0x400,
        HEALING_AURA_INITIAL_RADIUS      = 0x80,
        HEALING_AURA_BRIGHTNESS_STEP     = 16,
        HEALING_AURA_FADE_END_BRIGHTNESS = 17,
        HEALING_AURA_HOLD_FRAMES         = 31,
        HEALING_AURA_ANGLE_MASK          = 0xFFF,
        HEALING_AURA_TRIG_FRACTION_BITS  = 12,
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

    attachment = &Gp_StateC08;
    work       = task->spawnArg2.pointer;
    coord      = task->extra.coordBody->coord;
    if ((attachment->effectPhase == ATTACHMENT_EFFECT_HELD) || (gRoomEffectState->peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN)) {
        if (task->state == HEALING_AURA_STATE_INITIALIZE) {
            attachment->flags |= ATTACHMENT_FLAG_APPLY_STATS;
        }
        effectKillTask(work, task);
        return;
    }

    work->age = work->age + 1;
    switch (task->state) {
        case HEALING_AURA_STATE_INITIALIZE:
            coord->parent = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
            gfxSetRotIdentity(&coord->coord);
            coord->coord.t[0]   = 0;
            coord->coord.t[1]   = HEALING_AURA_LOCAL_Y;
            coord->coord.t[2]   = 0;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            task->state        = HEALING_AURA_STATE_GROWING;
            work->index        = (Gp_StateC08.attachId % 10) - 1;
            work->angle        = HEALING_AURA_INITIAL_RADIUS;
            attachment->flags |= ATTACHMENT_FLAG_APPLY_STATS;
            pan                = (s8)worldCoordGetOriginAudioPan(coord);
            sndEvtRequestScriptStart(D_healing_8012FC34[work->index], pan,
                                     (s8)worldCoordGetOriginAudioDepth(coord));
            /* fallthrough */
        case HEALING_AURA_STATE_GROWING:
            brightness = work->scale;
            if (brightness < D_healing_8012FC1C[work->index].brightness) {
                brightness += HEALING_AURA_BRIGHTNESS_STEP;
            }
            work->scale = brightness;
            work->angle = work->angle + D_healing_8012FC1C[work->index].radiusStep;
            gfxRotMatrixY(&coord->coord, -(D_healing_8012FC1C[work->index].radiusStep * 2), 0);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            // Scatter the next child in the spinning aura's local frame.
            randomState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            spawnAngle      = ((u32)randomState >> 16) & HEALING_AURA_ANGLE_MASK;
            gRandomLcgState = randomState;
            work->step      = spawnAngle;
            work->move.vx   = (rcos(spawnAngle) * (work->angle * 3 / 2)) >> HEALING_AURA_TRIG_FRACTION_BITS;
            verticalProduct = rsin(work->step) * (work->angle * 3 / 2);
            randomState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            gRandomLcgState = randomState;
            work->move.vy   = verticalProduct >> HEALING_AURA_TRIG_FRACTION_BITS;
            work->move.vz   = (rsin(((u32)randomState >> 16) & HEALING_AURA_ANGLE_MASK) * work->move.vx) >> HEALING_AURA_TRIG_FRACTION_BITS;
            spawned         = effectSpawn(EFFECT_HEALING_SPARKLE, coord, (s32)D_healing_8012FC1C[work->index].radiusLimit,
                                          &work->move);
            if (spawned != NULL) {
                taskReparent(task, spawned->task);
            }
            if (work->angle >= D_healing_8012FC1C[work->index].radiusLimit) {
                task->state = HEALING_AURA_STATE_FADING;
            }
            _healingDrawGlow(coord, work);
            return;
        case HEALING_AURA_STATE_FADING:
            gfxRotMatrixY(&coord->coord, -(D_healing_8012FC1C[work->index].radiusStep * 2), 0);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            work->scale = work->scale - HEALING_AURA_BRIGHTNESS_STEP;
            work->angle = work->angle + D_healing_8012FC1C[work->index].radiusStep;
            if (work->scale < HEALING_AURA_FADE_END_BRIGHTNESS) {
                task->state = HEALING_AURA_STATE_HOLDING;
            }
            _healingDrawGlow(coord, work);
            return;
        case HEALING_AURA_STATE_HOLDING:
            gfxRotMatrixY(&coord->coord, -(D_healing_8012FC1C[work->index].radiusStep * 2), 0);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            work->period = work->period + 1;
            if (work->period < HEALING_AURA_HOLD_FRAMES) {
                return;
            }
            effectKillTask(work, task);
            return;
        default:
            return;
    }
}

#include "../../shared/rising_spark_task.inc.c"

void healingRisingSparkTask(Task* task)
{
    _risingSparkTask(task);
}

/// Applies a sparkle's signed local Y step and refreshes its composed position.
///
/// Borrows writable `coord` and read-only `work`; the signed 16-bit displacement
/// is in parent-coordinate units and the sum must fit s32. Invalidates composition
/// before storing the new translation. The live parent chain is borrowed.
static inline void _healingAdvanceSparkleCoord(GfxCoord* coord, const EffectWork* work)
{
    s32 nextY;

    nextY               = coord->coord.t[1] + work->move.vy;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    coord->coord.t[1]   = nextY;
    actorRenderComposeCoord(coord);
}

void healingSparkleTask(Task* task)
{
    enum {
        HEALING_SPARKLE_STATE_INITIALIZE = 0,
        HEALING_SPARKLE_STATE_RISING     = 1,
        HEALING_SPARKLE_Y_STEP           = 4,
        HEALING_SPARKLE_RELEASE_AGE      = 30,
        HEALING_SPARKLE_FADE_START_AGE   = 16,
        HEALING_SPARKLE_SIZE_MASK        = 0xFFF,
        HEALING_SPARKLE_GLOW_LEVEL_INDEX = 2,
        HEALING_SPARKLE_SPARK_PERIOD     = 8,
    };
    EffectWork* work;
    GfxCoord*   coord;
    s16         levelIndex;
    EffectWork* spawned;

    work      = task->spawnArg2.pointer;
    coord     = task->extra.coordBody->coord;
    work->age = work->age + 1;
    if (task->state == HEALING_SPARKLE_STATE_INITIALIZE) {
        coord->parent       = work->parent;
        coord->coord.t[0]   = work->pos.vx;
        coord->coord.t[1]   = work->pos.vy;
        coord->coord.t[2]   = work->pos.vz;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(coord);
        work->move.vy = HEALING_SPARKLE_Y_STEP;
        work->move.vx = 0;
        work->move.vz = 0;
        task->state   = HEALING_SPARKLE_STATE_RISING;
        levelIndex    = (Gp_StateC08.attachId % 10U) - 1;
        work->step    = levelIndex;
        work->scale   = D_healing_8012FC1C[levelIndex].brightness;
        work->angle   = (u16)task->spawnArg1.value & HEALING_SPARKLE_SIZE_MASK;
    }
    // Initialization moves too; fading and child emission occur only on draw ticks.
    _healingAdvanceSparkleCoord(coord, work);
    if (work->age < HEALING_SPARKLE_RELEASE_AGE) {
        if (work->age & 1) {
            work->index = work->index + 1;
            if (work->age >= HEALING_SPARKLE_FADE_START_AGE) {
                work->scale = work->scale - (D_healing_8012FC1C[work->step].brightness >> 4);
            }
            if (work->step < HEALING_SPARKLE_GLOW_LEVEL_INDEX) {
                effectDrawModulatedBillboard(coord, work->index, work->angle,
                                             work->scale);
            } else {
                _healingDrawSparkle(coord, work->index, work->angle, work->scale);
            }
            if ((work->age & (HEALING_SPARKLE_SPARK_PERIOD - 1)) == 1) {
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

/// Sets a sparkle quad's eight screen coordinates from its projected centre.
///
/// Borrows a writable quad and a read-only scratch block with initialized
/// `screenX`, `screenY` and `screenExtent`. Signed word edge arithmetic must
/// fit s32; packet stores retain only its low 16 bits. Nonnegative extents put
/// corners 0/1 above 2/3 and corners 0/2 left of 1/3. No clipping occurs and
/// no pointer is retained.
static inline void _healingSetSparkleScreenBounds(POLY_FT4* quad, const EffectCentreScratch* scratch)
{
    quad->x0 = quad->x2 = scratch->screenX - scratch->screenExtent;
    quad->x1 = quad->x3 = scratch->screenX + scratch->screenExtent;
    quad->y0 = quad->y1 = scratch->screenY - scratch->screenExtent;
    quad->y2 = quad->y3 = scratch->screenY + scratch->screenExtent;
}

/// Draws a Healing level-three sparkle with an alternating-palette outer glow.
///
/// `coord` supplies a composed translation in the input space of `GsWSMATRIX`;
/// XYZ narrows to signed 16-bit game-coordinate units and rotation is unused.
/// `frame` wraps modulo four for the 24-by-24 core and modulo two for the
/// glow palette. `brightness` supplies the core's RGB modulation byte; the
/// glow uses its arithmetic half, narrowed to a byte (128 is neutral modulation).
///
/// Both quads align to the screen axes. With depth SZ3 / 4 + 1, their pixel
/// half-extents are `sizeFactor * 23 / depth` and
/// `(sizeFactor >> 1) * 55 / depth`, with signed division truncating toward zero.
/// Edge arithmetic retains only the low 16 bits. The Healing task supplies
/// size 1536 and brightness 126..224; neither value packs any selector bits.
///
/// Borrows the coordinate, reserves/releases one word-aligned
/// `EffectCentreScratch`, and clobbers GTE working registers. A negative GTE
/// FLAG drops both quads. Otherwise requires unchecked frame-arena space for
/// two additive, modulated `POLY_FT4` packets and a current ordering table.
/// Packets remain live until GPU drawing completes; no scratch pointer survives.
static void _healingDrawSparkle(const GfxCoord* coord, s16 frame, s16 sizeFactor, s16 brightness)
{
    enum {
        HEALING_SPARKLE_FRAME_COUNT         = 4,
        HEALING_SPARKLE_CELL_SIZE           = 24,
        HEALING_SPARKLE_UV_SPAN             = HEALING_SPARKLE_CELL_SIZE - 1,
        HEALING_SPARKLE_TEXTURE_DEPTH_4BIT  = 0,
        HEALING_SPARKLE_TEXTURE_PAGE        = getTPage(HEALING_SPARKLE_TEXTURE_DEPTH_4BIT, GPU_BLEND_ADD, 640, 0),
        HEALING_SPARKLE_CLUT                = getClut(80, 267),
        HEALING_SPARKLE_GLOW_TEXTURE_PAGE   = getTPage(HEALING_SPARKLE_TEXTURE_DEPTH_4BIT, GPU_BLEND_ADD, 576, 0),
        HEALING_SPARKLE_GLOW_PALETTE_COUNT  = 2,
        HEALING_SPARKLE_GLOW_CLUT_X         = 256,
        HEALING_SPARKLE_GLOW_CLUT_Y         = 268,
        HEALING_SPARKLE_GLOW_PALETTE_X_STEP = 16,
        HEALING_SPARKLE_GLOW_LEFT_U         = 56,
        HEALING_SPARKLE_GLOW_TOP_V          = 200,
        HEALING_SPARKLE_GLOW_UV_SPAN        = 55,
        HEALING_SPARKLE_DEPTH_BIAS          = 1,
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
        scratch->depth += HEALING_SPARKLE_DEPTH_BIAS;
        quad            = gGpuPrimCursor;
        gGpuPrimCursor  = quad + 1;
        setPolyFT4(quad);
        setSemiTrans(quad, true);
        quad->tpage = HEALING_SPARKLE_TEXTURE_PAGE;
        quad->clut  = HEALING_SPARKLE_CLUT;
        leftU       = (frame & (HEALING_SPARKLE_FRAME_COUNT - 1)) * HEALING_SPARKLE_CELL_SIZE;
        rightU      = leftU + HEALING_SPARKLE_UV_SPAN;
        quad->u0    = leftU;
        quad->u1    = rightU;
        quad->u2    = leftU;
        quad->u3    = rightU;
        quad->v2    = HEALING_SPARKLE_UV_SPAN;
        quad->v3    = HEALING_SPARKLE_UV_SPAN;
        setRGB0(quad, brightness, brightness, brightness);
        quad->v0              = 0;
        quad->v1              = 0;
        scratch->screenExtent = (sizeFactor * HEALING_SPARKLE_UV_SPAN) / scratch->depth;
        _healingSetSparkleScreenBounds(quad, scratch);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                quad);

        // The glow shares the centre and depth, but halves size and modulation.
        quad           = gGpuPrimCursor;
        gGpuPrimCursor = quad + 1;
        quad->tpage    = HEALING_SPARKLE_GLOW_TEXTURE_PAGE;
        quad->clut     = getClut((frame & (HEALING_SPARKLE_GLOW_PALETTE_COUNT - 1)) * HEALING_SPARKLE_GLOW_PALETTE_X_STEP + HEALING_SPARKLE_GLOW_CLUT_X, HEALING_SPARKLE_GLOW_CLUT_Y);
        setPolyFT4(quad);
        setSemiTrans(quad, true);
        brightness = brightness >> 1;
        setRGB0(quad, brightness, brightness, brightness);
        setUV4(quad, HEALING_SPARKLE_GLOW_LEFT_U, HEALING_SPARKLE_GLOW_TOP_V,
               HEALING_SPARKLE_GLOW_LEFT_U + HEALING_SPARKLE_GLOW_UV_SPAN, HEALING_SPARKLE_GLOW_TOP_V,
               HEALING_SPARKLE_GLOW_LEFT_U, HEALING_SPARKLE_GLOW_TOP_V + HEALING_SPARKLE_GLOW_UV_SPAN,
               HEALING_SPARKLE_GLOW_LEFT_U + HEALING_SPARKLE_GLOW_UV_SPAN, HEALING_SPARKLE_GLOW_TOP_V + HEALING_SPARKLE_GLOW_UV_SPAN);
        scratch->screenExtent = ((sizeFactor >> 1) * HEALING_SPARKLE_GLOW_UV_SPAN) / scratch->depth;
        _healingSetSparkleScreenBounds(quad, scratch);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectCentreScratch);
}
