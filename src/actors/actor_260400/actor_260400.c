#include "common.h"

#include "actors/actor_260400.h"

#include "gameplay/1BC.h"
#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/task.h"

extern s32 D_actor_260400_8014C788;
extern s32 D_actor_260400_8014CF38;
extern s32 D_actor_260400_8014D118;
extern s32 D_actor_260400_8014D208;
extern s32 D_actor_260400_8014D340;
extern s32 D_actor_260400_8014D4A8;
extern s32 D_actor_260400_8014D610;

void func_actor_260400_80149E38(void)
{
    switch (GameFlag_GetNibble(0xE3)) {
        case 0:
            func_800E8634((s32)&D_actor_260400_8014C788, 0, (s32)&D_actor_260400_8014CF38);
            GameFlag_SetNibble(0xE3, 1);
            break;
        case 1:
            if ((Gp_GetCurBit2Flag(4) == 1) || (Gp_GetCurBit2Flag(5) == 1)) {
                func_800E8614((s32)&D_actor_260400_8014D118, 0);
            } else {
                func_800E8614((s32)&D_actor_260400_8014D208, 0);
                GameFlag_SetNibble(0xE3, 2);
            }
            break;
        case 2:
            func_800E8614((s32)&D_actor_260400_8014D340, 0);
            GameFlag_SetNibble(0xE3, 3);
            break;
        case 3:
            func_800E8614((s32)&D_actor_260400_8014D4A8, 0);
            GameFlag_SetNibble(0xE3, 4);
            break;
        case 4:
            func_800E8614((s32)&D_actor_260400_8014D610, 0);
            break;
    }
}

void func_actor_260400_80149F5C(s32 arg0)
{
    if (arg0 != 0) {
        Gp_CapFile = 0;
        Gp_LoadCapFile(1);
        func_800E6D4C(0x340, 0);
        return;
    }
    Gp_ResetCap();
}

INCLUDE_ASM("actors/nonmatchings/actor_260400/actor_260400", func_actor_260400_80149FA4);

INCLUDE_ASM("actors/nonmatchings/actor_260400/actor_260400", ActorsShared80131f9cSub0);

INCLUDE_ASM("actors/nonmatchings/actor_260400/actor_260400", func_actor_260400_8014A200);
