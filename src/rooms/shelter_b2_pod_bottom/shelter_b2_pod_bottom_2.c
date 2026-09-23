#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/3FB8.h"
#include "main/task.h"
#include "main/gfx.h"
#include "rooms/room_common.h"

extern u32 Gp_LcgState;

void func_shelter_b2_pod_bottom_8017E788(GsCOORDINATE2* coord, s32 arg1, s32 arg2);
void func_shelter_b2_pod_bottom_8017EEAC(RoomEffWork* work, GsCOORDINATE2* coord, s32 arg2);
void func_shelter_b2_pod_bottom_8018101C(GsCOORDINATE2* coord, s32 arg1, s32 arg2, s32 arg3);

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

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_pod_bottom/shelter_b2_pod_bottom_2", func_shelter_b2_pod_bottom_801805A0);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_pod_bottom/shelter_b2_pod_bottom_2", func_shelter_b2_pod_bottom_80180898);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_pod_bottom/shelter_b2_pod_bottom_2", func_shelter_b2_pod_bottom_80180A4C);

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

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_pod_bottom/shelter_b2_pod_bottom_2", func_shelter_b2_pod_bottom_8018101C);

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

INCLUDE_RODATA("rooms/nonmatchings/shelter_b2_pod_bottom/shelter_b2_pod_bottom_2", D_shelter_b2_pod_bottom_8017D5EC);
