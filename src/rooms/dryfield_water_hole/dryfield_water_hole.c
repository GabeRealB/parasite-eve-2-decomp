#include "common.h"

#include "gameplay/3CD8.h"

#include "main/gameflag.h"
#include "main/sound.h"

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_hole/dryfield_water_hole", func_dryfield_water_hole_8017D5F0);

s32 func_dryfield_water_hole_8017D73C(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 2) {
        Gp_RunCapCmd1(2);
        GameFlag_SetNibble(0x1BD, 2);
        SndEvt_EnqueueType6(0x52200004, 0, 0);
    }
    return 0;
}

INCLUDE_RODATA("rooms/nonmatchings/dryfield_water_hole/dryfield_water_hole", D_dryfield_water_hole_8017D5C0);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_water_hole/dryfield_water_hole", RoomsShared8017d878Table);
