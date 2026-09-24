#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>

#include "gameplay/3CD8.h"
#include "rooms/room_common.h"
#include "rooms/dryfield_night_main_street.h"

#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

extern u32 Gp_LcgState;

void func_dryfield_night_main_street_8017F3B0(Task* task)
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
        work->field_10.vx = -(((u32)rng2 >> 16) & 0x7F);
        rng3              = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState       = rng3;
        vz                = 0x80 - (((u32)rng3 >> 16) & 0xFF);
        work->field_10.vz = vz;
        VectorNormalSS(&work->field_10, &work->field_10);

        gte_lddp(work->field_2A);
        gte_ldsv(&work->field_10);
        gte_gpf12_real();
        gte_stsv(&work->field_10);
    }

    func_dryfield_night_main_street_8017F608(coord, work->field_20, (s16)work->field_24, (s16)work->field_26);

    coord->coord.t[0] += work->field_10.vx;
    coord->coord.t[1] += work->field_10.vy;
    coord->coord.t[2] += work->field_10.vz;
    coord->flg         = 0;

    if (((s16)work->field_22 % (s16)work->field_28) == 0) {
        work->field_20++;
        if ((s16)work->field_20 >= 0xA) {
            Gp_ReleaseState1CMem(work, task);
        }
    }
}
