#include "common.h"
#include "gameplay/3CD8.h"
#include "main/display.h"
#include "rooms/room_common.h"
#include "rooms/shelter_b6_growth_room.h"

#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

extern u32 Gp_LcgState;

void func_shelter_b6_growth_room_8017ED28(GsCOORDINATE2* coord, u16 arg1, s16 arg2, s16 arg3);

void func_shelter_b6_growth_room_8017E448(s16 arg0, s16 arg1)
{
    POLY_G4* prim;

    prim           = (POLY_G4*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(prim + 1);
    setPolyG4(prim);
    setRGB0(prim, 0, 0, 0);
    setRGB1(prim, 0, 0, 0);
    setRGB2(prim, arg1, arg1, arg1);
    setRGB3(prim, arg1, arg1, arg1);
    setXY4(prim, -160, 120 - arg0, 160, 120 - arg0, -160, 120, 160, 120);
    addPrim((u_long*)((((u32)(0x40 << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt), prim);
    Gp_AddTpageShift((P_TAG*)prim, 1, 0x40);
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_growth_room/shelter_b6_growth_room_3", func_shelter_b6_growth_room_8017E564);

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_growth_room/shelter_b6_growth_room_3", func_shelter_b6_growth_room_8017E7F0);

void func_shelter_b6_growth_room_8017EAC8(Task* task)
{
    RoomEffWork*   work  = task->spawnArg2;
    GsCOORDINATE2* coord = ((TmdObject*)task->extra)->coords;
    s32            vz;
    s16            f2a;
    u32            rng2;
    u32            rng3;

    work->field_22++;
    if (task->state == 0) {
        work->field_24 = (*(u16*)&task->spawnArg1) & 0xFFF;
        Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
        work->field_26 = (Gp_LcgState >> 16) & 0xFFF;

        if (task->spawnArg1 & 0xF000) {
            work->field_28 = (task->spawnArg1 >> 12) & 0x7;
        } else {
            work->field_28 = 1;
        }

        work->field_22 = 0;
        task->state    = 1;

        if (task->spawnArg1 & 0xFF0000) {
            f2a = (task->spawnArg1 >> 16) & 0xFF;
        } else {
            f2a = 0x40;
        }

        work->field_2A    = f2a;
        work->field_10.vy = 0;
        rng2              = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState       = rng2;
        work->field_10.vx = ((rng2 >> 16) & 0x7F) + 0x40;
        rng3              = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState       = rng3;
        vz                = 0x40 - ((rng3 >> 16) & 0x7F);
        work->field_10.vz = vz;
        VectorNormalSS(&work->field_10, &work->field_10);

        gte_lddp(work->field_2A);
        gte_ldsv(&work->field_10);
        gte_gpf12_real();
        gte_stsv(&work->field_10);
    }

    func_shelter_b6_growth_room_8017ED28(coord, work->field_20, (s16)work->field_24, (s16)work->field_26);

    coord->coord.t[0] += work->field_10.vx;
    coord->coord.t[1] += work->field_10.vy;
    coord->coord.t[2] += work->field_10.vz;
    coord->flg         = 0;
    work->field_10.vy += 2;

    if (((s16)work->field_22 % (s16)work->field_28) == 0) {
        work->field_20++;
        if ((s16)work->field_20 >= 0xA) {
            Gp_ReleaseState1CMem(work, task);
        }
    }
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_growth_room/shelter_b6_growth_room_3", func_shelter_b6_growth_room_8017ED28);
