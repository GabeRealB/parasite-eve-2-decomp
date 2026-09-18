#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "main/gameflag.h"
#include "main/task.h"

extern u8         D_80115598;
extern GpMsgEntry D_dryfield_main_street_80180EA0[];
extern s32        D_dryfield_main_street_80180ED0;
extern s32        D_dryfield_main_street_80181624;
extern s32        D_dryfield_main_street_80181A14;
void              func_dryfield_main_street_8017E4A4(s32 arg0);

s32 func_dryfield_main_street_8017E05C(Task* task, s32 msgId, GpMsg13EF* msg, s32 arg3)
{
    if ((msg->field_2 == 1) && (GameFlag_GetNibble(0x5F) == 0)) {
        func_dryfield_main_street_8017E4A4(0);
        func_800E8634((s32)&D_dryfield_main_street_80181624, 0, (s32)&D_dryfield_main_street_80181A14);
        GameFlag_SetNibble(0x5F, 1);
        GameFlag_SetNibble(3, 0);
        GameFlag_SetNibble(0x155, 1);
    }
    return 0;
}

void func_dryfield_main_street_8017E0D8(Task* task)
{
    task->msgTable = D_dryfield_main_street_80180EA0;
    Game_SetPtrSlot(task, 7);
    D_80115598 = 1;
    if (GameFlag_GetNibble(0x5F) == 0) {
        Gp_DispatchMsg(Game_GetPtrSlot(4), 0x7DA, (s32)&D_dryfield_main_street_80180ED0, 0x7DB);
    }
    task->state++;
}
