#include "common.h"

#include <psyq/inline_c.h>

#include "rooms/room_common.h"

#include <psyq/libgs.h>

#include "gameplay/3CD8.h"

#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

extern u32 Gp_LcgState;

void func_shelter_b4_reservoir_80181668(GsCOORDINATE2* coord, u16 arg1, s16 arg2);

void func_shelter_b4_reservoir_801813F0(Task* task)
{
    RoomEffWork*   work  = task->spawnArg2;
    GsCOORDINATE2* coord = ((TmdObject*)task->extra)->coords;
    s16            f2a;
    u32            rng;

    if (Gp_State1C->eventState != 0) {
        func_shelter_b4_reservoir_80181668(coord, work->field_20, (s16)work->field_24);
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
        return;
    }

    work->field_22++;
    switch (task->state) {
        case 0:
            work->field_24 = (*(u16*)&task->spawnArg1) & 0xFFF;

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
            work->field_10.vz = 0;
            rng               = Gp_LcgState * 5 + 0x71357911;
            Gp_LcgState       = rng;
            work->field_10.vx = -((rng >> 16) & 0x3F) - 0x40;
            VectorNormalSS(&work->field_10, &work->field_10);

            gte_lddp(work->field_2A);
            gte_ldsv(&work->field_10);
            gte_gpf12_real();
            gte_stsv(&work->field_10);
            break;
        case 1:
            func_shelter_b4_reservoir_80181668(coord, work->field_20, (s16)work->field_24);
            if ((s16)work->field_2A != 0) {
                coord->coord.t[0] += work->field_10.vx;
                coord->coord.t[1] += work->field_10.vy;
                coord->coord.t[2] += work->field_10.vz;
                coord->flg         = 0;
            }
            if (((s16)work->field_22 % (s16)work->field_28) == 0) {
                work->field_20++;
                if ((s16)work->field_20 >= 6) {
                    Gp_ReleaseState1CMem(work, task);
                }
            }
            break;
    }
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b4_reservoir/shelter_b4_reservoir_6", func_shelter_b4_reservoir_80181668);
