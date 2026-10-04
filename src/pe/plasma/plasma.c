#include "pe/plasma.h"

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
#include "gameplay/pad_script.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/random.h"
#include "main/scratch.h"
#include "main/sound.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "../../shared/glow_draw.h"

/// This overlay's id. Every package opens with one: a u16 in a u32
/// slot, distinct across all 448, with the families in contiguous blocks.

static void func_plasma_8012F568(EffectWork* arg0, GfxCoord* arg1, s32 arg2);

/// Per-level geometry for the plasma ring: rows are PE levels 1-3.
/// `baseRadius` is the inner radius, `lift` the height above the caster,
/// `spread` how far the ring grows before it breaks up.
static EffectBandShape D_plasma_8012FF34[] = {
    { 0x0100, 0x0800, 0x0200 },
    { 0x0200, 0x0600, 0x0300 },
    { 0x0300, 0x0400, 0x0400 },
};

/// The `sndEvtRequestScriptStart` id for each `D_plasma_8012FF34` row.
static s32 D_plasma_8012FF48[] = { 0xE0160001, 0xE0190001, 0xE01C0001 };

/// Three 16-entry columns of per-wedge jitter. `func_plasma_8012EF34` fills
/// them with LCG bytes when the ring spawns; `func_plasma_8012F568` reads
/// column `arg2` to pick each wedge's texture.
static s16 D_plasma_8012FF54[3][16] = { 0 };

