#include "common.h"
#include "main/task.h"
#include "gameplay/3CD8.h"

void func_actor_443500_80131E3C(s32 arg0)
{
    if (arg0 != 0) {
        Gp_CapFile = 0;
        Gp_LoadCapFile(1);
        func_800E6D4C(0x240, 0x100);
        return;
    }
    Gp_ResetCap();
}

void func_actor_443500_80131E84(s32 arg0)
{
    if (GameFlag_GetNibble(0xDF) > 0) {
        if (arg0 != 0) {
            Gp_CapFile = 0;
            Gp_LoadCapFile(1);
            func_800E6D4C(0x240, 0x100);
            return;
        }
        Gp_ResetCap();
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_443500/actor_443500", func_actor_443500_80131EE4);

INCLUDE_ASM("actors/nonmatchings/actor_443500/actor_443500", func_actor_443500_80131F18);

INCLUDE_ASM("actors/nonmatchings/actor_443500/actor_443500", func_actor_443500_80131F58);

void func_actor_443500_80131F88(Task* arg0)
{
    s16 temp_v0;
    s32 temp_a0;

    temp_a0 = (((0x1E - arg0->killCountdown) * 0xFF) / 30) & 0xFF;
    Fade_DrawOverlay(temp_a0, temp_a0, temp_a0, 2);
    temp_v0             = (u16)arg0->killCountdown + 1;
    arg0->killCountdown = temp_v0;
    if (temp_v0 >= 0x1E) {
        Task_Kill(arg0);
    }
}

void func_actor_443500_8013201C(s16 arg0)
{
    Gp_StartCapSlot(5, 1, arg0);
}

INCLUDE_ASM("actors/nonmatchings/actor_443500/actor_443500", func_actor_443500_80132048);

INCLUDE_ASM("actors/nonmatchings/actor_443500/actor_443500", func_actor_443500_8013206C);

INCLUDE_ASM("actors/nonmatchings/actor_443500/actor_443500", func_actor_443500_80132078);

INCLUDE_ASM("actors/nonmatchings/actor_443500/actor_443500", func_actor_443500_801321F0);

INCLUDE_RODATA("actors/nonmatchings/actor_443500/actor_443500", D_actor_443500_80131E20);

INCLUDE_ASM("actors/nonmatchings/actor_443500/actor_443500", func_actor_443500_8013253C);

INCLUDE_ASM("actors/nonmatchings/actor_443500/actor_443500", func_actor_443500_80132594);

INCLUDE_ASM("actors/nonmatchings/actor_443500/actor_443500", func_actor_443500_801326A0);

INCLUDE_ASM("actors/nonmatchings/actor_443500/actor_443500", func_actor_443500_80132738);

INCLUDE_ASM("actors/nonmatchings/actor_443500/actor_443500", func_actor_443500_801327A4);

INCLUDE_ASM("actors/nonmatchings/actor_443500/actor_443500", func_actor_443500_801327C4);

INCLUDE_ASM("actors/nonmatchings/actor_443500/actor_443500", func_actor_443500_801327E0);

INCLUDE_ASM("actors/nonmatchings/actor_443500/actor_443500", func_actor_443500_80132900);

INCLUDE_ASM("actors/nonmatchings/actor_443500/actor_443500", func_actor_443500_8013297C);

INCLUDE_ASM("actors/nonmatchings/actor_443500/actor_443500", func_actor_443500_80132A68);
