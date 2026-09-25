#include "common.h"

#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "actors/actor_535700.h"

extern GpMsgEntry D_actor_535700_801467E0[];
extern TaskDesc   D_actor_535700_80146810[];
extern u8         D_actor_535700_80146828[];

INCLUDE_ASM("actors/nonmatchings/actor_535700/actor_535700_6", func_actor_535700_80132960);

INCLUDE_ASM("actors/nonmatchings/actor_535700/actor_535700_6", func_actor_535700_80132ABC);

/// State 0 of the second enemy's task. Allocates its `Actor535700SpawnWork`,
/// spawns its sub-model task from `D_actor_535700_80146810`, takes the
/// sub-model's texture page and CLUT from the placement record the enemy's
/// `placeKey` selects, makes the sub-model a child of this task, lights the
/// model at its world position, starts the animation and runs the state
/// machine `func_actor_535700_80132D68` once.
///
/// Two codegen pins, both load-bearing. Left alone, CSE merges the two
/// call-site copies of `&key` into one pseudo live across the first call,
/// costing a callee-saved register; `SOFT_BARRIER()` keeps each
/// materialization next to its own call and `TOUCH_REG` makes the second a
/// fresh one. The `mem` / `work` pair reproduces the ROM's short-lived copy of
/// the `memCalloc` result beside the long-lived one.
void func_actor_535700_80132B58(GpEnemy* enemy, Task* task)
{
    VECTOR                vec;
    GpAreaKey             key;
    Actor535700SpawnWork* work;
    Actor535700SpawnWork* mem;
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

    obj        = task->extra;
    coord      = obj->coords;
    mem        = (Actor535700SpawnWork*)memCalloc(0x4C0, false);
    work       = mem;
    task->work = mem;
    if (mem == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback   = func_actor_535700_80132FF8;
    coord->sub           = &gGfxViewCoord;
    enemy->field_4       = &coord->coord;
    enemy->field_48      = 0;
    enemy->node.targeted = 0;
    enemy->node.flags    = 1;
    obj->otOffset        = 1;
    mem->field_4BC       = enemy;
    spawned              = Gp_SpawnEnemyFromTable(D_actor_535700_80146810, 1, 0, enemy);
    model                = (TmdObject*)spawned->task->extra;
    raw                  = enemy->placeKey;
    sessionKey           = (GpAreaKey*)&gGameSession->at4.loc;
    key.stage            = sessionKey->stage;
    key.area             = sessionKey->area;
    key.room             = sessionKey->room;
    idx                  = raw >> 12;
    areaByte0            = sessionKey->view;
    SOFT_BARRIER();
    keyPtr = &key;
    TOUCH_REG(keyPtr);
    key.view = areaByte0;
    Gp_SyncAreaKeyIndex(keyPtr);
    rec          = Gp_GetNestedAreaRec(&key);
    place        = (GpAreaPlace*)((idx << 4) + (s32)rec->field_0);
    model->tpage = place->tpage;
    model->clut  = place->clut;
    if (model->buffer != NULL) {
        tmdProcessStream(model);
        tmdProcessStream(model);
    }
    Task_Reparent(task, spawned->task);
    work->field_4B8 = spawned->task;
    obj->lightMtx   = &work->light;
    obj->colorMtx   = &work->color;
    vec.vx          = coord->workm.t[0];
    vec.vy          = coord->workm.t[1] - 0x320;
    vec.vz          = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_800B3F84(&work->anim, D_actor_535700_80146828, obj,
                  &work->field_34C, work->slots);
    work->animId   = 1;
    work->state    = 2;
    task->msgTable = D_actor_535700_801467E0;
    func_actor_535700_80132D68(task);
    task->state++;
}
