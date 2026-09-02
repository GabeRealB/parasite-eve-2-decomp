#include "common.h"

#include "gameplay/268.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/stream.h"
#include "main/task.h"

extern s16 D_80071076;

extern s32 D_mist_parking_80195320;

extern TaskDesc D_mist_parking_8018D75C;
extern Task*    D_mist_parking_80195324;

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_9", func_mist_parking_8018345C);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_9", func_mist_parking_801834D4);

void func_mist_parking_8018354C(void)
{
    Task_SpawnFromTable(&D_mist_parking_8018D75C, 3, 0, 0);
}

void func_mist_parking_8018357C(Task* arg0)
{
    func_800BC4E4();
    Mc_SaveData.field_7 = 2;
    Mc_SaveData.field_6 = 1;
    Mc_SaveData.field_8 = 1;
    Mc_SaveData.field_5 = 1;
    D_80071076          = 1;
    Fs_BeginBootLoad(&Mc_SaveData.field_4, 1);
    SndEvt_EnqueueType7(0x80000000, 0);
    Task_Spawn(0, 0x11, 0, 0);
    Task_Kill(arg0);
}

void func_mist_parking_80183600(void)
{
    D_mist_parking_80195324 = Task_SpawnFromTable(&D_mist_parking_8018D75C, 4, 0, 0);
}

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_9", func_mist_parking_80183634);

void func_mist_parking_80183688(s32 arg0)
{
    Display_InitModeObj(Task_GetDescAt(&D_mist_parking_8018D75C, 5U), arg0, 0, 0);
}

void func_mist_parking_801836CC(Task* arg0)
{
    s32 temp_v0;

    temp_v0         = arg0->spawnArg1 - 1;
    arg0->spawnArg1 = temp_v0;
    if (temp_v0 < 0) {
        Task_Kill(arg0);
        Stage_SetEndingFlag();
    }
}

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_9", func_mist_parking_80183708);

void func_mist_parking_80183780(s32 arg0)
{
    GameFlag_SetNibble(0xF1, arg0);
}

void func_mist_parking_801837A4(void)
{
    D_mist_parking_80195320 = 0;
    D_mist_parking_80195324 = 0;
}

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

INCLUDE_RODATA("rooms/nonmatchings/mist_parking/mist_parking_9", D_mist_parking_8017D85C);
