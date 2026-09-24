#include "common.h"
#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/shelter_b6_nursery.h"
#include <psyq/inline_c.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

extern u32 Gp_LcgState;

void func_shelter_b6_nursery_801829E4(GsCOORDINATE2* coord, s16 arg1, s16 arg2);

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_nursery/shelter_b6_nursery_5", func_shelter_b6_nursery_80181314);

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_nursery/shelter_b6_nursery_5", func_shelter_b6_nursery_80181820);

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_nursery/shelter_b6_nursery_5", func_shelter_b6_nursery_80181EDC);

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_nursery/shelter_b6_nursery_5", func_shelter_b6_nursery_80182330);

void func_shelter_b6_nursery_80182730(Task* task)
{
    SVECTOR        step;
    RoomEffWork*   work;
    GsCOORDINATE2* coord;
    s16            eventState;

    work       = task->spawnArg2;
    eventState = Gp_State1C->eventState;
    coord      = ((TmdObject*)task->extra)->coords;
    if (eventState >= 2) {
        if (eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
    } else {
        if (task->state == 0) {
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            work->field_10.vx = 0x80 - ((Gp_LcgState >> 16) & 0xFF);
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            work->field_10.vy = 0x80 - ((Gp_LcgState >> 16) & 0xFF);
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            work->field_10.vz = 0x80 - ((Gp_LcgState >> 16) & 0xFF);
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            work->field_24    = ((Gp_LcgState >> 16) & 0x3F) + 0x40;
            work->field_26    = task->spawnArg1 & 0xFFF;
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            work->field_28    = ((Gp_LcgState >> 16) & 0x7F) + 0x40;
            VectorNormalSS(&work->field_10, &work->field_10);
            Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
            work->field_18 = 0x100 - ((Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
            work->field_1A = 0x100 - ((Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
            work->field_1C = 0x100 - ((Gp_LcgState >> 16) & 0x1FF);
            coord->flg     = 0;
            task->state++;
            return;
        }
        Gfx_RotMatrixXYZ(&coord->coord, (SVECTOR*)&work->field_18, 0);
        MatrixNormal(&coord->coord, &coord->coord);
        gte_lddp(work->field_24);
        gte_ldsv(&work->field_10);
        gte_gpf12_real();
        gte_stsv(&step);
        coord->coord.t[0] += step.vx;
        coord->coord.t[1] += step.vy;
        coord->coord.t[2] += step.vz;
        coord->flg         = 0;
        func_shelter_b6_nursery_801829E4(coord, work->field_26, work->field_28);
        if (coord->coord.t[1] > 0) {
            Gp_ReleaseState1CMem(work, task);
        } else {
            work->field_10.vy += 0x180;
        }
    }
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_nursery/shelter_b6_nursery_5", func_shelter_b6_nursery_801829E4);

void func_shelter_b6_nursery_80182D14(s16 arg0, s16 arg1)
{
    D_shelter_b6_nursery_801879F0.field_0 = arg0;
    D_shelter_b6_nursery_801879F0.field_2 = arg1;
}
