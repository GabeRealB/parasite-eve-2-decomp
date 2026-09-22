#include "common.h"

#include "actors/actors_shared_80138c6c.h"

extern u32 Gp_LcgState;

void ActorsShared80138c6c(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work)
{
    s32 t;
    s32 t2;
    u16 timer;

    if (work->field_BA8 == 0) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        t           = (Gp_LcgState >> 0x10) & 0xF;
        if (t < 4) {
            work->field_B8C = 2;
        } else if (t < 8) {
            work->field_B8C = 0x3C;
        } else if (t < 0xE) {
            work->field_B8C = 0x78;
        } else {
            work->field_B8C = 0xB4;
        }
        work->field_BA4 = 1;
        work->field_BA8 = (u8)work->field_BA8 + 1;
    }
    timer           = (u16)work->field_B8C - 1;
    work->field_B8C = timer;
    if (((u32)timer << 0x10) == 0) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        t2          = (Gp_LcgState >> 0x10) & 0xF;
        if (t2 < 3) {
            work->state = 1;
        } else if (t2 < 6) {
            work->state = 2;
        } else if (t2 < 0xE) {
            work->state = 3;
        } else {
            work->state = 4;
        }
        work->field_BA8 = 0;
    }
}
