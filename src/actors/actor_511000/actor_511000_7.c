#include "common.h"
#include "main/task.h"
#include "main/tmd.h"
#include "gameplay/gameplay.h"
#include "actors/actor_511000.h"

void func_actor_511000_80133F48(void* enemy, Task* task);
void func_actor_511000_80133F88(void* enemy, Task* task);

void func_actor_511000_80133EF4(Task* task)
{
    void (*fns[2])(void*, Task*) = { func_actor_511000_80133F48, func_actor_511000_80133F88 };

    fns[task->state](task->spawnArg2, task);
}

void func_actor_511000_80133F48(void* enemy, Task* task)
{
    Task*                  parent;
    TmdObject*             obj;
    Actor511000ParentWork* work;
    GsCOORDINATE2*         coord;
    GsCOORDINATE2*         parentCoords;

    parent       = task->parent;
    obj          = (TmdObject*)task->extra;
    parentCoords = ((TmdObject*)parent->extra)->coords;
    coord        = obj->coords;
    work         = (Actor511000ParentWork*)parent->work;

    coord->sub    = &parentCoords[8];
    obj->lightMtx = &work->field_45C;
    obj->flags    = 0;
    obj->colorMtx = &work->field_43C;
    task->state   = 1;
}

void func_actor_511000_80133F88(void* arg0, Task* arg1)
{
    ((TmdObject*)arg1->extra)->coords->flg = 0;
    Gp_UpdateCoord(((TmdObject*)arg1->extra)->coords);
}

void func_actor_511000_8013401C(void* enemy, Task* task);
void func_actor_511000_8013405C(void* enemy, Task* task);

void func_actor_511000_80133FC8(Task* task)
{
    void (*fns[2])(void*, Task*) = { func_actor_511000_8013401C, func_actor_511000_8013405C };

    fns[task->state](task->spawnArg2, task);
}

void func_actor_511000_8013401C(void* enemy, Task* task)
{
    Task*                  parent;
    TmdObject*             obj;
    Actor511000ParentWork* work;
    GsCOORDINATE2*         coord;
    GsCOORDINATE2*         parentCoords;

    parent       = task->parent;
    obj          = (TmdObject*)task->extra;
    parentCoords = ((TmdObject*)parent->extra)->coords;
    coord        = obj->coords;
    work         = (Actor511000ParentWork*)parent->work;

    coord->sub    = &parentCoords[3];
    obj->lightMtx = &work->field_45C;
    obj->flags    = 0;
    obj->colorMtx = &work->field_43C;
    task->state   = 1;
}

void func_actor_511000_8013405C(void* arg0, Task* arg1)
{
    ((TmdObject*)arg1->extra)->coords->flg = 0;
    Gp_UpdateCoord(((TmdObject*)arg1->extra)->coords);
}

void func_actor_511000_801340F0(void* enemy, Task* task);
void func_actor_511000_80134130(void* enemy, Task* task);

void func_actor_511000_8013409C(Task* task)
{
    void (*fns[2])(void*, Task*) = { func_actor_511000_801340F0, func_actor_511000_80134130 };

    fns[task->state](task->spawnArg2, task);
}

void func_actor_511000_801340F0(void* enemy, Task* task)
{
    Task*                  parent;
    TmdObject*             obj;
    Actor511000ParentWork* work;
    GsCOORDINATE2*         coord;
    GsCOORDINATE2*         parentCoords;

    parent       = task->parent;
    obj          = (TmdObject*)task->extra;
    parentCoords = ((TmdObject*)parent->extra)->coords;
    coord        = obj->coords;
    work         = (Actor511000ParentWork*)parent->work;

    coord->sub    = &parentCoords[12];
    obj->lightMtx = &work->field_45C;
    obj->flags    = 0;
    obj->colorMtx = &work->field_43C;
    task->state   = 1;
}

void func_actor_511000_80134130(void* arg0, Task* arg1)
{
    ((TmdObject*)arg1->extra)->coords->flg = 0;
    Gp_UpdateCoord(((TmdObject*)arg1->extra)->coords);
}
