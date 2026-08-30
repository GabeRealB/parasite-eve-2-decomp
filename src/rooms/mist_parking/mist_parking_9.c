#include "common.h"

#include "main/display.h"
#include "main/fs.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/stream.h"
#include "main/task.h"

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_9", func_mist_parking_8018345C);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_9", func_mist_parking_801834D4);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_9", func_mist_parking_8018354C);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_9", func_mist_parking_8018357C);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_9", func_mist_parking_80183600);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_9", func_mist_parking_80183634);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_9", func_mist_parking_80183688);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_9", func_mist_parking_801836CC);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_9", func_mist_parking_80183708);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_9", func_mist_parking_80183780);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_9", func_mist_parking_801837A4);

extern void Stage_RequestMidiFromMap(s32 arg0);

void func_mist_parking_801837B8(Task* arg0)
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
    Stage_RequestMidiFromMap(0xA);
    SetDispMask(0);
    Mem_AllocAuxWithImages(1);
    goto advance;

L_case1:
    key = ((SessionBytesAt4*)Game_Session)->field_4;
    if (task->spawnArg1 != 0) {
        key.data[0] = 0x65;
    } else {
        key.data[0] = 0x64;
    }
    slot = Stream_FindSlot(key.data, 0, 0);
    {
        register s32 cmd asm("a0");
        register s32 zero asm("a1");
        register u8* p asm("a2");
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
    if ((Stream_RestoreAfterLoad(0, 0) & 0xFFFF) == 0) {
        return;
    }
    Mem_Set(Fs_ImgBuffers, 0, 0x25800);
    SetDispMask(1);
    Task_Kill(task);
    Display_ResetHeapWrapper();
}

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_9", func_mist_parking_8018397C);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_9", func_mist_parking_801839CC);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_9", func_mist_parking_80183A28);

INCLUDE_RODATA("rooms/nonmatchings/mist_parking/mist_parking_9", D_mist_parking_8017D85C);
