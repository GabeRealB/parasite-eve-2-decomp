#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "main/task.h"

extern GpMsgEntry D_shelter_1f_parking_garage_80180BB8[];
extern TaskDesc   D_shelter_1f_parking_garage_80180BE0;

INCLUDE_ASM("rooms/nonmatchings/shelter_1f_parking_garage/shelter_1f_parking_garage", func_shelter_1f_parking_garage_8017D6AC);

INCLUDE_ASM("rooms/nonmatchings/shelter_1f_parking_garage/shelter_1f_parking_garage", func_shelter_1f_parking_garage_8017D7E8);

INCLUDE_ASM("rooms/nonmatchings/shelter_1f_parking_garage/shelter_1f_parking_garage", func_shelter_1f_parking_garage_8017D958);

INCLUDE_ASM("rooms/nonmatchings/shelter_1f_parking_garage/shelter_1f_parking_garage", func_shelter_1f_parking_garage_8017DAF0);

s32 func_shelter_1f_parking_garage_8017DCEC(void)
{
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/shelter_1f_parking_garage/shelter_1f_parking_garage", func_shelter_1f_parking_garage_8017DCF4);

s32 func_shelter_1f_parking_garage_8017DE44(void)
{
    return 0;
}

s32 func_shelter_1f_parking_garage_8017DE4C(Task* task, s32 msgId, GpMsg13EF* arg2)
{
    if (arg2->field_2 == 0xA) {
        Gp_MsgPlayerWeapon(0);
        Gp_RunCapCmd1(2);
        Task_SpawnFromTable(&D_shelter_1f_parking_garage_80180BE0, 0, 0, 0);
    }
    return 0;
}

void func_shelter_1f_parking_garage_8017DE9C(Task* task)
{
    task->msgTable = D_shelter_1f_parking_garage_80180BB8;
    Game_SetPtrSlot(task, 7);
    if (gGameSession->at4.loc.warp == 1) {
        Gp_RunCapCmd1(5);
    }
    task->state = task->state + 1;
}

INCLUDE_RODATA("rooms/nonmatchings/shelter_1f_parking_garage/shelter_1f_parking_garage", RoomsShared8017d878Table);
