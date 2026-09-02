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
extern TaskFuncTable3 D_acropolis_cafeteria_8017D5C4;

/// Per-frame entry point: runs the room task's current state. The table is a
/// local, so GCC copies it from `.rodata` onto the stack every frame.
INCLUDE_ASM("rooms/nonmatchings/acropolis_cafeteria/acropolis_cafeteria_5", func_acropolis_cafeteria_8017E424);
void func_acropolis_cafeteria_8017E47C(Task* arg0)
{
    u8          slotParam[4];
    GBytes8     key;
    CdCmdQueue* queue;
    Task*       task;

    task  = arg0;
    queue = &CdCmd_Queue;
    switch (task->state) {
        case 0:
            goto L_case0;
        case 1:
            goto L_case1;
        case 2:
            goto L_case2;
        case 3:
            goto L_case3;
        case 4:
            goto L_case4;
        case 5:
            goto L_case5;
    }
    return;

L_case0:
    SetDispMask(0);
    Mem_AllocAuxWithImages(1);
    goto advance;

L_case1:
    key          = ((SessionBytesAt4*)Game_Session)->field_4;
    key.data[0]  = 0x64;
    slotParam[0] = Stream_FindSlot(key.data, 0, 0);
    CdCmd_Enqueue(0x61, 0, slotParam);
    goto advance;

L_case2:
    if (queue->field_1FA == 0) {
        return;
    }
    SetDispMask(1);
    task->killCountdown = 0;
    task->spawnArg1     = 0;
    task->state         = task->state + 1;
    return;

L_case3:
    if (++task->killCountdown == 0x443) {
        Stage_RequestFromAreaTable(0);
        task->spawnArg1 = 1;
    }
    if (CdCmd_IsIdle() & 0xFFFF) {
        SetDispMask(0);
        goto advance;
    }
    if (Pad_CheckFlag800() == 0) {
        return;
    }
    SetDispMask(0);
    CdCmd_ActivatePhase1();
    goto advance;

L_case4:
    if ((CdCmd_IsIdle() & 0xFFFF) == 0) {
        return;
    }
    Stream_ResetRestoreState();
advance:
    task->state = task->state + 1;
    return;

L_case5:
    if ((Stream_RestoreAfterLoad(0, 1) & 0xFFFF) == 0) {
        return;
    }
    if (task->spawnArg1 == 0) {
        Stage_RequestFromAreaTable(0);
    }
    Task_Kill(task);
    Display_ResetHeapWrapper();
}
