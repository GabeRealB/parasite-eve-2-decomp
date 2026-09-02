#include "common.h"

#include "main/display.h"
#include "main/fs.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/stream.h"
#include "main/task.h"

s32 func_neo_ark_altar_8017D908(void)
{
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/neo_ark_altar/neo_ark_altar_3", func_neo_ark_altar_8017D910);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_altar/neo_ark_altar_3", func_neo_ark_altar_8017D974);

void func_neo_ark_altar_8017D9E0(void)
{
}

INCLUDE_ASM("rooms/nonmatchings/neo_ark_altar/neo_ark_altar_3", func_neo_ark_altar_8017D9E8);

void func_neo_ark_altar_8017DA40(Task* arg0)
{
    u8          slotParam[4];
    GBytes8     key;
    s16         slot;
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
    key = ((SessionBytesAt4*)Game_Session)->field_4;
    if (task->spawnArg1 == 0) {
        key.data[0] = 0x64;
    } else if (task->spawnArg1 == 1) {
        key.data[0] = 0x65;
    } else {
        key.data[0] = 0x66;
    }
    slot = Stream_FindSlot(key.data, 0, 0);
    {
        s32 cmd;
        s32 zero;
        u8* p;
        cmd  = 0x61;
        zero = 0;
        p    = slotParam;
        SOFT_TOUCH_REG4(cmd, zero, p, slot);
        slotParam[0] = slot;
        CdCmd_Enqueue(cmd, zero, p);
    }
    goto advance;

L_case2:
    if (queue->field_1FA == 0) {
        return;
    }
    SetDispMask(1);
    goto advance;

L_case3:
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
    Task_Kill(task);
    Display_ResetHeapWrapper();
}
