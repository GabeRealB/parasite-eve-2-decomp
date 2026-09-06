#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/1A8.h"
#include "gameplay/3688.h"
#include "gameplay/3CD8.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

#include "rooms/room_common.h"
#include "rooms/rooms_shared_8017d5f0.h"

/// Sets up the room's mirror: re-attaches the player's own TMD source
/// to this task so the reflection draws the same model, allocates the
/// `RoomMirrorWork` block the reflection's coordinate frame and matrices live
/// in, and hangs the task off the player task so it dies with it. `spawnArg1`
/// selects which of the hall's two mirrors this is; mirror 0 also raises
/// `GameSession::field_4E`. The two child tasks mirror the player's held-object
/// tasks (`GameActor::field_920` / `field_924`).
void RoomsShared8017d5f0(Task* task)
{
    Task*           owner;
    GameActor*      actor;
    TmdObject*      extra;
    GsCOORDINATE2*  parts;
    RoomMirrorWork* work;
    Task*           child;
    Task*           spawned;
    s32             i;

    owner = Game_GetPtrSlot(3);
    if (Gp_AttachTmd(task, ((TmdObject*)owner->extra)->field_10) == NULL) {
        Task_Kill(task);
        return;
    }
    extra = task->extra;
    parts = extra->field_8;
    if ((u32)task->spawnArg1 >= 2U) {
        Task_Kill(task);
        return;
    }
    work = Mem_Calloc(sizeof(RoomMirrorWork), 0);
    if (work == NULL) {
        Task_Kill(task);
        return;
    }
    task->idMap     = (TaskIdMap*)work;
    extra->field_24 = 6;
    Tmd_ProcessStream(extra);
    Tmd_ProcessStream(extra);
    extra->field_C = 0x10;
    extra->field_E = 0x1F;
    if (task->spawnArg1 == 0) {
        Game_Session->field_4E = 1;
    }
    parts->sub      = &work->coord;
    extra->field_1C = &work->light;
    extra->field_20 = &work->color;
    Task_Reparent(owner, task);
    task->state++;
    work->viewFlg   = Gfx_ViewCoord.flg & 0x7FFFFFFF;
    work->field_4   = 1;
    work->configRev = -1;
    extra->field_C |= 0x80;
    work->field_4   = 0;
    work->viewFlg   = -1;
    actor           = (GameActor*)owner->idMap;
    for (i = 0; i < 2; i++) {
        child = (&actor->field_920)[i];
        if (child != NULL) {
            spawned = Task_SpawnFromTable(&RoomsShared8017d5f0Desc, 1, i, (s32)task);
            if (spawned != NULL) {
                Task_Reparent(child, spawned);
            }
        }
    }
    RoomsShared8017d7a4(task);
}
