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

INCLUDE_RODATA("actors/nonmatchings/actor_335800/actor_335800_2", ActorsShared80138404Table);

INCLUDE_RODATA("actors/nonmatchings/actor_335800/actor_335800_2", D_actor_335800_80161E30);

INCLUDE_RODATA("actors/nonmatchings/actor_335800/actor_335800_2", ActorsShared801327f8Table);

INCLUDE_RODATA("actors/nonmatchings/actor_335800/actor_335800_2", ActorsShared80132920Offset);
INCLUDE_RODATA("actors/nonmatchings/actor_335800/actor_335800_2", D_actor_335800_80161E5C);
INCLUDE_RODATA("actors/nonmatchings/actor_335800/actor_335800_2", D_actor_335800_80161E68);
INCLUDE_RODATA("actors/nonmatchings/actor_335800/actor_335800_2", D_actor_335800_80161E78);
