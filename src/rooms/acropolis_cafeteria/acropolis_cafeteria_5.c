#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "main/fs.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/stream.h"
#include "main/task.h"

extern void Stage_RequestFromAreaTable(s32 arg0);

extern GpMsgEntry D_acropolis_cafeteria_80182AA8[];
extern GpObj4A    D_acropolis_cafeteria_80189490[];
extern s32        D_acropolis_cafeteria_80182DDC;

void func_acropolis_cafeteria_8017E348(Task* task)
{
    task->field_24 = D_acropolis_cafeteria_80182AA8;
    Game_SetPtrSlot(task, 7);
    if (GameFlag_GetNibble(0) == 1) {
        Gp_MsgSlot4Chain(0, 0);
        Gp_MsgSlot4Chain(1, 1);
    } else if (GameFlag_GetNibble(0) == 2) {
        Gp_MsgSlot4Chain(0, 1);
        Gp_MsgSlot4Chain(1, 2);
        Gp_MsgSlot4Chain(2, 1);
        D_acropolis_cafeteria_80189490[0].field_4A &= 0xBF;
        Gp_DispatchMsg((Task*)Gp_LookupSlot4(2), 0x7D4, (s32)&D_acropolis_cafeteria_80182DDC, 0);
    }
    task->state = task->state + 1;
}
extern TaskFuncTable3 RoomsShared8017d878Table;
