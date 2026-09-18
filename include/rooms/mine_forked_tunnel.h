#ifndef ROOMS_MINE_FORKED_TUNNEL_H
#define ROOMS_MINE_FORKED_TUNNEL_H

#include "common.h"

#include "main/tmd.h"
#include "rooms/room_common.h"

/// The enemy's position / rotation path, one `SVECTOR` per step: `pos` and
/// `rot` are the halves `func_mine_forked_tunnel_8017D5E8` and
/// `func_mine_forked_tunnel_8017D8EC` compose into the `RoomPlacement` they
/// hand `Room_Util18` (entry 0 of each) and that
/// `func_mine_forked_tunnel_8017D724` walks one entry per step of
/// `Task::killCountdown`, which it clamps at 0x6E. Both are 240 entries - the
/// position table starts where the rotation table ends, and the pitch table
/// below starts where the position table ends.
extern SVECTOR D_mine_forked_tunnel_80181244[240];
extern SVECTOR D_mine_forked_tunnel_80180AC4[240];

/// The placement `func_mine_forked_tunnel_8017D5E8` uses instead when the
/// `0x75` game flag is set: a complete `RoomPlacement` sitting in the room's
/// `.data`, offset (0x8CD, 0x3C4, 0x46B) with a half-turn about Y.
extern RoomPlacement D_mine_forked_tunnel_80181BBC;

/// The `RoomPlacement` the tunnel's pitch-animated object adopts: state 0
/// (`func_mine_forked_tunnel_8017DE54`) copies it onto the task's coordinate
/// whole, and state 1 (`func_mine_forked_tunnel_8017DAB8`) then keeps its `pos`
/// while taking the `rot` from the pitch table below. Position
/// (0xB4, -0xEB, -0x30C), rotation zero.
extern RoomPlacement D_mine_forked_tunnel_80181BA4;

/// The pitch curve `func_mine_forked_tunnel_8017DAB8` walks that object
/// through, one `SVECTOR` per step of the counter it runs while
/// `Task::spawnArg1` is 1: entries 0-15 are zero, then `vx` falls to -8 and
/// climbs to 175 before settling at 173 (4096 is a full turn), so the object
/// rises over the sequence and holds. The table is 54 entries, the counter's
/// limit, so the last step lands on the settling value.
extern SVECTOR D_mine_forked_tunnel_801819C4[54];

/// Two-entry `TaskDesc` table `func_mine_forked_tunnel_8017D5E8` spawns the
/// child enemy from; `Task_SpawnFromTable` picks entry 1.
extern TaskDesc D_mine_forked_tunnel_80181B74;

/// Payload message 0x7DB carries into this room's enemy task, handed to the
/// handler as `Gp_DispatchMsg`'s `arg2`: the four bytes `3CD8.c` fills from
/// `GpCapSpawnArg` (`field_2` / `field_3` there), read here as the one
/// halfword at 0x2 that selects the command. Same record as
/// `Actor205200Msg7DB` and `NeoArkWoodlandPathMsg7DB`; the senders are the
/// game-script gates, not this overlay.
typedef struct MineForkedTunnelMsg7DB {
    /* 0x0 */ u8  field_0;
    /* 0x1 */ u8  field_1;
    /* 0x2 */ u16 field_2;
} MineForkedTunnelMsg7DB;
STATIC_ASSERT_SIZEOF(MineForkedTunnelMsg7DB, 0x4);

/// Message 0x7DB handler for the tunnel's enemy (`D_mine_forked_tunnel_80181B8C`
/// routes the id here). Command 0 rewinds the enemy and its spawned child
/// (`spawnArg1` and `killCountdown` cleared on both) and drops it back on the
/// placement `func_mine_forked_tunnel_8017D5E8` uses while flag 0x75 is clear;
/// 1 starts only the child's pitch walk, 2 starts the enemy's own, and 3 puts
/// the enemy on `D_mine_forked_tunnel_80181BBC` - the `Room_Util18` elsewhere
/// room's flag-set variant - then refreshes the flag-dependent state through
/// `func_mine_forked_tunnel_8017DF34` and rewinds the enemy again. `arg1` is
/// the message id, which nothing here reads.
///
/// The `do { } while (0)` around the last command is an allocator lever, not
/// logic (the `break` leaves it for the switch's own tail, so the two are
/// equivalent): `flow` weights each reference by the loop depth, and
/// local-alloc's quantity rank is built from those counts, so the wrapper -
/// and only the wrapper - lifts the six placement reads above the placement
/// pointer and gives `$v0` to the values instead of the address.
s32 func_mine_forked_tunnel_8017D8EC(Task* task, s32 arg1, MineForkedTunnelMsg7DB* msg);

