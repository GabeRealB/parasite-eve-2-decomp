#include "common.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/fs.h"
#include "main/session.h"

#include "rooms/dryfield_water_tower.h"
#include "rooms/room_common.h"

extern s8  D_8007216C;
extern u16 D_dryfield_water_tower_801827A0[];

/// Placement `func_dryfield_water_tower_80180220` sends to `field_0` with
/// message 0x3E9.
extern RoomPlacement D_dryfield_water_tower_801823A8;

/// The pair of placements the same function sends with message 0x7D4, one to
/// each of `field_8` and `field_4`; the second is the element at 0x18, so the
/// run is declared as an array. Both are `Room_Util08` payloads, the handler
/// the room's script table pairs with 0x7D4.
extern RoomPlacement D_dryfield_water_tower_801823D8[];

void func_dryfield_water_tower_80180174(s16 arg0)
{
    DwtwWork* work = (DwtwWork*)D_dryfield_water_tower_801876AC->work;

    work->field_C = arg0;
    work->field_E = 0;
}

/// Armed once per room: hands the slot-4 task the session's two id bytes as
/// message 0x7DA's payload, then sets `field_14` so the message goes out only
/// the first time. The halfword it zeroes is the state the 0x7DB handler reads.
void func_dryfield_water_tower_80180194(void)
{
    DwtwWork*  work = (DwtwWork*)D_dryfield_water_tower_801876AC->work;
    DwtwMsg7DB msg;

    if (work->field_14 == 0) {
        Gp_ArmStateF0(1);
        msg.field_0 = gGameSession->loc.stage;
        msg.field_1 = gGameSession->loc.area;
        msg.field_2 = 0;
        Gp_DispatchMsg(Game_GetPtrSlot(4), 0x7DA, (s32)&msg, 0x7DB);
        work->field_14 = 1;
    }
}

/// Places the room's two prop tasks and the slot-3 game task: the first two
/// 0x7D4 placements go to `field_8` / `field_4`, then `field_0` gets the 0x3F3
/// (1) and 0x3E9 commands that move the player to `D_..._801823A8`. The area
/// record takes view 4 and the session is dropped back to state 1 before the
/// stream RNG is restored.
void func_dryfield_water_tower_80180220(void)
{
    DwtwWork* work = (DwtwWork*)D_dryfield_water_tower_801876AC->work;

    Gp_DispatchMsg(work->field_8, 0x7D4, (s32)&D_dryfield_water_tower_801823D8[0], 0);
    Gp_DispatchMsg(work->field_4, 0x7D4, (s32)&D_dryfield_water_tower_801823D8[1], 0);
    Gp_DispatchMsg(work->field_0, 0x3F3, 1, 0);
    Gp_DispatchMsg(work->field_0, 0x3E9, (s32)&D_dryfield_water_tower_801823A8, 0);
    D_8007216C              = Gp_FindViewIndex(4);
    gGameSession->viewDirty = 1;
    CdCmd_CancelReplaceAndActivate();
    Gp_RestoreStreamRng();
}

/// Sets the current view's skip-OT-link byte from the nibble-0x55 band: the
/// argument's low byte zero skips the view's sprites, non-zero draws them. Only
/// the stage byte 2 (the first stage table) has a record to write, and the
/// lookup walks it exactly like the `Room_Util16` / `Room_Util17` bodies, down
/// to taking `&gGameSession->loc.view` as its own pointer.
void func_dryfield_water_tower_801802D8(u8 arg0)
{
    GameSessionFrom4*  sess;
    DwtwSprtViewState* vs;

    sess = &gGameSession->loc;
    if (sess->stage == 2) {
        vs = ((DwtwSprtRec*)Gp_SprtTables[sess->stage - 1]->field_0[sess->area - 1])->field_DC;
        if (!(arg0 & 0xFF)) {
            vs->field_C = 1;
            return;
        }
        vs->field_C = 0;
    }
}

void func_dryfield_water_tower_80180348(void)
{
    Gp_State1C->field_A = D_dryfield_water_tower_801827A0[(Gp_GetViewIndex() & 0xFF) - 1];
}
