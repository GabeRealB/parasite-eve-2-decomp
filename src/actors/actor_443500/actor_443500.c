#include "common.h"
#include "main/task.h"
#include "gameplay/3CD8.h"

extern TaskDesc D_80187618;
extern TaskDesc D_actor_443500_80140E38;

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

void func_actor_443500_80131F18(void)
{
    Task_SpawnFromTable(&D_80187618, 0, 1, 0);
    Gp_MsgPlayer3F3(0);
    Gp_MsgPlayerWeapon(0);
}

void func_actor_443500_80131F58(void)
{
    Task_SpawnFromTable(&D_actor_443500_80140E38, 0, 0, 0);
}

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

INCLUDE_RODATA("actors/nonmatchings/actor_443500/actor_443500", D_actor_443500_80131E20);

INCLUDE_RODATA("actors/nonmatchings/actor_443500/actor_443500", D_actor_443500_80131E24);

INCLUDE_RODATA("actors/nonmatchings/actor_443500/actor_443500", D_actor_443500_80131E30);
