#include "common.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stream.h"
#include "main/task.h"
#include "psyq/libgpu.h"

/// Spawn table for the task that takes over once the intro stream is done.
extern TaskDesc D_dryfield_water_tank_80180764;

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank", func_dryfield_water_tank_8017D618);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank", func_dryfield_water_tank_8017D7BC);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank", func_dryfield_water_tank_8017D7C4);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank", func_dryfield_water_tank_8017D7EC);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank", func_dryfield_water_tank_8017D910);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank", func_dryfield_water_tank_8017D948);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank", func_dryfield_water_tank_8017D9D4);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank", func_dryfield_water_tank_8017DA4C);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank", D_dryfield_water_tank_8017D5C0);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank", func_dryfield_water_tank_8017DAF0);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank", func_dryfield_water_tank_8017DB48);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank", func_dryfield_water_tank_8017DB98);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank", func_dryfield_water_tank_8017DD20);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank", func_dryfield_water_tank_8017DEA4);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank", func_dryfield_water_tank_8017E0B4);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank", func_dryfield_water_tank_8017E0E8);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank", func_dryfield_water_tank_8017E174);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank", func_dryfield_water_tank_8017E194);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank", func_dryfield_water_tank_8017E1B4);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank", func_dryfield_water_tank_8017E220);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank", func_dryfield_water_tank_8017E3C4);

/// Water-tank intro cutscene driver: fades out, streams the room's movie via
/// CdCmd, and on completion clears the image buffers and hands off to the
/// follow-up task.
void func_dryfield_water_tank_8017E568(Task* task)
{
    u8          slotParam[4];
    GBytes8     key;
    CdCmdQueue* queue;
    s16         slot;

    queue = &CdCmd_Queue;
    switch (task->state) {
        case 0:
            SetDispMask(0);
            Mem_AllocAuxWithImages(1);
            SndEvt_EnqueueType7(0x52150009, 0x3C);
            task->state = task->state + 1;
            return;
        case 1:
            key          = ((SessionBytesAt4*)Game_Session)->field_4;
            key.data[0]  = 0x64;
            slot         = Stream_FindSlot(key.data, 0, 0);
            slotParam[0] = slot;
            CdCmd_Enqueue(0x61, 0, slotParam);
            task->state = task->state + 1;
            return;
        case 2:
            if (queue->field_1FA == 0) {
                return;
            }
            SndEvt_EnqueueType6(0x52150006, 0, 0);
            SndEvt_EnqueueType6(0x52150007, 0, 0);
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
            SndEvt_EnqueueType7(0x52150006, 0x1E);
            SndEvt_EnqueueType7(0x52150007, 0x1E);
            SetDispMask(0);
            CdCmd_ActivatePhase1();
            task->state = task->state + 1;
            return;
        case 4:
            if ((CdCmd_IsIdle() & 0xFFFF) == 0) {
                return;
            }
            Stream_ResetRestoreState();
            task->state = task->state + 1;
            return;
        case 5:
            if ((Stream_RestoreAfterLoad(0, 1) & 0xFFFF) == 0) {
                return;
            }
            SndEvt_EnqueueType6(0x52150009, 0, 0);
            Mem_Set(Fs_ImgBuffers, 0, 0x25800);
            SetDispMask(1);
            Task_Kill(task);
            Task_SpawnOnDefaultList(&D_dryfield_water_tank_80180764, 2, 8, 0);
            Display_ResetHeapWrapper();
            return;
    }
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank", func_dryfield_water_tank_8017E78C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank", func_dryfield_water_tank_8017E9F8);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank", func_dryfield_water_tank_8017EB80);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank", func_dryfield_water_tank_8017EBA0);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank", func_dryfield_water_tank_8017EC38);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank", func_dryfield_water_tank_8017EC6C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank", func_dryfield_water_tank_8017ED30);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank", func_dryfield_water_tank_8017EDF4);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank", func_dryfield_water_tank_8017EFF4);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank", func_dryfield_water_tank_8017F084);
