#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/gameplay.h"
#include "main/task.h"
#include "main/tmd.h"

void func_actor_460200_801336B4(Task* task);
void func_actor_460200_80133AB0(Task* task);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200_2", func_actor_460200_80133580);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200_2", func_actor_460200_8013364C);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200_2", func_actor_460200_801336B4);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200_2", func_actor_460200_8013386C);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200_2", func_actor_460200_801338C0);

void func_actor_460200_80133A04(void* arg0, Task* task)
{
    TmdObject*     obj;
    GsCOORDINATE2* coord;
    VECTOR         vec;

    obj   = task->extra;
    coord = obj->field_8;
    Gp_UpdateCoord(coord);
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1] - 0x320;
    vec.vz = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_actor_460200_801336B4(task);
    func_actor_460200_80133AB0(task);
}

void func_actor_460200_80133A88(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2, task);
}

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200_2", func_actor_460200_80133AB0);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200_2", func_actor_460200_80133B3C);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200_2", func_actor_460200_80133B88);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200_2", func_actor_460200_80133C00);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200_2", func_actor_460200_80133C64);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200_2", func_actor_460200_80133CD0);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200_2", func_actor_460200_80133D4C);

s32 func_actor_460200_80133DC4(void)
{
    return 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200_2", func_actor_460200_80133DCC);
