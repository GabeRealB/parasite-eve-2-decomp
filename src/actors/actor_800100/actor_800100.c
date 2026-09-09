#include "common.h"

#include "actors/actor_800100.h"

INCLUDE_RODATA("actors/nonmatchings/actor_800100/actor_800100", D_actor_800100_80161E20);

INCLUDE_ASM("actors/nonmatchings/actor_800100/actor_800100", func_actor_800100_80161F20);

INCLUDE_ASM("actors/nonmatchings/actor_800100/actor_800100", func_actor_800100_80162264);

INCLUDE_ASM("actors/nonmatchings/actor_800100/actor_800100", func_actor_800100_801624F0);

INCLUDE_ASM("actors/nonmatchings/actor_800100/actor_800100", func_actor_800100_80162A14);

INCLUDE_ASM("actors/nonmatchings/actor_800100/actor_800100", func_actor_800100_80162E90);

void func_actor_800100_801631C8(Task* arg0)
{
    GpObj* temp_a0;
    void*  temp_s1;

    temp_a0 = arg0->idMap;
    temp_s1 = arg0->spawnArg2;
    if (temp_a0 != NULL) {
        Gp_UnlinkObj(temp_a0);
    }
    Gp_ReleaseState1CMem(temp_s1, arg0);
}

INCLUDE_ASM("actors/nonmatchings/actor_800100/actor_800100", func_actor_800100_80163214);

INCLUDE_ASM("actors/nonmatchings/actor_800100/actor_800100", func_actor_800100_801635F4);

INCLUDE_ASM("actors/nonmatchings/actor_800100/actor_800100", func_actor_800100_80163A58);

void func_actor_800100_80163BF8(Task* arg0)
{
    arg0->state = 3;
}

void func_actor_800100_80163C04(GpActorWork* arg0)
{
    GameActor* actor;
    GpActorD4* d4;
    Task*      task;

    actor            = arg0->actor;
    d4               = actor->field_910;
    Gp_ActorSlots[1] = NULL;
    task             = actor->field_914;
    if (task != NULL) {
        Task_Kill(task);
    }
    task = actor->field_918;
    if (task != NULL) {
        Task_Kill(task);
    }
    task = actor->field_91C;
    if (task != NULL) {
        Task_Kill(task);
    }
    task = actor->field_920;
    if (task != NULL) {
        Task_Kill(task);
    }
    task = actor->field_924;
    if (task != NULL) {
        Task_Kill(task);
    }
    Gp_UnlinkObj((GpObj*)actor->field_AC);
    Gp_UnlinkObj((GpObj*)actor->field_CC);
    Gp_UnlinkObj((GpObj*)actor->field_EC);
    Gp_UnlinkObj((GpObj*)actor->field_10C);
    Gp_UnlinkObj((GpObj*)d4->field_68);
    Task_Kill((Task*)arg0);
}

INCLUDE_RODATA("actors/nonmatchings/actor_800100/actor_800100", ActorsShared801328ccTable);
