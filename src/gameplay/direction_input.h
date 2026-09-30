#ifndef GAMEPLAY_PRIVATE_DIRECTION_INPUT_H
#define GAMEPLAY_PRIVATE_DIRECTION_INPUT_H

#include "types.h"

#include "gameplay/direction.h"

// Direction input state, action dispatch and warp handling.

/// Direction and warp state shared within gameplay.
extern s16 D_80114CD0;

extern u16 Gp_DirFlags;

extern u16 D_80114CD4;

extern u16 Gp_DirPhase;

extern u8 Gp_DirByte;

extern u8 Gp_DirNibble;

extern u8 Gp_DirAlt;

extern u8 Gp_DirAltNibble;

extern u8 D_80114CDC;

extern u8 D_80114CDD;

extern u8 D_80114CDE;

extern s16 D_80114CE0;

extern RoomEventMsg Gp_WarpLoc;

extern u16 Gp_DirFadeLevel;

/// Dual area-id bitmask (1–32 / 33–64), rebuilt from the area bit-2 flags.
extern s32 Gp_AreaIdBits[2];

/// Per-stage signed counts, indexed by `GameSession.at4.loc.stage - 1`.
/// `Gp_RebuildAreaIdBits` loops area ids `1..count` when the stage is 1–5.
extern s8 Gp_AreaIdCounts[];

void Gp_SetupDirWarp(void);

void Gp_FadeDirWaitMsg(void);

void Gp_CommitWarp(void);

void Gp_WarpPhase4(void);

void func_800AD6BC(void);

void Gp_PostDirIfCapIdle(void);

void Gp_RunDirAction(void);

void Gp_MsgPlayerDirFacing(void);

void Gp_CommitDirWarp(void);

#endif // GAMEPLAY_PRIVATE_DIRECTION_INPUT_H
