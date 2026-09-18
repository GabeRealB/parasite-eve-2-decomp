#include "common.h"

#include "actors/actor_511000.h"

#include "gameplay/3A34.h"
#include "gameplay/gameplay.h"

#include "main/task.h"
#include "main/tmd.h"

/// The actor's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern TaskFuncTable3 D_actor_511000_80131E30;
extern TaskFuncTable3 D_actor_511000_80131E3C;

#include "common.h"

#include "actors/actor_511000.h"

#include "gameplay/3A34.h"
#include "gameplay/gameplay.h"

#include "main/task.h"
#include "main/tmd.h"

/// The actor's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern TaskFuncTable3 D_actor_511000_80131E30;
extern TaskFuncTable3 D_actor_511000_80131E3C;

void func_actor_511000_80132224(void)
{
}

void func_actor_511000_8013222C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_511000_80131E30;
    sp.funcs[task->state](task);
}

void func_actor_511000_80132284(Task* task)
{
    Task*          parent;
    TmdObject*     obj;
    TmdObject*     parentObj;
    GsCOORDINATE2* coords;
    GsCOORDINATE2* root;

    parent      = task->spawnArg2;
    obj         = task->extra;
    parentObj   = parent->extra;
    coords      = parentObj->coords;
    obj->flags |= 0x80;
    root        = obj->coords;
    if (!(parentObj->flags & 0x80)) {
        obj->flags &= 0xFF7F;
    }
    if (!(parentObj->flags & 4)) {
        obj->flags &= 0xFFFB;
        Tmd_AllocBuffers(obj);
    } else {
        obj->flags |= 4;
    }
    obj->otOffset = -2;
    coords       += task->spawnArg1;
    root->flg     = 0;
    root->sub     = coords;
    obj->lightMtx = parentObj->lightMtx;
    obj->colorMtx = parentObj->colorMtx;
    Task_Reparent(parent, task);
    task->state++;
}

INCLUDE_RODATA("actors/nonmatchings/actor_511000/actor_511000_2", D_actor_511000_80131E48);

INCLUDE_RODATA("actors/nonmatchings/actor_511000/actor_511000_2", D_actor_511000_80131E54);

INCLUDE_RODATA("actors/nonmatchings/actor_511000/actor_511000_2", D_actor_511000_80131E60);

INCLUDE_RODATA("actors/nonmatchings/actor_511000/actor_511000_2", ActorsShared80135df4Table);
