#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "main/gameflag.h"
#include "main/task.h"

#include "rooms/room_common.h"

extern GpMsgEntry D_shelter_b1_elevator_hall_80182CB8[];

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_elevator_hall/shelter_b1_elevator_hall", func_shelter_b1_elevator_hall_8017D620);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_elevator_hall/shelter_b1_elevator_hall", func_shelter_b1_elevator_hall_8017D810);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_elevator_hall/shelter_b1_elevator_hall", RoomsShared8017d878Table);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_elevator_hall/shelter_b1_elevator_hall", func_shelter_b1_elevator_hall_8017D99C);

s32 func_shelter_b1_elevator_hall_8017DB54(void)
{
    return 0;
}

s32 func_shelter_b1_elevator_hall_8017DB5C(void)
{
    return 0;
}

s32 func_shelter_b1_elevator_hall_8017DB64(void)
{
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_elevator_hall/shelter_b1_elevator_hall", func_shelter_b1_elevator_hall_8017DB6C);

void func_shelter_b1_elevator_hall_8017DBB8(Task* arg0)
{
    arg0->msgTable = D_shelter_b1_elevator_hall_80182CB8;
    Game_SetPtrSlot(arg0, 7);
    if (GameFlag_GetNibble(0x122) == 0) {
        GameFlag_SetNibble(0x122, 1);
        func_800E3FAC(0xA2, 0x1D);
    }
    arg0->state++;
}

void func_shelter_b1_elevator_hall_8017DC20(void)
{
}

INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_elevator_hall/shelter_b1_elevator_hall", jtbl_shelter_b1_elevator_hall_8017D600);
