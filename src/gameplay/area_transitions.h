#ifndef GAMEPLAY_PRIVATE_AREA_TRANSITIONS_H
#define GAMEPLAY_PRIVATE_AREA_TRANSITIONS_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "direction.h"

#include "main/task_types.h"

extern const DirectionActionTable Gp_DirActionFns;

/// Per-stage flag-nibble lookup. `idx` indexes a u16 table selected by
/// `gGameSession->location.loc.stage` (1..5). Low 11 bits are the `gameFlagGetNibble`
/// index; bit `0x800` is added onto the result. Unknown stage or out-of-range
/// index returns -1.
s16 Gp_LookupStageFlag(s16 idx);

s32 Gp_YawToPosXZ(Task* arg0, SVECTOR* arg1);

void Gp_SetCurAreaFlag4(void);

/// For each area id `1..Gp_AreaIdCounts[stage-1]`, set or clear the matching
/// bit in `Gp_AreaIdBits`. The bit is set only when that area's
/// `AreaSavedState.spawnFlags` has `AREA_SAVED_MAP_MARK` set and
/// `areaIsSavedPoseRestoreEnabled` returns 0.
void Gp_RebuildAreaIdBits(void);

void Gp_ApplyNewGameAreaFlags(void);

#endif // GAMEPLAY_PRIVATE_AREA_TRANSITIONS_H
