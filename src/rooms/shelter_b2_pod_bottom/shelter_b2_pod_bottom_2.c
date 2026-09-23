#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/3FB8.h"
#include "main/task.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "rooms/room_common.h"

#include <psyq/inline_c.h>

extern u32 Gp_LcgState;

/// The bytes of `rtps` as this build emits them, with its two leading hazard
/// nops; the `inline_c.h` macro of that name assembles to a different word.
#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")
#define gte_rtpt_real() __asm__ volatile("nop; nop; .word 0x4A280030")
#define gte_rtv0_real() __asm__ volatile("nop; nop; .word 0x4A486012")

/// Scratch block `func_shelter_b2_pod_bottom_8017F994` pops from
/// `G_SCRATCH_HEAD`: the projected world position, its `gte_stszotz` depth
/// and `gte_stflg` flag, the two screen radii derived from them, and the
/// `gte_stsxy` screen point.
typedef struct _BillboardScratch {
    SVECTOR vec;
    s32     otz;
    s32     flag;
    s32     rOuter;
    s32     rInner;
    s16     sx;
    s16     sy;
} _BillboardScratch;

void func_shelter_b2_pod_bottom_8017E788(GsCOORDINATE2* coord, s16 arg1, s16 arg2);
void func_shelter_b2_pod_bottom_8017EEAC(RoomEffWork* work, GsCOORDINATE2* coord, s32 arg2);

/// Per-ring offsets `func_shelter_b2_pod_bottom_8017EEAC` adds to the work's
/// ramps: `rInner` widens the ring drawn at the frame's origin height, `rExtra`
/// widens the second ring beyond that plus the work's step, and `yOff` raises
/// the second ring on top of the work's height.
typedef struct {
    s16 rInner;
    s16 yOff;
    s16 rExtra;
} _RingScale;

extern u16        D_shelter_b2_pod_bottom_80188790[3][16];
extern _RingScale D_shelter_b2_pod_bottom_80181C94[];

/// Tint rows for `func_shelter_b2_pod_bottom_8017F448`: per-channel right
/// shifts (0-2) applied to its colour ramp, one row chosen at random.
extern u16 D_shelter_b2_pod_bottom_80181CA8[][3];

void func_shelter_b2_pod_bottom_8017F994(GsCOORDINATE2* coord, s32 arg1, u8* rgb);
void func_shelter_b2_pod_bottom_801805A0(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, u8* rgb);

extern s16 D_shelter_b2_pod_bottom_801887F0[8];

/// On its first frame (state 0) fills three rows of 16 random bytes in
/// `D_shelter_b2_pod_bottom_80188790` from the gameplay LCG and turns off
/// `groundTrace`; every frame, disables the ground shadow in view 0xF and
/// selects shade row 0 elsewhere.
void func_shelter_b2_pod_bottom_8017D760(Task* task)
{
    s32 i;

    if (task->state == 0) {
        for (i = 0; i < 16; i++) {
            Gp_LcgState                            = Gp_LcgState * 5 + 0x71357911;
            D_shelter_b2_pod_bottom_80188790[0][i] = (Gp_LcgState >> 16) & 0xFF;
            Gp_LcgState                            = Gp_LcgState * 5 + 0x71357911;
            D_shelter_b2_pod_bottom_80188790[1][i] = (Gp_LcgState >> 16) & 0xFF;
            Gp_LcgState                            = Gp_LcgState * 5 + 0x71357911;
            D_shelter_b2_pod_bottom_80188790[2][i] = (Gp_LcgState >> 16) & 0xFF;
        }
        task->state             = 1;
        Gp_State1C->groundTrace = 0;
    }
    if ((Gp_GetViewIndex() & 0xFF) == 0xF) {
        Gp_State1C->groundShade = -1;
    } else {
        Gp_State1C->groundShade = 0;
    }
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_pod_bottom/shelter_b2_pod_bottom_2", func_shelter_b2_pod_bottom_8017D850);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_pod_bottom/shelter_b2_pod_bottom_2", func_shelter_b2_pod_bottom_8017DECC);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_pod_bottom/shelter_b2_pod_bottom_2", func_shelter_b2_pod_bottom_8017E334);

