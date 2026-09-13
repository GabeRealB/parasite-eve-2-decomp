#include "common.h"
#include "actors/actor_510900.h"

void func_actor_510900_8013BC38(Actor510900* arg0, Actor510900Coord* arg1)
{
    VECTOR pos;

    pos.vx = arg1->field_0.workm.t[0];
    pos.vy = arg1->field_0.workm.t[1];
    pos.vz = arg1->field_0.workm.t[2];
    Gp_UpdateActorColor(arg0->field_20, &pos, 0, 0);
}

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900_4", func_actor_510900_8013BC80);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900_4", func_actor_510900_8013BD5C);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900_4", func_actor_510900_8013BD84);
