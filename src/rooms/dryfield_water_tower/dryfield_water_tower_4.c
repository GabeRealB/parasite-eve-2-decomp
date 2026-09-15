#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "main/mc.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"

#include "rooms/dryfield_water_tower.h"
#include "rooms/room_common.h"

/// Placement `func_dryfield_water_tower_8017F908` sends to the prop task at
/// `DryfieldWaterTowerState::field_44` with message 0x7D4, whose handler is
/// `Room_Util08` -- the same message and handler the table's other 0x0D entry
/// `func_dryfield_water_tower_8017FA5C` uses below. One whole 0x18-byte run,
/// unlike the placement that one sends, which splat splits in three.
extern RoomPlacement D_dryfield_water_tower_80181A58;

/// Placement `func_dryfield_water_tower_8017FA5C` sends to the prop task at
/// `DryfieldWaterTowerState::field_48` with message 0x7D4, whose handler is
/// `Room_Util08`. The first of three chunks splat splits it into; the other two
/// (`80181AA4` / `80181AA8`) are the rest of the 0x18 bytes.
extern RoomPlacement D_dryfield_water_tower_80181AA0;

/// Placement the same function sends to the slot-3 game task at `field_40` with
/// message 0x3E9, the player move `func_dryfield_water_tower_80180220` also
/// uses. 0x18 bytes, one word run in the split.
extern RoomPlacement D_dryfield_water_tower_80181AD0;

/// `func_dryfield_water_tower_8017F908` writes the room's view index here and
/// `func_dryfield_water_tower_8017FA5C` writes view 9's; the byte is
/// `Mc_SaveData.field_4`, the saved location `Gp_CommitSpawnLoc` sets and
/// `Gp_ReloadAtLoc` restores. Spelled by address because that is the name the
/// room imports: the struct spelling prints `%hi(Mc_SaveData)` /
/// `%lo(Mc_SaveData+4)` and costs the scratch scorer 0.24% on an object whose
/// words are identical, while the entry below needs it for aliasing. Both
/// spellings are deliberate.
extern s8 D_8007216C;

/// The three effect-definition tables `func_dryfield_water_tower_8017FA5C`
/// installs into the room's live copies -- 0x10, 0x18 and 0x40 bytes. The same
/// three pairs, at the same sizes, are copied by
/// `func_dryfield_water_tower_8017F128` on its state-7 path, so each is one
/// variant of a table the room selects between rather than the only content.
/// The destinations are also reached with an offset from a lower base
/// (`0x801828CC + 0x10` is `801828DC`), i.e. the live table is a region the
/// room walks. The record types are not pinned yet, so the blobs stay bytes.
extern u8 D_dryfield_water_tower_80181B10[];
extern u8 D_dryfield_water_tower_801828DC[];
extern u8 D_dryfield_water_tower_80181BA0[];
extern u8 D_dryfield_water_tower_80182F44[];
extern u8 D_dryfield_water_tower_80181B20[];
extern u8 D_dryfield_water_tower_801829F4[];

void func_dryfield_water_tower_8017F8E8(s16 arg0)
{
    DryfieldWaterTowerState* state = (DryfieldWaterTowerState*)D_dryfield_water_tower_801876A4->idMap;

    state->field_5C = arg0;
    state->field_5E = 0;
}

