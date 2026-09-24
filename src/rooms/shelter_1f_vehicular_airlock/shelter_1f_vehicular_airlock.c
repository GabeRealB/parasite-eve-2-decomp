#include "common.h"
#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "main/gameflag.h"

INCLUDE_ASM("rooms/nonmatchings/shelter_1f_vehicular_airlock/shelter_1f_vehicular_airlock", func_shelter_1f_vehicular_airlock_8017D644);

INCLUDE_ASM("rooms/nonmatchings/shelter_1f_vehicular_airlock/shelter_1f_vehicular_airlock", func_shelter_1f_vehicular_airlock_8017D7DC);

s32 func_shelter_1f_vehicular_airlock_8017D988(void)
{
    return 0;
}

s32 func_shelter_1f_vehicular_airlock_8017D990(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 3) {
        if (Gp_GetCurBit2Flag(6) == 2 && GameFlag_GetNibble(0x7A) >= 6) {
            arg2 = 5;
        }
        Gp_SpawnIfCapIdle(arg2, 0);
    }
    return 0;
}

s32 func_shelter_1f_vehicular_airlock_8017D9F4(void)
{
    return 0;
}

INCLUDE_RODATA("rooms/nonmatchings/shelter_1f_vehicular_airlock/shelter_1f_vehicular_airlock", RoomsShared8017d878Table);