/// Draws a glowing band: two 16-vertex rings in the XZ plane, the inner of
/// radius `arg1` at a height of -0x180 and the outer of radius `arg1 + 0x200`
/// at 0, are rotated by `coord`'s `workm` and offset by its translation, then
/// each of the 16 segments is projected through `GsWSMATRIX` as one
/// `POLY_G4`. The inner edge carries the `(arg2 >> 1, arg2 >> 1, arg2)` colour
/// and the outer edge fades to black; a negative `gte_stflg` drops the
/// segment.
void func_shelter_b2_pod_bottom_8017E788(GsCOORDINATE2* coord, s16 arg1, s16 arg2)
{
    void**         scratch;
    u8*            head;
    GpBandScratch* block;
    SVECTOR*       op;
    POLY_G4*       prim;
    s32            i;
    s32            next;
    s32            ang;
    s16            r0;
    s16            r1;
    u8             red;
    u8             grn;
    u8             blu;

    r1       = arg1 + 0x200;
    scratch  = (void**)G_SCRATCH_HEAD;
    head     = (u8*)*scratch - 0x118;
    *scratch = head;
    red      = arg2 >> 1;
    grn      = arg2 >> 1;
    blu      = arg2;
    block    = (GpBandScratch*)head;
    gte_SetTransMatrix(&GsWSMATRIX);
    r0 = arg1;
    for (i = 0; i < 16; i++) {
        ang                = i << 8;
        block->inner[i].vx = (rsin(ang) * r0) >> 12;
        block->inner[i].vy = -0x180;
        block->inner[i].vz = (rcos(ang) * r0) >> 12;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&block->inner[i]);
        gte_rtv0_real();
        gte_stsv(&block->inner[i]);
        block->inner[i].vx = *(u16*)&block->inner[i].vx + *(u16*)&coord->workm.t[0];
        block->inner[i].vy = *(u16*)&block->inner[i].vy + *(u16*)&coord->workm.t[1];
        block->inner[i].vz = *(u16*)&block->inner[i].vz + *(u16*)&coord->workm.t[2];
        block->outer[i].vx = (rsin(ang) * r1) >> 12;
        op                 = &block->inner[i] + 16;
        op->vy             = 0;
        op->vz             = (rcos(ang) * r1) >> 12;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&block->outer[i]);
        gte_rtv0_real();
        gte_stsv(&block->outer[i]);
        block->outer[i].vx = *(u16*)&block->outer[i].vx + *(u16*)&coord->workm.t[0];
        op->vy             = *(u16*)&op->vy + *(u16*)&coord->workm.t[1];
        op->vz             = *(u16*)&op->vz + *(u16*)&coord->workm.t[2];
    }
    gte_SetRotMatrix(&GsWSMATRIX);
    for (i = 0; i < 16; i++) {
        gte_ldv0(&block->inner[i]);
        gte_rtps_real();
        gte_stsxy(&block->sxy0);
        next = (i + 1) & 0xF;
        gte_ldv3(&block->inner[next], &block->outer[i], &block->outer[next]);
        gte_rtpt_real();
        gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
        gte_stflg(&block->flag);
        if (block->flag >= 0) {
            gte_stszotz(&block->otz);
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, red, grn, blu);
            setRGB1(prim, red, grn, blu);
            setRGB2(prim, 0, 0, 0);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = *(u16*)&block->sxy0.vx;
            prim->y0 = *(u16*)&block->sxy0.vy;
            prim->x1 = *(u16*)&block->sxy1.vx;
            prim->y1 = *(u16*)&block->sxy1.vy;
            prim->x2 = *(u16*)&block->sxy2.vx;
            prim->y2 = *(u16*)&block->sxy2.vy;
            prim->x3 = *(u16*)&block->sxy3.vx;
            prim->y3 = *(u16*)&block->sxy3.vy;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x118;
}

