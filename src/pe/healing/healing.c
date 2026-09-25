#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "pe/healing.h"

#include <psyq/inline_c.h>
#include "gte.h"
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/libgte.h>

/// Per-level tuning for the healing aura: rows are PE levels 1-3, selected by
/// `index`. `field_2` is the brightness ceiling, `field_4` the per-tick
/// spin, `field_6` the radius the ring grows to before the effect ends.
HealingScale D_healing_8012FC1C[] = {
    { 0x0008, 0x0080, 0x0040, 0x0400 },
    { 0x000C, 0x00B0, 0x0048, 0x0500 },
    { 0x0010, 0x00E0, 0x0050, 0x0600 },
};

/// The `SndEvt_EnqueueType6` id for each `D_healing_8012FC1C` row.
s32 D_healing_8012FC34[] = { 0xE0200001, 0xE0230001, 0xE0260001 };

void func_healing_8012F7FC(GpCoord* arg0, s16 arg1, s16 arg2, s16 arg3);

/// Healing PE ring. Cancel (`Gp_StateC08.field_3 == -2` or
/// `Gp_State1C->fadeState >= 4`) releases the work block, and if the effect has
/// not started yet also sets `field_6` bit 3. State 0 parents the coordinate
/// to the player, plays the combo-indexed cue from `D_healing_8012FC34`, and
/// falls into state 1, which grows brightness / radius, randomizes a spawn
/// offset and parents a `0x60017` spark. State 2 shrinks brightness. Both
/// draw two rings plus one or two arcs. State 3 holds for 0x1F frames then
/// releases.
void func_healing_8012EF34(Task* arg0)
{
    GpEffWork*  mem;
    GpCoord*    coord;
    GpStateC08* state;
    GpMtxWords* rot;
    GpEffWork*  spawned;
    s32         pan;
    s32         bright;
    s16         ang;
    s32         rng;
    s32         temp_lo;
    u8          rgb[3];

    state = &Gp_StateC08;
    mem   = arg0->spawnArg2;
    coord = arg0->extra.tmd->coords;
    if ((state->field_3 == -2) || (Gp_State1C->fadeState >= 4)) {
        if (arg0->state == 0) {
            state->field_6 |= 8;
        }
        Gp_ReleaseState1CMem(mem, arg0);
        return;
    }

    mem->age = mem->age + 1;
    switch (arg0->state) {
        case 0:
            rot               = (GpMtxWords*)&coord->coord;
            coord->sub        = (gameGetPtrSlot(3))->extra.tmd->coords;
            rot->m00_m01      = 0x1000;
            rot->m02_m10      = 0;
            rot->m11_m12      = 0x1000;
            rot->m20_m21      = 0;
            rot->m22          = 0x1000;
            coord->coord.t[0] = 0;
            coord->coord.t[1] = -0x400;
            coord->coord.t[2] = 0;
            coord->flg        = 0;
            Gp_UpdateCoord(coord);
            arg0->state     = 1;
            mem->index      = (Gp_StateC08.field_0 % 10) - 1;
            mem->angle      = 0x80;
            state->field_6 |= 8;
            pan             = (s8)Gp_GetObjPan(coord);
            SndEvt_EnqueueType6(D_healing_8012FC34[mem->index], pan,
                                (s8)gpGetObjDepth(coord));
            /* fallthrough */
        case 1:
            bright = mem->scale;
            if (bright < D_healing_8012FC1C[mem->index].field_2) {
                bright += 0x10;
            }
            mem->scale = bright;
            mem->angle = mem->angle + (u16)D_healing_8012FC1C[mem->index].field_4;
            Gfx_RotMatrixY(&coord->coord, -(D_healing_8012FC1C[mem->index].field_4 * 2), 0);
            coord->flg = 0;
            Gp_UpdateCoord(coord);
            rng          = Gp_LcgState * 5 + 0x71357911;
            ang          = ((u32)rng >> 16) & 0xFFF;
            Gp_LcgState  = rng;
            mem->step    = ang;
            mem->move.vx = (rcos(ang) * (mem->angle * 3 / 2)) >> 12;
            temp_lo      = rsin(mem->step) * (mem->angle * 3 / 2);
            rng          = Gp_LcgState * 5 + 0x71357911;
            Gp_LcgState  = rng;
            mem->move.vy = temp_lo >> 12;
            mem->move.vz = (rsin(((u32)rng >> 16) & 0xFFF) * mem->move.vx) >> 12;
            spawned      = Gp_SpawnEff(0x60017, coord, D_healing_8012FC1C[mem->index].field_6,
                                       &mem->move);
            if (spawned != NULL) {
                Task_Reparent(arg0, spawned->task);
            }
            if (mem->angle >= D_healing_8012FC1C[mem->index].field_6) {
                arg0->state = 2;
            }
            goto draw;
        case 2:
            Gfx_RotMatrixY(&coord->coord, -(D_healing_8012FC1C[mem->index].field_4 * 2), 0);
            coord->flg = 0;
            Gp_UpdateCoord(coord);
            mem->scale = mem->scale - 0x10;
            mem->angle = mem->angle + (u16)D_healing_8012FC1C[mem->index].field_4;
            if (mem->scale < 0x11) {
                arg0->state = 3;
            }
        draw:
            rgb[0] = mem->scale >> 2;
            rgb[1] = mem->scale >> 1;
            rgb[2] = (u8)mem->scale;
            Gp_DrawRing(coord, (s32)((u16)mem->angle << 16) >> 17, rgb);
            Gp_DrawRing(coord, (s32)((u16)mem->angle << 16) >> 17, rgb);
            {
                GpCoord*     c;
                s32          span;
                unsigned int r;
                unsigned int g;
                unsigned int b;

                c = coord;
                COPY_REG_EC(c, coord);
                span = 0x80;
                TOUCH_REG(span);
                r      = rgb[0];
                b      = rgb[2];
                rgb[0] = r >> 1;
                SOFT_COMPILER_BARRIER();
                g      = rgb[1];
                rgb[2] = b >> 1;
                rgb[1] = g >> 1;
                Gp_DrawArc(c, mem->angle, span, rgb);
            }
            if (mem->age & 1) {
                Gp_DrawArc(coord, 0x80, mem->angle, rgb);
            }
            if (mem->index != 0) {
                GpCoord*     c;
                s32          span;
                unsigned int r;
                unsigned int g;
                unsigned int b;

                c = coord;
                COPY_REG_EC(c, coord);
                span = 0x80;
                TOUCH_REG(span);
                r      = rgb[0];
                b      = rgb[2];
                rgb[0] = r >> 1;
                SOFT_COMPILER_BARRIER();
                g      = rgb[1];
                rgb[2] = b >> 1;
                rgb[1] = g >> 1;
                Gp_DrawArc(c, (s16)(mem->angle + 0x200), span, rgb);
            }
            return;
        case 3:
            Gfx_RotMatrixY(&coord->coord, -(D_healing_8012FC1C[mem->index].field_4 * 2), 0);
            coord->flg = 0;
            Gp_UpdateCoord(coord);
            mem->period = mem->period + 1;
            if (mem->period < 0x1F) {
                return;
            }
            Gp_ReleaseState1CMem(mem, arg0);
            return;
        default:
            return;
    }
}

