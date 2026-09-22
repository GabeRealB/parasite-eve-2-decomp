#include "common.h"

#include "actors/actors_shared_8014adfc.h"

s32 ActorsShared8014adfc(Actor104000Work* arg0)
{
    u16 id;
    s32 v;

    switch (arg0->field_174) {
        case 2:
            id = arg0->field_4A & 0x3FF;
            v  = id;
            if (v != 0x15) {
                goto not15;
            }
        check:
            if (arg0->field_474 == v) {
                goto same;
            }
            arg0->field_474 = id;
            return 0x40280001;
        not15:
            if (v == 0x11) {
                goto check;
            }
        clear:
            arg0->field_474 = 0;
            break;
        case 3:
            id = arg0->field_4A & 0x3FF;
            v  = id;
            if (v != 0xD && v != 0x12) {
                goto clear;
            }
            goto check;
        same:
            arg0->field_474 = id;
            break;
        case 5:
            if (arg0->field_58 & 2) {
                return 0x400C0005;
            }
            break;
    }
    return 0;
}
