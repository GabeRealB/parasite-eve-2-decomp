#include "common.h"

#include "main/gameflag.h"

#include "gameplay/3CD8.h"

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_operating_room/shelter_b2_operating_room", func_shelter_b2_operating_room_8017D628);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b2_operating_room/shelter_b2_operating_room", D_shelter_b2_operating_room_8017D5C0);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_operating_room/shelter_b2_operating_room", func_shelter_b2_operating_room_8017D78C);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_operating_room/shelter_b2_operating_room", func_shelter_b2_operating_room_8017D8FC);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_operating_room/shelter_b2_operating_room", func_shelter_b2_operating_room_8017DA94);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_operating_room/shelter_b2_operating_room", func_shelter_b2_operating_room_8017DC9C);

s32 func_shelter_b2_operating_room_8017DCA4(s32 arg0, s32 arg1, s32 arg2)
{
    switch (arg2) {
        case 4:
            Gp_SpawnIfCapIdle(GameFlag_GetNibble(0xC7) == 0 ? 4 : 0x10, 0);
            break;
        case 5:
            Gp_SpawnIfCapIdle(GameFlag_GetNibble(0xC7) != 0 ? 0xF : 5, 0);
            break;
    }
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_operating_room/shelter_b2_operating_room", func_shelter_b2_operating_room_8017DD0C);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_operating_room/shelter_b2_operating_room", func_shelter_b2_operating_room_8017DD14);

void func_shelter_b2_operating_room_8017DD58(void)
{
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_operating_room/shelter_b2_operating_room", func_shelter_b2_operating_room_8017DD60);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_operating_room/shelter_b2_operating_room", func_shelter_b2_operating_room_8017DDB8);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b2_operating_room/shelter_b2_operating_room", jtbl_shelter_b2_operating_room_8017D614);