/// Healing spark billboard, spawned through gameplay's effect table. State 0
/// seeds the spin and colour from the spawn argument and the LCG; state 1 lifts
/// the frame and draws the additive quad on odd ticks until the animation runs
/// out. Life Drain carries an identical copy.
void func_healing_8012F494(Task* arg0)
{
    GpEffWork* mem;
    GpCoord*   coord;
    s32        y;
    s32        state;
    s16        step;
    u16        spawn;

    mem      = arg0->spawnArg2;
    coord    = arg0->extra.tmd->coords;
    mem->age = mem->age + 1;
    state    = arg0->state;
    switch (state) {
        case 0:
            mem->move.vy = 4;
            mem->move.vx = 0;
            mem->move.vz = 0;
            arg0->state  = 1;
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            mem->scale   = ((u32)Gp_LcgState >> 16) & 0xFFF;
            spawn        = (u16)arg0->spawnArg1;
            mem->period  = 0x1000;
            mem->angle   = spawn & 0xFFF;
            return;
        case 1:
            step              = mem->move.vy;
            y                 = coord->coord.t[1] + step;
            coord->flg        = 0;
            coord->coord.t[1] = y;
            Gp_UpdateCoord(coord);
            if (!(mem->age & 1)) {
                mem->index = mem->index + 1;
            }
            if (mem->index < 8) {
                if (mem->age & 1) {
                    Gp_DrawFxQuad(coord, mem->index, mem->angle,
                                  mem->scale | mem->period);
                    return;
                }
            } else {
                Gp_ReleaseState1CMem(mem, arg0);
                return;
            }
            break;
    }
}

