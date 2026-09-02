#include "common.h"

#include "actors/actor_335800.h"

#include "gameplay/3CD8.h"

#include "gameplay/gameplay.h"

#include "main/sound.h"

#include "main/task.h"

#include "main/unknown_syms.h"

extern TaskDesc D_actor_335800_80164DE0;

extern TaskDesc D_80182834;

extern s8 D_8007272D;

void func_actor_335800_801620C0(void)
{
    Task_SpawnFromTable(&D_actor_335800_80164DE0, 0, 0, 0);
}

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_2", func_actor_335800_801620F0);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_2", func_actor_335800_80162114);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_2", func_actor_335800_801621B4);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_2", func_actor_335800_8016224C);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_2", func_actor_335800_801622C0);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_2", func_actor_335800_80162364);

void func_actor_335800_801623D8(void)
{
    Task_SpawnFromTable(&D_80182834, 0, 0, 0);
}

void func_actor_335800_80162408(void)
{
    SetDispMask(1);
}

void func_actor_335800_80162428(s8 arg0)
{
    D_8007272D = arg0;
}

void func_actor_335800_80162434(s32 arg0)
{
    SndEvt_EnqueueType2(D_80062735, arg0 & 0xFFFF);
}

void func_actor_335800_80162460(void)
{
    func_800E3FAC(0xA2, 0x18);
}

void func_actor_335800_80162484(void)
{
    Gp_StateC08.field_6 |= 1;
    Gp_PulseState1C();
}

void func_actor_335800_801624B8(s32 arg0)
{
    GameFlag_SetNibble(0x108, arg0);
}

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_2", func_actor_335800_801624DC);

void func_actor_335800_80162558(void)
{
    Task_SpawnFromTable(&D_actor_335800_80164DE0, 3, 0, 0);
}

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_2", func_actor_335800_80162588);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_2", func_actor_335800_80162640);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_2", func_actor_335800_80162844);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_2", func_actor_335800_80162B3C);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_2", func_actor_335800_80162C80);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_2", func_actor_335800_80162E34);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_2", func_actor_335800_80162E8C);

void func_actor_335800_80162F08(void)
{
}

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_2", func_actor_335800_80162F10);

void func_actor_335800_80162F7C(Task* arg0)
{
    Gp_EnemyTaskExit(arg0);
}

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_2", func_actor_335800_80162F9C);

void func_actor_335800_80162FF4(void)
{
}

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_2", func_actor_335800_80162FFC);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_2", func_actor_335800_80163064);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_2", func_actor_335800_80163124);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_2", func_actor_335800_801631A4);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_2", func_actor_335800_801632A4);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_2", func_actor_335800_801633C0);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_2", func_actor_335800_8016343C);

s32 func_actor_335800_8016354C(Task* arg0, s32 arg1, Actor335800Msg* arg2, s32 arg3)
{
    Actor335800MainWork* work;

    work = (Actor335800MainWork*)arg0->idMap;
    if (arg2->field_2 == 0) {
        work->field_504 = 0;
    }
    return 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_2", func_actor_335800_80163568);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_2", func_actor_335800_8016373C);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_2", func_actor_335800_80163880);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_2", func_actor_335800_80163A34);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_2", func_actor_335800_80163AA0);

void func_actor_335800_80163B34(Task* arg0)
{
    Gp_EnemyTaskExit(arg0);
}

void func_actor_335800_80163B54(Task* arg0)
{
    TmdObject*       ext;
    Actor335800Work* work;

    ext           = arg0->extra;
    work          = (Actor335800Work*)arg0->idMap;
    ext->field_1C = &work->light;
    ext->field_20 = &work->color;
}

void func_actor_335800_80163B70(void)
{
}

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_2", func_actor_335800_80163B78);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_2", func_actor_335800_80163BE0);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_2", func_actor_335800_80163CA0);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_2", func_actor_335800_80163D20);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_2", func_actor_335800_80163E20);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_2", func_actor_335800_80163F3C);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_2", func_actor_335800_80163FB8);

s32 func_actor_335800_80164098(void)
{
    return 0;
}
