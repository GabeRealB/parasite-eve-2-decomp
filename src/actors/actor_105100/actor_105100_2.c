#include "common.h"
#include "gameplay/1BC.h"

#include "actors/actor_105100.h"

#include "main/task.h"

/// Each enemy task's three state handlers - spawn/setup, per-frame tick
/// and teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_105100_80131E90;
extern GpEnemyTaskFuncTable3 D_actor_105100_80131EB0;

INCLUDE_RODATA("actors/nonmatchings/actor_105100/actor_105100_2", D_actor_105100_80131EB0);

void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);

void func_actor_105100_80136524(Actor105100* arg0)
{
    GsCOORDINATE2* coord;
    VECTOR3        vec;

    coord  = arg0->field_2C->field_8;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_DrawEffGroundQuad(&vec, 0x9C4, 0x80);
}
