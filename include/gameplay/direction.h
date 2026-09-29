#ifndef GAMEPLAY_DIRECTION_H
#define GAMEPLAY_DIRECTION_H

#include "common.h"

#include "gameplay/view.h"
#include "gameplay/message.h"

/// 8-byte dest-location payload at `Gp_WarpLoc`. `Gp_CommitDirWarp` fills it
/// (halfword `field_0`/`field_1` from `Gp_DirAlt`, `field_2` from
/// `Gp_DirAltNibble & 0xF`, `field_3`/`field_4` = 1, `field_5` = 0), posts slot-7
/// msg `0x13EE`, then copies `field_0` / `field_2` / `field_3` into
/// `Mc_SaveData[0].state.at4.loc.area` / `field_8` / `field_5` before `Task_Spawn(0, 0x11,
/// ...)`. `Gp_CommitWarp` fills the same payload from `Gp_DirByte` /
/// `Gp_DirNibble & 0xF` and `GpWarpRec.field_36`. `Gp_CommitSaveLoc` does the
/// same copy + spawn.
typedef RoomEventMsg GpSaveLoc;
STATIC_ASSERT_SIZEOF(GpSaveLoc, 8);

/// 4-byte stack payload for slot-7 msg `0x13EF`. `Gp_PostMsg13EF` copies
/// `Gp_DirFlags` / `Gp_DirByte` / `Gp_DirNibble` into the three fields.
typedef struct _GpMsg13EF {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u8  field_2;
    /* 0x3 */ u8  field_3;
} GpMsg13EF;
STATIC_ASSERT_SIZEOF(GpMsg13EF, 4);

/// 0x38-byte record in tables pointed to by `Gp_WarpTables`. Indexed
/// 1-based by `GpAreaKey.stage` / `area`, then
/// `(Gp_DirNibble >> 4)`. `Gp_CommitWarp` copies one record onto the
/// stack and writes `field_36` into `GpSaveLoc.field_6`. The transform words
/// keep the record 4-aligned for its 56-byte assignment.
/// func_800AA548 uses the transforms at 0x00 / 0x14 to spawn the player /
/// companion, field_28 as a sound event, and field_34 as the initial view.
typedef struct _GpWarpRec {
    /* 0x00 */ GpSpawnTransform player;
    /* 0x10 */ byte             pad_10[4];
    /* 0x14 */ GpSpawnTransform companion;
    /* 0x24 */ byte             pad_24[4];
    /* 0x28 */ s32              field_28;
    /* 0x2C */ s32              field_2C;
    /* 0x30 */ s32              field_30;
    /* 0x34 */ u8               field_34;
    /* 0x35 */ u8               field_35;
    /* 0x36 */ u16              field_36;
} GpWarpRec;
STATIC_ASSERT_SIZEOF(GpWarpRec, 0x38);

#endif // GAMEPLAY_DIRECTION_H
