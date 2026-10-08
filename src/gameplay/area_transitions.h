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

/// Sets the current area's saved map mark without changing its other spawn flags.
///
/// Uses the live session's stage and area, which must index their loaded
/// directories. Missing stage tables and saved state are skipped. Updates the
/// referenced save-bank state directly; the map screen refreshes its cache
/// separately.
void areaSetCurrentMapMark(void);

/// Refreshes the map bits for marked areas with saved-pose restoration disabled.
///
/// Requires an active stage 1..5 and loaded area directories. Refreshes ids
/// `1..Gp_AreaIdCounts[stage-1]` (17, 38, 38, 49, 33), setting a bit only when
/// `AREA_SAVED_MAP_MARK` is set and `AREA_SPAWN_RESTORE_SAVED_POSES` is clear.
/// `Gp_AreaIdBits[0]` covers ids 1..32 and `[1]` covers 33..64. Bits above the
/// stage's count retain their prior values; the cache is separate from visitation.
/// Stages above 5 leave it untouched; stage 0 is not a valid input.
void menuMapRebuildMarkedAreaBits(void);

/// Applies the saved map marks selected by the new-game lists.
///
/// Sets marks through the Acropolis, daytime Dryfield, Mine/Shelter and
/// Shelter/Neo Ark directories, in that order. Day and night Dryfield share
/// saved state. Each list ends with `AREA_MAP_MARK_END`; zero entries preserve
/// existing marks. Missing stage tables and saved state are skipped. Updates
/// the referenced save-bank state directly; it does not clear visitation or
/// rebuild the map screen's cache.
void areaApplyNewGameMapMarks(void);

#endif // GAMEPLAY_PRIVATE_AREA_TRANSITIONS_H
