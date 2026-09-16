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

    parent        = task->spawnArg2;
    obj           = task->extra;
    parentObj     = parent->extra;
    coords        = parentObj->field_8;
    obj->field_C |= 0x80;
    root          = obj->field_8;
    if (!(parentObj->field_C & 0x80)) {
        obj->field_C &= 0xFF7F;
    }
    if (!(parentObj->field_C & 4)) {
        obj->field_C &= 0xFFFB;
        Tmd_AllocBuffers(obj);
    } else {
        obj->field_C |= 4;
    }
    obj->field_E  = -2;
    coords       += task->spawnArg1;
    root->flg     = 0;
    root->sub     = coords;
    obj->field_1C = parentObj->field_1C;
    obj->field_20 = parentObj->field_20;
    Task_Reparent(parent, task);
    task->state++;
}

INCLUDE_RODATA("actors/nonmatchings/actor_511000/actor_511000_2", D_actor_511000_80131E48);

INCLUDE_RODATA("actors/nonmatchings/actor_511000/actor_511000_2", D_actor_511000_80131E54);

INCLUDE_RODATA("actors/nonmatchings/actor_511000/actor_511000_2", D_actor_511000_80131E60);

INCLUDE_RODATA("actors/nonmatchings/actor_511000/actor_511000_2", ActorsShared80135df4Table);
