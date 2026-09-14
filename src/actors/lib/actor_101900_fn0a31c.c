#include "common.h"

#include "actors/actor_101900.h"

s32 Actor01900_Fn0A31C(Actor01900* arg0, s32 arg1, Actor01900Msg7D3* arg2)
{
    Actor01900Work* work = arg0->field_1C;

    switch (arg2->field_4) {
        case 0:
            work->field_89E = 0x22;
            break;
        case 1:
            work->field_89E = 0x23;
            break;
        case 2:
            work->field_89E = 0x24;
            break;
        case 3:
            work->field_89E = 0x25;
            break;
        case 4:
            work->field_89E = 0x27;
            break;
    }
    work->field_0 = 0x11;
    work->field_2 = -1;
    return 0;
}
