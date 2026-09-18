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
    coord           = extra->field_8;
    dest            = &parentExtra->field_8[part];
    coord->flg      = 0;
    coord->sub      = dest;
    extra->field_1C = parentExtra->field_1C;
    extra->field_20 = parentExtra->field_20;
    Task_Reparent(parent, task);
    task->state += 1;
}

void func_actor_335800_80162F08(void)
{
}

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_3", func_actor_335800_80162F10);

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
    ext->field_1C = &work->light;
    ext->field_20 = &work->color;
    func_800D7A9C(ext, (VECTOR*)((TmdObject*)arg0->extra)->field_8[1].workm.t, 0, 3);
    work->field_504 = 1;
}

void func_actor_335800_80162FF4(Task* arg0)
{
}
