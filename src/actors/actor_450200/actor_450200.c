#include "common.h"

#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"
extern TaskDesc D_actor_450200_8013FB40;

extern void func_8017FA98(s32);

extern s32      D_actor_450200_80138870;
extern s32      D_actor_450200_80138A68;
extern s32      D_actor_450200_80138C60;
extern s32      D_actor_450200_80138E88;
extern s32      D_actor_450200_80139098;
extern TaskDesc D_actor_450200_80137A60;
extern Task*    D_actor_450200_801401E0;

INCLUDE_ASM("actors/nonmatchings/actor_450200/actor_450200", func_actor_450200_80131E24);

INCLUDE_ASM("actors/nonmatchings/actor_450200/actor_450200", func_actor_450200_80131FA8);

INCLUDE_ASM("actors/nonmatchings/actor_450200/actor_450200", func_actor_450200_801320D4);

void func_actor_450200_8013215C(void)
{
    Gp_PulseState1C();
}

INCLUDE_ASM("actors/nonmatchings/actor_450200/actor_450200", func_actor_450200_8013217C);

INCLUDE_ASM("actors/nonmatchings/actor_450200/actor_450200", func_actor_450200_8013219C);

void func_actor_450200_80132220(void)
{
    switch (GameFlag_GetNibble(0x101)) {
        case 0:
            func_800E8614((s32)&D_actor_450200_80138870, 0);
            GameFlag_SetNibble(0x101, 1);
            break;
        case 1:
            func_800E8614((s32)&D_actor_450200_80138A68, 0);
            GameFlag_SetNibble(0x101, 2);
            break;
        case 2:
            func_800E8614((s32)&D_actor_450200_80138C60, 0);
            GameFlag_SetNibble(0x101, 3);
            break;
        case 3:
            func_800E8614((s32)&D_actor_450200_80138E88, 0);
            break;
    }
}

void func_actor_450200_801322F8(void)
{
    if (GameFlag_GetNibble(0xD7) != 0) {
        func_800E8614((s32)&D_actor_450200_80139098, 1);
    } else {
        func_8017FA98(0);
    }
    if (Game_GetPtrSlot(0xA) != NULL) {
        D_actor_450200_801401E0 = Task_SpawnFromTable(&D_actor_450200_80137A60, 2, 0, 0);
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_450200/actor_450200", func_actor_450200_80132368);

INCLUDE_ASM("actors/nonmatchings/actor_450200/actor_450200", func_actor_450200_80132538);

void func_actor_450200_80132848(s32 arg0)
{
    if (arg0 == 1) {
        Task_SpawnFromTable(&D_actor_450200_8013FB40, 0, 0, 0);
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_450200/actor_450200", func_actor_450200_80132880);

INCLUDE_ASM("actors/nonmatchings/actor_450200/actor_450200", func_actor_450200_801328A0);

INCLUDE_RODATA("actors/nonmatchings/actor_450200/actor_450200", D_actor_450200_80131E20);
