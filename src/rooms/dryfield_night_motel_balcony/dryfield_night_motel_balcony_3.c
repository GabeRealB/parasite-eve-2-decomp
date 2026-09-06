#include "common.h"

#include "main/display.h"
#include "main/fs.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/wipsys.h"
#include "psyq/libgpu.h"

extern void func_dryfield_night_motel_balcony_8017E250(s32 arg0, s16 arg1);

void func_dryfield_night_motel_balcony_8017DDD0(Task* task)
{
    u8          slotParam[4];
    GBytes8     introKey;
    GBytes8     loopKey;
    CdCmdQueue* queue;
    s16         slot;

    queue = &CdCmd_Queue;
    switch (task->state) {
        case 0:
            SetDispMask(0);
            Mem_AllocAuxWithImages(1);
            task->state = task->state + 1;
            return;
        case 1:
            introKey = ((SessionBytesAt4*)Game_Session)->field_4;
            if (Wip_SysFlags.field_0 == 2) {
                introKey.data[0] = 0x67;
            } else {
                introKey.data[0] = 0x65;
            }
            slot         = Stream_FindSlot(introKey.data, 0, 0);
            slotParam[0] = slot;
            CdCmd_Enqueue(0x61, 0, slotParam);
            task->state = task->state + 1;
            return;
        case 2:
            if (queue->field_1FA == 0) {
                return;
            }
            SetDispMask(1);
            task->state = task->state + 1;
            return;
        case 3:
            if (CdCmd_IsIdle() & 0xFFFF) {
                SetDispMask(0);
                task->state = task->state + 1;
                return;
            }
            if (Pad_CheckFlag800() == 0) {
                return;
            }
            SetDispMask(0);
            CdCmd_ActivatePhase1();
            task->state = 7;
            return;
        case 4:
            if (CdCmd_IsIdle() & 0xFFFF) {
                loopKey = ((SessionBytesAt4*)Game_Session)->field_4;
                if (Wip_SysFlags.field_0 == 2) {
                    loopKey.data[0] = 0x66;
                } else {
                    loopKey.data[0] = 0x64;
                }
                slot         = Stream_FindSlot(loopKey.data, 0, 0);
                slotParam[0] = slot;
                CdCmd_Enqueue(0x61, 0, slotParam);
            }
            task->state = task->state + 1;
            return;
        case 5:
            if (queue->field_1FA == 0) {
                return;
            }
            SetDispMask(1);
            task->state = task->state + 1;
            return;
        case 6:
            if (CdCmd_IsIdle() & 0xFFFF) {
                SetDispMask(0);
                task->state = task->state + 1;
                return;
            }
            if (Pad_CheckFlag800() == 0) {
                return;
            }
            SetDispMask(0);
            CdCmd_ActivatePhase1();
            task->state = task->state + 1;
            return;
        case 7:
            if ((CdCmd_IsIdle() & 0xFFFF) == 0) {
                return;
            }
            Stream_ResetRestoreState();
            task->state = task->state + 1;
            return;
        case 8:
            if ((Stream_RestoreAfterLoad(0, 1) & 0xFFFF) == 0) {
                return;
            }
            Task_Kill(task);
            Display_ResetHeapWrapper();
            return;
    }
}
