#include "common.h"

#include "main/display.h"
#include "main/fs.h"
#include "main/gameflow.h"
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
#include "rooms/room.h"

/// Work block the gas station's shaft sequencer (`func_dryfield_gas_station_801802C0`)
/// allocates as 4 bytes in its state 0 and hangs off `Task::work` (0x1C) for
/// the next run of the state machine to pick up. `child` is the task spawned
/// from `D_dryfield_gas_station_8018312C` entry 0 in state 3 and polled with
/// `Task_PollKill` in state 4.
typedef struct DgsCutsceneSlot {
    /* 0x0 */ Task* child;
} DgsCutsceneSlot;
STATIC_ASSERT_SIZEOF(DgsCutsceneSlot, 0x4);

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

/// Fade task: on its first tick it allocates the 8-byte fade block and seeds
/// its three channels to 0xFF, then every frame it draws the fade overlay (the
/// red channel standing in for blue) and steps each channel down by
/// `Task::spawnArg1`, killing itself once red has gone negative.
void func_dryfield_gas_station_801801E4(Task* arg0)
{
    RoomFadeWork* fade;
    RoomFadeWork* alloc;

    fade = (RoomFadeWork*)arg0->work;
    switch (arg0->state) {
        case 0:
            alloc      = (RoomFadeWork*)Mem_Malloc(8, 0);
            arg0->work = (TaskIdMap*)alloc;
            if (alloc == NULL) {
                taskKill(arg0);
                return;
            }
            fade         = alloc;
            fade->b      = 0xFF;
            fade->g      = 0xFF;
            fade->r      = 0xFF;
            arg0->state += 1;
            /* fallthrough */
        case 1:
            Fade_DrawOverlay((u8)fade->r, (u8)fade->g, (u8)fade->r, 2);
            fade->r = (s16)((u16)fade->r - (u16)arg0->spawnArg1);
            fade->g = (s16)((u16)fade->g - (u16)arg0->spawnArg1);
            fade->b = (s16)((u16)fade->b - (u16)arg0->spawnArg1);
            if (fade->r >= 0) {
                return;
            }
            taskKill(arg0);
            break;
    }
}

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
