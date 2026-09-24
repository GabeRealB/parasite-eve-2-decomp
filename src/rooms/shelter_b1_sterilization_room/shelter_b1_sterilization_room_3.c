#include "common.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/gameflag.h"
#include "gameplay/3CD8.h"

void func_shelter_b1_sterilization_room_8018049C(void);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room_3", D_shelter_b1_sterilization_room_8017D610);
INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room_3", D_shelter_b1_sterilization_room_8017D61C);
INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room_3", D_shelter_b1_sterilization_room_8017D624);
INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room_3", D_shelter_b1_sterilization_room_8017D630);
INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room_3", D_shelter_b1_sterilization_room_8017D638);

s32 func_shelter_b1_sterilization_room_80180430(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 0x63) {
        SndEvt_EnqueueType6(0x54100016, 0, 0);
    }
    return 0;
}

void func_shelter_b1_sterilization_room_80180464(void)
{
    if (gGameSession->at4.loc.place == 5) {
        func_shelter_b1_sterilization_room_8018049C();
    }
}

void func_shelter_b1_sterilization_room_8018049C(void)
{
    s32 view;

    view = gGameSession->at4.loc.view;
    if ((GameFlag_GetNibble(0xEA) == 1) && (gGameSession->eventState == 0)) {
        if (view == 2 || view == 3) {
            Gp_MsgSlot4Chain(0, 1);
        } else {
            Gp_MsgSlot4Chain(0, 0);
        }
    }
}
