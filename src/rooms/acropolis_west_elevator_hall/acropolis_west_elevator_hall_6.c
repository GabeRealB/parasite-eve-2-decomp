#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/1A8.h"
#include "gameplay/3688.h"
#include "gameplay/3CD8.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

#include "rooms/acropolis_west_elevator_hall.h"
#include "rooms/room_common.h"

/// First state of the hall's mirror task: re-attaches the player's own TMD
/// source to this task so the reflection draws the player's model, allocates
/// the `RoomMirrorWork` block the reflection's coordinate frame and matrices
/// live in, and reparents the task under the player task. `spawnArg1` must be 0
/// or 1, and 0 also raises `GameSession::field_4E`. For each held-object task
/// the player has (`GameActor::field_920` / `field_924`) it spawns a reflection
/// from entry 1 of the mirror's task table and reparents it under that task,
/// then runs the mirror's per-frame update once.
void func_acropolis_west_elevator_hall_8017D5FC(Task* task)
{
    Task*           owner;
    GameActor*      actor;
    TmdObject*      extra;
    GsCOORDINATE2*  parts;
    RoomMirrorWork* work;
    Task*           child;
    Task*           spawned;
    s32             i;

    owner = gameGetPtrSlot(3);
    if (Gp_AttachTmd(task, ((TmdObject*)owner->extra)->source) == NULL) {
        taskKill(task);
        return;
    }
    extra = task->extra;
    parts = extra->coords;
    if ((u32)task->spawnArg1 >= 2U) {
        taskKill(task);
        return;
    }
    work = memCalloc(sizeof(RoomMirrorWork), 0);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->work   = (TaskIdMap*)work;
    extra->tpage = 6;
    tmdProcessStream(extra);
    tmdProcessStream(extra);
    extra->flags    = 0x10;
    extra->otOffset = 0x1F;
    if (task->spawnArg1 == 0) {
        gGameSession->field_4E = 1;
    }
    parts->sub      = &work->coord;
    extra->lightMtx = &work->light;
    extra->colorMtx = &work->color;
    Task_Reparent(owner, task);
    task->state++;
    work->viewFlg   = gGfxViewCoord.flg & 0x7FFFFFFF;
    work->field_4   = 1;
    work->configRev = -1;
    extra->flags   |= 0x80;
    work->field_4   = 0;
    work->viewFlg   = -1;
    actor           = (GameActor*)owner->work;
    for (i = 0; i < 2; i++) {
        child = (&actor->field_920)[i];
        if (child != NULL) {
            spawned = Task_SpawnFromTable(D_acropolis_west_elevator_hall_801802A8, 1, i, (s32)task);
            if (spawned != NULL) {
                Task_Reparent(child, spawned);
            }
        }
    }
    func_acropolis_west_elevator_hall_8017D7B0(task);
}
