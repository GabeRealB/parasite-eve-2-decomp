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

#endif // ROOMS_MINE_FORKED_TUNNEL_H
