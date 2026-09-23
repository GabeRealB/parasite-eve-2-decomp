#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/session.h"
#include "main/task.h"

extern s32 D_shelter_b6_training_room_80182B24;
extern s32 D_shelter_b6_training_room_80183BB4;
extern s32 D_shelter_b6_training_room_80184124;
extern s32 D_shelter_b6_training_room_80185C58;

s32 func_shelter_b6_training_room_8017D764(void)
{
    gGameSession->flowFlags |= 0x80;
    func_800E8634((s32)&D_shelter_b6_training_room_80183BB4, 0, (s32)&D_shelter_b6_training_room_80184124);
    Gp_DispatchMsg((Task*)Gp_LookupSlot4(3), 0x7DB, (s32)&D_shelter_b6_training_room_80182B24, 0);
    D_shelter_b6_training_room_80185C58 = 1;
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_training_room/shelter_b6_training_room_2", func_shelter_b6_training_room_8017D7D4);

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_training_room/shelter_b6_training_room_2", func_shelter_b6_training_room_8017D874);
