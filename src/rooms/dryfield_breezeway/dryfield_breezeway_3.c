#include "common.h"

#include <psyq/libgte.h>

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway_3", func_dryfield_breezeway_8017FA80);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway_3", func_dryfield_breezeway_8017FAD0);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway_3", func_dryfield_breezeway_8017FB30);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway_3", func_dryfield_breezeway_8017FBC8);

s16 func_dryfield_breezeway_8017FBEC(s32 arg0, s32 arg1, s32 arg2, s32 arg3)
{
    SVECTOR vec;

    vec.vx = arg2 - arg0;
    vec.vy = arg3 - arg1;
    vec.vz = 0;
    VectorNormalSS(&vec, &vec);
    return ratan2(vec.vx, vec.vy);
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway_3", func_dryfield_breezeway_8017FC38);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway_3", func_dryfield_breezeway_8017FCB4);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway_3", func_dryfield_breezeway_8017FD68);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway_3", func_dryfield_breezeway_8017FD9C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway_3", func_dryfield_breezeway_8017FE08);
