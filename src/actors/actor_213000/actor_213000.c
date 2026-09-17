#include "common.h"

#include "actors/actor_213000.h"

#include "gameplay/1BC.h"
#include "gameplay/D4.h"

#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

/// The actor's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern TaskFuncTable3 D_actor_213000_80149E24;

/// The actor's `TaskDesc` table; the spawn handler starts entries 1 to 4.
extern TaskDesc D_actor_213000_80157DE0;

/// The actor's message table: `(message id, handler)` pairs for 0x7D3 / 0x7D4 /
/// 0x7D5 / 0x7DB, ended by `0x7FFFFFFF`. The spawn handler parks its address in
/// `Task::field_24`.
extern GpMsgEntry D_actor_213000_80157E1C[];

void func_actor_213000_8014A6AC(Task* task);

/// Spawn handler: allocates the 0x4C4 work block, seeds its head, raises the
/// model's 0x80 flag, then spawns the four children the actor's `TaskDesc`
/// table holds -- table entries 1 and 2 parked at `field_4BC` / `field_4C0`,
/// entries 3 and 4 kept here. Each of the two model tasks has
/// `TmdObject::field_24` / `field_25` loaded with the texture page and CLUT row
/// of the `GpAreaPlace` that entry selects, reached through the area key
/// `&Game_Session->field_4` and indexed by the model id the parent's
/// `spawnArg2` carries at `GpEnemy::field_8 >> 12`, and has its texture stream
/// processed twice when it has an aux buffer. The body ends by handing the
/// parent to `func_actor_213000_8014A6AC`, pointing `field_24` at the message
/// table and installing `Gp_EnemyTaskExit` as its exit callback.
void func_actor_213000_80149E54(Task* task)
{
    Actor213000Work* work;
    TmdObject*       obj;
    GpAreaKey        key;
    Task*            spawned1;
    Task*            spawned2;

    obj  = task->extra;
    work = (Actor213000Work*)Mem_Calloc(0x4C4, 0);
    if (work == NULL) {
        Gp_EnemyTaskExit(task);
        return;
    }
    task->idMap     = (TaskIdMap*)work;
    work->field_475 = -1;
    work->field_476 = -1;
    work->field_478 = 0;
    work->field_477 = -1;
    obj->field_C   |= 0x80;
    work->field_4BC = Task_SpawnFromTable(&D_actor_213000_80157DE0, 1, 8, (s32)task);
    work->field_4C0 = Task_SpawnFromTable(&D_actor_213000_80157DE0, 2, 8, (s32)task);
    spawned1        = Task_SpawnFromTable(&D_actor_213000_80157DE0, 3, 9, (s32)task);
    spawned2        = Task_SpawnFromTable(&D_actor_213000_80157DE0, 4, 0xC, (s32)task);
    if (spawned1 != NULL) {
        TmdObject*   model;
        GpAreaRec*   rec;
        GpAreaPlace* place;
        GpAreaKey*   sessionKey;
        s32          idx;

        idx         = ((GpEnemy*)task->spawnArg2)->field_8 >> 12;
        model       = (TmdObject*)spawned1->extra;
        sessionKey  = (GpAreaKey*)&Game_Session->field_4;
        key.field_3 = sessionKey->field_3;
        key.field_2 = sessionKey->field_2;
        key.field_1 = sessionKey->field_1;
        key.field_0 = sessionKey->field_0;
        Gp_SyncAreaKeyIndex(&key);
        rec             = Gp_GetNestedAreaRec(&key);
        place           = (GpAreaPlace*)((idx << 4) + (s32)rec->field_0);
        model->field_24 = place->field_D;
        model->field_25 = place->field_E;
        if (model->field_18 != NULL) {
            Tmd_ProcessStream(model);
            Tmd_ProcessStream(model);
        }
    }
    if (spawned2 != NULL) {
        TmdObject*   model;
        GpAreaRec*   rec;
        GpAreaPlace* place;
        GpAreaKey*   sessionKey;
        s32          idx;

        model       = (TmdObject*)spawned2->extra;
        idx         = ((GpEnemy*)task->spawnArg2)->field_8 >> 12;
        sessionKey  = (GpAreaKey*)&Game_Session->field_4;
        key.field_3 = sessionKey->field_3;
        key.field_2 = sessionKey->field_2;
        key.field_1 = sessionKey->field_1;
        key.field_0 = sessionKey->field_0;
        Gp_SyncAreaKeyIndex(&key);
        rec             = Gp_GetNestedAreaRec(&key);
        place           = (GpAreaPlace*)((idx << 4) + (s32)rec->field_0);
        model->field_24 = place->field_D;
        model->field_25 = place->field_E;
        if (model->field_18 != NULL) {
            Tmd_ProcessStream(model);
            Tmd_ProcessStream(model);
        }
    }
    func_actor_213000_8014A6AC(task);
    task->field_24     = D_actor_213000_80157E1C;
    task->exitCallback = Gp_EnemyTaskExit;
    task->state++;
}

INCLUDE_RODATA("actors/nonmatchings/actor_213000/actor_213000", D_actor_213000_80149E20);

INCLUDE_RODATA("actors/nonmatchings/actor_213000/actor_213000", D_actor_213000_80149E24);

void func_actor_213000_8014A084(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_213000_80149E24;
    sp.funcs[task->state](task);
}

INCLUDE_RODATA("actors/nonmatchings/actor_213000/actor_213000", D_actor_213000_80149E30);

INCLUDE_RODATA("actors/nonmatchings/actor_213000/actor_213000", D_actor_213000_80149E3C);

INCLUDE_RODATA("actors/nonmatchings/actor_213000/actor_213000", D_actor_213000_80149E48);
