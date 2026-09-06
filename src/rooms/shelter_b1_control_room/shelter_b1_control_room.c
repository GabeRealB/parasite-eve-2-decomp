#include "common.h"

#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/task.h"

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_control_room/shelter_b1_control_room", func_shelter_b1_control_room_8017D600);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_control_room/shelter_b1_control_room", func_shelter_b1_control_room_8017D7B8);

s32 func_shelter_b1_control_room_8017ECCC(void)
{
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_control_room/shelter_b1_control_room", func_shelter_b1_control_room_8017ECD4);

s32 func_shelter_b1_control_room_8017ED68(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    switch (arg2) {
        case 3:
            if (GameFlag_GetNibble(0xF7) != 0) {
                Gp_RunCapCmd1(6);
            } else {
                Gp_RunCapCmd1(3);
            }
            break;
        case 4:
            if (GameFlag_GetNibble(0xF7) != 0) {
                Gp_RunCapCmd1(7);
            } else {
                Gp_RunCapCmd1(4);
            }
            break;
        case 5:
            if (GameFlag_GetNibble(0xF7) != 0) {
                Gp_RunCapCmd1(8);
            } else {
                Gp_RunCapCmd1(5);
            }
            break;
        case 9:
            if (GameFlag_GetNibble(0x7A) == 6) {
                Gp_RunCapCmd1(9);
            }
            break;
    }
    return 0;
}

s32 func_shelter_b1_control_room_8017EE24(void)
{
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_control_room/shelter_b1_control_room", func_shelter_b1_control_room_8017EE2C);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_control_room/shelter_b1_control_room", D_shelter_b1_control_room_8017D5C0);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_control_room/shelter_b1_control_room", RoomsShared8017d878Table);
