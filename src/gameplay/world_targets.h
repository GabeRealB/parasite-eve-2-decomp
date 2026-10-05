#ifndef GAMEPLAY_PRIVATE_WORLD_TARGETS_H
#define GAMEPLAY_PRIVATE_WORLD_TARGETS_H

#include "types.h"

#include "gameplay/world_targets_types.h"

#include "main/mc_types.h"

/// First node on the list of enemies the targeting passes track.
///
/// NULL when the list is empty. Lock-on, the reticle, the radar and the area
/// scans walk from here through `next`.
extern WorldTargetNode* gWorldTargetListHead;

void Gp_DrawTargetCursor(void);

/// Starts empty area target tracking, clears readouts and resets cursor coordinates.
///
/// Call before linking the new area's enemies. Previous tracked entries must
/// have been unlinked, destroyed or abandoned with the old area; clearing the
/// head does not change their links, membership bytes or actor locks. Any
/// surviving entry must be made off-list before it can be linked again.
/// Readout positions are retained, and both cursor accumulators become -4096
/// screen pixels with eight fractional bits. Cursor target/easing state is
/// retained; no node storage is freed.
void worldTargetResetAreaTracking(void);

/// Clears the target marks on the player and companion's current nodes.
///
/// Retains the actors' borrowed target pointers and each node's flags and
/// membership. Occupied tasks require live `GameActor` work blocks, and any
/// referenced target must remain writable and live.
void worldTargetClearActorTargetMarks(void);

s32 Gp_GrantLocationItems(InventoryItemRange* arg0);

void Gp_InitStateF0(void);

#endif // GAMEPLAY_PRIVATE_WORLD_TARGETS_H
