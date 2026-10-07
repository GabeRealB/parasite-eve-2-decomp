#ifndef GAMEPLAY_WORLD_TARGETS_H
#define GAMEPLAY_WORLD_TARGETS_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "types.h"

#include "gameplay/world_targets_types.h"

#include "main/session_types.h"
#include "main/task_types.h"

/// Adds a signed amount to a target's floating damage or healing readout.
///
/// Non-negative amounts (including zero) share a non-negative total; negative
/// amounts share a negative total. A target can therefore occupy two of the
/// 32 slots. A missing total takes the first empty slot; a full table drops
/// the addition. The sum narrows to signed 16 bits without clamping, and each
/// addition restarts the readout's 20-draw-pass lifetime. This does not change HP.
///
/// Pass a non-NULL enemy target entry. The borrowed pointer is only compared
/// and retained here; tracking refreshes its screen position, and unlinking
/// leaves the last position for the rest of the readout's lifetime.
/// `unusedArg` is ignored.
void worldTargetAddReadoutAmount(WorldTargetNode* node, s32 amount, s32 unusedArg);

/// Releases actor locks on `node` and removes it from target tracking.
///
/// `node` must point to a live enemy's target entry; ownership stays with the
/// caller. An on-list value of 1 enables removal and clears both `onList` and
/// `targeted`, even if the entry is absent from the list. Other membership
/// values only release the player and companion references. The successor and
/// flags are retained, and no storage is freed.
void worldTargetUnlinkNode(WorldTargetNode* node);

/// Enables lock-on for `node`, appending it to target tracking if off-list.
///
/// `node` must be live and non-NULL, and the tracked list must be acyclic.
/// An `onList` byte of zero appends at the tail, clears `targeted` and sets
/// membership to one. A nonzero byte preserves the links and target mark.
/// Both paths clear only `WORLD_TARGET_NOT_LOCKABLE`. The caller owns the
/// entry and must unlink it before its storage expires.
void worldTargetLinkNode(WorldTargetNode* node);

/// Returns the player/companion slots whose current target equals `node`.
///
/// Bit zero is the player and bit one the companion; absent tasks add no bit.
/// This compares borrowed pointers without dereferencing `node`. Passing NULL
/// selects occupied actor slots with no current target. Occupied tasks must
/// have live `GameActor` work blocks.
s32 worldTargetGetActorLockMask(const WorldTargetNode* node);

/// Sets the player's lock to `node` and enables lock-on to that target.
///
/// `node` must be a live, non-NULL enemy target entry. If the player task exists,
/// clears its previous target's mark and replaces its borrowed target pointer.
/// Always marks `node` targeted and clears only `WORLD_TARGET_NOT_LOCKABLE`,
/// even without a player task. Retains list membership and the companion's
/// target pointer. The previous mark is cleared even if the companion holds
/// that node. No storage is freed or transferred.
void worldTargetSetPlayerLock(WorldTargetNode* node);

/// Releases actor locks on `node` and disables further lock-on to it.
///
/// `node` must be live and non-NULL. Clears its target mark and sets
/// `WORLD_TARGET_NOT_LOCKABLE`, preserving other flags and list membership.
/// Occupied player/companion tasks must have live `GameActor` work blocks.
/// The caller retains ownership; this does not unlink or free the entry.
void worldTargetDisableNodeLockOn(WorldTargetNode* node);

/// Finds a visible lock-on target, preferring forward bearing and nearby bodies.
///
/// Returns a borrowed live enemy target entry or NULL; does not set an actor
/// lock or change list membership. The current target is penalized, so an
/// alternative visible target wins when available. Bearings/distances use the
/// per-frame player-relative cache, including for a companion's query.
/// `aimingTask` must have live `GameActor` work and a model root whose translation
/// is in world game units. The tracked list must be acyclic with live embedded
/// enemy nodes, current player-relative offsets and valid coordinate chains.
/// Requires current view/occluder state and an initialized scratch stack with
/// 200 bytes available for nested queries. Releases scratch before return;
/// updates coordinate caches and GTE state, with signed-halfword narrowing and
/// saturation in the visibility transforms. No allocation or ownership transfer.
WorldTargetNode* worldTargetFindLockNode(Task* aimingTask);

/// Finds a lock-on target using held left/right input from controller port zero.
///
/// Left takes precedence when both buttons are held. Either direction cycles
/// around the current target's player-relative bearing; no direction uses the
/// automatic ranking of `worldTargetFindLockNode`. A missing current target
/// uses forward bearing as the cycle origin. The current target remains eligible
/// with a score penalty. Uses the same actor/list/scratch and borrowed-pointer
/// contract as `worldTargetFindLockNode`, and does not change the actor's lock.
WorldTargetNode* worldTargetFindLockNodeFromPad(Task* aimingTask);

/// Writes the target's body point in world-space game coordinates.
///
/// `node` is an enemy's embedded target entry; list membership is not required.
/// NULL prints a diagnostic and writes zero. `outPosition` must provide three
/// writable, word-aligned signed 32-bit components (12 bytes); no pad word is
/// accessed and no pointer is retained. Keep the output disjoint from the
/// enemy's body point and coordinate storage.
///
/// A body already in the world frame is copied directly. Otherwise refreshes
/// the enemy's coordinate cache, removes the current view transform and applies
/// the resulting matrix to its body point. The GTE path takes signed 16-bit
/// input components and saturates each result to -32768..32767 before storing
/// it as a 32-bit word; the direct copy retains all 32 bits. GTE error flags
/// are not checked. Requires a live, acyclic coordinate
/// chain beneath the view coordinate, current view state, and an initialized
/// scratch stack with 40 bytes plus the called transform's 48-byte reservation.
/// Releases scratch before return and changes GTE transform/arithmetic state.
void worldTargetGetBodyPosition(const WorldTargetNode* node, VECTOR3* outPosition);

#endif // GAMEPLAY_WORLD_TARGETS_H
