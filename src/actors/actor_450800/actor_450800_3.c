#include "common.h"

#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "actors/actor_450800.h"
#include "actors/actors_shared_801366fc.h"

extern GpMsgEntry D_actor_450800_801539AC[];
extern TaskDesc   D_actor_450800_801539DC[];
extern u8         D_actor_450800_801539F4[];

/// Message handler 0x7DB of `D_actor_450800_8014AC58`: recolour this actor's
/// body (or spawn its 0x6002B burst) according to the message's selector.
///
/// The model is the actor's own -- `task->extra`, the `TmdObject` a spawnType-1
/// task carries -- and the one it is driven through is that of the helper task
/// in `Actor450800Work::field_4F8`. Both pointers, and `field_8` of the helper's
/// model, are resolved before the switch: the ROM reads them there, and a
/// scheduler pass cannot lift the loads into the entry block on its own.
s32 func_actor_450800_80132CE0(Task* task, s32 arg1, Actor450800Msg* msg, s32 arg3)
{
    Actor450800Work* work  = (Actor450800Work*)task->idMap;
    TmdObject*       obj   = (TmdObject*)work->field_4F8->extra;
    GsCOORDINATE2*   coord = obj->field_8;
    TmdObject*       self  = (TmdObject*)task->extra;
    s32              mode  = msg->field_2;

    switch (mode) {
        case 0:
            Gp_SpawnEff(0x6002B, coord, 0x21, 0);
            break;
        case 1:
            work->field_500 = mode;
            obj->field_C    = self->field_C;
            break;
        case 2:
            work->field_500 = 0;
            obj->field_C    = 0x84;
            break;
    }
    return 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_450800/actor_450800_3", func_actor_450800_80132D74);

void func_actor_450800_801330AC(Task* task);

/// Spawn handler of the enemy this actor's model task carries: state 0 of
/// `func_actor_450800_80133264`'s `fns` table, and the twin of
/// `ActorsShared80131e24Sub0` - the same body, minus that variant's
/// `obj->field_C` store. Builds the enemy's `Actor450800SpawnWork` block,
/// spawns its own model task out of the same `D_actor_450800_801539DC` table,
/// faces it at the placed spawn point, starts the animation and hands the state
/// machine to `func_actor_450800_801330AC`.
///
/// Two codegen pins, both load-bearing. `key` lands at `vfp+0x28`, and left
/// alone CSE merges the two call-site copies of `&key` into one pseudo live
/// across the first call, which costs a callee-saved register and grows the
/// frame; `SOFT_BARRIER()` keeps each materialization next to its own call and
/// `TOUCH_REG` makes the second a fresh computation. The `mem` / `work` pair is
/// the same kind of pin on the `Mem_Calloc` result: the ROM keeps a short-lived
/// copy for the `idMap` store, the NULL test and `field_4BC`, and a longer-lived
/// one for everything after.
void func_actor_450800_80132E9C(void* enemyArg, Task* task)
{
    GpEnemy*              enemy = (GpEnemy*)enemyArg;
    VECTOR                vec;
    GpAreaKey             key;
    Actor450800SpawnWork* work;
    Actor450800SpawnWork* mem;
    GsCOORDINATE2*        coord;
    TmdObject*            obj;
    GpEnemy*              spawned;
    TmdObject*            model;
    GpAreaKey*            sessionKey;
    GpAreaKey*            keyPtr;
    u8                    areaByte0;
    GpAreaRec*            rec;
    GpAreaPlace*          place;
    s32                   idx;
    u32                   raw;

    obj         = task->extra;
    coord       = obj->field_8;
    mem         = (Actor450800SpawnWork*)Mem_Calloc(0x4C0, false);
    work        = mem;
    task->idMap = (TaskIdMap*)mem;
    if (mem == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback  = ActorsShared801366fc;
    coord->sub          = &Gfx_ViewCoord;
    enemy->field_4      = &coord->coord;
    enemy->field_48     = 0;
    enemy->node.field_5 = 0;
    enemy->node.field_4 = 1;
    obj->field_E        = 1;
    mem->field_4BC      = enemy;
    spawned             = Gp_SpawnEnemyFromTable(D_actor_450800_801539DC, 1, 0, enemy);
    model               = (TmdObject*)spawned->task->extra;
    raw                 = enemy->field_8;
    sessionKey          = (GpAreaKey*)&Game_Session->field_4;
    key.field_3         = sessionKey->field_3;
    key.field_2         = sessionKey->field_2;
    key.field_1         = sessionKey->field_1;
    idx                 = raw >> 12;
    areaByte0           = sessionKey->field_0;
    SOFT_BARRIER();
    keyPtr = &key;
    TOUCH_REG(keyPtr);
    key.field_0 = areaByte0;
    Gp_SyncAreaKeyIndex(keyPtr);
    rec             = Gp_GetNestedAreaRec(&key);
    place           = (GpAreaPlace*)((idx << 4) + (s32)rec->field_0);
    model->field_24 = place->field_D;
    model->field_25 = place->field_E;
    if (model->field_18 != NULL) {
        Tmd_ProcessStream(model);
        Tmd_ProcessStream(model);
    }
    Task_Reparent(task, spawned->task);
    work->field_4B8 = spawned->task;
    obj->field_1C   = &work->light;
    obj->field_20   = &work->color;
    vec.vx          = coord->workm.t[0];
    vec.vy          = coord->workm.t[1] - 0x320;
    vec.vz          = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_800B3F84(&work->anim, D_actor_450800_801539F4, (GpAnimObj*)obj,
                  &work->field_34C, work->slots);
    work->animId   = 1;
    work->state    = 2;
    task->field_24 = D_actor_450800_801539AC;
    func_actor_450800_801330AC(task);
    task->state++;
}

INCLUDE_ASM("actors/nonmatchings/actor_450800/actor_450800_3", func_actor_450800_801330AC);

void func_actor_450800_80132E9C(void* enemy, Task* task);
void func_actor_450800_801332B8(void* enemy, Task* task);

void func_actor_450800_80133264(Task* task)
{
    void (*fns[2])(void*, Task*) = { func_actor_450800_80132E9C, func_actor_450800_801332B8 };

    fns[task->state](task->spawnArg2, task);
}

INCLUDE_ASM("actors/nonmatchings/actor_450800/actor_450800_3", func_actor_450800_801332B8);
