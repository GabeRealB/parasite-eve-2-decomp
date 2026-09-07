#include "common.h"
#include "main/task.h"
#include "actors/actor_342400.h"

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_4", func_actor_342400_8016A950);

void func_actor_342400_8016A9AC(Task* arg0)
{
    Actor342400Work* work;

    work            = (Actor342400Work*)arg0->idMap;
    arg0->state     = 5;
    work->field_420 = 0;
    work->field_422 = 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_4", func_actor_342400_8016A9C4);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_4", func_actor_342400_8016AA08);

void func_actor_342400_8016AA9C(Task* arg0)
{
    Actor342400Work* work;

    work            = (Actor342400Work*)arg0->idMap;
    work->field_412 = 0;
    work->field_420 = work->field_420 + 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_4", func_actor_342400_8016AAB8);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_4", func_actor_342400_8016AB6C);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_4", func_actor_342400_8016AC80);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_4", func_actor_342400_8016AD94);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_4", func_actor_342400_8016AE24);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_4", func_actor_342400_8016AEAC);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_4", func_actor_342400_8016AF34);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_4", func_actor_342400_8016AFA8);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_4", func_actor_342400_8016B038);