void func_healing_8012F5E4(Task* arg0)
{
    GpEffWork* mem;
    GpCoord*   coord;
    s32        y;
    s16        step;
    s16        kind;
    GpEffWork* spawned;

    mem      = arg0->spawnArg2;
    coord    = arg0->extra.tmd->coords;
    mem->age = mem->age + 1;
    if (arg0->state == 0) {
        coord->sub        = mem->parent;
        coord->coord.t[0] = mem->pos.vx;
        coord->coord.t[1] = mem->pos.vy;
        coord->coord.t[2] = mem->pos.vz;
        coord->flg        = 0;
        Gp_UpdateCoord(coord);
        mem->move.vy = 4;
        mem->move.vx = 0;
        mem->move.vz = 0;
        arg0->state  = 1;
        kind         = (Gp_StateC08.field_0 % 10U) - 1;
        mem->step    = kind;
        mem->scale   = D_healing_8012FC1C[kind].field_2;
        mem->angle   = (u16)arg0->spawnArg1 & 0xFFF;
    }
    step              = mem->move.vy;
    y                 = coord->coord.t[1] + step;
    coord->flg        = 0;
    coord->coord.t[1] = y;
    Gp_UpdateCoord(coord);
    if (mem->age < 0x1E) {
        if (mem->age & 1) {
            mem->index = mem->index + 1;
            if (mem->age >= 0x10) {
                mem->scale = mem->scale - ((s16)D_healing_8012FC1C[mem->step].field_2 >> 4);
            }
            if (mem->step < 2) {
                func_800EB6E8(coord, mem->index, mem->angle,
                              mem->scale);
            } else {
                func_healing_8012F7FC(coord, mem->index, mem->angle, mem->scale);
            }
            if ((mem->age & 7) == 1) {
                spawned = Gp_SpawnEff(0x60016, coord, mem->angle, 0);
                if (spawned != NULL) {
                    Task_Reparent(arg0, spawned->task);
                }
            }
        }
    } else {
        Gp_ReleaseState1CMem(mem, arg0);
    }
}

/// Links the two quads of one healing pulse. `arg0`'s world position is
/// projected through `GsWSMATRIX` by a single `RTPS` and both quads are
/// dropped when that sets a negative `gte_stflg`. The inner quad takes one of
/// the four 0x18-wide frames on tpage 0x2A (CLUT 0x42C5) picked by
/// `arg1 & 3`, is tinted `arg3` and sits `arg2 * 23 / otz` from the projected
/// centre; the outer glow takes the single 0x38..0x6F cell on tpage 0x29 with
/// the CLUT alternating on `arg1 & 1`, is tinted `arg3 / 2` and sits
/// `(arg2 / 2) * 55 / otz` out. Both are axis-aligned and linked into
/// `gGpuCurrentOt` at the shared `otz`. Same 0x18-byte scratch as gameplay
/// `Gp_EffSprTask8D`.
void func_healing_8012F7FC(GpCoord* arg0, s16 arg1, s16 arg2, s16 arg3)
{
    u8*            head;
    GpRingScratch* block;
    POLY_FT4*      prim;
    SVECTOR*       vec;
    s32            u0;
    s32            u1;
    s16            x;
    s16            y;
    u16            vz;

    head                                    = SCRATCH_HEAD(u8);
    ((GpRingScratch*)(head - 0x18))->vec.vx = (u16)arg0->workm.t[0];
    block                                   = (GpRingScratch*)(head - 0x18);
    block->vec.vy                           = (u16)arg0->workm.t[1];
    vz                                      = (u16)arg0->workm.t[2];
    SCRATCH_HEAD(GpRingScratch)             = block;
    block->vec.vz                           = vz;
    vec                                     = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&((GpRingScratch*)(head - 0x18))->sx);
    gte_stflg(&((GpRingScratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpRingScratch*)(head - 0x18))->otz);
        block->otz++;
        prim           = (POLY_FT4*)gGpuPrimCursor;
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
        prim->v0    = 0;
        prim->v1    = 0;
        block->step = (arg2 * 0x17) / block->otz;
        x           = (u16)block->sx - (u16)block->step;
        prim->x2    = x;
        prim->x0    = x;
        x           = (u16)block->sx + (u16)block->step;
        prim->x3    = x;
        prim->x1    = x;
        y           = (u16)block->sy - (u16)block->step;
        prim->y1    = y;
        prim->y0    = y;
        y           = (u16)block->sy + (u16)block->step;
        prim->y3    = y;
        prim->y2    = y;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);

        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        prim->tpage    = 0x29;
        prim->clut     = ((u32)(((arg1 & 1) * 0x10) + 0x100) >> 4) | 0x4300;
        setlen(prim, 9);
        setcode(prim, 0x2E);
        arg3 = arg3 >> 1;
        setRGB0(prim, arg3, arg3, arg3);
        setUV4(prim, 0x38, 0xC8, 0x6F, 0xC8, 0x38, 0xFF, 0x6F, 0xFF);
        block->step = ((arg2 >> 1) * 0x37) / block->otz;
        x           = (u16)block->sx - (u16)block->step;
        prim->x2    = x;
        prim->x0    = x;
        x           = (u16)block->sx + (u16)block->step;
        prim->x3    = x;
        prim->x1    = x;
        y           = (u16)block->sy - (u16)block->step;
        prim->y1    = y;
        prim->y0    = y;
        y           = (u16)block->sy + (u16)block->step;
        prim->y3    = y;
        prim->y2    = y;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    SCRATCH_POP_BYTES(0x18);
}
