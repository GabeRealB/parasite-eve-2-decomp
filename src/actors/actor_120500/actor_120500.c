#include "common.h"

#include "actors/actor_120500.h"
#include "gameplay/1BC.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/tmd.h"

extern TaskDesc D_actor_120500_80138418;

extern u32 D_actor_120500_80138088[];
extern u32 D_actor_120500_80138408[];

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

/// Builds the actor's work block for its scene: `Mem_Malloc(0x4CC, 0)`, and
/// on failure it kills the task and returns.  The allocation's `Game_GetPtrSlot(3)`
/// task goes to `field_4B4`, the task itself is published in
/// `D_actor_120500_80138454`, and the model in `Task::extra` gets its
/// coordinate parented to `Gfx_ViewCoord` plus the light and colour matrices at
/// 0x474 / 0x494.  The area placement record is then walked to the one whose id
/// is 0x65 -- as in the sibling actors, the search stops on either that id or
/// the 0xFF terminator -- and its two texture bytes are handed to
/// `Gp_SetTmdBytes` before the animation banks seed the work block.
/// Finally `Task::field_24` takes the message table and all twenty animation
/// slots are armed, slot 0 excepted.
void func_actor_120500_801322A0(Task* arg0)
{
    Actor120500Work* work;
    Actor120500Work* slotsWork;
    TaskIdMap*       map;
    TmdObject*       tmd;
    GsCOORDINATE2*   coord;
    GpAreaPlace*     place;
    s32              i;
    u8               id;

    tmd         = arg0->extra;
    coord       = tmd->field_8;
    map         = Mem_Malloc(0x4CC, 0);
    arg0->idMap = map;
    if (map == NULL) {
        Task_Kill(arg0);
        return;
    }
    work = (Actor120500Work*)map;
    Mem_Set(work, 0, 0x4CC);
    work->field_4B4         = Game_GetPtrSlot(3);
    D_actor_120500_80138454 = (Actor120500*)arg0;
    coord->sub              = &Gfx_ViewCoord;
    tmd->field_1C           = &work->field_474;
    tmd->field_C            = 0;
    tmd->field_20           = &work->field_494;
    place                   = (GpAreaPlace*)Gp_GetNestedAreaRec((GpAreaKey*)&Game_Session->field_4)->field_0;
    id                      = place->field_0;
    while (id != 0xFF) {
        if (id == 0x65) {
            break;
        }
        place++;
        id = place->field_0;
    }
    Gp_SetTmdBytes(tmd, ((s8*)place)[0xD], ((s8*)place)[0xE]);
    func_800B3F84(&work->anim, D_actor_120500_80138088, (GpAnimObj*)tmd, work->field_334, work->slots);
    slotsWork      = (Actor120500Work*)arg0->idMap;
    arg0->field_24 = &D_actor_120500_80138408;
    i              = 1;
    do {
        slotsWork->slots[(u16)i].field_9 = 0x10;
        Gp_AnimResetSlot(&slotsWork->anim, (u16)i, 1);
        i++;
    } while ((u16)i < 0x14U);
}

INCLUDE_ASM("actors/nonmatchings/actor_120500/actor_120500", func_actor_120500_8013241C);
