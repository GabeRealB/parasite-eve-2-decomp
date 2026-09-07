#include "common.h"

#include "actors/actor_403600.h"
#include "main/sound.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"

void func_actor_403600_801411D4(Actor403600* arg0, s32 arg1);
void func_actor_403600_801412D0(Actor403600Ctx* arg0, Actor403600* arg1);
void func_actor_403600_80141338(Actor403600* arg0);
void func_actor_403600_801414FC(Actor403600* arg0);
void func_actor_403600_8014161C(Actor403600* arg0);
void func_actor_403600_80141A34(Actor403600* arg0);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600_2", func_actor_403600_801411D4);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600_2", func_actor_403600_801412D0);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600_2", func_actor_403600_80141338);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600_2", func_actor_403600_801414FC);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600_2", func_actor_403600_80141598);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600_2", func_actor_403600_8014161C);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600_2", func_actor_403600_8014174C);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600_2", func_actor_403600_801417A8);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600_2", func_actor_403600_80141840);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600_2", func_actor_403600_80141954);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600_2", func_actor_403600_801419E8);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600_2", func_actor_403600_80141A34);

void func_actor_403600_80141B24(Actor403600* arg0)
{
    Actor403600Work* work = arg0->field_1C;

    Gp_HaltPadScripts();
    SndEvt_EnqueueType7(0x54160001, 1);
    work->field_708 = 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600_2", func_actor_403600_80141B60);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600_2", func_actor_403600_80141BE0);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600_2", func_actor_403600_80141C3C);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600_2", func_actor_403600_80141C7C);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600_2", func_actor_403600_80141CD4);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600_2", func_actor_403600_80141D30);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600_2", func_actor_403600_80141E78);

void func_actor_403600_80141F28(Actor403600* arg0)
{
    arg0->field_2C->field_8->sub = &Gfx_ViewCoord;
    Gp_EnemyTaskExit((Task*)arg0);
}

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600_2", func_actor_403600_80141F58);
