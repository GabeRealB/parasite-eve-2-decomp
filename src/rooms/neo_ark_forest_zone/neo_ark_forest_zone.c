#include "common.h"

#include "gameplay/D4.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"

extern s32   D_neo_ark_forest_zone_80181E30;
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

/// Room entry task tick: on the first visit (`Game_Session::field_9 == 1`) with
/// flag 0xBD unset, broadcast message 0x7DB to the room's own task carrying its
/// first payload record, then advance state.
void func_neo_ark_forest_zone_8017DB40(Task* arg0)
{
    if (Game_Session->field_9 == 1 && GameFlag_GetNibble(0xBD) == 0) {
        Gp_DispatchMsg(D_neo_ark_forest_zone_80181E68, 0x7DB, (s32)&D_neo_ark_forest_zone_80181E30, 0);
    }
    arg0->state = arg0->state + 1;
}

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_forest_zone/neo_ark_forest_zone", D_neo_ark_forest_zone_8017D5D8);

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_forest_zone/neo_ark_forest_zone", D_neo_ark_forest_zone_8017D5E8);

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_forest_zone/neo_ark_forest_zone", D_neo_ark_forest_zone_8017D5F8);
