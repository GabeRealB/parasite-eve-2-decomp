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

void func_shelter_b2_pod_bottom_8017E788(GsCOORDINATE2* coord, s32 arg1, s32 arg2);
void func_shelter_b2_pod_bottom_8017EEAC(RoomEffWork* work, GsCOORDINATE2* coord, s32 arg2);

extern s16 D_shelter_b2_pod_bottom_80188790[3][16];

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

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_pod_bottom/shelter_b2_pod_bottom_2", func_shelter_b2_pod_bottom_8017E788);

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

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_pod_bottom/shelter_b2_pod_bottom_2", func_shelter_b2_pod_bottom_8017EEAC);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_pod_bottom/shelter_b2_pod_bottom_2", func_shelter_b2_pod_bottom_8017F448);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_pod_bottom/shelter_b2_pod_bottom_2", func_shelter_b2_pod_bottom_8017F994);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_pod_bottom/shelter_b2_pod_bottom_2", func_shelter_b2_pod_bottom_8018016C);

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
