#include "common.h"

#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/task.h"
#include "rooms/room_common.h"

extern s32      func_80179A04(RoomEventMsg* in, RoomEventMsg* out);
extern TaskDesc D_shelter_b1_sleeping_quarters_80180540;

s32 func_shelter_b1_sleeping_quarters_8017D668(void)
{
    return 0;
}

s32 func_shelter_b1_sleeping_quarters_8017D670(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_80179A04(in, out);
    if (in->msgId != 0xC) {
        return 1;
    }
    if (GameFlag_GetNibble(0x7A) < 6) {
        return 1;
    }
    if (in->field_5 != 0) {
        return 0;
    }
    Gp_RunCapCmd1(0xE);
    return 0;
}

s32 func_shelter_b1_sleeping_quarters_8017D6FC(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 8) {
        Gp_MsgPlayerWeapon(0);
        Task_SpawnFromTable(&D_shelter_b1_sleeping_quarters_80180540, 0, 8, 0);
    }
    if (arg2 == 3) {
        Gp_SpawnIfCapIdle(GameFlag_GetNibble(0x16F) == 0 ? 3 : 0xF, 0);
    }
    return 0;
}

s32 func_shelter_b1_sleeping_quarters_8017D770(void)
{
    return 0;
}

void func_shelter_b1_sleeping_quarters_8017D778(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_CapFile = 0;
            Gp_LoadCapFile(1);
            func_800E6D4C(0x2C0, 0);
            Gp_RunCapCmd(task->spawnArg1, 1);
            task->state++;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                task->state++;
            }
            break;
        case 2:
            Gp_MsgPlayerWeapon(1);
            Gp_ResetCap();
            taskKill(task);
            break;
    }
}
INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_sleeping_quarters/shelter_b1_sleeping_quarters", D_shelter_b1_sleeping_quarters_8017D5C4);