/// The `{id, TaskFunc}` pairs the tunnel's enemy restores through
/// `Task::msgTable` - `0x7D5` maps to `func_mine_forked_tunnel_8017DD08` and
/// `0x7DB` to `func_mine_forked_tunnel_8017D8EC`, the two ids
/// `func_mine_forked_tunnel_8017D5E8` registers.
extern s32 D_mine_forked_tunnel_80181B8C;

/// Per-task work block for the tunnel's enemy: a 0x48-byte `Mem_Calloc`
/// allocation `func_mine_forked_tunnel_8017D5E8` stores at `Task::work`,
/// seeding `field_44` to -1. The two leading matrices are the light and colour
/// matrices `func_mine_forked_tunnel_8017DC70` republishes onto the task's
/// `TmdObject` (`field_1C` / `field_20`) so `Tmd_SetupDraw` picks them up.
/// Same layout as `Actor503500ColorMtx`, whose overlay carries a byte-identical
/// copy of that function.
///
/// `field_40` is the `Task_SpawnFromTable` child the room's `func_mine_forked_tunnel_8017D724`
/// frees; `field_44` is the signed lifetime counter it decrements.
typedef struct MineForkedTunnelWork {
    /* 0x00 */ MATRIX light;
    /* 0x20 */ MATRIX color;
    /* 0x40 */ void*  field_40;
    /* 0x44 */ s32    field_44;
} MineForkedTunnelWork;
STATIC_ASSERT_SIZEOF(MineForkedTunnelWork, 0x48);

/// One of the per-view objects the room's sprite-table record points at, and
/// whose `field_1C` this room's `func_mine_forked_tunnel_8017E48C` flips. Same
/// shape as the `field_C` byte the other rooms carrying this sprite table
/// write (the dryfield water tower's `DwtwSprtViewState`): non-zero leaves the
/// view's sprites out of the ordering table, zero draws them.
typedef struct MineForkedTunnelViewA {
    /* 0x00 */ byte pad_0[0x1C];
    /* 0x1C */ u8   field_1C;
} MineForkedTunnelViewA;
STATIC_ASSERT_SIZEOF(MineForkedTunnelViewA, 0x1D);

/// The second per-view object, written together with and to the same value as
/// `MineForkedTunnelViewA`'s byte.
typedef struct MineForkedTunnelViewB {
    /* 0x00 */ byte pad_0[0x2C];
    /* 0x2C */ u8   field_2C;
} MineForkedTunnelViewB;
STATIC_ASSERT_SIZEOF(MineForkedTunnelViewB, 0x2D);

/// The record `Gp_SprtTables[stage - 1]->field_0[room - 1]` really points at: a
/// room-sized block, far larger than the 0xC-byte `GpSprtRec` the table's
/// element type declares, so the room reaches its tail through a cast (as the
/// water tower's `DwtwSprtRec` and the dumping hole's `SprtBigRec` do). The
/// tail is a run of per-view pointers; this room's pair sits at 0x28 / 0x34.
typedef struct MineForkedTunnelSprtRec {
    /* 0x00 */ byte                   pad_0[0x28];
    /* 0x28 */ MineForkedTunnelViewB* field_28;
    /* 0x2C */ byte                   pad_2C[0x8];
    /* 0x34 */ MineForkedTunnelViewA* field_34;
} MineForkedTunnelSprtRec;
STATIC_ASSERT_SIZEOF(MineForkedTunnelSprtRec, 0x38);

#endif // ROOMS_MINE_FORKED_TUNNEL_H
