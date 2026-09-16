#include "common.h"

#include "gameplay/3CD8.h"

s32 func_dryfield_garage_8017D8BC(s32 arg0, s32 arg1, s32 arg2)
{
    switch (arg2) {
        case 0x9:
            Gp_EnqueueStageSnd6(0x52030009, 0, 0);
            break;
        case 0x6C:
            Gp_GetCapEventKey();
            break;
    }
    return 0;
}

INCLUDE_RODATA("rooms/nonmatchings/dryfield_garage/dryfield_garage", RoomsShared8017d878Table);
