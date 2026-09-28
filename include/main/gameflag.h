#ifndef GAMEFLAG_H
#define GAMEFLAG_H

#include "common.h"

// Packed 4-bit game flags (src/main/gameflag.c)

void GameFlag_SetNibble(s32 index, s32 value);
s32  GameFlag_GetNibble(s32 index);

/// Per-index flag object pointed to by `Gp_FlagBanks`. `field_4[0]` / `[1]` are
/// bitmasks (ids 1–32 and 33–64) cleared by `Gp_ClearFlagBank` and set by
/// `Gp_MarkAreaVisited`.
typedef struct _GpFlagBank {
    /* 0x00 */ byte pad_0[4];
    /* 0x04 */ s32  field_4[2];
} GpFlagBank;
STATIC_ASSERT_SIZEOF(GpFlagBank, 0xC);

/// Main-executable table of `GpFlagBank*`, indexed by slot / session field_7.
extern GpFlagBank* Gp_FlagBanks[];

#endif // GAMEFLAG_H
