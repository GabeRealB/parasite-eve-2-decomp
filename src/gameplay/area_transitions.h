#ifndef GAMEPLAY_PRIVATE_AREA_TRANSITIONS_H
#define GAMEPLAY_PRIVATE_AREA_TRANSITIONS_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "direction.h"

#include "main/task_types.h"

extern const DirectionActionTable Gp_DirActionFns;

/// Returns the yaw from a model task's root translation toward a point in its parent frame.
///
/// Requires a live TMD body and root coordinate. Borrows both inputs without
/// composing transforms; `targetPoint` must use the root's parent coordinate
/// frame and whole game-coordinate units. Ignores Y, narrows the X/Z differences
/// modulo 65536 into signed halfwords, then normalizes them before measuring.
/// Returns 0..4095, with +Z at zero and +X at a quarter turn.
s32 actorAngleTaskYawTowardPoint(const Task* modelTask, const SVECTOR* targetPoint);

void Gp_SetCurAreaFlag4(void);

/// For each area id `1..Gp_AreaIdCounts[stage-1]`, set or clear the matching
/// bit in `Gp_AreaIdBits`. The bit is set only when that area's
/// `AreaSavedState.spawnFlags` has `AREA_SAVED_MAP_MARK` set and
/// `areaIsSavedPoseRestoreEnabled` returns 0.
void Gp_RebuildAreaIdBits(void);

void Gp_ApplyNewGameAreaFlags(void);

#endif // GAMEPLAY_PRIVATE_AREA_TRANSITIONS_H
