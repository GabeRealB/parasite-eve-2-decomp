#include "common.h"

#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/stream.h"
#include "main/task.h"

extern s8       D_8007106B;
extern s16      D_80071076;
extern s8       D_801153F4;
extern TaskDesc D_shelter_1f_bulwark_80180360;

void func_shelter_1f_bulwark_8017DC78(Task* arg0)
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
    Task_Kill(task);
    Display_ResetHeapWrapper();
}

void func_shelter_1f_bulwark_8017DE04(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            Display_SpawnWithOt(&D_shelter_1f_bulwark_80180360, 1, 0, 0);
            D_8007106B = 1;
            Gp_SpawnViewTasks();
            D_801153F4 = 1;
            /* fallthrough */
        case 1:
        case 2:
            arg0->state = arg0->state + 1;
            break;
        case 3:
            Mc_SaveData.field_7 = 5;
            Mc_SaveData.field_6 = 0x1A;
            Mc_SaveData.field_8 = 1;
            Mc_SaveData.field_5 = 1;
            D_80071076          = 1;
            Fs_BeginBootLoad(&Mc_SaveData.field_4, 0);
            Task_Spawn(0, 0x11, 0x10, 0);
            Task_Kill(arg0);
            break;
    }
}