/// State 0 resets the coordinate frame's rotation to identity and starts the
/// colour ramp at 0xA0. State 1 steps the ramps while no event is running
/// (holding the tick otherwise), calls `func_shelter_b2_pod_bottom_8017EEAC`
/// for indices 0-2, then draws three arcs stacked up the frame's Y axis and a
/// fade quad in the ramp colour. The work is released once the ramp reaches 8
/// or an event of state 4 or above starts.
void func_shelter_b2_pod_bottom_8017EC78(Task* task)
{
    RoomEffWork*   work;
    GsCOORDINATE2* coord;
    GpMtxWords*    rot;
    u16            tick;
    u8             rgb[3];

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    if (Gp_State1C->eventState < 4) {
        coord->flg     = 0;
        tick           = work->field_22;
        work->field_22 = tick + 1;
        switch (task->state) {
            case 0:
                rot            = (GpMtxWords*)&coord->coord;
                rot->w0        = 0x1000;
                rot->w1        = 0;
                rot->w2        = 0x1000;
                rot->w3        = 0;
                rot->h4        = 0x1000;
                work->field_24 = 0xA0;
                task->state++;
                return;
            case 1:
                if ((s16)work->field_24 < 9) {
                    break;
                }
                if (Gp_State1C->eventState == 0) {
                    work->field_24 -= 8;
                    work->field_26 += 0x80;
                    work->field_28 -= 0x20;
                    work->field_2A += 0x20;
                } else {
                    work->field_22 = tick;
                }
                func_shelter_b2_pod_bottom_8017EEAC(work, coord, 0);
                func_shelter_b2_pod_bottom_8017EEAC(work, coord, 1);
                func_shelter_b2_pod_bottom_8017EEAC(work, coord, 2);
                rgb[0] = rgb[1]    = work->field_24;
                rgb[2]             = (s16)work->field_24 * 3 / 2;
                coord->workm.t[1] -= (s16)work->field_22 * 0x30;
                Gp_DrawArc(coord, (s16)(work->field_22 << 6), 0x100, rgb);
                coord->workm.t[1] -= (s16)work->field_22 * 0x30;
                Gp_DrawArc(coord, (s16)(work->field_22 << 7), 0x100, rgb);
                coord->workm.t[1] -= (s16)work->field_22 * 0x30;
                Gp_DrawArc(coord, (s16)((s16)work->field_22 * 0xC0), 0x100, rgb);
                Gp_DrawFadeQuad(rgb, 1);
                return;
            default:
                return;
        }
    }
    Gp_ReleaseState1CMem(work, task);
}

