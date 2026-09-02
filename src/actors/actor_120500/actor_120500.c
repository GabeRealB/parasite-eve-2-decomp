#include "common.h"

#include "actors/actor_120500.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stream.h"
#include "main/task.h"

extern TaskDesc D_actor_120500_80138418;

void func_actor_120500_80131E58(Task* arg0)
{
    u8          slotParam[4];
    GBytes8     key;
    CdCmdQueue* queue;
    s16         slot;
    Task*       task;

    task  = arg0;
    queue = &CdCmd_Queue;
    switch (task->state) {
        case 0:
            SetDispMask(0);
            Mem_AllocAuxWithImages(1);
            goto advance;
        case 1:
            key          = ((SessionBytesAt4*)Game_Session)->field_4;
            key.data[0]  = 0x64;
            slot         = Stream_FindSlot(key.data, 0, 0);
            slotParam[0] = slot;
            CdCmd_Enqueue(0x61, 0, slotParam);
            goto advance;
        case 2:
            if (queue->field_1FA == 0) {
                return;
            }
            SndEvt_EnqueueType6(0x521E0007, 0, 0);
            SetDispMask(1);
            goto advance;
        case 3:
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
        case 4:
            if ((CdCmd_IsIdle() & 0xFFFF) == 0) {
                return;
            }
            Stream_ResetRestoreState();
        advance:
            task->state = task->state + 1;
            return;
        case 5:
            if ((Stream_RestoreAfterLoad(0, 1) & 0xFFFF) == 0) {
                return;
            }
            Task_SpawnOnDefaultList(&D_actor_120500_80138418, 1, 8, 0);
            Mem_Set(Fs_ImgBuffers, 0, 0x25800);
            SetDispMask(1);
            Task_Kill(task);
            Display_ResetHeapWrapper();
            return;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_120500/actor_120500", func_actor_120500_80132028);

INCLUDE_ASM("actors/nonmatchings/actor_120500/actor_120500", func_actor_120500_801322A0);

INCLUDE_ASM("actors/nonmatchings/actor_120500/actor_120500", func_actor_120500_8013241C);

INCLUDE_ASM("actors/nonmatchings/actor_120500/actor_120500", func_actor_120500_80132708);

INCLUDE_ASM("actors/nonmatchings/actor_120500/actor_120500", func_actor_120500_801327E4);

INCLUDE_ASM("actors/nonmatchings/actor_120500/actor_120500", func_actor_120500_801328C0);

INCLUDE_ASM("actors/nonmatchings/actor_120500/actor_120500", func_actor_120500_801328E0);

void func_actor_120500_80132900(s16 arg0)
{
    Actor120500Work* work = D_actor_120500_80138454->field_1C;

    work->field_4C8 = arg0;
    work->field_4CA = 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_120500/actor_120500", func_actor_120500_80132920);

INCLUDE_ASM("actors/nonmatchings/actor_120500/actor_120500", func_actor_120500_80132A04);

INCLUDE_ASM("actors/nonmatchings/actor_120500/actor_120500", func_actor_120500_80132A74);
