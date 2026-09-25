#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "rooms/room.h"
#include "rooms/room_common.h"

/// 0x120-byte scratch block `func_shelter_b2_pod_bottom_80180A4C` takes from
/// `G_SCRATCH_HEAD`: the 32 rotated ring points and the disc centre, the
/// quad's depth and GTE flag, and the four projected corners of one quad.
typedef struct {
    SVECTOR pts[32];
    SVECTOR center;
    s32     otz;
    s32     flag;
    DVECTOR sxy0;
    DVECTOR sxy1;
    DVECTOR sxy2;
    DVECTOR sxy3;
} _ShelterB2PodBottomRingScratch;

void func_shelter_b2_pod_bottom_8017DECC(GsCOORDINATE2* arg0, u16 arg1, s16 arg2, s16 arg3);
void func_shelter_b2_pod_bottom_8017E334(GsCOORDINATE2* arg0, u16 arg1, s16 arg2, s16 arg3);
void func_shelter_b2_pod_bottom_8017E788(GsCOORDINATE2* coord, s16 arg1, s16 arg2);
void func_shelter_b2_pod_bottom_8017EEAC(RoomEffWork* work, GsCOORDINATE2* coord, s32 arg2);
void func_shelter_b2_pod_bottom_8018101C(GsCOORDINATE2* coord, s16 size, u16 color, u16 scale);

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

