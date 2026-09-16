#include "common.h"

#include "actors/actor_511000.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

/// The actor's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern TaskFuncTable3 D_actor_511000_80131E48;
extern TaskFuncTable3 D_actor_511000_80131E54;
extern TaskFuncTable3 D_actor_511000_80131E60;

/// Translation presets `func_actor_511000_80133760` copies onto the root
/// coordinate; `Task::spawnArg1` selects the entry.
extern SVECTOR D_actor_511000_80148FE4[];

/// Spawn table for the three children `func_actor_511000_80133958` creates.
extern TaskDesc D_actor_511000_80155070[];
/// Message table and animation data `func_actor_511000_80133958` installs.
extern void* D_actor_511000_801550A0;
extern void* D_actor_511000_801550C0;

INCLUDE_ASM("actors/nonmatchings/actor_511000/actor_511000_5", func_actor_511000_80133554);

/// Places the task's model at the indexed rotation and translation: copies
/// `rots[index]` onto the root coordinate's Euler angles, `trans[index]` into
/// its local translation, rebuilds the rotation matrix and marks the
/// coordinate dirty.
void func_actor_511000_801336E0(Task* task, SVECTOR* rots, SVECTOR* trans, s32 index)
{
    Actor511000Coord* coord;
    SVECTOR*          rot;
    SVECTOR*          pos;
    s32               off;

    off               = (index << 16) >> 13;
    rot               = (SVECTOR*)(off + (s32)rots);
    coord             = (Actor511000Coord*)((TmdObject*)task->extra)->field_8;
    coord->rot.vx     = rot->vx;
    coord->rot.vy     = rot->vy;
    pos               = (SVECTOR*)(off + (s32)trans);
    coord->rot.vz     = rot->vz;
    coord->coord.t[0] = pos->vx;
    coord->coord.t[1] = pos->vy;
    coord->coord.t[2] = pos->vz;
    RotMatrix(&coord->rot, &coord->coord);
    coord->flg = 0;
}

/// Places the task's model at the indexed translation: copies
/// `D_actor_511000_80148FE4[spawnArg1]` into the root coordinate's local
/// translation, zeros the Euler angles, rebuilds the rotation matrix and
/// marks the coordinate dirty.
void func_actor_511000_80133760(Task* task)
{
    Actor511000Coord* coord;

    coord             = (Actor511000Coord*)((TmdObject*)task->extra)->field_8;
    coord->coord.t[0] = D_actor_511000_80148FE4[task->spawnArg1].vx;
    coord->coord.t[1] = D_actor_511000_80148FE4[task->spawnArg1].vy;
    coord->coord.t[2] = D_actor_511000_80148FE4[task->spawnArg1].vz;
    coord->rot.vx     = 0;
    coord->rot.vy     = 0;
    coord->rot.vz     = 0;
    RotMatrix(&coord->rot, &coord->coord);
    coord->flg = 0;
}

/// Binds the task's TMD object to the work-block light/color matrices, clears
/// the root coordinate flag, and rebuilds lighting from the world translation.
void func_actor_511000_801337F0(Task* task)
{
    GsCOORDINATE2*   coord;
    Actor511000Work* work;
    TmdObject*       extra;

    work            = (Actor511000Work*)task->idMap;
    extra           = (TmdObject*)task->extra;
    coord           = extra->field_8;
    extra->field_1C = &work->light;
    extra->field_20 = &work->color;
    coord->flg      = 0;
    Gp_UpdateCoord(coord);
    func_800D7A9C(extra, (VECTOR*)coord->workm.t, 0, 3);
}

void func_actor_511000_80133850(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_511000_80131E48;
    sp.funcs[task->state](task);
}

void func_actor_511000_801338A8(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_511000_80131E54;
    sp.funcs[task->state](task);
}

void func_actor_511000_80133900(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_511000_80131E60;
    sp.funcs[task->state](task);
}

/// Spawn handler: allocates the 0x488-byte work block and parks it in
/// `Task::idMap`, binds the task's model to the block's matrices and
/// animation state, installs the message table, then spawns table entries 1
/// and 2 - tinting each child's model from the current area's record - and
/// entry 3, and advances to state 1. An allocation failure destroys the enemy.
void func_actor_511000_80133958(GpEnemy* enemy, Task* task)
{
    GpAreaKey              key;
    GpAreaKey*             sessionKey;
    u8                     areaByte0;
    u8                     areaByte3;
    GpAreaRec*             rec;
    TmdObject*             model;
    GsCOORDINATE2*         coord;
    Actor511000ParentWork* work;
    TaskDesc*              table;
    u32                    idx;
    GpEnemy*               spawned;
    GameSession*           session;

    model = task->extra;
    coord = model->field_8;
    work  = Mem_Calloc(0x488, 0);
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->idMap     = (TaskIdMap*)work;
    model->field_C  = 0x80;
    model->field_1C = &work->field_45C;
    model->field_20 = &work->field_43C;
    func_800B3F84(&work->anim, &D_actor_511000_801550C0, (GpAnimObj*)model, work->field_30C, work->slots);
    work->field_47C = 0;
    task->field_24  = &D_actor_511000_801550A0;
    coord->flg      = 0;

    table   = D_actor_511000_80155070;
    spawned = Gp_SpawnEnemyFromTable(table, 1, 0, enemy);
    session = Game_Session;
    /* Reusing `spawned` for the task, rather than a new local, keeps the
       spawn result's v0 preference off the byte copied into a0. */
    spawned     = (GpEnemy*)spawned->task;
    sessionKey  = (GpAreaKey*)&session->field_4;
    idx         = enemy->field_8;
    areaByte3   = sessionKey->field_3;
    model       = ((Task*)spawned)->extra;
    key.field_3 = areaByte3;
    key.field_2 = sessionKey->field_2;
    key.field_1 = sessionKey->field_1;
    areaByte0   = session->field_4;
    idx         = idx >> 12;
    key.field_0 = areaByte0;
    Gp_SyncAreaKeyIndex(&key);
    rec             = Gp_GetNestedAreaRec(&key);
    idx           <<= 4;
    idx            += (s32)rec->field_0;
    model->field_24 = ((GpCdRec10*)idx)->field_D;
    model->field_25 = ((GpCdRec10*)idx)->field_E;
    if (model->field_18 != NULL) {
        Tmd_ProcessStream(model);
        Tmd_ProcessStream(model);
    }
    spawned     = Gp_SpawnEnemyFromTable(table, 2, 0, enemy);
    session     = Game_Session;
    spawned     = (GpEnemy*)spawned->task;
    sessionKey  = (GpAreaKey*)&session->field_4;
    idx         = enemy->field_8;
    areaByte3   = sessionKey->field_3;
    model       = ((Task*)spawned)->extra;
    key.field_3 = areaByte3;
    key.field_2 = sessionKey->field_2;
    key.field_1 = sessionKey->field_1;
    areaByte0   = session->field_4;
    idx         = idx >> 12;
    key.field_0 = areaByte0;
    Gp_SyncAreaKeyIndex(&key);
    rec             = Gp_GetNestedAreaRec(&key);
    idx           <<= 4;
    idx            += (s32)rec->field_0;
    model->field_24 = ((GpCdRec10*)idx)->field_D;
    model->field_25 = ((GpCdRec10*)idx)->field_E;
    if (model->field_18 != NULL) {
        Tmd_ProcessStream(model);
        Tmd_ProcessStream(model);
    }
    Gp_SpawnEnemyFromTable(table, 3, 0, enemy);
    task->state = 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_511000/actor_511000_5", func_actor_511000_80133B80);
