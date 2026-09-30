#include "dryfield_gas_station_private.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "gameplay/hud_sprites.h"

#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflow.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/stage.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/task_types.h"

#include "overlay.h"
#include "../../shared/screen_fade.h"

/// Work block the gas station's shaft sequencer (`func_dryfield_gas_station_801802C0`)
/// allocates as 4 bytes in its state 0 and hangs off `Task::work` (0x1C) for
/// the next run of the state machine to pick up. `child` is the task spawned
/// from `D_dryfield_gas_station_8018312C` entry 0 in state 3 and polled with
/// `Task_PollKill` in state 4.
typedef struct DgsCutsceneSlot {
    /* 0x0 */ Task* child;
} DgsCutsceneSlot;
STATIC_ASSERT_SIZEOF(DgsCutsceneSlot, 0x4);

void func_dryfield_gas_station_8017FFE4(Task* arg0)
{
    u8          slotParam[4];
    GameLoc     key;
    CdCmdQueue* queue;
    s16         slot;
    Task*       task;

    task  = arg0;
    queue = &gCdCmdQueue;
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
    key          = gGameSession->location;
    key.loc.view = 0x64;
    slot         = Stream_FindSlot((u8*)&key, 0, 0);
    slotParam[0] = slot;
    CdCmd_Enqueue(CD_COMMAND_PLAY_STREAM, 0, slotParam);
    goto advance;

L_case2:
    if (queue->movieReady == 0) {
        return;
    }
    task->killCountdown   = 0;
    task->spawnArg1.value = 0;
    SetDispMask(1);
    goto advance;

L_case3:
    if (++task->killCountdown == 0x186) {
        task->spawnArg1.value = 1;
        Stage_RequestFromAreaTable(0xA);
    }
    if (CdCmd_IsIdle() & 0xFFFF) {
        SetDispMask(0);
        goto advance;
    }
    if (Pad_CheckFlag800() == 0) {
        return;
    }
    if (task->spawnArg1.value == 0) {
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

#include "../../shared/screen_fade_in.inc.c"

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
    task->work = child;
    if (child == NULL) {
        taskKill(task);
        return;
    }
    Display_SpawnWithOt(D_dryfield_gas_station_80181E7C, 1, 0, 0);
    gDisplayState.control.flags.flipMode = DISPLAY_FLIP_TASK_ONLY;
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
