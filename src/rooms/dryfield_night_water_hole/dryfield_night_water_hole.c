#include "common.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/dryfield_night_water_hole.h"

extern GpMsgEntry        D_dryfield_night_water_hole_801805F8[];
extern s32               D_dryfield_night_water_hole_8018065C;
extern s32               D_dryfield_night_water_hole_80180660;
extern TaskDesc          D_dryfield_night_water_hole_80180964[];
extern DnwhParamOverride D_dryfield_night_water_hole_801835D8[];
extern TaskDesc          D_801351FC[];
INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_water_hole/dryfield_night_water_hole", D_dryfield_night_water_hole_8017D5C0);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_water_hole/dryfield_night_water_hole", func_dryfield_night_water_hole_8017D6AC);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_water_hole/dryfield_night_water_hole", func_dryfield_night_water_hole_8017D7E8);

/// The room's message table, the `GpMsgEntry` list the room task publishes in
/// `Task::msgTable` for `Gp_DispatchMsg` to walk: 0x13EE, 0x13F1, 0x13EF and
/// 0x13F0, whose handler is `func_dryfield_night_water_hole_8017DC28`.
extern GpMsgEntry D_dryfield_night_water_hole_801805F8[];
/// The two four-byte records this room hands the slot-4 task as the message
/// 0x7DB payload, picked by `gGameSession::at4.loc.warp`. They are the last two of
/// the four-record run at 0x80180654, which differ only in the halfword at 0x2.
extern s32 D_dryfield_night_water_hole_8018065C;
extern s32 D_dryfield_night_water_hole_80180660;
/// Descriptor of the room's event task, spawned while progress nibble 0xB8 is
/// still clear. Its callback is `func_dryfield_night_water_hole_8017E630`.
extern TaskDesc D_dryfield_night_water_hole_80180964[];
/// Override list applied once nibble 0xB8 is set.
extern DnwhParamOverride D_dryfield_night_water_hole_801835D8[];
/// Resident task table the ending task is spawned from, descriptor 1.
extern TaskDesc D_801351FC[];

/// Room entry task tick: publish the message table above in `Task::msgTable`
/// and claim game pointer slot 7. Progress nibble 0xB8 then picks the opening
/// move: while it is clear the room's event task is spawned from
/// `D_dryfield_night_water_hole_80180964`, and once it is set the parameter
/// overrides are applied instead.
///
/// On the visit whose sub-id (`gGameSession::at4.loc.place`) is 1 and that has
/// already latched nibble 0x95, and with the slot-4 task present, the room
/// announces itself to it with message 0x7DB, carrying the payload record
/// `gGameSession::at4.loc.warp` selects. On sub-id 0xA, with pointer slot 0xA
/// filled and nibble 0xCF still clear, it latches 0xCF, arms
/// `func_800E3FAC(0xA2, 0x25)` and spawns the ending task. Then advances state.
void func_dryfield_night_water_hole_8017D958(Task* arg0)
{
    arg0->msgTable = D_dryfield_night_water_hole_801805F8;
    Game_SetPtrSlot(arg0, 7);
    if (GameFlag_GetNibble(0xB8) == 0) {
        Task_SpawnFromTable(D_dryfield_night_water_hole_80180964, 0, 0, 0);
    } else {
        func_dryfield_night_water_hole_8017DE88(D_dryfield_night_water_hole_801835D8);
    }
    if (gGameSession->at4.loc.place == 1 && Gp_LookupSlot4(0) != 0 && GameFlag_GetNibble(0x95) != 0) {
        if (gGameSession->at4.loc.warp == 2) {
            Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7DB, (s32)&D_dryfield_night_water_hole_80180660, 0);
        } else {
            Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7DB, (s32)&D_dryfield_night_water_hole_8018065C, 0);
        }
    }
    if (gGameSession->at4.loc.place == 0xA && Game_GetPtrSlot(0xA) != 0 && GameFlag_GetNibble(0xCF) == 0) {
        GameFlag_SetNibble(0xCF, 2);
        func_800E3FAC(0xA2, 0x25);
        Task_SpawnFromTable(D_801351FC, 1, 0, 0);
    }
    arg0->state = arg0->state + 1;
}

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_water_hole/dryfield_night_water_hole", RoomsShared8017d878Table);
