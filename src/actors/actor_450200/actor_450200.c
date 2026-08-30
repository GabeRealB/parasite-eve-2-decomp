#include "common.h"

#include "gameplay/3CD8.h"
#include "main/gameflag.h"

extern s32 D_actor_450200_80138870;
extern s32 D_actor_450200_80138A68;
extern s32 D_actor_450200_80138C60;
extern s32 D_actor_450200_80138E88;

INCLUDE_ASM("actors/nonmatchings/actor_450200/actor_450200", func_actor_450200_80131E24);

INCLUDE_ASM("actors/nonmatchings/actor_450200/actor_450200", func_actor_450200_80131FA8);

INCLUDE_ASM("actors/nonmatchings/actor_450200/actor_450200", func_actor_450200_801320D4);

INCLUDE_ASM("actors/nonmatchings/actor_450200/actor_450200", func_actor_450200_8013215C);

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

INCLUDE_ASM("actors/nonmatchings/actor_450200/actor_450200", func_actor_450200_801322F8);

INCLUDE_ASM("actors/nonmatchings/actor_450200/actor_450200", func_actor_450200_80132368);

INCLUDE_ASM("actors/nonmatchings/actor_450200/actor_450200", func_actor_450200_80132538);

INCLUDE_ASM("actors/nonmatchings/actor_450200/actor_450200", func_actor_450200_80132848);

INCLUDE_ASM("actors/nonmatchings/actor_450200/actor_450200", func_actor_450200_80132880);

INCLUDE_ASM("actors/nonmatchings/actor_450200/actor_450200", func_actor_450200_801328A0);

INCLUDE_RODATA("actors/nonmatchings/actor_450200/actor_450200", D_actor_450200_80131E20);
