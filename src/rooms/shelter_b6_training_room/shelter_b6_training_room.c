#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"

#include "main/gameflag.h"

s32 func_shelter_b6_training_room_8017D684(s32 arg0, s32 arg1, s32 arg2)
{
    switch (arg2) {
        case 5:
            if (GameFlag_GetNibble(0x153) != 0) {
                Gp_RunCapCmd1(7);
            } else if (Gp_StateF0.field_0 == 1) {
                Gp_RunCapCmd1(5);
            } else {
                Gp_RunCapCmd1(7);
            }
            break;
        case 6:
            if (GameFlag_GetNibble(0x154) != 0) {
                Gp_RunCapCmd1(8);
            } else if (Gp_StateF0.field_0 == 1) {
                Gp_RunCapCmd1(6);
            } else {
                Gp_RunCapCmd1(8);
            }
            break;
        case 4:
            if (GameFlag_GetNibble(0x146) != 0) {
                Gp_RunCapCmd1(7);
            } else if (Gp_StateF0.field_0 != 1) {
                Gp_RunCapCmd1(0xA);
            } else {
                Gp_RunCapCmd1(4);
            }
            break;
    }
    return 0;
}

INCLUDE_RODATA("rooms/nonmatchings/shelter_b6_training_room/shelter_b6_training_room", D_shelter_b6_training_room_8017D5C0);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b6_training_room/shelter_b6_training_room", RoomsShared8017d878Table);
