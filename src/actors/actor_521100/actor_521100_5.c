#include "common.h"

#include "actors/actor_521100.h"
#include "gameplay/gameplay.h"
#include "main/task.h"
#include "main/tmd.h"

void func_800D7A9C(TmdObject* arg0, VECTOR* arg1, s32 arg2, s32 arg3);
void func_actor_521100_80135F2C(Task* task);

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100_5", func_actor_521100_80135D10);

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100_5", func_actor_521100_80135D58);

s32 func_actor_521100_80135D9C(Actor521100* arg0)
{
    arg0->field_1C->field_6B0 = 1;
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    return 0;
}

s16 func_actor_521100_80135DC8(Actor521100* arg0)
{
    return arg0->field_1C->field_6B2;
}

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100_5", func_actor_521100_80135DDC);

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100_5", func_actor_521100_80135F2C);

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100_5", func_actor_521100_801360C4);

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100_5", func_actor_521100_80136290);

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100_5", func_actor_521100_80136404);

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100_5", func_actor_521100_80136604);

void func_actor_521100_80136680(void* arg0, Task* task)
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
    func_actor_521100_80135F2C(task);
}
