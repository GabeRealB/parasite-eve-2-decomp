#include "common.h"
#include "actors/actor_510900.h"

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900_3", func_actor_510900_8013B988);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900_3", func_actor_510900_8013BA58);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900_3", func_actor_510900_8013BB20);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900_3", func_actor_510900_8013BBE4);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900_3", func_actor_510900_8013BC38);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900_3", func_actor_510900_8013BC80);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900_3", func_actor_510900_8013BD5C);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900_3", func_actor_510900_8013BD84);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900_3", func_actor_510900_8013BE00);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900_3", func_actor_510900_8013BE64);

s16 func_actor_510900_8013BE84(Actor510900* arg0)
{
    return arg0->field_1C->field_592;
}

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900_3", func_actor_510900_8013BE98);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900_3", func_actor_510900_8013BEEC);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900_3", func_actor_510900_8013BF90);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900_3", func_actor_510900_8013BFE4);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900_3", func_actor_510900_8013C034);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900_3", func_actor_510900_8013C090);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900_3", func_actor_510900_8013C0E4);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900_3", func_actor_510900_8013C134);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900_3", func_actor_510900_8013C190);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900_3", func_actor_510900_8013C1EC);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900_3", func_actor_510900_8013C240);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900_3", func_actor_510900_8013C338);

void func_actor_510900_8013C380(Actor510900* arg0)
{
    GpEnemy*         enemy = arg0->field_20;
    Actor510900Work* work  = arg0->field_1C;

    Gp_UnlinkNode(&enemy->node);
    Gp_UnlinkObj(&work->obj2BC);
    Gp_UnlinkObj(&work->obj2F4);
    Gp_DestroyEnemy(enemy, (Task*)arg0);
}

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900_3", func_actor_510900_8013C3DC);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900_3", func_actor_510900_8013C430);
