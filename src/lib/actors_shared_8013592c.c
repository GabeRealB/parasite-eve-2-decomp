#include "common.h"

#include "actors/actors_shared_8013592c.h"

void ActorsShared8013592c(ActorShared8013592c* arg0)
{
    ActorShared8013592cWork* work;
    s16                      state;
    s32                      next;

    work  = arg0->field_1C;
    state = work->field_6CE;
    switch (state) {
        case 0:
            next = work->field_6F0;
            if (next == 1) {
                work->field_6C0 = 0xE;
                work->field_6CE = next;
            } else {
                work->field_6C0 = 0x12;
                work->field_6CE = 2;
            }
            break;
        case 1:
            if (work->field_6C4 >= 0x10) {
                work->field_6C0 = 0x10;
                work->field_6CC = 7;
                work->field_6CE = 3;
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_6D4 = ((u32)Gp_LcgState >> 16) & 0x3F;
            }
            break;
        case 2:
            if (work->field_6C4 >= 0x16) {
                work->field_6C0 = 0x14;
                work->field_6CC = 7;
                work->field_6CE = 3;
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_6D4 = ((u32)Gp_LcgState >> 16) & 0x3F;
            }
            break;
    }
}
