#include "common.h"

#include "actors/actor_335800.h"

#include "gameplay/3A34.h"

#include "gameplay/3CD8.h"

#include "gameplay/gameplay.h"

#include "main/sound.h"

#include "main/task.h"

#include "main/unknown_syms.h"

extern TaskDesc D_actor_335800_80164DE0;

extern TaskDesc D_80182834;

extern s8 D_8007272D;

/// Spawn, tick and teardown handlers of the parent block.
extern TaskFuncTable3 D_actor_335800_80161E30;

/// Global freeze byte in the main executable; the state dispatchers run
/// nothing while it is non-zero.
extern u8 D_801153F4;

void func_actor_335800_80162E8C(Task* task)
{
    Task*          parent;
    s32            part;
    TmdObject*     extra;
    TmdObject*     parentExtra;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* dest;

    parent          = (Task*)task->spawnArg2;
    part            = task->spawnArg1;
    extra           = (TmdObject*)task->extra;
    parentExtra     = (TmdObject*)parent->extra;
    coord           = extra->coords;
    dest            = &parentExtra->coords[part];
    coord->flg      = 0;
    coord->sub      = dest;
    extra->lightMtx = parentExtra->lightMtx;
    extra->colorMtx = parentExtra->colorMtx;
    Task_Reparent(parent, task);
    task->state += 1;
}

void func_actor_335800_80162F08(void)
{
}

/// State dispatcher of the parent block: copies its three-handler table onto
/// the stack and, unless the game is frozen, runs the entry `Task::state`
/// selects.
void func_actor_335800_80162F10(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_335800_80161E30;
    if (D_801153F4 == 0) {
        sp.funcs[task->state](task);
    }
}

void func_actor_335800_80162F7C(Task* arg0)
{
    Gp_EnemyTaskExit(arg0);
}

void func_actor_335800_80162F9C(Task* arg0)
{
    TmdObject*           ext;
    Actor335800MainWork* work;

    work          = (Actor335800MainWork*)arg0->work;
    ext           = arg0->extra;
    ext->lightMtx = &work->light;
    ext->colorMtx = &work->color;
    func_800D7A9C(ext, (VECTOR*)((TmdObject*)arg0->extra)->coords[1].workm.t, 0, 3);
    work->field_504 = 1;
}

void func_actor_335800_80162FF4(Task* arg0)
{
}
