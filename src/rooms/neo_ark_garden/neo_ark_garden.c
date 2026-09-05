#include "common.h"

#include "main/gameflag.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_garden/neo_ark_garden", D_neo_ark_garden_8017D5C0);

extern GpAreaApplyRec D_neo_ark_garden_80182BF8[];

INCLUDE_ASM("rooms/nonmatchings/neo_ark_garden/neo_ark_garden", func_neo_ark_garden_8017D64C);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_garden/neo_ark_garden", func_neo_ark_garden_8017E2A0);

s32 func_neo_ark_garden_8017E840(void)
{
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/neo_ark_garden/neo_ark_garden", func_neo_ark_garden_8017E848);

s32 func_neo_ark_garden_8017E8DC(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 4) {
        Gp_RunCapCmd1(GameFlag_GetNibble(0x141) != 0 ? 6 : 4);
        if ((GameFlag_GetNibble(0xFA) == 0) && (GameFlag_GetNibble(0xDC) == 0)) {
            GameFlag_SetNibble(0xFA, 1);
            Gp_ApplyAreaRecs(D_neo_ark_garden_80182BF8);
        }
    }
    if (arg2 == 7) {
        Gp_SpawnIfCapIdle(GameFlag_GetNibble(0xC7) != 0 ? 9 : 7, 0);
    }
    if (arg2 == 5) {
        Gp_SpawnIfCapIdle(GameFlag_GetNibble(0xC7) != 0 ? 0xA : 5, 0);
    }
    return 0;
}

s32 func_neo_ark_garden_8017E9AC(void)
{
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/neo_ark_garden/neo_ark_garden", func_neo_ark_garden_8017E9B4);

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_garden/neo_ark_garden", RoomsShared8017d878Table);