/// Plasma PE ring. `Task::spawnArg2` is the `EffectWork` block (`scale`
/// brightness, `index` combo index, `age` tick / inner radius);
/// `Task::extra` reaches the coordinate. Cancel (`Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_HELD`
/// or `gRoomEffectState->peEffectControl >= 4`) releases the pool block.
///
/// State 0 seeds brightness, the combo index, and three 16-entry LCG columns
/// in `D_plasma_8012FF54`, plays the combo-indexed cue, and starts a pad
/// lerp. States 1 and 2 decay brightness and draw three rings via
/// `glowDrawHalo` (the third only when `index != 0`) after
/// `func_plasma_8012F568` has applied each jitter column. State 1 is the
/// weaker combo (`index < 2`). Either state releases once brightness
/// drops below 9.
void func_plasma_8012EF34(Task* arg0)
{
    EffectWork*      mem;
    GfxCoord*        coord;
    AttachmentState* state;
    s32              pan;
    s32              i;
    s32              st;
    u16              prev;
    u16              next;
    u8               rgb[3];
    s16              span;

    state = &Gp_StateC08;
    mem   = arg0->spawnArg2.pointer;
    coord = arg0->extra.coordBody->coord;
    if ((state->effectPhase == ATTACHMENT_EFFECT_HELD) || (gRoomEffectState->peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN)) {
        goto release;
    }

    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
    prev     = mem->age;
    next     = prev + 1;
    mem->age = next;
    switch (arg0->state) {
        case 0:
            mem->scale = 0xA0;
            mem->index = (Gp_StateC08.attachId % 10) - 1;
            i          = 0;
            do {
                gRandomLcgState         = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                D_plasma_8012FF54[0][i] = (gRandomLcgState >> 16) & 0xFF;
                gRandomLcgState         = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                D_plasma_8012FF54[1][i] = (gRandomLcgState >> 16) & 0xFF;
                gRandomLcgState         = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                D_plasma_8012FF54[2][i] = (gRandomLcgState >> 16) & 0xFF;
                i++;
            } while (i < 0x10);
            st = 2;
            if (mem->index < 2) {
                st = 1;
            }
            arg0->state = st;
            pan         = (s8)worldCoordGetOriginAudioPan(coord);
            sndEvtRequestScriptStart(D_plasma_8012FF48[(u16)(Gp_StateC08.attachId % 10) - 1], pan,
                                     (s8)worldCoordGetOriginAudioDepth(coord));
            Gp_SpawnPadLerp((s16)(mem->index * 4 + 0x10), 0xFF, 8);
            return;
        case 1:
            if (mem->scale < 9) {
                effectKillTask(mem, arg0);
                return;
            }
            if (gRoomEffectState->peEffectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                if ((s16)next == 8) {
                    state->flags |= ATTACHMENT_FLAG_APPLY_STATS;
                }
                mem->scale  -= 8;
                mem->angle  += 0x60 + mem->index * 0x30;
                mem->period -= 0x20;
                mem->step   += 0x20;
            } else {
                mem->age = prev;
            }
            func_plasma_8012F568(mem, coord, 0);
            func_plasma_8012F568(mem, coord, 1);
            func_plasma_8012F568(mem, coord, 2);
            rgb[0] = rgb[1]    = mem->scale;
            rgb[2]             = mem->scale * 3 / 2;
            coord->workm.t[1] -= mem->age * 64;
            glowDrawHalo(coord, (s16)(mem->age * 64), (s16)(mem->index * 128 + 0x100), rgb);
            rgb[0]           >>= 1;
            rgb[1]           >>= 1;
            rgb[2]           >>= 1;
            coord->workm.t[1] -= mem->age * 64;
            glowDrawHalo(coord, (s16)(mem->age * 128), (s16)(mem->index * 128 + 0x100), rgb);
            if (mem->index != 0) {
                rgb[0]           >>= 1;
                rgb[1]           >>= 1;
                rgb[2]           >>= 1;
                coord->workm.t[1] -= mem->age * 64;
                glowDrawHalo(coord, (s16)(mem->age * 192), (s16)(mem->index * 128 + 0x100), rgb);
            }
            return;
        case 2:
            if (mem->scale < 9) {
                effectKillTask(mem, arg0);
                return;
            }
            if (gRoomEffectState->peEffectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                if ((s16)next == 8) {
                    state->flags |= ATTACHMENT_FLAG_APPLY_STATS;
                }
                mem->scale  -= 8;
                mem->angle  += 0xC0;
                mem->period -= 0x20;
                mem->step   += 0x20;
            } else {
                mem->age = prev;
            }
            func_plasma_8012F568(mem, coord, 0);
            func_plasma_8012F568(mem, coord, 1);
            func_plasma_8012F568(mem, coord, 2);
            rgb[0] = rgb[1]    = mem->scale;
            rgb[2]             = mem->scale * 3 / 2;
            coord->workm.t[1] -= mem->age * 128;
            span               = mem->age * 64;
            glowDrawHalo(coord, span, span, rgb);
            rgb[0]           >>= 1;
            rgb[1]           >>= 1;
            rgb[2]           >>= 1;
            coord->workm.t[1] -= mem->age * 128;
            span               = mem->age * 128;
            glowDrawHalo(coord, span, span, rgb);
            if (mem->index != 0) {
                rgb[0]           >>= 1;
                rgb[1]           >>= 1;
                rgb[2]           >>= 1;
                coord->workm.t[1] -= mem->age * 128;
                span               = mem->age * 192;
                glowDrawHalo(coord, span, span, rgb);
            }
            return;
    }
    return;
release:
    effectKillTask(mem, arg0);
}

