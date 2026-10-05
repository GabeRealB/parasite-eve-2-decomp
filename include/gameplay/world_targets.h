#ifndef GAMEPLAY_WORLD_TARGETS_H
#define GAMEPLAY_WORLD_TARGETS_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "types.h"

#include "gameplay/world_targets_types.h"

#include "main/session_types.h"
#include "main/task_types.h"

void func_800DA6E8(void* arg0, s32 arg1, s32 arg2);

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

/// Locks actor slot 0 onto `node`, releasing whichever node held it, and marks
/// `node` lockable.
void Gp_AssignNodeSlot0(WorldTargetNode* node);

/// Releases actor locks on `node` and disables further lock-on to it.
///
/// `node` must be live and non-NULL. Clears its target mark and sets
/// `WORLD_TARGET_NOT_LOCKABLE`, preserving other flags and list membership.
/// Occupied player/companion tasks must have live `GameActor` work blocks.
/// The caller retains ownership; this does not unlink or free the entry.
void worldTargetDisableNodeLockOn(WorldTargetNode* node);

void* Gp_FindLockNode(Task* arg0);

void* Gp_FindLockNodePad(Task* arg0);

void Gp_GetLockPos(WorldTargetNode* arg0, VECTOR3* out);

void Gp_ArmStateF0(s32 arg0);

void Gp_ReleaseStateF0Add(Task* arg0, s32 arg1);

/// Clear accumulated rewards on the last release. Both caller-supplied
/// arguments are unused.
void Gp_ReleaseStateF0Clear(Task* unusedTask, s32 unusedArg);

void Gp_ReleaseStateF0(Task* arg0, s32 arg1);

#endif // GAMEPLAY_WORLD_TARGETS_H
