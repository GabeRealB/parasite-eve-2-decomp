#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"

#include "rooms/room_common.h"

extern GpMsgEntry D_shelter_r48_80182FB8[];

extern s32      D_8014BD48;
extern s32      D_8014C288;
extern TaskDesc D_8014B964;
extern s32      D_8014C540;
extern s32      D_8014CAF8;
extern s32      D_8014D158;

void func_80132F58(s32 arg0);

s32 func_shelter_r48_8017E090(Task* task, s32 msgId, RoomEventMsg* in)
{
    if (in->field_2 == 1) {
        switch (GameFlag_GetNibble(0x100)) {
            case 0:
                Gp_RunCapCmd1(6);
                break;
            case 1:
                Gp_RunCapCmd1(7);
                break;
        }
    }
    return 0;
}

s32 func_shelter_r48_8017E0EC(void)
{
    if (GameFlag_GetNibble(0x12A) == 1) {
        func_800E8614((s32)&D_8014D158, 0);
        GameFlag_SetNibble(0x12A, 2);
    } else {
        func_800E8634((s32)&D_8014C540, 0, (s32)&D_8014CAF8);
        Task_SpawnFromTable(&D_8014B964, 0, 0, 0);
        Task_SpawnFromTable(&D_8014B964, 0, 1, 0);
        Task_SpawnFromTable(&D_8014B964, 0, 3, 0);
        GameFlag_SetNibble(0x12A, 3);
    }
    return 0;
}

void func_shelter_r48_8017E1A4(Task* arg0)
{
    arg0->msgTable = D_shelter_r48_80182FB8;
    Game_SetPtrSlot(arg0, 7);
    func_80132F58(0);
    func_800E8634((s32)&D_8014BD48, 0, (s32)&D_8014C288);
    GameFlag_SetNibble(0x12A, 1);
    arg0->state = (s32)(arg0->state + 1);
}
