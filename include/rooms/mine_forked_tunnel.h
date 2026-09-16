#ifndef ROOMS_MINE_FORKED_TUNNEL_H
#define ROOMS_MINE_FORKED_TUNNEL_H

#include "common.h"

#include "main/tmd.h"

/// Per-task work block for the tunnel's enemy: a 0x48-byte `Mem_Calloc`
/// allocation `func_mine_forked_tunnel_8017D5E8` stores at `Task::idMap`,
/// seeding `field_44` to -1. The two leading matrices are the light and colour
/// matrices `func_mine_forked_tunnel_8017DC70` republishes onto the task's
/// `TmdObject` (`field_1C` / `field_20`) so `Tmd_SetupDraw` picks them up.
/// Same layout as `Actor503500ColorMtx`, whose overlay carries a byte-identical
/// copy of that function.
typedef struct MineForkedTunnelWork {
    /* 0x00 */ MATRIX light;
    /* 0x20 */ MATRIX color;
    /* 0x40 */ byte   pad_40[0x4];
    /* 0x44 */ s8     field_44;
    /* 0x45 */ byte   pad_45[0x3];
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
