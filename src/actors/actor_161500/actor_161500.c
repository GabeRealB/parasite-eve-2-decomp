#include "common.h"

#include "gameplay/3CD8.h"
#include "main/gameflag.h"

extern s32 D_actor_161500_80135668;
extern s32 D_actor_161500_801357E8;
extern s32 D_actor_161500_80135968;
extern s32 D_actor_161500_80135AE8;
extern s32 D_actor_161500_80135C68;

void func_actor_161500_80131E38(void)
{
    if ((GameFlag_GetNibble(0x116) != 1) && (GameFlag_GetNibble(0x116) != 2) && (GameFlag_GetNibble(0x113) == 4)) {
        GameFlag_SetNibble(0x113, 5);
        GameFlag_SetNibble(0x116, 4);
    }

    switch (GameFlag_GetNibble(0x116)) {
        case 0:
            func_800E8614((s32)&D_actor_161500_80135668, 0);
            break;
        case 1:
            func_800E8614((s32)&D_actor_161500_801357E8, 0);
            GameFlag_SetNibble(0x116, 3);
            break;
        case 2:
            func_800E8614((s32)&D_actor_161500_80135968, 0);
            GameFlag_SetNibble(0x116, 3);
            break;
        case 3:
            func_800E8614((s32)&D_actor_161500_80135AE8, 0);
            GameFlag_SetNibble(0x116, 0);
            break;
        case 4:
            func_800E8614((s32)&D_actor_161500_80135C68, 0);
            GameFlag_SetNibble(0x116, 0);
            break;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_161500/actor_161500", func_actor_161500_80131F50);

INCLUDE_ASM("actors/nonmatchings/actor_161500/actor_161500", func_actor_161500_80131FBC);

INCLUDE_ASM("actors/nonmatchings/actor_161500/actor_161500", func_actor_161500_80132038);

INCLUDE_ASM("actors/nonmatchings/actor_161500/actor_161500", func_actor_161500_801320B4);

INCLUDE_ASM("actors/nonmatchings/actor_161500/actor_161500", func_actor_161500_801320F0);

INCLUDE_ASM("actors/nonmatchings/actor_161500/actor_161500", func_actor_161500_80132110);

INCLUDE_ASM("actors/nonmatchings/actor_161500/actor_161500", func_actor_161500_80132150);

INCLUDE_ASM("actors/nonmatchings/actor_161500/actor_161500", func_actor_161500_801321B4);

INCLUDE_ASM("actors/nonmatchings/actor_161500/actor_161500", func_actor_161500_80132210);

INCLUDE_ASM("actors/nonmatchings/actor_161500/actor_161500", func_actor_161500_80132294);

INCLUDE_ASM("actors/nonmatchings/actor_161500/actor_161500", func_actor_161500_801322A0);

INCLUDE_ASM("actors/nonmatchings/actor_161500/actor_161500", func_actor_161500_8013230C);

INCLUDE_ASM("actors/nonmatchings/actor_161500/actor_161500", func_actor_161500_80132394);

INCLUDE_ASM("actors/nonmatchings/actor_161500/actor_161500", func_actor_161500_8013252C);

INCLUDE_ASM("actors/nonmatchings/actor_161500/actor_161500", func_actor_161500_801326E8);

INCLUDE_ASM("actors/nonmatchings/actor_161500/actor_161500", func_actor_161500_8013273C);

INCLUDE_ASM("actors/nonmatchings/actor_161500/actor_161500", func_actor_161500_8013284C);

INCLUDE_ASM("actors/nonmatchings/actor_161500/actor_161500", func_actor_161500_80132874);

INCLUDE_ASM("actors/nonmatchings/actor_161500/actor_161500", func_actor_161500_80132900);

INCLUDE_ASM("actors/nonmatchings/actor_161500/actor_161500", func_actor_161500_8013294C);

INCLUDE_ASM("actors/nonmatchings/actor_161500/actor_161500", func_actor_161500_801329C4);

INCLUDE_ASM("actors/nonmatchings/actor_161500/actor_161500", func_actor_161500_80132A28);

INCLUDE_ASM("actors/nonmatchings/actor_161500/actor_161500", func_actor_161500_80132A94);

INCLUDE_ASM("actors/nonmatchings/actor_161500/actor_161500", func_actor_161500_80132B10);

INCLUDE_ASM("actors/nonmatchings/actor_161500/actor_161500", func_actor_161500_80132B88);

INCLUDE_ASM("actors/nonmatchings/actor_161500/actor_161500", func_actor_161500_80132BA0);

INCLUDE_ASM("actors/nonmatchings/actor_161500/actor_161500", func_actor_161500_80132C6C);