/// Draws textured band `arg2` (0..2) of the plasma ring around `arg1`: sixteen
/// `POLY_FT4` wedges between an outer circle of radius
/// `field_26 + baseRadius + field_2A + spread` and an inner one of radius
/// `field_26 + baseRadius`, the outer ring lifted by `-(field_28 + lift)`. Both
/// circles are rotated by the coordinate's `workm`, translated by its `t[]`
/// and projected through `GsWSMATRIX`; wedge `i` picks its texture column
/// from `(D_plasma_8012FF54[arg2][i] + field_22) % 6`, and `field_24` sets the
/// brightness. A negative `gte_stflg` on the wedge's first vertex drops it.
/// Works out of an `EffectBandScratch` taken from the scratch stack.
static void func_plasma_8012F568(EffectWork* arg0, GfxCoord* arg1, s32 arg2)
{
    EffectBandScratch* block;
    SVECTOR*           op;
    POLY_FT4*          prim;
    EffectBandShape*   row;
    s32                i;
    s32                next;
    s32                ang;
    s32                u;
    s16                idx;
    s16                r0;
    s16                r1;
    u16                y;
    u16                f28;

    row   = &D_plasma_8012FF34[arg2];
    f28   = arg0->period;
    r1    = arg0->angle;
    y     = f28 + row->lift;
    r1   += row->baseRadius;
    r0    = r1 + arg0->step + row->spread;
    block = SCRATCH_STACK_RESERVE_BLOCK(EffectBandScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    for (i = 0; i < EFFECT_BAND_SEGMENT_COUNT; i++) {
        ang                  = i << 8;
        block->topRing[i].vx = (rsin(ang) * r0) >> 12;
        block->topRing[i].vy = -y;
        block->topRing[i].vz = (rcos(ang) * r0) >> 12;
        gte_SetRotMatrix(&arg1->workm);
        gte_ldv0(&block->topRing[i]);
        gte_rtv0();
        gte_stsv(&block->topRing[i]);
        block->topRing[i].vx    = (u16)block->topRing[i].vx + (u16)arg1->workm.t[0];
        block->topRing[i].vy    = (u16)block->topRing[i].vy + (u16)arg1->workm.t[1];
        block->topRing[i].vz    = (u16)block->topRing[i].vz + (u16)arg1->workm.t[2];
        block->bottomRing[i].vx = (rsin(ang) * r1) >> 12;
        op                      = &block->topRing[i] + EFFECT_BAND_SEGMENT_COUNT;
        op->vy                  = 0;
        op->vz                  = (rcos(ang) * r1) >> 12;
        gte_SetRotMatrix(&arg1->workm);
        gte_ldv0(&block->bottomRing[i]);
        gte_rtv0();
        gte_stsv(&block->bottomRing[i]);
        block->bottomRing[i].vx = (u16)block->bottomRing[i].vx + (u16)arg1->workm.t[0];
        op->vy                  = (u16)op->vy + (u16)arg1->workm.t[1];
        op->vz                  = (u16)op->vz + (u16)arg1->workm.t[2];
    }
    gte_SetRotMatrix(&GsWSMATRIX);
    for (i = 0; i < EFFECT_BAND_SEGMENT_COUNT; i++) {
        gte_ldv0(&block->topRing[i]);
        gte_rtps();
        idx = (D_plasma_8012FF54[arg2][i] + arg0->age) % 6;
        gte_stsxy(&block->sxy0);
        next = (i + 1) & (EFFECT_BAND_SEGMENT_COUNT - 1);
        gte_ldv3(&block->topRing[next], &block->bottomRing[i], &block->bottomRing[next]);
        gte_rtpt();
        gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
        gte_stflg(&block->projectionFlags);
        if (block->projectionFlags >= 0) {
            gte_stszotz(&block->otz);
            block->otz++;
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyFT4(prim);
            setRGB0(prim, *(u8*)&arg0->scale, *(u8*)&arg0->scale, *(u8*)&arg0->scale);
            setSemiTrans(prim, 1);
            prim->tpage = 0x2A;
            prim->clut  = 0x42C1;
            u           = idx * 0x28;
            setUV4(prim, u, 0x60, u + 0x27, 0x60, u, 0x87, u + 0x27, 0x87);
            prim->x0 = (u16)block->sxy0.vx;
            prim->y0 = (u16)block->sxy0.vy;
            prim->x1 = (u16)block->sxy1.vx;
            prim->y1 = (u16)block->sxy1.vy;
            prim->x2 = (u16)block->sxy2.vx;
            prim->y2 = (u16)block->sxy2.vy;
            prim->x3 = (u16)block->sxy3.vx;
            prim->y3 = (u16)block->sxy3.vy;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectBandScratch);
}

#include "../../shared/glow_draw_halo.inc.c"
