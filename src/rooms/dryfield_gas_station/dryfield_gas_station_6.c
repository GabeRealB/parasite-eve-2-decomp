#include "common.h"

#include "main/display.h"
#include "main/fs.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/stream.h"
#include "main/task.h"

extern void     Stage_RequestFromAreaTable(s32 arg0);
extern TaskDesc D_dryfield_gas_station_80181E7C[];

INCLUDE_ASM("rooms/nonmatchings/dryfield_gas_station/dryfield_gas_station_6", func_dryfield_gas_station_8017FD54);

INCLUDE_ASM("rooms/nonmatchings/dryfield_gas_station/dryfield_gas_station_6", func_dryfield_gas_station_8017FE20);

INCLUDE_ASM("rooms/nonmatchings/dryfield_gas_station/dryfield_gas_station_6", func_dryfield_gas_station_8017FEDC);

void func_dryfield_gas_station_8017FF84(void)
{
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_gas_station/dryfield_gas_station_6", func_dryfield_gas_station_8017FF8C);

void func_dryfield_gas_station_8017FFE4(Task* arg0)
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
    task->killCountdown = 0;
    task->spawnArg1     = 0;
    SetDispMask(1);
    goto advance;

L_case3:
    if (++task->killCountdown == 0x186) {
        task->spawnArg1 = 1;
        Stage_RequestFromAreaTable(0xA);
    }
    if (CdCmd_IsIdle() & 0xFFFF) {
        SetDispMask(0);
        goto advance;
    }
    if (Pad_CheckFlag800() == 0) {
        return;
    }
    if (task->spawnArg1 == 0) {
        Stage_RequestFromAreaTable(0xA);
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
    Mem_Set(Fs_ImgBuffers, 0, 0x25800);
    Task_Kill(task);
    Task_SpawnOnDefaultList(D_dryfield_gas_station_80181E7C, 2, 8, 0);
    Display_ResetHeapWrapper();
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_gas_station/dryfield_gas_station_6", func_dryfield_gas_station_801801E4);

INCLUDE_ASM("rooms/nonmatchings/dryfield_gas_station/dryfield_gas_station_6", func_dryfield_gas_station_801802C0);

INCLUDE_ASM("rooms/nonmatchings/dryfield_gas_station/dryfield_gas_station_6", func_dryfield_gas_station_801803C0);

INCLUDE_ASM("rooms/nonmatchings/dryfield_gas_station/dryfield_gas_station_6", func_dryfield_gas_station_801807E0);

INCLUDE_ASM("rooms/nonmatchings/dryfield_gas_station/dryfield_gas_station_6", func_dryfield_gas_station_80180944);

INCLUDE_ASM("rooms/nonmatchings/dryfield_gas_station/dryfield_gas_station_6", func_dryfield_gas_station_80180984);

INCLUDE_ASM("rooms/nonmatchings/dryfield_gas_station/dryfield_gas_station_6", func_dryfield_gas_station_80180A60);

INCLUDE_ASM("rooms/nonmatchings/dryfield_gas_station/dryfield_gas_station_6", func_dryfield_gas_station_80180B2C);