/// Per-frame handler for one animated sprite effect, drawn by
/// `func_shelter_b2_pod_bottom_8017DECC` (state 1) or
/// `func_shelter_b2_pod_bottom_8017E334` (state 2). Its first frame unpacks
/// `spawnArg1`: the low 12 bits are the sprite size, bits 12..14 the frames per
/// animation cell (1 when zero), bits 28..30 are kept as the drawer's clut
/// selector, and the sign bit picks the second drawer. When the work block
/// arrives without a velocity, bits 24..27 choose how one is rolled from
/// `Gp_LcgState` (0 leaves it still) and it is scaled to a speed from bits
/// 16..23 (0x40 when zero). Each later frame draws the current cell, moves the
/// coordinate by the velocity and bends its Y component, then frees the effect
/// after the drawer's last cell (12 or 10). While the player is in an event it
/// only draws, and frees once the event state reaches 4.
void func_shelter_b2_pod_bottom_8017D850(Task* task)
{
    RoomEffWork*   work;
    GsCOORDINATE2* coord;
    SVECTOR*       vec;
    s32            step;
    s32            level;

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    if (Gp_State1C->eventState != 0) {
        if (task->state < 2) {
            func_shelter_b2_pod_bottom_8017DECC(coord, work->field_20 | work->field_18, work->field_24, work->field_26);
        } else {
            func_shelter_b2_pod_bottom_8017E334(coord, work->field_20 | work->field_18, work->field_24, work->field_26);
        }
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
        return;
    }
    work->field_22++;
    switch (task->state) {
        case 0:
            work->field_24 = task->spawnArg1 & 0xFFF;
            Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
            work->field_26 = ((u32)Gp_LcgState >> 16) & 0xFFF;
            if (task->spawnArg1 & 0xF000) {
                step = (task->spawnArg1 >> 12) & 7;
            } else {
                step = 1;
            }
            work->field_28 = step;
            work->field_22 = 0;
            task->state    = 1;
            task->state    = task->spawnArg1 < 0 ? 2 : 1;
            work->field_18 = (task->spawnArg1 >> 16) & 0x7000;
            if (((u16)work->field_10.vx | (u16)work->field_10.vy | (u16)work->field_10.vz) == 0) {
                if (task->spawnArg1 & 0xFF0000) {
                    level = (task->spawnArg1 >> 16) & 0xFF;
                } else {
                    level = 0x40;
                }
                work->field_2A = level;
                switch ((task->spawnArg1 >> 24) & 0xF) {
                    case 0:
                        work->field_2A = 0;
                        break;
                    case 1:
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vy = 0xFFC0 - (((u32)Gp_LcgState >> 16) & 0x7F);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                    case 2:
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vy = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                    case 3:
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vx = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vy = -(((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vz = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        break;
                    case 5:
                        work->field_10.vx = work->field_18;
                        work->field_10.vy = work->field_1A;
                        work->field_10.vz = work->field_1C;
                        break;
                    case 6:
                        work->field_10.vy = 0;
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                }
                vec = &work->field_10;
                VectorNormalSS(vec, vec);
                gte_lddp(work->field_2A);
                gte_ldsv(vec);
                gte_gpf12();
                gte_stsv(vec);
            } else {
                work->field_2A = 0x40;
            }
            break;
        case 1:
            func_shelter_b2_pod_bottom_8017DECC(coord, work->field_20 | work->field_18, work->field_24, work->field_26);
            if ((s16)work->field_2A != 0) {
                coord->coord.t[0] += (s16)work->field_10.vx;
                coord->coord.t[1] += (s16)work->field_10.vy;
                coord->coord.t[2] += (s16)work->field_10.vz;
                coord->flg         = 0;
                if (((task->spawnArg1 >> 24) & 0xF) == 7) {
                    work->field_10.vy += (s16)work->field_22 / 10;
                } else {
                    work->field_10.vy -= 2;
                }
            }
            if (((s16)work->field_22 % (s16)work->field_28) == 0) {
                work->field_20++;
                if ((s16)work->field_20 >= 12) {
                    Gp_ReleaseState1CMem(work, task);
                }
            }
            break;
        case 2:
            func_shelter_b2_pod_bottom_8017E334(coord, work->field_20 | work->field_18, work->field_24, work->field_26);
            if ((s16)work->field_2A != 0) {
                coord->coord.t[0] += (s16)work->field_10.vx;
                coord->coord.t[1] += (s16)work->field_10.vy;
                coord->coord.t[2] += (s16)work->field_10.vz;
                coord->flg         = 0;
                if (((task->spawnArg1 >> 24) & 0xF) == 7) {
                    work->field_10.vy += (s16)work->field_22 / 10;
                } else {
                    work->field_10.vy -= 1;
                }
            }
            if (((s16)work->field_22 % (s16)work->field_28) == 0) {
                work->field_20++;
                if ((s16)work->field_20 >= 10) {
                    Gp_ReleaseState1CMem(work, task);
                }
            }
            break;
    }
}

/// Draws a camera-facing sprite at `arg0`'s world position, the same way as
/// `func_shelter_b2_pod_bottom_8017E334` but from texture page 0x2B, with each
/// 48x48 cell's rows starting 0x90 lines higher. Here the bits of `arg1` above
/// the low 12 choose the CLUT: 0 and 1 select CLUT row 0x10E or 0x10F, at a
/// column taken from the low six bits of the cell index, and anything larger
/// selects the fixed CLUT 0x428F.
void func_shelter_b2_pod_bottom_8017DECC(GsCOORDINATE2* arg0, u16 arg1, s16 arg2, s16 arg3)
{
    u8*                head;
    RoomDraw19Scratch* block;
    POLY_FT4*          prim;
    u16                col;
    u16                row;
    s32                u0;
    s32                v0;
    s32                ang;
    s32                ang2;
    u16                bank;
    u32                idx;

    head                                        = *(u8**)G_SCRATCH_HEAD;
    ((RoomDraw19Scratch*)(head - 0x1C))->vec.vx = arg0->workm.t[0];
    *(void**)G_SCRATCH_HEAD                     = head - 0x1C;
    block                                       = *(RoomDraw19Scratch**)G_SCRATCH_HEAD;
    block->vec.vy                               = arg0->workm.t[1];
    block->vec.vz                               = arg0->workm.t[2];
    idx                                         = arg1;
    idx                                        &= 0xFFF;
    bank                                        = arg1 >> 12;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(block);
    gte_rtps();
    gte_stsxy(&((RoomDraw19Scratch*)(head - 0x1C))->sx);
    gte_stflg(&((RoomDraw19Scratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((RoomDraw19Scratch*)(head - 0x1C))->otz);
        block->otz++;
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x2B;
        if (bank >= 2) {
            prim->clut = 0x428F;
        } else {
            prim->clut = ((bank + 0x10E) << 6) | (idx & 0x3F);
        }
        col = (u16)idx % 5;
        row = (u16)idx / 5;
        ang = arg3;
        u0  = col * 0x30;
        v0  = row * 0x30;
        setUV4(prim, u0, v0 + 0x70, u0 + 0x2F, v0 + 0x70, u0, v0 - 0x61, u0 + 0x2F, v0 - 0x61);
        block->dx = (((arg2 * 47) / block->otz) * rsin(ang)) >> 12;
        block->dy = (((arg2 * 47) / block->otz) * rcos(ang)) >> 12;
        prim->x0  = *(u16*)&block->sx + *(u16*)&block->dx;
        prim->x3  = *(u16*)&block->sx - *(u16*)&block->dx;
        prim->y0  = *(u16*)&block->sy - *(u16*)&block->dy;
        prim->y3  = *(u16*)&block->sy + *(u16*)&block->dy;
        ang2      = ang + 0x400;
        block->dx = (((arg2 * 47) / block->otz) * rsin(ang2)) >> 12;
        block->dy = (((arg2 * 47) / block->otz) * rcos(ang2)) >> 12;
        prim->x1  = *(u16*)&block->sx + *(u16*)&block->dx;
        prim->x2  = *(u16*)&block->sx - *(u16*)&block->dx;
        prim->y1  = *(u16*)&block->sy - *(u16*)&block->dy;
        prim->y2  = *(u16*)&block->sy + *(u16*)&block->dy;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    *(u8**)G_SCRATCH_HEAD += 0x1C;
}

/// Draws a camera-facing sprite at `arg0`'s world position: the point is
/// projected through `GsWSMATRIX` into a scratch block popped from
/// `G_SCRATCH_HEAD` and, when the GTE flag is non-negative, one `POLY_FT4` is
/// queued one depth step behind it. Its corners sit on two perpendicular screen
/// radii at angles `arg3` and `arg3 + 0x400`, of length `arg2 * 47` divided by
/// the depth. The low 12 bits of `arg1` pick a 48x48 cell of a five-column
/// texture grid, and the bits above them select the alternate CLUT.
void func_shelter_b2_pod_bottom_8017E334(GsCOORDINATE2* arg0, u16 arg1, s16 arg2, s16 arg3)
{
    u8*                head;
    RoomDraw19Scratch* block;
    POLY_FT4*          prim;
    u16                col;
    u16                row;
    s32                u0;
    s32                v0;
    s32                ang;
    s32                ang2;
    u16                bank;

    bank                                        = arg1 >> 12;
    arg1                                       &= 0xFFF;
    head                                        = *(u8**)G_SCRATCH_HEAD;
    ((RoomDraw19Scratch*)(head - 0x1C))->vec.vx = arg0->workm.t[0];
    *(void**)G_SCRATCH_HEAD                     = head - 0x1C;
    block                                       = *(RoomDraw19Scratch**)G_SCRATCH_HEAD;
    block->vec.vy                               = arg0->workm.t[1];
    block->vec.vz                               = arg0->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(block);
    gte_rtps();
    gte_stsxy(&((RoomDraw19Scratch*)(head - 0x1C))->sx);
    gte_stflg(&((RoomDraw19Scratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((RoomDraw19Scratch*)(head - 0x1C))->otz);
        block->otz++;
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x2C;
        prim->clut  = bank ? 0x428F : 0x43D0;
        col         = arg1 % 5;
        row         = arg1 / 5;
        ang         = arg3;
        u0          = col * 0x30;
        v0          = row * 0x30;
        setUV4(prim, u0, v0 - 0x80, u0 + 0x2F, v0 - 0x80, u0, v0 - 0x51, u0 + 0x2F, v0 - 0x51);
        block->dx = (((arg2 * 47) / block->otz) * rsin(ang)) >> 12;
        block->dy = (((arg2 * 47) / block->otz) * rcos(ang)) >> 12;
        prim->x0  = *(u16*)&block->sx + *(u16*)&block->dx;
        prim->x3  = *(u16*)&block->sx - *(u16*)&block->dx;
        prim->y0  = *(u16*)&block->sy - *(u16*)&block->dy;
        prim->y3  = *(u16*)&block->sy + *(u16*)&block->dy;
        ang2      = ang + 0x400;
        block->dx = (((arg2 * 47) / block->otz) * rsin(ang2)) >> 12;
        block->dy = (((arg2 * 47) / block->otz) * rcos(ang2)) >> 12;
        prim->x1  = *(u16*)&block->sx + *(u16*)&block->dx;
        prim->x2  = *(u16*)&block->sx - *(u16*)&block->dx;
        prim->y1  = *(u16*)&block->sy - *(u16*)&block->dy;
        prim->y2  = *(u16*)&block->sy + *(u16*)&block->dy;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    *(u8**)G_SCRATCH_HEAD += 0x1C;
}

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
        gte_rtv0();
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
        gte_rtv0();
        gte_stsv(&block->outer[i]);
        block->outer[i].vx = *(u16*)&block->outer[i].vx + *(u16*)&coord->workm.t[0];
        op->vy             = *(u16*)&op->vy + *(u16*)&coord->workm.t[1];
        op->vz             = *(u16*)&op->vz + *(u16*)&coord->workm.t[2];
    }
    gte_SetRotMatrix(&GsWSMATRIX);
    for (i = 0; i < 16; i++) {
        gte_ldv0(&block->inner[i]);
        gte_rtps();
        gte_stsxy(&block->sxy0);
        next = (i + 1) & 0xF;
        gte_ldv3(&block->inner[next], &block->outer[i], &block->outer[next]);
        gte_rtpt();
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
        gte_rtv0();
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
        gte_rtv0();
        gte_stsv(&block->outer[i]);
        block->outer[i].vx = *(u16*)&block->outer[i].vx + *(u16*)&coord->workm.t[0];
        op->vy             = *(u16*)&op->vy + *(u16*)&coord->workm.t[1];
        op->vz             = *(u16*)&op->vz + *(u16*)&coord->workm.t[2];
    }
    gte_SetRotMatrix(&GsWSMATRIX);
    for (i = 0; i < 16; i++) {
        gte_ldv0(&block->inner[i]);
        gte_rtps();
        idx = (D_shelter_b2_pod_bottom_80188790[arg2][i] + (s16)work->field_22) % 6;
        gte_stsxy(&block->sxy0);
        next = (i + 1) & 0xF;
        gte_ldv3(&block->inner[next], &block->outer[i], &block->outer[next]);
        gte_rtpt();
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
    register RoomDraw07Scratch* block asm("s3");
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
        ((RoomDraw07Scratch*)(head - 0x1C))->vec.vx = vx;
    }
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x1C;
        block = (RoomDraw07Scratch*)tmp;
    }
    block->vec.vy = *(u16*)&coord->workm.t[1];
    vz            = *(u16*)&coord->workm.t[2];
    *scratch      = block;
    block->vec.vz = vz;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&((RoomDraw07Scratch*)(head - 0x1C))->sx);
    gte_stflg(&((RoomDraw07Scratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((RoomDraw07Scratch*)(head - 0x1C))->otz);
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
    gte_rtps();
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

/// Per-frame driver of a rising sprite effect task.
///
/// On its first frame it seeds the rise speed (`field_10.vy`, 0x10-0x4F from
/// the LCG, negated when bit 16 of `Task::spawnArg1` is set), a random spin
/// angle (`field_24`) and the sprite radius (`field_26`, the low 12 bits of
/// `Task::spawnArg1`). While no event runs, every frame moves the coordinate
/// frame along Y by the speed and advances the animation frame (`field_20`)
/// every fourth tick, releasing the effect after frame 7. During an event of
/// state 1-3 it keeps drawing without moving or animating; state 4 or above
/// releases it. Each draw picks one of six sprite CLUTs at random.
void func_shelter_b2_pod_bottom_80180898(Task* task)
{
    RoomEffWork*   work;
    GsCOORDINATE2* coord;

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
            return;
        }
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        Gp_DrawFxQuad(coord, work->field_20, work->field_26, work->field_24 | (((Gp_LcgState >> 16) % 6) << 12));
        return;
    }
    work->field_22++;
    if (task->state == 0) {
        work->field_10.vx = 0;
        work->field_10.vz = 0;
        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
        work->field_10.vy = ((Gp_LcgState >> 16) & 0x3F) + 0x10;
        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
        work->field_24    = (Gp_LcgState >> 16) & 0xFFF;
        work->field_26    = task->spawnArg1 & 0xFFF;
        if (task->spawnArg1 & 0x10000) {
            work->field_10.vy = -work->field_10.vy;
        }
        task->state = 1;
    }
    coord->coord.t[1] += work->field_10.vy;
    coord->flg         = 0;
    if (!(work->field_22 & 3)) {
        work->field_20++;
    }
    if ((s16)work->field_20 < 8) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        Gp_DrawFxQuad(coord, work->field_20, work->field_26, work->field_24 | (((Gp_LcgState >> 16) % 6) << 12));
        return;
    }
    Gp_ReleaseState1CMem(work, task);
}

/// Draws a flat white disc of radius `radius` in `coord`'s local XY plane,
/// centred on `center` (the frame's origin when it is NULL). 32 rim points are
/// rotated by `coord->workm`, the centre is projected through the full
/// `workm`, and the disc is queued as 16 `POLY_F4` fans, each joining the
/// centre to three consecutive rim points; any piece with a negative GTE flag
/// is dropped.
void func_shelter_b2_pod_bottom_80180A4C(GsCOORDINATE2* coord, s16 radius, SVECTOR* center)
{
    void**                          scratch;
    _ShelterB2PodBottomRingScratch* block;
    POLY_F4*                        prim;
    s32                             i;
    s32                             ang;
    u8*                             head;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = (u8*)*scratch - 0x120;
    *scratch = head;
    block    = (_ShelterB2PodBottomRingScratch*)head;
    gte_SetTransMatrix(&coord->workm);
    if (center != NULL) {
        for (i = 0; i < 32; i++) {
            block->pts[i].vx = *(u16*)&center->vx + ((rsin(i << 7) * radius) >> 12);
            block->pts[i].vy = *(u16*)&center->vy + ((rcos(i << 7) * radius) >> 12);
            block->pts[i].vz = center->vz;
            gte_SetRotMatrix(&coord->workm);
            gte_ldv0(&block->pts[i]);
            gte_rtv0();
            gte_stsv(&block->pts[i]);
        }
        block->center.vx = center->vx;
        block->center.vy = center->vy;
        block->center.vz = center->vz;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&block->center);
        gte_rtv0();
        gte_stsv(&block->center);
    } else {
        for (i = 0; i < 32; i++) {
            ang              = i << 7;
            block->pts[i].vx = (rsin(ang) * radius) >> 12;
            block->pts[i].vy = (rcos(ang) * radius) >> 12;
            block->pts[i].vz = 0;
            gte_SetRotMatrix(&coord->workm);
            gte_ldv0(&block->pts[i]);
            gte_rtv0();
            gte_stsv(&block->pts[i]);
        }
        block->center.vx = 0;
        block->center.vy = 0;
        block->center.vz = 0;
    }
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&block->center);
    gte_rtps();
    gte_stsxy(&block->sxy0);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        for (i = 0; i < 32; i += 2) {
            gte_ldv3(&block->pts[i], &block->pts[(i + 2) & 0x1F], &block->pts[i + 1]);
            gte_rtpt();
            gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
            gte_stflg(&block->flag);
            if (block->flag >= 0) {
                gte_stszotz(&block->otz);
                prim           = (POLY_F4*)gGpuPrimCursor;
                gGpuPrimCursor = prim + 1;
                setPolyF4(prim);
                setRGB0(prim, 0xFF, 0xFF, 0xFF);
                prim->x0 = *(u16*)&block->sxy0.vx;
                prim->y0 = *(u16*)&block->sxy0.vy;
                prim->x1 = *(u16*)&block->sxy1.vx;
                prim->y1 = *(u16*)&block->sxy1.vy;
                prim->x2 = *(u16*)&block->sxy2.vx;
                prim->y2 = *(u16*)&block->sxy2.vy;
                prim->x3 = *(u16*)&block->sxy3.vx;
                prim->y3 = *(u16*)&block->sxy3.vy;
                addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                        prim);
            }
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x120;
}

void func_shelter_b2_pod_bottom_80180F10(Task* arg0)
{
    RoomEffWork*   work;
    GsCOORDINATE2* coord;
    u32            rnd;

    work  = arg0->spawnArg2;
    coord = ((TmdObject*)arg0->extra)->coords;
    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState < 4) {
            rnd         = Gp_LcgState * 5 + 0x71357911;
            Gp_LcgState = rnd;
            func_shelter_b2_pod_bottom_8018101C(coord, 0x100, (rnd >> 16) & 0x777, 0x10);
            return;
        }
        Gp_ReleaseState1CMem(work, arg0);
        return;
    }
    work->field_22++;
    coord->coord.t[1] += arg0->spawnArg1;
    coord->flg         = 0;
    if ((s16)work->field_22 < 8) {
        func_shelter_b2_pod_bottom_8018101C(coord, 0x100, 0xCCC, 0x10);
        return;
    }
    func_shelter_b2_pod_bottom_8018101C(coord, 0x100, 0xCCC, (u16)((0x10 - (s16)work->field_22) * 2));
    if ((s16)work->field_22 >= 0x10) {
        Gp_ReleaseState1CMem(work, arg0);
    }
}