/// Draws band `arg2` of the effect as sixteen semi-transparent `POLY_FT4`
/// segments. Builds two 16-vertex rings in the XZ plane - one at the frame's
/// origin height, one raised and wider - from the work's ramps plus the row's
/// `D_shelter_b2_pod_bottom_80181C94` offsets, moves them into world space
/// through `coord`, then projects each segment between the rings and picks its
/// texture cell from the row's `D_shelter_b2_pod_bottom_80188790` value and the
/// work's tick.
void func_shelter_b2_pod_bottom_8017EEAC(RoomEffWork* work, GsCOORDINATE2* coord, s32 arg2)
{
    void**         scratch;
    u8*            head;
    GpBandScratch* block;
    SVECTOR*       op;
    POLY_FT4*      prim;
    _RingScale*    row;
    s32            i;
    s32            next;
    s32            ang;
    s32            u;
    u16            idx;
    s16            r0;
    s16            r1;
    u16            y;
    u16            f28;

    row      = &D_shelter_b2_pod_bottom_80181C94[arg2];
    f28      = work->field_28;
    r1       = work->field_26;
    y        = f28 + (u16)row->yOff;
    r1      += (u16)row->rInner;
    r0       = r1 + work->field_2A + (u16)row->rExtra;
    scratch  = (void**)G_SCRATCH_HEAD;
    head     = (u8*)*scratch;
    *scratch = head - 0x118;
    block    = (GpBandScratch*)(head - 0x118);
    gte_SetTransMatrix(&GsWSMATRIX);
    for (i = 0; i < 16; i++) {
        ang                = i << 8;
        block->inner[i].vx = (rsin(ang) * r0) >> 12;
        block->inner[i].vy = -y;
        block->inner[i].vz = (rcos(ang) * r0) >> 12;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&block->inner[i]);
        gte_rtv0_real();
        gte_stsv(&block->inner[i]);
        block->inner[i].vx = *(u16*)&block->inner[i].vx + *(u16*)&coord->workm.t[0];
        block->inner[i].vy = *(u16*)&block->inner[i].vy + *(u16*)&coord->workm.t[1];
        block->inner[i].vz = *(u16*)&block->inner[i].vz + *(u16*)&coord->workm.t[2];
        block->outer[i].vx = (rsin(ang) * r1) >> 12;
        op                 = &block->inner[i] + 16;
        op->vy             = 0;
        op->vz             = (rcos(ang) * r1) >> 12;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&block->outer[i]);
        gte_rtv0_real();
        gte_stsv(&block->outer[i]);
        block->outer[i].vx = *(u16*)&block->outer[i].vx + *(u16*)&coord->workm.t[0];
        op->vy             = *(u16*)&op->vy + *(u16*)&coord->workm.t[1];
        op->vz             = *(u16*)&op->vz + *(u16*)&coord->workm.t[2];
    }
    gte_SetRotMatrix(&GsWSMATRIX);
    for (i = 0; i < 16; i++) {
        gte_ldv0(&block->inner[i]);
        gte_rtps_real();
        idx = (D_shelter_b2_pod_bottom_80188790[arg2][i] + (s16)work->field_22) % 6;
        gte_stsxy(&block->sxy0);
        next = (i + 1) & 0xF;
        gte_ldv3(&block->inner[next], &block->outer[i], &block->outer[next]);
        gte_rtpt_real();
        gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
        gte_stflg(&block->flag);
        if (block->flag >= 0) {
            gte_stszotz(&block->otz);
            prim           = (POLY_FT4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyFT4(prim);
            setRGB0(prim, *(u8*)&work->field_24, *(u8*)&work->field_24, *(u8*)&work->field_24);
            setSemiTrans(prim, 1);
            prim->tpage = 0x2A;
            prim->clut  = 0x42C1;
            u           = idx * 0x28;
            setUV4(prim, u, 0x60, u + 0x27, 0x60, u, 0x87, u + 0x27, 0x87);
            prim->x0 = *(u16*)&block->sxy0.vx;
            prim->y0 = *(u16*)&block->sxy0.vy;
            prim->x1 = *(u16*)&block->sxy1.vx;
            prim->y1 = *(u16*)&block->sxy1.vy;
            prim->x2 = *(u16*)&block->sxy2.vx;
            prim->y2 = *(u16*)&block->sxy2.vy;
            prim->x3 = *(u16*)&block->sxy3.vx;
            prim->y3 = *(u16*)&block->sxy3.vy;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x118;
}

/// State 0 resets the coordinate frame's rotation to identity, starts the
/// colour ramp at 0 and the ring radius at 0x80, derives the colour step from
/// `Task::spawnArg1` and picks a random tint row. State 1 grows both while no
/// event is running (holding the tick otherwise) and draws two rings, at the
/// radius and twice it, plus an arc whose sweep cycles with `spawnArg1 % 10`;
/// when `spawnArg1` reaches 0 the colour is set to 0xFF and state 2 begins.
/// State 2 draws the two rings, fading the colour by 0x10 per frame. Each
/// channel is the colour shifted right by the tint row's entry for it, and
/// the row is re-rolled below 18 on every animating frame. The work is
/// released once the fade reaches 0x10 or an event of state 4 or above starts.
void func_shelter_b2_pod_bottom_8017F448(Task* task)
{
    RoomEffWork*   work;
    GsCOORDINATE2* coord;
    GpMtxWords*    rot;
    s32            sum;
    u8             rgb[3];

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    if (Gp_State1C->eventState < 4) {
        work->field_22++;
        switch (task->state) {
            case 0:
                rot            = (GpMtxWords*)&coord->coord;
                rot->w0        = 0x1000;
                rot->w1        = 0;
                rot->w2        = 0x1000;
                rot->w3        = 0;
                rot->h4        = 0x1000;
                coord->flg     = 0;
                work->field_24 = 0;
                work->field_26 = 0x80;
                work->field_2A = 0xC0 / task->spawnArg1;
                task->state    = 1;
                Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
                work->field_20 = (Gp_LcgState >> 16) % 18;
            case 1:
                if (Gp_State1C->eventState != 0) {
                    work->field_22--;
                    rgb[0] = (s16)work->field_24 >> D_shelter_b2_pod_bottom_80181CA8[(s16)work->field_20][0];
                    rgb[1] = (s16)work->field_24 >> D_shelter_b2_pod_bottom_80181CA8[(s16)work->field_20][1];
                    rgb[2] = (s16)work->field_24 >> D_shelter_b2_pod_bottom_80181CA8[(s16)work->field_20][2];
                    Gp_DrawRing(coord, (s16)work->field_26, rgb);
                    Gp_DrawRing(coord, (s16)(work->field_26 * 2), rgb);
                    Gp_DrawArc(coord, (s16)(task->spawnArg1 % 10 * ((s16)work->field_24 << 2)), 0x80, rgb);
                    return;
                }
                sum            = work->field_24 + work->field_2A;
                work->field_24 = sum;
                work->field_26 = sum * 4 + 0x80;
                task->spawnArg1--;
                rgb[0] = (s16)work->field_24 >> D_shelter_b2_pod_bottom_80181CA8[(s16)work->field_20][0];
                rgb[1] = (s16)work->field_24 >> D_shelter_b2_pod_bottom_80181CA8[(s16)work->field_20][1];
                rgb[2] = (s16)work->field_24 >> D_shelter_b2_pod_bottom_80181CA8[(s16)work->field_20][2];
                Gp_DrawRing(coord, (s16)work->field_26, rgb);
                Gp_DrawRing(coord, (s16)(work->field_26 * 2), rgb);
                Gp_DrawArc(coord, (s16)(task->spawnArg1 % 10 * ((s16)work->field_24 << 2)), 0x80, rgb);
                if (task->spawnArg1 == 0) {
                    work->field_24 = 0xFF;
                    task->state    = 2;
                    work->field_28 = 0x300;
                    work->field_2A = 0;
                }
                Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
                work->field_20 = (Gp_LcgState >> 16) % 18;
                return;
            case 2:
                if ((s16)work->field_24 < 0x11) {
                    break;
                }
                rgb[0] = (s16)work->field_24 >> D_shelter_b2_pod_bottom_80181CA8[(s16)work->field_20][0];
                rgb[1] = (s16)work->field_24 >> D_shelter_b2_pod_bottom_80181CA8[(s16)work->field_20][1];
                rgb[2] = (s16)work->field_24 >> D_shelter_b2_pod_bottom_80181CA8[(s16)work->field_20][2];
                Gp_DrawRing(coord, (s16)work->field_26, rgb);
                Gp_DrawRing(coord, (s16)(work->field_26 * 2), rgb);
                if (Gp_State1C->eventState == 0) {
                    Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                    work->field_24 -= 0x10;
                    work->field_20  = (Gp_LcgState >> 16) % 18;
                }
                return;
            default:
                return;
        }
    }
    Gp_ReleaseState1CMem(work, task);
}

/// Projects `coord`'s world position through `GsWSMATRIX` into a scratch block
/// popped from `G_SCRATCH_HEAD` and, when the GTE flag is non-negative, queues
/// twenty gouraud `POLY_G4` wedges fanned about the projected point. The radii
/// are `(s16)arg1 * 64 / otz` (outer) and `(s16)arg1 * 8 / otz` (inner). Each
/// of the first eight steps draws a half-bright wedge at the outer radius and
/// a full-bright one at half of it; four half-bright spikes follow, reaching
/// twice the outer radius between two inner-radius corners. Only the apex at
/// the projected point is coloured, from `rgb`; every rim corner is black.
void func_shelter_b2_pod_bottom_8017F994(GsCOORDINATE2* coord, s32 arg1, u8* rgb)
{
    register _BillboardScratch* block asm("s3");
    register POLY_G4*           prim asm("s2");
    register s32                ang asm("s4");
    register void**             scratch asm("a1");
    register u8*                head asm("a2");
    s32                         t;
    s32                         t2;
    u16                         vz;

    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    {
        register u16 vx asm("v0");
        vx                                          = *(u16*)&coord->workm.t[0];
        ((_BillboardScratch*)(head - 0x1C))->vec.vx = vx;
    }
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x1C;
        block = (_BillboardScratch*)tmp;
    }
    block->vec.vy = *(u16*)&coord->workm.t[1];
    vz            = *(u16*)&coord->workm.t[2];
    *scratch      = block;
    block->vec.vz = vz;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps_real();
    gte_stsxy(&((_BillboardScratch*)(head - 0x1C))->sx);
    gte_stflg(&((_BillboardScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((_BillboardScratch*)(head - 0x1C))->otz);
        USE_REG(head);
        block->rOuter = ((s16)arg1 * 64) / block->otz;
        block->rInner = ((s16)arg1 * 8) / block->otz;

        ang = 0;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0] >> 1, rgb[1] >> 1, rgb[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = *(u16*)&block->sx + ((block->rOuter * rsin(ang)) >> 12);
            t        = ang + 0x100;
            prim->y0 = *(u16*)&block->sy + ((block->rOuter * rcos(ang)) >> 12);
            prim->x1 = *(u16*)&block->sx + ((block->rOuter * rsin(t)) >> 12);
            prim->y1 = *(u16*)&block->sy + ((block->rOuter * rcos(t)) >> 12);
            t2       = ang + 0x200;
            prim->x2 = *(u16*)&block->sx;
            prim->y2 = *(u16*)&block->sy;
            prim->x3 = *(u16*)&block->sx + ((block->rOuter * rsin(t2)) >> 12);
            prim->y3 = *(u16*)&block->sy + ((block->rOuter * rcos(t2)) >> 12);
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = *(u16*)&block->sx + ((block->rOuter * rsin(ang)) >> 13);
            prim->y0 = *(u16*)&block->sy + ((block->rOuter * rcos(ang)) >> 13);
            prim->x1 = *(u16*)&block->sx + ((block->rOuter * rsin(t)) >> 13);
            prim->y1 = *(u16*)&block->sy + ((block->rOuter * rcos(t)) >> 13);
            prim->x2 = *(u16*)&block->sx;
            prim->y2 = *(u16*)&block->sy;
            prim->x3 = *(u16*)&block->sx + ((block->rOuter * rsin(t2)) >> 13);
            prim->y3 = *(u16*)&block->sy + ((block->rOuter * rcos(t2)) >> 13);
            ang      = t2;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);

        ang = 0x200;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0] >> 1, rgb[1] >> 1, rgb[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = *(u16*)&block->sx + ((block->rInner * rsin(ang)) >> 12);
            t2       = ang + 0x400;
            prim->y0 = *(u16*)&block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x1 = *(u16*)&block->sx + ((block->rOuter * rsin(t2)) >> 11);
            prim->y1 = *(u16*)&block->sy + ((block->rOuter * rcos(t2)) >> 11);
            t        = ang + 0x800;
            prim->x2 = *(u16*)&block->sx;
            prim->y2 = *(u16*)&block->sy;
            prim->x3 = *(u16*)&block->sx + ((block->rInner * rsin(t)) >> 12);
            prim->y3 = *(u16*)&block->sy + ((block->rInner * rcos(t)) >> 12);
            ang      = t2;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
}

/// State 0 resets the coordinate frame's rotation to identity, starts the
/// colour ramp at 0 and the size ramp at 0x80, derives the colour step from
/// `Task::spawnArg1`, and fills `D_shelter_b2_pod_bottom_801887F0` with eight
/// angles, one random angle inside each eighth of the circle. State 1 grows
/// both ramps once per frame while no event is running (holding the tick
/// otherwise) and draws the ramp-coloured effect, a ring and an arc that
/// shrinks as `spawnArg1` counts down; when it reaches 0 the colour is set to
/// 0xFF and state 2 begins. State 2 draws the effect, the ring and a blade at
/// each of the eight angles, fading the colour by 0x10 per frame. While
/// animating, `field_20` is re-rolled to a random value below 18 every frame.
/// The work is released once the fade reaches 0x10 or an event of state 4 or
/// above starts.
void func_shelter_b2_pod_bottom_8018016C(Task* task)
{
    RoomEffWork*   work;
    GsCOORDINATE2* coord;
    GpMtxWords*    rot;
    s32            i;
    s32            sum;
    u8             rgb[3];

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    if (Gp_State1C->eventState < 4) {
        work->field_22++;
        switch (task->state) {
            case 0:
                rot            = (GpMtxWords*)&coord->coord;
                rot->w0        = 0x1000;
                rot->w1        = 0;
                rot->w2        = 0x1000;
                rot->w3        = 0;
                rot->h4        = 0x1000;
                coord->flg     = 0;
                work->field_24 = 0;
                work->field_26 = 0x80;
                work->field_2A = 0xC0 / task->spawnArg1;
                task->state    = 1;
                for (i = 0; i < 8; i++) {
                    Gp_LcgState                         = Gp_LcgState * 5 + 0x71357911;
                    D_shelter_b2_pod_bottom_801887F0[i] = (i << 9) + ((Gp_LcgState >> 16) & 0x1FF);
                }
            case 1:
                if (Gp_State1C->eventState != 0) {
                    work->field_22--;
                    rgb[0] = work->field_24;
                    rgb[1] = work->field_24;
                    rgb[2] = work->field_24 >> 1;
                    func_shelter_b2_pod_bottom_8017F994(coord, (s16)(work->field_26 * 2), rgb);
                    Gp_DrawRing(coord, (s16)(work->field_26 * 4), rgb);
                    Gp_DrawArc(coord, (s16)(task->spawnArg1 * (s16)work->field_2A * 16), 0x80, rgb);
                    return;
                }
                sum            = work->field_24 + work->field_2A;
                work->field_24 = sum;
                work->field_26 = sum * 4 + 0x80;
                task->spawnArg1--;
                rgb[0] = work->field_24;
                rgb[1] = work->field_24;
                rgb[2] = work->field_24 >> 1;
                func_shelter_b2_pod_bottom_8017F994(coord, (s16)(work->field_26 * 2), rgb);
                Gp_DrawRing(coord, (s16)(work->field_26 * 4), rgb);
                Gp_DrawArc(coord, (s16)(task->spawnArg1 * (s16)work->field_2A * 16), 0x80, rgb);
                if (task->spawnArg1 == 0) {
                    work->field_24 = 0xFF;
                    task->state    = 2;
                    work->field_28 = 0x300;
                    work->field_2A = 0;
                }
                Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
                work->field_20 = (Gp_LcgState >> 16) % 18;
                return;
            case 2:
                if ((s16)work->field_24 < 0x11) {
                    break;
                }
                rgb[0] = work->field_24;
                rgb[1] = work->field_24;
                rgb[2] = work->field_24 >> 1;
                func_shelter_b2_pod_bottom_8017F994(coord, (s16)(work->field_26 * 2), rgb);
                Gp_DrawRing(coord, (s16)(work->field_26 * 4), rgb);
                for (i = 0; i < 8; i++) {
                    func_shelter_b2_pod_bottom_801805A0(coord, (s16)(work->field_26 * 2), D_shelter_b2_pod_bottom_801887F0[i], rgb);
                }
                if (Gp_State1C->eventState == 0) {
                    Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                    work->field_24 -= 0x10;
                    work->field_20  = (Gp_LcgState >> 16) % 18;
                }
                return;
            default:
                return;
        }
    }
    Gp_ReleaseState1CMem(work, task);
}

/// Draws one Gouraud triangle as a fan blade about `arg2`. `arg0`'s world
/// position is projected through `GsWSMATRIX` into a scratch block popped from
/// `G_SCRATCH_HEAD`; the apex sits on that point in `rgb`, and the two black
/// outer corners sit at angles `arg2 - 0x20` and `arg2 + 0x20`, `arg1` scaled
/// down by the projected depth away. A negative GTE flag drops the triangle.
void func_shelter_b2_pod_bottom_801805A0(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, u8* rgb)
{
    void**         scratch;
    u8*            head;
    GpRingScratch* block;
    SVECTOR*       vec;
    POLY_G3*       prim;
    s32            ang;
    s32            ang2;
    u16            vz;

    scratch                                 = (void**)G_SCRATCH_HEAD;
    head                                    = *scratch;
    ((GpRingScratch*)(head - 0x18))->vec.vx = *(u16*)&arg0->workm.t[0];
    block                                   = (GpRingScratch*)(head - 0x18);
    block->vec.vy                           = *(u16*)&arg0->workm.t[1];
    vz                                      = *(u16*)&arg0->workm.t[2];
    *scratch                                = block;
    block->vec.vz                           = vz;
    vec                                     = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps_real();
    gte_stsxy(&((GpRingScratch*)(head - 0x18))->sx);
    gte_stflg(&((GpRingScratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        prim           = (POLY_G3*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setPolyG3(prim);
        gte_stszotz(&((GpRingScratch*)(head - 0x18))->otz);
        setRGB0(prim, rgb[0], rgb[1], rgb[2]);
        setRGB1(prim, 0, 0, 0);
        setRGB2(prim, 0, 0, 0);
        block->step = ((s16)arg1 * 128) / block->otz;
        ang         = (s16)arg2;
        ang2        = ang - 0x20;
        prim->x0    = *(u16*)&block->sx;
        prim->y0    = *(u16*)&block->sy;
        prim->x1    = *(u16*)&block->sx + ((block->step * rsin(ang2)) >> 12);
        prim->y1    = *(u16*)&block->sy + ((block->step * rcos(ang2)) >> 12);
        ang        += 0x20;
        prim->x2    = *(u16*)&block->sx + ((block->step * rsin(ang)) >> 12);
        prim->y2    = *(u16*)&block->sy + ((block->step * rcos(ang)) >> 12);
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
        Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x18;
}

INCLUDE_RODATA("rooms/nonmatchings/shelter_b2_pod_bottom/shelter_b2_pod_bottom_2", D_shelter_b2_pod_bottom_8017D5EC);
