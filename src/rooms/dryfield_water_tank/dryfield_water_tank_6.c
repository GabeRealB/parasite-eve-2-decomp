#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "main/task.h"

#include "rooms/room_common.h"
#include "rooms/rooms_shared_80180b2c.h"

extern TaskDesc D_dryfield_water_tank_80184DF4;

/// Per-view halfword table, indexed 1-based by `Gp_GetViewIndex()`. The value
/// the room publishes as its `Gp_State1C::field_A` variant index.
extern u16 D_dryfield_water_tank_801868CC[];

/// The placement the room sends slot 3 with message 0x3E9.
extern RoomPlacement D_dryfield_water_tank_801804F4;

/// The water tank's run: one `SVECTOR` position per frame, `y` fixed at -12000
/// and `z` stepping up the room, 52 entries of movement before the tail clamps.
extern SVECTOR D_dryfield_water_tank_80184530[];

/// Main-executable globals with no module header yet: `D_80073BA9` is the
/// equipped-weapon index the slot-3 msg 0x3E8 record is keyed on, and
/// `D_8007218A` picks which of the two weapon-id bases that record uses.
extern u8 D_80073BA9;
extern s8 D_8007218A;

/// Stages the room's cutscene: tells the script task the water-tank scene is
/// entered (msg 0x3E9 with the room's fixed placement), then hands slot 3 the
/// 0x3E8 record that installs the equipped weapon's animation set -- the same
/// five-word record `acropolis_observatory_4` fills, `field_4` 1 and the rest
/// zero. The display is turned back on last, because entering the scene is what
/// blanked it.
void func_dryfield_water_tank_8017EBA0(void)
{
    GpRec14 rec;
    s32     weaponId;
    s32     anim;

    Gp_DispatchMsg(((RoomsShared80180b2cWork*)RoomsShared80180b2cTask->idMap)->owner, 0x3E9,
                   (s32)&D_dryfield_water_tank_801804F4, 0);
    weaponId     = D_80073BA9;
    anim         = (D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22;
    rec.field_0  = anim;
    rec.field_4  = 1;
    rec.field_8  = 0;
    rec.field_C  = 0;
    rec.field_10 = 0;
    Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3E8, (s32)&rec, 0);
    SetDispMask(1);
}

void func_dryfield_water_tank_8017EC38(u32 arg0)
{
    Task_SpawnFromTable(&D_dryfield_water_tank_80184DF4, arg0 & 0xFFFF, (s32)(arg0 >> 0x10), 0);
}

/// Walks the water tank one step along `D_dryfield_water_tank_80184530` per
/// frame: sends slot 3 that entry as a `RoomPlacement` -- the spline position
/// with the tank's fixed half-turn about `y` -- and advances `killCountdown`.
/// At 0x34 the tank has finished its run, and the task kills itself.
void func_dryfield_water_tank_8017EC6C(Task* arg0)
{
    RoomPlacement rec;

    if (arg0->killCountdown >= 0x34) {
        Task_Kill(arg0);
        return;
    }
    rec.pos.vx = D_dryfield_water_tank_80184530[arg0->killCountdown].vx;
    rec.pos.vy = D_dryfield_water_tank_80184530[arg0->killCountdown].vy;
    rec.pos.vz = D_dryfield_water_tank_80184530[arg0->killCountdown].vz;
    rec.rot.vx = 0;
    rec.rot.vy = -0x7FF;
    rec.rot.vz = 0;
    arg0->killCountdown++;
    Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3E9, (s32)&rec, 0);
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank_6", func_dryfield_water_tank_8017ED30);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank_6", func_dryfield_water_tank_8017EDF4);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank_6", func_dryfield_water_tank_8017EFF4);

/// Publishes the variant index the current camera view maps to: reads the view
/// index back and stores `D_dryfield_water_tank_801868CC[view - 1]` into the
/// shared work block's `field_A`. Gameplay holds this address in its data
/// (0x80110614, pointing at the room overlay), and `dryfield_parking_lot` and
/// `dryfield_water_tower` carry the same body.
void func_dryfield_water_tank_8017F084(void)
{
    Gp_State1C->field_A = D_dryfield_water_tank_801868CC[(Gp_GetViewIndex() & 0xFF) - 1];
}
