#include "common.h"

#include "actors/actor_401300.h"

s32 func_actor_401300_80141494(Actor401300* arg0, s32 arg1, Actor401300Msg* arg2)
{
    Actor401300Work* work = arg0->field_1C;

    switch (arg2->field_4) {
        case 0:
            work->field_8A2 = 0x22;
            break;
        case 1:
            work->field_8A2 = 0x23;
            break;
        case 2:
            work->field_8A2 = 0x24;
            break;
        case 3:
            work->field_8A2 = 0x25;
            break;
        case 4:
            work->field_8A2 = 0x27;
            break;
    }
    work->field_0 = 0x11;
    work->field_2 = -1;
    return 0;
}

/* Closes this unit's .rodata after the 5-entry jump table above, so
   actor_401300_4's tables start at 0x80132044. Nothing reads it. */
const u32 D_actor_401300_80132040 = 0;
