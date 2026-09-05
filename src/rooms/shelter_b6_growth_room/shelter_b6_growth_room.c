#include "common.h"

#include "main/gameflag.h"
#include "main/task.h"

#include "gameplay/3CD8.h"

extern TaskDesc D_80135E78;

s32 func_shelter_b6_growth_room_8017D634(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 1) {
        if (GameFlag_GetNibble(0xD8) == 0) {
            Gp_MsgPlayerWeapon(0);
            Task_SpawnFromTable(&D_80135E78, 3, 0, 0);
        } else {
            Gp_RunCapCmd1(1);
        }
    }
    if (arg2 == 0x10) {
        Gp_SpawnIfCapIdle(GameFlag_GetNibble(0xD8) == 0 ? 0x10 : 0x11, 0);
    }
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_growth_room/shelter_b6_growth_room", func_shelter_b6_growth_room_8017D6C8);

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_growth_room/shelter_b6_growth_room", func_shelter_b6_growth_room_8017D71C);

void func_shelter_b6_growth_room_8017D7CC(void)
{
}

INCLUDE_RODATA("rooms/nonmatchings/shelter_b6_growth_room/shelter_b6_growth_room", D_shelter_b6_growth_room_8017D5C0);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b6_growth_room/shelter_b6_growth_room", RoomsShared8017d878Table);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b6_growth_room/shelter_b6_growth_room", jtbl_shelter_b6_growth_room_8017D5D0);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b6_growth_room/shelter_b6_growth_room", D_shelter_b6_growth_room_8017D5E8);
