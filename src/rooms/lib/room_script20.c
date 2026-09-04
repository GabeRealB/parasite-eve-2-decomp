#include "common.h"

#include "gameplay/3CD8.h"

s32 Room_Script20(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 4) {
        Gp_RunCapCmd1(4);
    }
    return 0;
}
