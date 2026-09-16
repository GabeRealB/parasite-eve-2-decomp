#include "common.h"

#include "gameplay/3FB8.h"

extern GpActorFuncTable3 D_actor_800300_80161E34;

INCLUDE_RODATA("actors/nonmatchings/actor_800300/actor_800300", D_actor_800300_80161E20);

INCLUDE_RODATA("actors/nonmatchings/actor_800300/actor_800300", ActorsShared801328ccTable);

INCLUDE_RODATA("actors/nonmatchings/actor_800300/actor_800300", D_actor_800300_80161E34);

INCLUDE_RODATA("actors/nonmatchings/actor_800300/actor_800300", D_actor_800300_80161E40);

INCLUDE_ASM("actors/nonmatchings/actor_800300/actor_800300_2", func_actor_800300_80162658);

INCLUDE_ASM("actors/nonmatchings/actor_800300/actor_800300_2", func_actor_800300_801628D0);

INCLUDE_ASM("actors/nonmatchings/actor_800300/actor_800300_2", func_actor_800300_80162A98);

void func_actor_800300_80162C2C(GpActorWork* arg0)
{
    GameActor*        actor;
    GpActorFuncTable3 sp;

    sp               = D_actor_800300_80161E34;
    actor            = arg0->actor;
    actor->field_973 = 0;
    actor->field_975 = 0;
    sp.funcs[actor->field_954](arg0);
    actor->field_986 = 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_800300/actor_800300_2", func_actor_800300_80162C98);

INCLUDE_ASM("actors/nonmatchings/actor_800300/actor_800300_2", func_actor_800300_80162D74);

void func_actor_800300_80162EEC(GpActorWork* arg0)
{
    if (arg0->actor->field_95E == 1) {
        Gp_ResetActorMove(arg0, 0);
    }
}

void func_actor_800300_80162F24(GpActorWork* arg0)
{
    GameActor*     actor;
    GsCOORDINATE2* coord;
    s32            flag;

    actor = arg0->actor;
    coord = arg0->extra->field_8;
    switch (actor->field_95E) {
        case 0:
            flag               = 1;
            actor->field_95E   = flag;
            coord->coord.t[1] += 0xC0;
        case 1:
            Gp_AnimTickChildSlots(arg0);
            Gp_TurnPlayer(arg0);
            Gp_StepPlayerMove(arg0);
            break;
    }
}

INCLUDE_RODATA("actors/nonmatchings/actor_800300/actor_800300", D_actor_800300_80161E64);

INCLUDE_ASM("actors/nonmatchings/actor_800300/actor_800300_2", func_actor_800300_80162F98);

INCLUDE_ASM("actors/nonmatchings/actor_800300/actor_800300_2", func_actor_800300_80163048);

INCLUDE_ASM("actors/nonmatchings/actor_800300/actor_800300_2", func_actor_800300_80163074);
