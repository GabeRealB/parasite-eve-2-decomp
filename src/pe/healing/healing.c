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

/// Healing PE ring. Cancel (`Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_HELD` or
/// `gRoomEffectState->peEffectControl >= 4`) releases the work block, and if the effect has
/// not started yet also sets `field_6` bit 3. State 0 parents the coordinate
/// to the player, plays the combo-indexed cue from `D_healing_8012FC34`, and
/// falls into state 1, which grows brightness / radius, randomizes a spawn
/// offset and parents a `0x60017` spark. State 2 shrinks brightness. Both
/// draw two rings plus one or two arcs. State 3 holds for 0x1F frames then
/// releases.
void func_healing_8012EF34(Task* arg0)
{
    EffectWork*       mem;
    GfxCoord*         coord;
    AttachmentState*  state;
    GfxRotationWords* rot;
    EffectWork*       spawned;
    s32               pan;
    s32               bright;
    s16               ang;
    s32               rng;
    s32               temp_lo;

    state = &Gp_StateC08;
    mem   = arg0->spawnArg2.pointer;
    coord = arg0->extra.coordBody->coord;
    if ((state->effectPhase == ATTACHMENT_EFFECT_HELD) || (gRoomEffectState->peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN)) {
        if (arg0->state == 0) {
            state->flags |= ATTACHMENT_FLAG_APPLY_STATS;
        }
        effectKillTask(mem, arg0);
        return;
    }

    mem->age = mem->age + 1;
    switch (arg0->state) {
        case 0:
            rot                 = (GfxRotationWords*)&coord->coord;
            coord->parent       = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
            rot->m00M01         = ONE;
            rot->m02M10         = 0;
            rot->m11M12         = ONE;
            rot->m20M21         = 0;
            rot->m22            = ONE;
            coord->coord.t[0]   = 0;
            coord->coord.t[1]   = -0x400;
            coord->coord.t[2]   = 0;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            arg0->state   = 1;
            mem->index    = (Gp_StateC08.attachId % 10) - 1;
            mem->angle    = 0x80;
            state->flags |= ATTACHMENT_FLAG_APPLY_STATS;
            pan           = (s8)worldCoordGetOriginAudioPan(coord);
            sndEvtRequestScriptStart(D_healing_8012FC34[mem->index], pan,
                                     (s8)worldCoordGetOriginAudioDepth(coord));
            /* fallthrough */
        case 1:
            bright = mem->scale;
            if (bright < D_healing_8012FC1C[mem->index].brightness) {
                bright += 0x10;
            }
            mem->scale = bright;
            mem->angle = mem->angle + D_healing_8012FC1C[mem->index].radiusStep;
            gfxRotMatrixY(&coord->coord, -(D_healing_8012FC1C[mem->index].radiusStep * 2), 0);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            ang             = ((u32)rng >> 16) & 0xFFF;
            gRandomLcgState = rng;
            mem->step       = ang;
            mem->move.vx    = (rcos(ang) * (mem->angle * 3 / 2)) >> 12;
            temp_lo         = rsin(mem->step) * (mem->angle * 3 / 2);
            rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            gRandomLcgState = rng;
            mem->move.vy    = temp_lo >> 12;
            mem->move.vz    = (rsin(((u32)rng >> 16) & 0xFFF) * mem->move.vx) >> 12;
            spawned         = Gp_SpawnEff(EFFECT_HEALING_SPARKLE, coord, (s32)D_healing_8012FC1C[mem->index].radiusLimit,
                                          &mem->move);
            if (spawned != NULL) {
                taskReparent(arg0, spawned->task);
            }
            if (mem->angle >= D_healing_8012FC1C[mem->index].radiusLimit) {
                arg0->state = 2;
            }
            _healingDrawGlow(coord, mem);
            return;
        case 2:
            gfxRotMatrixY(&coord->coord, -(D_healing_8012FC1C[mem->index].radiusStep * 2), 0);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            mem->scale = mem->scale - 0x10;
            mem->angle = mem->angle + D_healing_8012FC1C[mem->index].radiusStep;
            if (mem->scale < 0x11) {
                arg0->state = 3;
            }
            _healingDrawGlow(coord, mem);
            return;
        case 3:
            gfxRotMatrixY(&coord->coord, -(D_healing_8012FC1C[mem->index].radiusStep * 2), 0);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            mem->period = mem->period + 1;
            if (mem->period < 0x1F) {
                return;
            }
            effectKillTask(mem, arg0);
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

void func_healing_8012F5E4(Task* arg0)
{
    EffectWork* mem;
    GfxCoord*   coord;
    s32         y;
    s16         step;
    s16         kind;
    EffectWork* spawned;

    mem      = arg0->spawnArg2.pointer;
    coord    = arg0->extra.coordBody->coord;
    mem->age = mem->age + 1;
    if (arg0->state == 0) {
        coord->parent       = mem->parent;
        coord->coord.t[0]   = mem->pos.vx;
        coord->coord.t[1]   = mem->pos.vy;
        coord->coord.t[2]   = mem->pos.vz;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(coord);
        mem->move.vy = 4;
        mem->move.vx = 0;
        mem->move.vz = 0;
        arg0->state  = 1;
        kind         = (Gp_StateC08.attachId % 10U) - 1;
        mem->step    = kind;
        mem->scale   = D_healing_8012FC1C[kind].brightness;
        mem->angle   = (u16)arg0->spawnArg1.value & 0xFFF;
    }
    step                = mem->move.vy;
    y                   = coord->coord.t[1] + step;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    coord->coord.t[1]   = y;
    actorRenderComposeCoord(coord);
    if (mem->age < 0x1E) {
        if (mem->age & 1) {
            mem->index = mem->index + 1;
            if (mem->age >= 0x10) {
                mem->scale = mem->scale - (D_healing_8012FC1C[mem->step].brightness >> 4);
            }
            if (mem->step < 2) {
                effectDrawModulatedBillboard(coord, mem->index, mem->angle,
                                             mem->scale);
            } else {
                _healingDrawSparkle(coord, mem->index, mem->angle, mem->scale);
            }
            if ((mem->age & 7) == 1) {
                spawned = Gp_SpawnEff(EFFECT_HEALING_SPARK, coord, (s32)(mem->angle), 0);
                if (spawned != NULL) {
                    taskReparent(arg0, spawned->task);
                }
            }
        }
    } else {
        effectKillTask(mem, arg0);
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
