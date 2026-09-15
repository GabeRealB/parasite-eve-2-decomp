#include "common.h"

#include "main/display.h"
#include "main/fs.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stream.h"
#include "main/task.h"

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"

#include "rooms/dryfield_gas_station.h"
#include "rooms/rooms_shared_80180b2c.h"

extern s32      D_dryfield_gas_station_80182E30;
extern s32      D_dryfield_gas_station_80182E74;
extern void     Stage_RequestFromAreaTable(s32 arg0);
extern TaskDesc D_dryfield_gas_station_80181E7C[];

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

INCLUDE_ASM("rooms/nonmatchings/dryfield_gas_station/dryfield_gas_station_4", func_dryfield_gas_station_801801E4);

INCLUDE_ASM("rooms/nonmatchings/dryfield_gas_station/dryfield_gas_station_4", func_dryfield_gas_station_801802C0);

INCLUDE_ASM("rooms/nonmatchings/dryfield_gas_station/dryfield_gas_station_4", func_dryfield_gas_station_801803C0);

INCLUDE_ASM("rooms/nonmatchings/dryfield_gas_station/dryfield_gas_station_4", func_dryfield_gas_station_801807E0);

/// Latches the player-effect flag and kills the effects once. The 1 is loaded
/// before the branch and stored in the `jal` delay slot.
void func_dryfield_gas_station_80180944(void)
{
    DgsWork* work = (DgsWork*)RoomsShared80180b2cTask->idMap;
    if (work->playerEffActive == 0) {
        work->playerEffActive = 1;
        Gp_KillPlayerEffs();
    }
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_gas_station/dryfield_gas_station_4", func_dryfield_gas_station_80180984);

/// Tells slot 3 that the cutscene is opening: it ends the weapon effect the
/// player may still be carrying (flag at `DgsWork::playerEffActive`), echoes the
/// equipped weapon back with msg 0x3E9 and, once the cutscene task has an owner,
/// hands that owner the `D_dryfield_gas_station_80182E30` script record as msg
/// 0x3F4. The record is a `GpRec14` built on the stack, only its first field
/// (the script pointer) set.
void func_dryfield_gas_station_80180A60(void)
{
    Task*    task;
    DgsWork* work;
    DgsWork* work2;
    GpRec14  script;

    task = RoomsShared80180b2cTask;
    work = (DgsWork*)task->idMap;
    if (work->playerEffActive != 0) {
        Gp_SpawnWeaponEff();
        work->playerEffActive = 0;
        Gp_MsgPlayerWeapon(0);
    }
    Gp_DispatchMsg((Task*)work->owner, 0x3E9, (s32)&D_dryfield_gas_station_80182E74, 0);
    work2 = (DgsWork*)task->idMap;
    if (work2->owner != 0) {
        script.field_0  = (s32)&D_dryfield_gas_station_80182E30;
        script.field_4  = 0;
        script.field_8  = 0;
        script.field_C  = 0;
        script.field_10 = 0;
        Gp_DispatchMsg((Task*)work2->owner, 0x3F4, (s32)&script, 0);
    }
    SndEvt_EnqueueType7(0x52010011, 0x3C);
    SetDispMask(1);
}
