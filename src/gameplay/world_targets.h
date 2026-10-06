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

/// Draws and ages floating damage/heal readouts, then draws the lock-on reticle.
///
/// The input HUD-hide flag suppresses the entire pass. Readouts still age when
/// the reticle is hidden by pause state 1, an attachment wheel/armed/cast mode,
/// an event or the session HUD-hide flag; those gates retain cursor tracking.
/// The reticle uses the first marked, lockable node on the tracked list. Initial
/// acquisition snaps; target changes ease for up to five calls using eight
/// fractional bits and a 16-pixel reticle, then draw at 32 pixels. An ungated
/// scan without a target clears cursor tracking. Animation uses the display
/// frame counter, and the reticle compensates for vertical screen shake.
///
/// Requires an acyclic list of live enemy target entries with composed working
/// matrices for readouts, initialized scratch stack, cursor/font/UI textures,
/// writable ordering-table entries 0 and -10, and sufficient primitive-arena
/// space. Body coordinates narrow to signed 16 bits; projection errors and
/// off-screen reticle positions are not filtered. Scratch blocks are released
/// before return; queued packets remain borrowed until GPU drawing completes.
void worldTargetDrawOverlay(void);

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

#endif // GAMEPLAY_PRIVATE_WORLD_TARGETS_H
