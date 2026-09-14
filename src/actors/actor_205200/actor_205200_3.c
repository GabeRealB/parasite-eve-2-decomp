#include "common.h"

#include "actors/actor_205200.h"

void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);

void func_actor_205200_8014C8D4(Actor205200* arg0)
{
    GsCOORDINATE2* coord;
    VECTOR3        vec;

    coord  = arg0->field_2C->field_8;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_DrawEffGroundQuad(&vec, 0x180, 0x80);
}

void func_actor_205200_8014C924(Actor205200Ctx* arg0, Actor205200* arg1)
{
    Actor205200Work* work;

    work = arg1->field_1C;
    Gp_UnlinkNode(&arg0->node);
    Gp_UnlinkObj(&work->field_47C);
    Gp_UnlinkObj(&work->field_4E4);
    Gp_DestroyEnemy(arg0, arg1);
}
