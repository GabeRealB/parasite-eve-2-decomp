#include "common.h"

#include "gameplay/D4.h"

#include "main/task.h"

extern s32   D_neo_ark_forest_zone_80181E38;
extern Task* D_neo_ark_forest_zone_80181E68;

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_forest_zone/neo_ark_forest_zone", D_neo_ark_forest_zone_8017D5C0);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_forest_zone/neo_ark_forest_zone", func_neo_ark_forest_zone_8017D644);

s32 func_neo_ark_forest_zone_8017D7DC(void)
{
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/neo_ark_forest_zone/neo_ark_forest_zone", func_neo_ark_forest_zone_8017D7E4);

s32 func_neo_ark_forest_zone_8017D950(void)
{
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/neo_ark_forest_zone/neo_ark_forest_zone", func_neo_ark_forest_zone_8017D958);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_forest_zone/neo_ark_forest_zone", func_neo_ark_forest_zone_8017DA14);

/// Once the room's own task exists, broadcast message 0x7DB to it, carrying
/// the room's payload record as `arg2`.
void func_neo_ark_forest_zone_8017DA48(void)
{
    if (D_neo_ark_forest_zone_80181E68 != 0) {
        Gp_DispatchMsg(D_neo_ark_forest_zone_80181E68, 0x7DB, (s32)&D_neo_ark_forest_zone_80181E38, 0);
    }
}

INCLUDE_ASM("rooms/nonmatchings/neo_ark_forest_zone/neo_ark_forest_zone", func_neo_ark_forest_zone_8017DA80);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_forest_zone/neo_ark_forest_zone", func_neo_ark_forest_zone_8017DB40);

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_forest_zone/neo_ark_forest_zone", D_neo_ark_forest_zone_8017D5D8);

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_forest_zone/neo_ark_forest_zone", D_neo_ark_forest_zone_8017D5E8);

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_forest_zone/neo_ark_forest_zone", D_neo_ark_forest_zone_8017D5F8);