/// The 0x0D entry of the room's other script table,
/// `D_dryfield_water_tower_80181DC8` -- the word at 0x80181DF8 is the opcode
/// and 0x80181DFC this function's address. It is the sibling of
/// `func_dryfield_water_tower_8017FA5C` on `D_dryfield_water_tower_80182248`
/// and differs in the view it selects: 7 here against that one's 9.
///
/// It plays event 0x5214000C unless the latch `DryfieldWaterTowerState::field_78`
/// says the view has already been announced, records the view in the saved
/// location byte `D_8007216C`, sends its 0x7D4 (`Room_Util08`) placement
/// `80181A58` to the prop task at `field_44` and restarts that task on state 1,
/// then stops the pad scripts and queues event 0x52140006. The latch is what
/// separates it from that sibling: this one is the re-entry the 0x5214000C
/// announcement is gated on, and `func_dryfield_water_tower_8017EB7C` clears
/// the latch when it re-arms the room.
void func_dryfield_water_tower_8017F908(void)
{
    DryfieldWaterTowerState* state = (DryfieldWaterTowerState*)D_dryfield_water_tower_801876A4->idMap;

    if (state->field_78 == 0) {
        SndEvt_EnqueueType6(0x5214000C, 0, 0);
    }
    D_8007216C             = Gp_FindViewIndex(7);
    Game_Session->field_52 = 1;
    Gp_DispatchMsg(state->field_44, 0x7D4, (s32)&D_dryfield_water_tower_80181A58, 0);
    state->field_44->state = 1;
    Gp_HaltPadScripts();
    SndEvt_EnqueueType7(0x52140006, 0x1E);
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tower/dryfield_water_tower_4", func_dryfield_water_tower_8017F9AC);

/// Script opcode 0x0D of the room's command table `D_dryfield_water_tower_80182248`:
/// it hands the stream to view 9, restarts the prop task at `field_48` on state 1
/// and gives it its 0x7D4 placement, moves the player to `80181AD0`, stops the pad
/// scripts, plays event 0x5214000B and installs three of the room's effect tables.
///
/// `Mc_SaveData.field_4` rather than a bare `extern` for 0x8007216C: the store is
/// a struct member, so the read of `field_48` below still conflicts with it in
/// `true_dependence`. As a scalar global the pair is fixed-address against
/// varying-struct and GCC 2.8.1 drops the dependence, which lets the scheduler
/// sink the store into `Gp_DispatchMsg`'s delay slot. Measured; see
/// `DECOMPILATION_LEARNINGS.md` on struct-typing and aliasing.
void func_dryfield_water_tower_8017FA5C(void)
{
    DryfieldWaterTowerState* state = (DryfieldWaterTowerState*)D_dryfield_water_tower_801876A4->idMap;

    Mc_SaveData.field_4 = Gp_FindViewIndex(9);
    Gp_DispatchMsg(state->field_48, 0x7D4, (s32)&D_dryfield_water_tower_80181AA0, 0);
    state->field_48->state = 1;
    Gp_DispatchMsg(state->field_40, 0x3E9, (s32)&D_dryfield_water_tower_80181AD0, 0);
    Gp_HaltPadScripts();
    SndEvt_EnqueueType7(0x5214000B, 0xA);
    Mem_CopyUnaligned(D_dryfield_water_tower_80181B10, D_dryfield_water_tower_801828DC, 0x10);
    Mem_CopyUnaligned(D_dryfield_water_tower_80181BA0, D_dryfield_water_tower_80182F44, 0x18);
    Mem_CopyUnaligned(D_dryfield_water_tower_80181B20, D_dryfield_water_tower_801829F4, 0x40);
    ((DryfieldWaterTowerState*)state->field_48->idMap)->field_70 = 1;
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tower/dryfield_water_tower_4", func_dryfield_water_tower_8017FB4C);

void func_dryfield_water_tower_8017FBC8(Task* task)
{
    ((DryfieldWaterTowerState*)task->idMap)->field_6C = 1;
}

void func_dryfield_water_tower_8017FBD8(Task* task)
{
    ((DryfieldWaterTowerState*)task->idMap)->field_6E = 1;
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tower/dryfield_water_tower_4", func_dryfield_water_tower_8017FBE8);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tower/dryfield_water_tower_4", func_dryfield_water_tower_8017FD64);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tower/dryfield_water_tower_4", func_dryfield_water_tower_8017FF5C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tower/dryfield_water_tower_4", func_dryfield_water_tower_80180038);
