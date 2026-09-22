#include "common.h"

#include "actors/actors_shared_8014a7b0.h"

s32 ActorsShared8014a7b0(ActorsShared8014a7b0Work* arg0)
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
            if (arg0->field_3D4 == v) {
                goto same;
            }
            arg0->field_3D4 = id;
            return 0x400C0001;
        not15:
            if (v == 0x11) {
                goto check;
            }
        clear:
            arg0->field_3D4 = 0;
            break;
        case 3:
            id = arg0->field_4A & 0x3FF;
            v  = id;
            if (v != 0xD && v != 0x12) {
                goto clear;
            }
            goto check;
        same:
            arg0->field_3D4 = id;
            break;
        case 4:
            if (arg0->field_58 & 1) {
                return 0x400C0005;
            }
            break;
    }
    return 0;
}
