#include "common.h"

#include "gameplay/3CD8.h"

s32 func_dryfield_night_breezeway_8017D600(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 1) {
        Gp_SpawnIfCapIdle(1, 1);
    }
    return 0;
}
INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_breezeway/dryfield_night_breezeway", RoomsShared8017d878Table);
