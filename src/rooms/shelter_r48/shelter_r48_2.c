#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"

extern GpMsgEntry D_shelter_r48_80182FB8[];

extern s32 D_8014BD48;
extern s32 D_8014C288;

void func_80132F58(s32 arg0);

INCLUDE_ASM("rooms/nonmatchings/shelter_r48/shelter_r48_2", func_shelter_r48_8017E090);

INCLUDE_ASM("rooms/nonmatchings/shelter_r48/shelter_r48_2", func_shelter_r48_8017E0EC);

void func_shelter_r48_8017E1A4(Task* arg0)
{
    arg0->msgTable = D_shelter_r48_80182FB8;
    Game_SetPtrSlot(arg0, 7);
    func_80132F58(0);
    func_800E8634((s32)&D_8014BD48, 0, (s32)&D_8014C288);
    GameFlag_SetNibble(0x12A, 1);
    arg0->state = (s32)(arg0->state + 1);
}
