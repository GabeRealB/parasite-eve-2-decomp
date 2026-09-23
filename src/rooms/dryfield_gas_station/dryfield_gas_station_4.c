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

extern s8       D_8007106B;
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
