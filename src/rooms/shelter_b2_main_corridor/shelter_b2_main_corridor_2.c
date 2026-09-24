#include "common.h"

#include "gameplay/D4.h"

#include "main/session.h"
#include "main/task.h"

#include "rooms/room_common.h"

extern GpMsgEntry     D_shelter_b2_main_corridor_80182C14[];
extern TaskDesc       D_shelter_b2_main_corridor_80182DE0[];
extern GpAreaApplyRec D_shelter_b2_main_corridor_80189644;

void func_shelter_b2_main_corridor_8017E264(RoomEventMsg* msg)
{
    if ((GameFlag_GetNibble(0x4C) == 9) && (GameFlag_GetNibble(0xD1) == 3) && (msg->field_5 == 0)) {
        GameFlag_SetNibble(0x4C, 0);
        Gp_ApplyAreaRecs(&D_shelter_b2_main_corridor_80189644);
    }
}

void func_shelter_b2_main_corridor_8017E2D4(Task* arg0)
{
    arg0->msgTable = D_shelter_b2_main_corridor_80182C14;
    Game_SetPtrSlot(arg0, 7);
    Task_SpawnFromTable(D_shelter_b2_main_corridor_80182DE0, 0, 0, 0);
    arg0->state = (s32)(arg0->state + 1);
}

void func_shelter_b2_main_corridor_8017E330(void)
{
}
