#include "common.h"

#include "gameplay/3CD8.h"

/* func_dryfield_night_water_tower_8017D8E0 is the room's message handler; it
   is the shared body RoomsShared8017daf8 (src/rooms/lib/, see its header),
   linked into both water tower rooms from the `shared` span at 0x320. */

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_water_tower/dryfield_night_water_tower", RoomsShared8017d878Table);

s32 func_dryfield_night_water_tower_8017DA9C(void)
{
    return 0;
}

s32 func_dryfield_night_water_tower_8017DAA4(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 7) {
        Gp_StartCapSlot(7, 1, 3);
    }
    return 0;
}

s32 func_dryfield_night_water_tower_8017DAD4(void)
{
    return 0;
}
