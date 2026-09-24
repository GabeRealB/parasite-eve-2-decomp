#include "common.h"

#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/stream.h"
#include "main/task.h"

extern s8 D_8007106B;
/// The stream playback descriptors: the one-shot launcher, then the player.
extern TaskDesc D_shelter_b6_training_room_8018431C[];

/// Task that plays the stream filed under the current location with view 0x64.
/// It blanks the display and allocates the image buffers, queues the stream,
/// shows it once the CD queue reports it started, and waits for it to finish
/// or for the pad to skip it. It then restores the room from the stream and,
/// unless skipped, holds for 0x3C more frames before killing itself and
/// resetting the display heap.
void func_shelter_b6_training_room_8017DBBC(Task* arg0)
{
    u8          slotParam[4];
    s32         state;
    GameLoc     key;
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
        case 6:
            goto L_case6;
    }
    return;

L_case0:
    SetDispMask(0);
    Mem_AllocAuxWithImages(1);
    goto advance;

L_case1:
    key          = gGameSession->at4;
    key.loc.view = 0x64;
    slotParam[0] = Stream_FindSlot(key.raw.data, 0, 0);
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
        state           = task->state;
        task->spawnArg1 = 0;
        task->state     = state + 1;
        return;
    }
    if (Pad_CheckFlag800() == 0) {
        return;
    }
    SetDispMask(0);
    CdCmd_ActivatePhase1();
    task->spawnArg1 = 1;
    task->state     = task->state + 1;
    return;

L_case4:
    if ((CdCmd_IsIdle() & 0xFFFF) == 0) {
        return;
    }
    Stream_ResetRestoreState();
    goto advance;

L_case5:
    if ((Stream_RestoreAfterLoad(0, 1) & 0xFFFF) == 0) {
        return;
    }
    if (task->spawnArg1 != 0) {
        goto kill;
    }
advance:
    task->state = task->state + 1;
    return;

L_case6:
    task->killCountdown = task->killCountdown + 1;
    if (task->killCountdown < 0x3D) {
        return;
    }
kill:
    taskKill(task);
    Display_ResetHeapWrapper();
}

/// One-shot task: spawns the stream player, the second entry of the room's
/// descriptor pair, as the display's owning task, sets `D_8007106B`, spawns
/// the view tasks and kills itself.
void func_shelter_b6_training_room_8017DD98(Task* arg0)
{
    Display_SpawnWithOt(D_shelter_b6_training_room_8018431C, 1, 0, 0);
    D_8007106B = 1;
    Gp_SpawnViewTasks();
    taskKill(arg0);
}
