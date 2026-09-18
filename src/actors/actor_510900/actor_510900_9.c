#include "common.h"
#include "main/task.h"
#include "main/tmd.h"
#include "gameplay/gameplay.h"
#include "actors/actor_510900.h"

s16 func_actor_510900_8013BE84(Actor510900* arg0)
{
    return arg0->field_1C->field_592;
}

void func_actor_510900_8013BEEC(void* enemy, Task* task);
void func_actor_510900_801395AC(void* enemy, Task* task);

void func_actor_510900_8013BE98(Task* task)
{
    void (*fns[2])(void*, Task*) = { func_actor_510900_8013BEEC, func_actor_510900_801395AC };

    fns[task->state](task->spawnArg2, task);
}

void func_actor_510900_8013BEEC(void* enemy, Task* task)
{
    TmdObject*       obj;
    Actor510900Work* work;
    GsCOORDINATE2*   coord;

    obj        = (TmdObject*)task->extra;
    work       = (Actor510900Work*)task->parent->work;
    coord      = obj->coords;
    obj->clut += 2;
    Tmd_ProcessStream(obj);
    Tmd_ProcessStream(obj);
    coord->sub    = &((TmdObject*)task->parent->extra)->coords[12];
    coord->flg    = 0;
    obj->flags    = 0x80;
    obj->lightMtx = &work->field_45C;
    obj->colorMtx = &work->field_43C;
    task->state   = 1;
}

void func_actor_510900_8013BFE4(void* enemy, Task* task);
void func_actor_510900_8013C034(void* enemy, Task* task);

void func_actor_510900_8013BF90(Task* task)
{
    void (*fns[2])(void*, Task*) = { func_actor_510900_8013BFE4, func_actor_510900_8013C034 };

    fns[task->state](task->spawnArg2, task);
}

void func_actor_510900_8013BFE4(void* enemy, Task* task)
{
    TmdObject*       obj;
    Actor510900Work* work;

    obj              = (TmdObject*)task->extra;
    work             = (Actor510900Work*)task->parent->work;
    obj->flags       = 0x80;
    obj->coords->sub = &((TmdObject*)task->parent->extra)->coords[8];
    obj->lightMtx    = &work->field_45C;
    obj->colorMtx    = &work->field_43C;
    task->state      = 1;
}

void func_actor_510900_8013C034(void* enemy, Task* task)
{
    ((TmdObject*)task->extra)->flags       = ((TmdObject*)task->parent->extra)->flags;
    ((TmdObject*)task->extra)->coords->flg = 0;
    Gp_UpdateCoord(((TmdObject*)task->extra)->coords);
}

void func_actor_510900_8013C0E4(void* enemy, Task* task);
void func_actor_510900_8013C134(void* enemy, Task* task);

void func_actor_510900_8013C090(Task* task)
{
    void (*fns[2])(void*, Task*) = { func_actor_510900_8013C0E4, func_actor_510900_8013C134 };

    fns[task->state](task->spawnArg2, task);
}

void func_actor_510900_8013C0E4(void* enemy, Task* task)
{
    TmdObject*       obj;
    Actor510900Work* work;

    obj              = (TmdObject*)task->extra;
    work             = (Actor510900Work*)task->parent->work;
    obj->flags       = 0x80;
    obj->coords->sub = &((TmdObject*)task->parent->extra)->coords[3];
    obj->lightMtx    = &work->field_45C;
    obj->colorMtx    = &work->field_43C;
    task->state      = 1;
}

void func_actor_510900_8013C134(void* enemy, Task* task)
{
    ((TmdObject*)task->extra)->flags       = ((TmdObject*)task->parent->extra)->flags;
    ((TmdObject*)task->extra)->coords->flg = 0;
    Gp_UpdateCoord(((TmdObject*)task->extra)->coords);
}
