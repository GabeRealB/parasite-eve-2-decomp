#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "pe/plasma.h"

#include <psyq/inline_c.h>
#include "gte.h"
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/libgte.h>

/// This overlay's id. Every package opens with one: a u16 in a u32
/// slot, distinct across all 448, with the families in contiguous blocks.

/// Per-level geometry for the plasma ring: rows are PE levels 1-3. `rInner` is
/// the inner radius, `yOff` the height above the caster, `rExtra` how far the
/// ring grows before it breaks up.
PlasmaRingScale D_plasma_8012FF34[] = {
    { 0x0100, 0x0800, 0x0200 },
    { 0x0200, 0x0600, 0x0300 },
    { 0x0300, 0x0400, 0x0400 },
};

/// The `SndEvt_EnqueueType6` id for each `D_plasma_8012FF34` row.
s32 D_plasma_8012FF48[] = { 0xE0160001, 0xE0190001, 0xE01C0001 };

/// Per-vertex jitter the ring walks each frame; three banks of 16.
s16 D_plasma_8012FF54[3][16] = { 0 };

/// Plasma PE ring. `Task::spawnArg2` is the `GpEffWork` block (`scale`
/// brightness, `index` combo index, `age` tick / inner radius);
/// `Task::extra` reaches the coordinate. Cancel (`Gp_StateC08.field_3 == -2`
/// or `Gp_State1C->fadeState >= 4`) releases the pool block.
///
/// State 0 seeds brightness, the combo index, and three 16-entry LCG columns
/// in `D_plasma_8012FF54`, plays the combo-indexed cue, and starts a pad
/// lerp. States 1 and 2 decay brightness and draw three rings via
/// `func_plasma_8012FB10` (the third only when `index != 0`) after
/// `func_plasma_8012F568` has applied each jitter column. State 1 is the
/// weaker combo (`index < 2`). Either state releases once brightness
/// drops below 9.
void func_plasma_8012EF34(Task* arg0)
{
    GpEffWork*  mem;
    GpCoord*    coord;
    GpStateC08* state;
    s32         pan;
    s32         i;
    s32         st;
    u16         prev;
    u16         next;
    u8          rgb[3];
    s16         span;

    state = &Gp_StateC08;
    mem   = arg0->spawnArg2;
    coord = arg0->extra.tmd->coords;
    if ((state->field_3 == -2) || (Gp_State1C->fadeState >= 4)) {
        goto release;
    }

    coord->flg = 0;
    Gp_UpdateCoord(coord);
    prev     = mem->age;
    next     = prev + 1;
    mem->age = next;
    switch (arg0->state) {
        case 0:
            mem->scale = 0xA0;
            mem->index = (Gp_StateC08.field_0 % 10) - 1;
            i          = 0;
            do {
                Gp_LcgState             = Gp_LcgState * 5 + 0x71357911;
                D_plasma_8012FF54[0][i] = ((u32)Gp_LcgState >> 16) & 0xFF;
                Gp_LcgState             = Gp_LcgState * 5 + 0x71357911;
                D_plasma_8012FF54[1][i] = ((u32)Gp_LcgState >> 16) & 0xFF;
                Gp_LcgState             = Gp_LcgState * 5 + 0x71357911;
                D_plasma_8012FF54[2][i] = ((u32)Gp_LcgState >> 16) & 0xFF;
                i++;
            } while (i < 0x10);
            st = 2;
            if (mem->index < 2) {
                st = 1;
            }
            arg0->state = st;
            pan         = (s8)Gp_GetObjPan(coord);
            SndEvt_EnqueueType6(D_plasma_8012FF48[(u16)(Gp_StateC08.field_0 % 10) - 1], pan,
                                (s8)gpGetObjDepth(coord));
            Gp_SpawnPadLerp((s16)(mem->index * 4 + 0x10), 0xFF, 8);
            return;
        case 1:
            if (mem->scale < 9) {
                Gp_ReleaseState1CMem(mem, arg0);
                return;
            }
            if (Gp_State1C->fadeState == 0) {
                if ((s16)next == 8) {
                    state->field_6 |= 8;
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
            func_plasma_8012FB10(coord, (s16)(mem->age * 64), (s16)(mem->index * 128 + 0x100), rgb);
            rgb[0]           >>= 1;
            rgb[1]           >>= 1;
            rgb[2]           >>= 1;
            coord->workm.t[1] -= mem->age * 64;
            func_plasma_8012FB10(coord, (s16)(mem->age * 128), (s16)(mem->index * 128 + 0x100), rgb);
            if (mem->index != 0) {
                rgb[0]           >>= 1;
                rgb[1]           >>= 1;
                rgb[2]           >>= 1;
                coord->workm.t[1] -= mem->age * 64;
                func_plasma_8012FB10(coord, (s16)(mem->age * 192), (s16)(mem->index * 128 + 0x100), rgb);
            }
            return;
        case 2:
            if (mem->scale < 9) {
                Gp_ReleaseState1CMem(mem, arg0);
                return;
            }
            if (Gp_State1C->fadeState == 0) {
                if ((s16)next == 8) {
                    state->field_6 |= 8;
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
            func_plasma_8012FB10(coord, span, span, rgb);
            rgb[0]           >>= 1;
            rgb[1]           >>= 1;
            rgb[2]           >>= 1;
            coord->workm.t[1] -= mem->age * 128;
            span               = mem->age * 128;
            func_plasma_8012FB10(coord, span, span, rgb);
            if (mem->index != 0) {
                rgb[0]           >>= 1;
                rgb[1]           >>= 1;
                rgb[2]           >>= 1;
                coord->workm.t[1] -= mem->age * 128;
                span               = mem->age * 192;
                func_plasma_8012FB10(coord, span, span, rgb);
            }
            return;
    }
    return;
release:
    Gp_ReleaseState1CMem(mem, arg0);
}

void func_plasma_8012F568(GpEffWork* arg0, GpCoord* arg1, s32 arg2)
{
    u8*              head;
    GpBandScratch*   block;
    SVECTOR*         op;
    POLY_FT4*        prim;
    PlasmaRingScale* row;
    s32              i;
    s32              next;
    s32              ang;
    s32              u;
    s16              idx;
    s16              r0;
    s16              r1;
    u16              y;
    u16              f28;

    row                = &D_plasma_8012FF34[arg2];
    f28                = arg0->period;
    r1                 = arg0->angle;
    y                  = f28 + (u16)row->yOff;
    r1                += (u16)row->rInner;
    r0                 = r1 + arg0->step + (u16)row->rExtra;
    head               = SCRATCH_HEAD(u8);
    SCRATCH_HEAD(void) = head - 0x118;
    block              = (GpBandScratch*)(head - 0x118);
    gte_SetTransMatrix(&GsWSMATRIX);
    for (i = 0; i < 16; i++) {
        ang                = i << 8;
        block->inner[i].vx = (rsin(ang) * r0) >> 12;
        block->inner[i].vy = -y;
        block->inner[i].vz = (rcos(ang) * r0) >> 12;
        gte_SetRotMatrix(&arg1->workm);
        gte_ldv0(&block->inner[i]);
        gte_rtv0();
        gte_stsv(&block->inner[i]);
        block->inner[i].vx = (u16)block->inner[i].vx + (u16)arg1->workm.t[0];
        block->inner[i].vy = (u16)block->inner[i].vy + (u16)arg1->workm.t[1];
        block->inner[i].vz = (u16)block->inner[i].vz + (u16)arg1->workm.t[2];
        block->outer[i].vx = (rsin(ang) * r1) >> 12;
        op                 = &block->inner[i] + 16;
        op->vy             = 0;
        op->vz             = (rcos(ang) * r1) >> 12;
        gte_SetRotMatrix(&arg1->workm);
        gte_ldv0(&block->outer[i]);
        gte_rtv0();
        gte_stsv(&block->outer[i]);
        block->outer[i].vx = (u16)block->outer[i].vx + (u16)arg1->workm.t[0];
        op->vy             = (u16)op->vy + (u16)arg1->workm.t[1];
        op->vz             = (u16)op->vz + (u16)arg1->workm.t[2];
    }
    gte_SetRotMatrix(&GsWSMATRIX);
    for (i = 0; i < 16; i++) {
        gte_ldv0(&block->inner[i]);
        gte_rtps();
        idx = (D_plasma_8012FF54[arg2][i] + arg0->age) % 6;
        gte_stsxy(&block->sxy0);
        next = (i + 1) & 0xF;
        gte_ldv3(&block->inner[next], &block->outer[i], &block->outer[next]);
        gte_rtpt();
        gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
        gte_stflg(&block->flag);
        if (block->flag >= 0) {
            gte_stszotz(&block->otz);
            block->otz++;
            prim           = (POLY_FT4*)gGpuPrimCursor;
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
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
        }
    }
    SCRATCH_POP_BYTES(0x118);
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, when
/// the GTE flag is non-negative, queues sixteen gouraud `POLY_G4` wedges that
/// form a ring. `arg1` is the inner half-extent and `arg2` the extra outer
/// width; on-screen radii are `(s16)arg1 * 64 / (otz + 1)` and
/// `(s16)(arg1 + arg2) * 64 / (otz + 1)`. The RGB triple tints the inner edge
/// so each wedge fades to a black outer rim. Byte-identical to the rooms
/// family's `Room_Draw07` (src/lib/room_draw07.c).
void func_plasma_8012FB10(GpCoord* arg0, s32 arg1, s32 arg2, u8* rgb)
{
    GpArcScratch* block;
    POLY_G4*      prim;
    s32           ang;
    s32           next;
    s32           outer;

    block         = SCRATCH_PUSH(GpArcScratch);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];
    outer         = arg1 + arg2;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        block->otz++;
        block->inner = ((s16)arg1 * 64) / block->otz;
        block->outer = ((s16)outer * 64) / block->otz;
        for (ang = 0; ang < 0x1000; ang = next) {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, rgb[0], rgb[1], rgb[2]);
            prim->x0 = block->sx + ((block->inner * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->inner * rcos(ang)) >> 12);
            next     = ang + 0x100;
            prim->x1 = block->sx + ((block->inner * rsin(next)) >> 12);
            prim->y1 = block->sy + ((block->inner * rcos(next)) >> 12);
            prim->x2 = block->sx + ((block->outer * rsin(ang)) >> 12);
            prim->y2 = block->sy + ((block->outer * rcos(ang)) >> 12);
            prim->x3 = block->sx + ((block->outer * rsin(next)) >> 12);
            prim->y3 = block->sy + ((block->outer * rcos(next)) >> 12);
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }
    }
    SCRATCH_POP(GpArcScratch);
}
