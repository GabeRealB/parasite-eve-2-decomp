#include "common.h"

#include "main/display.h"
#include "main/fs.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/stream.h"
#include "main/task.h"

void func_acropolis_plaza_8017DBFC(Task* arg0)
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
        case 6:
            goto L_case6;
    }
    return;

L_case0:
    SetDispMask(0);
    Mem_AllocAuxWithImages(1);
    goto advance;

L_case1:
    key          = ((SessionBytesAt4*)Game_Session)->field_4;
    key.data[0]  = 0x64;
    slot         = Stream_FindSlot(key.data, 0, 0);
    slotParam[0] = slot;
    CdCmd_Enqueue(0x61, 0, slotParam);
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
        SOFT_BARRIER();
        goto advance;
    }
    if (Pad_CheckFlag800() == 0) {
        return;
    }
    CdCmd_ActivatePhase1();
    goto advance;

L_case4:
    if ((CdCmd_IsIdle() & 0xFFFF) == 0) {
        return;
    }
    SetDispMask(0);
    goto advance;

L_case5:
    Stream_ResetRestoreState();
advance:
    task->state = task->state + 1;
    return;

L_case6:
    if ((Stream_RestoreAfterLoad(1, 0) & 0xFFFF) == 0) {
        return;
    }
    Task_Kill(task);
    Display_ResetHeapWrapper();
}

/// Pad word that closes this unit's `.rodata`. The original object aligned its
/// section to 8 after the jump table above, which is what puts the next unit's
/// tables at 0x8017D5E4; nothing reads it.
const u32 D_acropolis_plaza_8017D5E0 = 0;
