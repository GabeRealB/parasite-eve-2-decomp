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
#include "gameplay/gameplay.h"

#include "rooms/dryfield_gas_station.h"
#include "rooms/rooms_shared_80180b2c.h"

extern s32      D_dryfield_gas_station_80182E30;
extern s32      D_dryfield_gas_station_80182E74;
extern s32      D_dryfield_gas_station_80182E8C;
extern s32      D_dryfield_gas_station_8018303C;
extern s8       D_8007106B;
extern u8       D_80071075;
extern s8       D_80114C12;
extern void     Stage_RequestFromAreaTable(s32 arg0);
extern TaskDesc D_dryfield_gas_station_80181E7C[];
extern TaskDesc D_dryfield_gas_station_8018312C[];

void func_dryfield_gas_station_8017FFE4(Task* arg0)
{
    u8          slotParam[4];
    GameLoc     key;
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
    key          = gGameSession->at4;
    key.loc.view = 0x64;
    slot         = Stream_FindSlot(key.raw.data, 0, 0);
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
    taskKill(task);
    Task_SpawnOnDefaultList(D_dryfield_gas_station_80181E7C, 2, 8, 0);
    Display_ResetHeapWrapper();
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_gas_station/dryfield_gas_station_4", func_dryfield_gas_station_801801E4);

/// Runs the gas station's shaft sequence. State 0 parks the 4-byte child slot
/// in `Task::work`, spawns the `D_dryfield_gas_station_80181E7C` entry 1
/// loader through `Display_SpawnWithOt` and turns the view tasks on; states 1
/// and 2 only step, so state 3 spawns the cutscene task from
/// `D_dryfield_gas_station_8018312C` entry 0 into that slot, and state 4 kills
/// this task once the cutscene has died. The slot is read at function entry,
/// before state 0 writes the freshly allocated block, so only a later run of
/// the state machine sees it.
void func_dryfield_gas_station_801802C0(Task* task)
{
    DgsCutsceneSlot* slot;
    void*            child;
    s32              killed;

    slot = (DgsCutsceneSlot*)task->work;
    switch (task->state) {
        case 0:
            goto L_case0;
        case 1:
            goto advance;
        case 2:
            goto advance;
        case 3:
            goto L_case3;
        case 4:
            goto L_case4;
    }
    return;

L_case0:
    child      = Mem_Malloc(4, false);
    task->work = (TaskIdMap*)child;
    if (child == NULL) {
        taskKill(task);
        return;
    }
    Display_SpawnWithOt(D_dryfield_gas_station_80181E7C, 1, 0, 0);
    D_8007106B = 1;
    Gp_SpawnViewTasks();
    goto advance;

L_case3:
    child       = Task_SpawnFromTable(D_dryfield_gas_station_8018312C, 0, 0, 0);
    slot->child = child;
    if (child == NULL) {
        goto L_kill;
    }

advance:
    task->state = task->state + 1;
    return;

L_case4:
    if (Task_PollKill(slot->child, &killed) == 0) {
        return;
    }
L_kill:
    Task_RequestKill(task, 0);
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_gas_station/dryfield_gas_station_4", func_dryfield_gas_station_801803C0);

/// Spawns the gas station's cutscene owner. State 0 refuses to run twice (a
/// `D_80114C12` of 1 and a live `D_80071075` both mean the cutscene is already
/// up), otherwise it parks the freshly zeroed 0x10-byte `DgsWork` block in
/// `Task::work`, fills `owner` from pointer slot 3 and republishes this task as
/// `RoomsShared80180b2cTask` so the room's script helpers can reach that block.
/// Two kills: a failed `Mem_Malloc` kills the task outright, and state 1 kills
/// it once the session has torn down (`gGameSession->eventState`). Between the two
/// it hands slot 3 the `D_dryfield_gas_station_80182E30` script record as msg
/// 0x3F4 -- only when a previous state 0 already found an owner, since the
/// reloaded `work` is dereferenced unconditionally.
void func_dryfield_gas_station_801807E0(Task* task)
{
    DgsWork* work;
    DgsWork* work2;
    GpRec14  script;

    switch (task->state) {
        case 0:
            if ((D_80114C12 != 1) && (D_80071075 == 0)) {
                work       = Mem_Malloc(0x10, false);
                task->work = (TaskIdMap*)work;
                if (work == NULL) {
                    taskKill(task);
                } else {
                    Mem_Set(work, 0, 0x10);
                    work->owner             = gameGetPtrSlot(3);
                    RoomsShared80180b2cTask = task;
                }
                work2 = (DgsWork*)task->work;
                if (work2->owner != 0) {
                    script.field_0  = (s32)&D_dryfield_gas_station_80182E30;
                    script.field_4  = 0;
                    script.field_8  = 0;
                    script.field_C  = 0;
                    script.field_10 = 0;
                    Gp_DispatchMsg((Task*)work2->owner, 0x3F4, (s32)&script, 0);
                }
                func_800E3FAC(0xA2, 9);
                func_800E8634((s32)&D_dryfield_gas_station_80182E8C, 0,
                              (s32)&D_dryfield_gas_station_8018303C);
                task->state = task->state + 1;
                return;
            }
            return;

        case 1:
            if (gGameSession->eventState == 0) {
                Task_RequestKill(task, 0);
                return;
            }
            func_dryfield_gas_station_801803C0(task);
            break;
    }
}

/// Latches the player-effect flag and kills the effects once. The 1 is loaded
/// before the branch and stored in the `jal` delay slot.
void func_dryfield_gas_station_80180944(void)
{
    DgsWork* work = (DgsWork*)RoomsShared80180b2cTask->work;
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
    work = (DgsWork*)task->work;
    if (work->playerEffActive != 0) {
        Gp_SpawnWeaponEff();
        work->playerEffActive = 0;
        Gp_MsgPlayerWeapon(0);
    }
    Gp_DispatchMsg((Task*)work->owner, 0x3E9, (s32)&D_dryfield_gas_station_80182E74, 0);
    work2 = (DgsWork*)task->work;
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
