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

static void func_healing_8012F7FC(GfxCoord* arg0, s16 arg1, s16 arg2, s16 arg3);

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
    u8                rgb[3];

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
            goto draw;
        case 2:
            gfxRotMatrixY(&coord->coord, -(D_healing_8012FC1C[mem->index].radiusStep * 2), 0);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            mem->scale = mem->scale - 0x10;
            mem->angle = mem->angle + D_healing_8012FC1C[mem->index].radiusStep;
            if (mem->scale < 0x11) {
                arg0->state = 3;
            }
        draw:
            rgb[0] = mem->scale >> 2;
            rgb[1] = mem->scale >> 1;
            rgb[2] = (u8)mem->scale;
            Gp_DrawRing(coord, (s32)((u16)mem->angle << 16) >> 17, rgb);
            Gp_DrawRing(coord, (s32)((u16)mem->angle << 16) >> 17, rgb);
            rgb[0] >>= 1;
            rgb[1] >>= 1;
            rgb[2] >>= 1;
            Gp_DrawArc(coord, mem->angle, 0x80, rgb);
            if (mem->age & 1) {
                Gp_DrawArc(coord, 0x80, mem->angle, rgb);
            }
            if (mem->index != 0) {
                rgb[0] >>= 1;
                rgb[1] >>= 1;
                rgb[2] >>= 1;
                Gp_DrawArc(coord, (s16)(mem->angle + 0x200), 0x80, rgb);
            }
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

/// Healing's spark billboard (see rising_spark.h), spawned through gameplay's
/// effect table.
void func_healing_8012F494(Task* task)
{
    risingSparkTask(task);
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
                func_800EB6E8(coord, mem->index, mem->angle,
                              mem->scale);
            } else {
                func_healing_8012F7FC(coord, mem->index, mem->angle, mem->scale);
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

/// Links the two quads of one healing pulse. `arg0`'s world position is
/// projected through `GsWSMATRIX` by a single `RTPS` and both quads are
/// dropped when that sets a negative `gte_stflg`. The inner quad takes one of
/// the four 0x18-wide frames on tpage 0x2A (CLUT 0x42C5) picked by
/// `arg1 & 3`, is tinted `arg3` and sits `arg2 * 23 / depth` from the projected
/// centre; the outer glow takes the single 0x38..0x6F cell on tpage 0x29 with
/// the CLUT alternating on `arg1 & 1`, is tinted `arg3 / 2` and sits
/// `(arg2 / 2) * 55 / depth` out. Both are axis-aligned and linked into
/// `gGpuCurrentOt` at the shared `depth`. Same 0x18-byte scratch as gameplay
/// `Gp_EffSprTask8D`.
static void func_healing_8012F7FC(GfxCoord* arg0, s16 arg1, s16 arg2, s16 arg3)
{
    u8*                  head;
    EffectCentreScratch* block;
    POLY_FT4*            prim;
    SVECTOR*             vec;
    s32                  u0;
    s32                  u1;
    s16                  x;
    s16                  y;
    u16                  vz;

    head                                                                        = SCRATCH_STACK_CURSOR(u8);
    ((EffectCentreScratch*)(head - sizeof(EffectCentreScratch)))->worldPoint.vx = (u16)arg0->workm.t[0];
    block                                                                       = (EffectCentreScratch*)(head - sizeof(EffectCentreScratch));
    block->worldPoint.vy                                                        = (u16)arg0->workm.t[1];
    vz                                                                          = (u16)arg0->workm.t[2];
    SCRATCH_STACK_CURSOR(EffectCentreScratch)                                   = block;
    block->worldPoint.vz                                                        = vz;
    vec                                                                         = &block->worldPoint;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&((EffectCentreScratch*)(head - sizeof(EffectCentreScratch)))->screenX);
    gte_stflg(&((EffectCentreScratch*)(head - sizeof(EffectCentreScratch)))->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(&((EffectCentreScratch*)(head - sizeof(EffectCentreScratch)))->depth);
        block->depth++;
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2E);
        prim->tpage = 0x2A;
        prim->clut  = 0x42C5;
        u0          = (arg1 & 3) * 0x18;
        u1          = u0 + 0x17;
        prim->u0    = u0;
        prim->u1    = u1;
        prim->u2    = u0;
        prim->u3    = u1;
        prim->v2    = 0x17;
        prim->v3    = 0x17;
        setRGB0(prim, arg3, arg3, arg3);
        prim->v0            = 0;
        prim->v1            = 0;
        block->screenExtent = (arg2 * 0x17) / block->depth;
        x                   = block->screenX - (u16)block->screenExtent;
        prim->x2            = x;
        prim->x0            = x;
        x                   = block->screenX + (u16)block->screenExtent;
        prim->x3            = x;
        prim->x1            = x;
        y                   = block->screenY - (u16)block->screenExtent;
        prim->y1            = y;
        prim->y0            = y;
        y                   = block->screenY + (u16)block->screenExtent;
        prim->y3            = y;
        prim->y2            = y;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);

        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        prim->tpage    = 0x29;
        prim->clut     = ((u32)(((arg1 & 1) * 0x10) + 0x100) >> 4) | 0x4300;
        setlen(prim, 9);
        setcode(prim, 0x2E);
        arg3 = arg3 >> 1;
        setRGB0(prim, arg3, arg3, arg3);
        setUV4(prim, 0x38, 0xC8, 0x6F, 0xC8, 0x38, 0xFF, 0x6F, 0xFF);
        block->screenExtent = ((arg2 >> 1) * 0x37) / block->depth;
        x                   = block->screenX - (u16)block->screenExtent;
        prim->x2            = x;
        prim->x0            = x;
        x                   = block->screenX + (u16)block->screenExtent;
        prim->x3            = x;
        prim->x1            = x;
        y                   = block->screenY - (u16)block->screenExtent;
        prim->y1            = y;
        prim->y0            = y;
        y                   = block->screenY + (u16)block->screenExtent;
        prim->y3            = y;
        prim->y2            = y;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectCentreScratch);
}
