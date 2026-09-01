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

    actor                             = arg0->actor;
    d4                                = actor->field_910;
    ((GpActorWork**)Gp_ActorSlots)[1] = NULL;
    task                              = actor->field_914;
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

INCLUDE_ASM("actors/nonmatchings/actor_800100/actor_800100", func_actor_800100_80163CF0);

INCLUDE_ASM("actors/nonmatchings/actor_800100/actor_800100", func_actor_800100_80163D54);

INCLUDE_RODATA("actors/nonmatchings/actor_800100/actor_800100", D_actor_800100_80161E4C);

INCLUDE_ASM("actors/nonmatchings/actor_800100/actor_800100", func_actor_800100_80163F04);

INCLUDE_ASM("actors/nonmatchings/actor_800100/actor_800100", func_actor_800100_80164184);

INCLUDE_ASM("actors/nonmatchings/actor_800100/actor_800100", func_actor_800100_801643F4);

INCLUDE_ASM("actors/nonmatchings/actor_800100/actor_800100", func_actor_800100_80164580);

INCLUDE_ASM("actors/nonmatchings/actor_800100/actor_800100", func_actor_800100_80164710);

INCLUDE_ASM("actors/nonmatchings/actor_800100/actor_800100", func_actor_800100_80164940);

INCLUDE_ASM("actors/nonmatchings/actor_800100/actor_800100", func_actor_800100_80164B9C);

INCLUDE_ASM("actors/nonmatchings/actor_800100/actor_800100", func_actor_800100_80164E60);

INCLUDE_ASM("actors/nonmatchings/actor_800100/actor_800100", func_actor_800100_80165010);

INCLUDE_ASM("actors/nonmatchings/actor_800100/actor_800100", func_actor_800100_801652B0);

INCLUDE_ASM("actors/nonmatchings/actor_800100/actor_800100", func_actor_800100_80165528);

void func_actor_800100_801655C0(GpActorWork* arg0)
{
    GameActor* actor;

    actor                      = arg0->actor;
    actor->field_956           = 3;
    actor->field_954           = 0;
    actor->field_95C           = 0;
    actor->field_95E           = 0;
    actor->field_910->field_CC = 0;
    actor->field_97E           = 2;
    actor->field_90C           = Gp_FindLockNode(arg0);
    Gp_AnimPlayChildSlotsEx(arg0, 1, 0, 6);
}

INCLUDE_ASM("actors/nonmatchings/actor_800100/actor_800100", func_actor_800100_80165630);

INCLUDE_ASM("actors/nonmatchings/actor_800100/actor_800100", func_actor_800100_80165664);

INCLUDE_ASM("actors/nonmatchings/actor_800100/actor_800100", func_actor_800100_801656C8);

INCLUDE_ASM("actors/nonmatchings/actor_800100/actor_800100", func_actor_800100_801656F4);

INCLUDE_ASM("actors/nonmatchings/actor_800100/actor_800100", func_actor_800100_80165720);

INCLUDE_ASM("actors/nonmatchings/actor_800100/actor_800100", func_actor_800100_80165748);

INCLUDE_ASM("actors/nonmatchings/actor_800100/actor_800100", func_actor_800100_801657D8);

INCLUDE_ASM("actors/nonmatchings/actor_800100/actor_800100", func_actor_800100_80165818);

INCLUDE_ASM("actors/nonmatchings/actor_800100/actor_800100", func_actor_800100_80165850);

INCLUDE_ASM("actors/nonmatchings/actor_800100/actor_800100", func_actor_800100_801658E8);

void func_actor_800100_80165928(void)
{
}

INCLUDE_ASM("actors/nonmatchings/actor_800100/actor_800100", func_actor_800100_80165930);

INCLUDE_ASM("actors/nonmatchings/actor_800100/actor_800100", func_actor_800100_801659EC);

INCLUDE_ASM("actors/nonmatchings/actor_800100/actor_800100", func_actor_800100_80165C38);

INCLUDE_ASM("actors/nonmatchings/actor_800100/actor_800100", func_actor_800100_80165DE8);

INCLUDE_RODATA("actors/nonmatchings/actor_800100/actor_800100", D_actor_800100_80161EC8);

INCLUDE_ASM("actors/nonmatchings/actor_800100/actor_800100", func_actor_800100_80165F50);

INCLUDE_ASM("actors/nonmatchings/actor_800100/actor_800100", func_actor_800100_80166190);

INCLUDE_ASM("actors/nonmatchings/actor_800100/actor_800100", func_actor_800100_80166514);

INCLUDE_ASM("actors/nonmatchings/actor_800100/actor_800100", func_actor_800100_8016666C);

INCLUDE_ASM("actors/nonmatchings/actor_800100/actor_800100", func_actor_800100_801668C0);

INCLUDE_ASM("actors/nonmatchings/actor_800100/actor_800100", func_actor_800100_80166B40);

INCLUDE_ASM("actors/nonmatchings/actor_800100/actor_800100", func_actor_800100_80166DD0);

INCLUDE_ASM("actors/nonmatchings/actor_800100/actor_800100", func_actor_800100_80166DF0);

INCLUDE_ASM("actors/nonmatchings/actor_800100/actor_800100", func_actor_800100_80166E14);

INCLUDE_ASM("actors/nonmatchings/actor_800100/actor_800100", func_actor_800100_80166E94);

INCLUDE_ASM("actors/nonmatchings/actor_800100/actor_800100", func_actor_800100_80166EE8);

INCLUDE_ASM("actors/nonmatchings/actor_800100/actor_800100", func_actor_800100_80166F50);

INCLUDE_ASM("actors/nonmatchings/actor_800100/actor_800100", func_actor_800100_8016709C);
