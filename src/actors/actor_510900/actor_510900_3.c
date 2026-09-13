#include "common.h"
#include "actors/actor_510900.h"

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900_3", func_actor_510900_8013B988);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900_3", func_actor_510900_8013BA58);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900_3", func_actor_510900_8013BB20);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900_3", func_actor_510900_8013BBE4);

void func_actor_510900_8013BC38(Actor510900* arg0, Actor510900Coord* arg1)
{
    VECTOR pos;

    pos.vx = arg1->field_38.vx;
    pos.vy = arg1->field_38.vy;
    pos.vz = arg1->field_38.vz;
    Gp_UpdateActorColor(arg0->field_20, &pos, 0, 0);
}

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900_3", func_actor_510900_8013BC80);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900_3", func_actor_510900_8013BD5C);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900_3", func_actor_510900_8013BD84);
