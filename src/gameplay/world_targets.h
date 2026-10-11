#ifndef GAMEPLAY_PRIVATE_WORLD_TARGETS_H
#define GAMEPLAY_PRIVATE_WORLD_TARGETS_H

#include "types.h"

#include "gameplay/world_targets_types.h"

/// Diagnostic printed when a body-position query receives a NULL target.
///
/// The NUL-terminated message occupies 34 bytes. Its two trailing stored bytes
/// preserve the read-only object and are not part of the printed text.
extern const char gWorldTargetNullBodyPositionDiagnostic[36];

/// First node on the list of enemies the targeting passes track.
///
/// NULL when the list is empty. Lock-on, the reticle, the radar and the area
/// scans walk from here through `next`.
extern WorldTargetNode* gWorldTargetListHead;

/// Lock-on reticle screen X before shake compensation, with eight fractional bits.
///
/// Signed s32 screen pixels relative to the centre, scaled by 256. Target
/// changes halve the displacement for at most five draws; acquisition snaps.
/// Initialized to -256 pixels and reset to -4096 pixels at area entry.
/// Gameplay owns this value; target/easing state has a separate lifetime.
extern s32 gWorldTargetCursorXFixed8;

/// Lock-on reticle screen Y before vertical shake compensation, with eight fractional bits.
///
/// Signed screen pixels relative to the centre, scaled by 256. Target changes
/// halve displacement for at most five draws; acquisition snaps. Initialized
/// to -256 pixels and reset to -4096 at area entry. Gameplay owns the value;
/// cursor target/easing state has a separate lifetime.
extern s32 gWorldTargetCursorYFixed8;

/// Previously drawn reticle target, borrowed from the live tracked enemy list.
///
/// Identity changes restart easing; a NULL previous target makes acquisition
/// snap. An ungated scan without an eligible target clears it. Suppression
/// gates and area-tracking reset retain the pointer, without owning the node.
extern WorldTargetNode* gWorldTargetCursorNode;

/// Signed reticle easing-pass state: 0..4 ease, 5 or 255 snap.
///
/// Changed targets start at zero. Initial acquisition and convergence store
/// 255; convergence still draws small for that pass. An ungated scan without
/// a target clears it. Suppression gates and area-tracking reset retain it.
extern s32 gWorldTargetCursorEasingPass;

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

#endif // GAMEPLAY_PRIVATE_WORLD_TARGETS_H
