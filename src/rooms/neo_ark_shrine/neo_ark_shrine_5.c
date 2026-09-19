#include "common.h"

#include "main/display.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/room_common.h"

/// Scratch state of the shrine's cap script, stored at `Task::work`
/// (`memCalloc(0x10)` in `func_neo_ark_shrine_8017ECC4`).
typedef struct {
    /* 0x00 */ u8  pad_0[8];
    /* 0x08 */ u16 timer; ///< frames the current script step has run
    /* 0x0A */ u8  pad_A[2];
    /* 0x0C */ s16 field_C;
    /* 0x0E */ s8  field_E;
    /* 0x0F */ s8  field_F;
} NeoArkShrineScript;

extern s8          D_8007216C;
extern TaskDesc    D_neo_ark_shrine_80182404[];
extern RoomHotspot D_neo_ark_shrine_80182430[];

/// Task callback of the descriptor at `D_neo_ark_shrine_80182404`: allocates
/// the cap script's state, sets the global mode byte, steps the task on one
/// state and clears the shrine's hotspot list.
void func_neo_ark_shrine_8017ECC4(Task* task)
{
    NeoArkShrineScript* st;
    RoomHotspot*        hs;

    st = memCalloc(0x10, 0);
    if (st == NULL) {
        taskKill(task);
        return;
    }
    task->spawnArg2 = Task_SpawnFromTable(D_neo_ark_shrine_80182404, 0, 1, 0);
    task->work      = (TaskIdMap*)st;
    D_8007216C      = 0xB;
    /* The once-loop folds away, but flow counts its references at loop depth
       2: without it the parameter's priority (6*2/42) loses to the state
       pointer's (3*1/10) and the two swap callee-saved homes. Keeping the
       state load below the mode store is the same loop's scheduling edge. */
    do {
        task->state++;
    } while (0);
    Display_AcquireRef();
    for (hs = D_neo_ark_shrine_80182430; hs->id != -1; hs++) {
        hs->hit = 0;
    }
    gGameSession->cutsceneHold = 1;
    gGameSession->hideHud      = 1;
    gGameSession->eventState   = 1;
}
