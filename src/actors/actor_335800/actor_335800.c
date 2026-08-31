#include "common.h"
#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"
#include "main/task.h"
extern TaskDesc D_actor_335800_80164DE0;

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800", func_actor_335800_80161E88);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800", func_actor_335800_80162040);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800", func_actor_335800_80162060);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800", func_actor_335800_80162080);

void func_actor_335800_801620A0(void)
{
    CdCmd_CancelReplaceAndActivate();
}

void func_actor_335800_801620C0(void)
{
    Task_SpawnFromTable(&D_actor_335800_80164DE0, 0, 0, 0);
}

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800", func_actor_335800_801620F0);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800", func_actor_335800_80162114);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800", func_actor_335800_801621B4);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800", func_actor_335800_8016224C);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800", func_actor_335800_801622C0);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800", func_actor_335800_80162364);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800", func_actor_335800_801623D8);

void func_actor_335800_80162408(void)
{
    SetDispMask(1);
}

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800", func_actor_335800_80162428);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800", func_actor_335800_80162434);

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

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800", func_actor_335800_801624DC);

void func_actor_335800_80162558(void)
{
    Task_SpawnFromTable(&D_actor_335800_80164DE0, 3, 0, 0);
}

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800", func_actor_335800_80162588);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800", func_actor_335800_80162640);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800", func_actor_335800_80162844);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800", func_actor_335800_80162B3C);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800", func_actor_335800_80162C80);

INCLUDE_RODATA("actors/nonmatchings/actor_335800/actor_335800", D_actor_335800_80161E20);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800", func_actor_335800_80162E34);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800", func_actor_335800_80162E8C);

void func_actor_335800_80162F08(void)
{
}

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800", func_actor_335800_80162F10);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800", func_actor_335800_80162F7C);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800", func_actor_335800_80162F9C);

void func_actor_335800_80162FF4(void)
{
}

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800", func_actor_335800_80162FFC);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800", func_actor_335800_80163064);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800", func_actor_335800_80163124);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800", func_actor_335800_801631A4);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800", func_actor_335800_801632A4);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800", func_actor_335800_801633C0);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800", func_actor_335800_8016343C);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800", func_actor_335800_8016354C);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800", func_actor_335800_80163568);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800", func_actor_335800_8016373C);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800", func_actor_335800_80163880);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800", func_actor_335800_80163A34);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800", func_actor_335800_80163AA0);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800", func_actor_335800_80163B34);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800", func_actor_335800_80163B54);

void func_actor_335800_80163B70(void)
{
}

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800", func_actor_335800_80163B78);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800", func_actor_335800_80163BE0);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800", func_actor_335800_80163CA0);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800", func_actor_335800_80163D20);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800", func_actor_335800_80163E20);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800", func_actor_335800_80163F3C);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800", func_actor_335800_80163FB8);

s32 func_actor_335800_80164098(void)
{
    return 0;
}
