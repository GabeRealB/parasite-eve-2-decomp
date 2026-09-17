#include "common.h"

#include <psyq/libgte.h>

#include "main/gameflag.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "rooms/dryfield_water_tower.h"
#include "rooms/room_common.h"

extern u8 D_8007216C;
extern u8 D_801153F4;

/// Main-executable globals with no module header yet: `D_80073BA9` is the
/// equipped-weapon index the slot-3 msg 0x3E8 record is keyed on, and
/// `D_8007218A` picks which of the two weapon-id bases that record uses.
extern u8 D_80073BA9;
extern s8 D_8007218A;

extern u32   D_dryfield_water_tower_8018768C;
extern Task* D_dryfield_water_tower_801876A0;

/// The task table `func_dryfield_water_tower_8017F128` spawns the room's two
/// cap props from, at 0x80182384: entry 0 is the cap script itself
/// (`func_dryfield_water_tower_8017F128`), entry 1 the prop
/// `func_dryfield_water_tower_8017E764` and entry 2 the prop
/// `func_dryfield_water_tower_8017E1DC`; the entry whose callback is -1 ends
/// the table.
extern TaskDesc D_dryfield_water_tower_80182384;

/// The room's two cap placements with message 0x7D4, the pair the cap props
/// publish to themselves through `Room_Util08`: `[0]` is the raised position
/// (Y = -0x1F40, the one a task spawned with a non-zero `spawnArg1` publishes)
/// and `[1]` the lowered one (Y = -0xFA0) the lowering prop stops at. They are
/// the two 0x18-byte records above them in the same run --
/// `func_dryfield_water_tower_8017F908` sends `-0x20` and `8017F9AC` `-0x38` --
/// so the split names only the first.
extern RoomPlacement D_dryfield_water_tower_80181A70[];

void func_dryfield_water_tower_8017DCB4(void);

/// Cap-prop task body, in two variants picked by `spawnArg1`. With it zero the
/// task lowers the cap, driven by `DryfieldWaterTowerState::field_58`: state 0
/// spawns the table's entry-2 prop into `field_48` and returns without moving
/// the cap, state 1 hands that prop a 0x7DB record whose `field_2` asks it for
/// state 2, and state 2 publishes the lowered placement above once the cap's
/// coordinate has sunk past it, i.e. once the cap has arrived -- state 3 does
/// nothing and only the states that arrive there reach the shared tail.
///
/// A task spawned with a non-zero `spawnArg1` has no state machine: it
/// publishes the raised placement on its first frame and then only runs the
/// tail, which advances the halfword `field_6A` by 4 and moves the cap's
/// coordinate down by it -- so the cap accelerates by 4 a frame -- leaving the
/// coordinate marked dirty for the next `Gp_UpdateCoord` pass.
void func_dryfield_water_tower_8017DE30(Task* arg0)
{
    DryfieldWaterTowerState* state = (DryfieldWaterTowerState*)arg0->idMap;
    GsCOORDINATE2*           coord = ((TmdObject*)arg0->extra)->field_8;
    DwtwMsg7DB               msg;

    if (arg0->spawnArg1 == 0) {
        switch (state->field_58) {
            case 0:
                state->field_48 = Task_SpawnFromTable(&D_dryfield_water_tower_80182384, 2, 1, 0);
                state->field_58++;
                return;

            case 1:
                msg.field_2 = 2;
                Gp_DispatchMsg(state->field_48, 0x7DB, (s32)&msg, 0);
                state->field_58++;
                /* fallthrough */

            case 2:
                if (coord->coord.t[1] > D_dryfield_water_tower_80181A70[1].pos.vy) {
                    Gp_DispatchMsg(arg0, 0x7D4, (s32)&D_dryfield_water_tower_80181A70[1], 0);
                    state->field_58++;
                    return;
                }
                break;

            case 3:
                return;
        }
    } else if (state->field_58 == 0) {
        Gp_DispatchMsg(arg0, 0x7D4, (s32)&D_dryfield_water_tower_80181A70[0], 0);
        state->field_58++;
    }

    state->field_6A   += 4;
    coord->coord.t[1] += (s16)state->field_6A;
    coord->flg         = 0;
}

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
