#include "common.h"

#include <psyq/libgte.h>

#include "main/gameflag.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

extern u8 D_8007216C;
extern u8 D_801153F4;

/// Main-executable globals with no module header yet: `D_80073BA9` is the
/// equipped-weapon index the slot-3 msg 0x3E8 record is keyed on, and
/// `D_8007218A` picks which of the two weapon-id bases that record uses.
extern u8 D_80073BA9;
extern s8 D_8007218A;

extern u32   D_dryfield_water_tower_8018768C;
extern Task* D_dryfield_water_tower_801876A0;

void func_dryfield_water_tower_8017DCB4(void);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tower/dryfield_water_tower_3", func_dryfield_water_tower_8017DE30);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tower/dryfield_water_tower_3", func_dryfield_water_tower_8017DFAC);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tower/dryfield_water_tower_3", func_dryfield_water_tower_8017E1DC);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tower/dryfield_water_tower_3", func_dryfield_water_tower_8017E428);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tower/dryfield_water_tower_3", func_dryfield_water_tower_8017E5B0);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tower/dryfield_water_tower_3", func_dryfield_water_tower_8017E764);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tower/dryfield_water_tower_3", func_dryfield_water_tower_8017E93C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tower/dryfield_water_tower_3", func_dryfield_water_tower_8017EB7C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tower/dryfield_water_tower_3", func_dryfield_water_tower_8017F128);

/// The 0x0D entry of three of the room's script tables -- at 0x80181C94,
/// 0x80181EEC and 0x801820E4, each one word above its `.word 0x0D` opcode. The
/// interpreter runs that opcode as `((void (*)(s32))arg0)(arg1)`, so the
/// script's `arg1` arrives here: 0 on the first two records and 0xA on the one
/// at 0x801820E4.
///
/// Republishes the player's weapon to slot 3 (msg 0x3E8) the way
/// `func_actor_136100_8013467C` does -- `D_80073BA9` picked through
/// `D_8007218A` is the record's `field_0` -- but fills the two halfword slots
/// from that script argument: `field_8` is its "non-zero" flag and `field_C`
/// the halfword itself.
void func_dryfield_water_tower_8017F700(s32 arg0)
{
    GpRec14 rec;
    s32     weaponId;
    s32     id;
    s32     value;

    weaponId     = D_80073BA9;
    id           = (D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22;
    value        = arg0 & 0xFFFF;
    rec.field_0  = id;
    rec.field_4  = 1;
    rec.field_8  = value != 0;
    rec.field_C  = value;
    rec.field_10 = 0;
    Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3E8, (s32)&rec, 0);
}