/// Queues a beam of gouraud `POLY_G4` wedges along `coord`'s local up axis:
/// the tip sits `size * 16` units above the coordinate's world position, and
/// both ends are projected. Two passes widen the wedge radii (`size * 64` and
/// `size * 128` over each end's depth) and draw a fan at each end plus a
/// connecting quad. `color` packs `[r][g][b]` nibbles scaled by `scale`, with
/// `gDisplayState.animFrame & 1` adding a 16-unit flicker to every channel.
void func_shelter_b2_pod_bottom_8018101C(GsCOORDINATE2* coord, s16 size, u16 color, u16 scale)
{
    void**           scratch;
    u8*              head;
    RoomBeamScratch* block;
    POLY_G4*         prim;
    s32              pass;
    u8               r;
    u8               g;
    u8               b;
    s32              limit;
    s32              angStart;
    s32              scaled;
    s32              ang;
    s32              next;
    s32              mid;
    s32              blend;

    scratch       = (void**)G_SCRATCH_HEAD;
    head          = *scratch;
    block         = (RoomBeamScratch*)(*scratch = head - 0x2C);
    block->tip.vy = -(size << 4);
    block->tip.vx = 0;
    block->tip.vz = 0;
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&block->tip);
    gte_rtv0();
    gte_stsv(&block->tip);
    block->base.vx = coord->workm.t[0];
    block->base.vy = coord->workm.t[1];
    block->base.vz = coord->workm.t[2];
    block->tip.vx += block->base.vx;
    block->tip.vy += block->base.vy;
    block->tip.vz += block->base.vz;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->base);
    gte_rtps();
    gte_stsxy(&block->sx0);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz0);
        gte_ldv0(&block->tip);
        gte_rtps();
        gte_stsxy(&block->sx1);
        gte_stflg(&block->flag);
        gte_stszotz(&block->otz1);
        if (block->flag >= 0) {
            blend = (*(u8*)&gDisplayState.animFrame & 1) << 4;
            r     = scale * ((color >> 8) & 0xF) + blend;
            g     = scale * ((color >> 4) & 0xF) + blend;
            b     = scale * (color & 0xF) + blend;
            for (pass = 1; pass < 3; pass++) {
                scaled    = size * (pass << 6);
                block->r0 = scaled / block->otz0;
                block->r1 = scaled / block->otz1;
                ang       = (s16)ratan2((s16)block->sy1 - (s16)block->sy0, (s16)block->sx0 - (s16)block->sx1);
                if (ang < ang + 0x800) {
                    angStart = ang;
                    limit    = ang + 0x800;
                    do {
                        prim           = (POLY_G4*)gGpuPrimCursor;
                        gGpuPrimCursor = prim + 1;
                        setPolyG4(prim);
                        setRGB0(prim, 0, 0, 0);
                        setRGB1(prim, 0, 0, 0);
                        setRGB2(prim, r, g, b);
                        setRGB3(prim, 0, 0, 0);
                        prim->x0 = block->sx1 + ((block->r1 * rsin(ang + 0x800)) >> 12);
                        prim->y0 = block->sy1 + ((block->r1 * rcos(ang + 0x800)) >> 12);
                        prim->x1 = block->sx1 + ((block->r1 * rsin(ang + 0xA00)) >> 12);
                        prim->y1 = block->sy1 + ((block->r1 * rcos(ang + 0xA00)) >> 12);
                        prim->x2 = block->sx1;
                        prim->y2 = block->sy1;
                        prim->x3 = block->sx1 + ((block->r1 * rsin(ang + 0xC00)) >> 12);
                        prim->y3 = block->sy1 + ((block->r1 * rcos(ang + 0xC00)) >> 12);
                        addPrim((u_long*)(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                                prim);
                        Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);

                        prim           = (POLY_G4*)gGpuPrimCursor;
                        gGpuPrimCursor = prim + 1;
                        setPolyG4(prim);
                        setRGB0(prim, 0, 0, 0);
                        setRGB1(prim, 0, 0, 0);
                        setRGB2(prim, r, g, b);
                        setRGB3(prim, 0, 0, 0);
                        prim->x0 = block->sx0 + ((block->r0 * rsin(ang)) >> 12);
                        prim->y0 = block->sy0 + ((block->r0 * rcos(ang)) >> 12);
                        prim->x1 = block->sx0 + ((block->r0 * rsin(ang + 0x200)) >> 12);
                        prim->y1 = block->sy0 + ((block->r0 * rcos(ang + 0x200)) >> 12);
                        next     = ang + 0x400;
                        prim->x2 = block->sx0;
                        prim->y2 = block->sy0;
                        prim->x3 = block->sx0 + ((block->r0 * rsin(next)) >> 12);
                        prim->y3 = block->sy0 + ((block->r0 * rcos(next)) >> 12);
                        addPrim((u_long*)(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                                prim);
                        Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

                        prim           = (POLY_G4*)gGpuPrimCursor;
                        mid            = angStart + (ang - angStart) * 2;
                        gGpuPrimCursor = prim + 1;
                        setPolyG4(prim);
                        setRGB0(prim, 0, 0, 0);
                        setRGB1(prim, 0, 0, 0);
                        setRGB2(prim, r, g, b);
                        setRGB3(prim, r, g, b);
                        prim->x0 = block->sx0 + ((block->r0 * rsin(mid)) >> 12);
                        prim->y0 = block->sy0 + ((block->r0 * rcos(mid)) >> 12);
                        prim->x1 = block->sx1 + ((block->r1 * rsin(mid)) >> 12);
                        prim->y1 = block->sy1 + ((block->r1 * rcos(mid)) >> 12);
                        prim->x2 = block->sx0;
                        prim->y2 = block->sy0;
                        prim->x3 = block->sx1;
                        prim->y3 = block->sy1;
                        addPrim((u_long*)(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                                prim);
                        Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);
                        ang = next;
                    } while (ang < limit);
                }
            }
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x2C;
}

void func_shelter_b2_pod_bottom_80181940(Task* arg0)
{
    GsCOORDINATE2* coord;
    u32            rnd;

    if (Gp_State1C->eventState == 0) {
        rnd         = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState = rnd;
        coord       = &((TmdObject*)arg0->extra)->coords[(u16)((rnd >> 16) % 18) + 2];
        Gp_SpawnEff(0x600E0, coord, 0x10300, 0);
        rnd         = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState = rnd;
        if ((rnd >> 16) & 1) {
            Gp_SpawnEff(0x600E1, coord, 0x10300, 0);
        }
    }
}

void func_shelter_b2_pod_bottom_80181A48(Task* arg0)
{
    GsCOORDINATE2* coord;
    u32            rnd;

    if (Gp_State1C->eventState == 0) {
        rnd         = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState = rnd;
        coord       = &((TmdObject*)arg0->extra)->coords[(u16)((rnd >> 16) % 18) + 2];
        Gp_SpawnEff(0x600F4, coord, 0x8600, 0);
        rnd         = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState = rnd;
        if (!((rnd >> 16) & 1)) {
            Gp_SpawnEff(0x600F4, coord, 0x8600, 0);
        }
    }
}

void func_shelter_b2_pod_bottom_80181B48(Task* arg0)
{
    RoomEffWork*   work;
    GsCOORDINATE2* coord;
    s16            y;

    work  = arg0->spawnArg2;
    coord = ((TmdObject*)arg0->extra)->coords;
    if (Gp_State1C->eventState >= 4) {
        Gp_ReleaseState1CMem(work, arg0);
        return;
    }
    work->field_22++;
    switch (arg0->state) {
        case 0:
            Gfx_RotMatrixX(&coord->coord, arg0->spawnArg1, 0);
            work->field_24 = 0xC0;
            work->field_26 = 0x180;
            arg0->state    = 1;
        case 1:
            func_shelter_b2_pod_bottom_8017E788(coord, (s16)work->field_26, (s16)work->field_24);
            if (Gp_State1C->eventState == 0) {
                work->field_26 += 0x60;
                y               = work->field_24 - 0x18;
                work->field_24  = y;
                if (y < 0x18) {
                    Gp_ReleaseState1CMem(work, arg0);
                }
            } else {
                work->field_22--;
            }
            break;
    }
}
