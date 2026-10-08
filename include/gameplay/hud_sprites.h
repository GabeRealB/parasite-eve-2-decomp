#ifndef GAMEPLAY_HUD_SPRITES_H
#define GAMEPLAY_HUD_SPRITES_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/view.h"

#include "main/session_types.h"
#include "main/task_types.h"

/// Queues battle-escape results for an engaged or resumed battle without a pending reset.
///
/// Clears all player ailments, requests PE cancellation, permits disconnect
/// pausing and queues the escape result with a forced room-resource reload.
/// Idle/finished battles and a nonzero battleResetPending leave state intact.
/// Queue rejection is ignored; the cleanup already performed is retained.
void sceneQueueBattleEscapeResult(void);

/// Expresses a target transform relative to a reference transform.
///
/// `reference` and `target` map into the same containing frame. Writes
/// `out.m = transpose(reference.m) * target.m` and
/// `out.t = transpose(reference.m) * (target.t - reference.t)` using GTE
/// arithmetic. Rotation elements use `ONE` (4096) for 1.0; translations are
/// signed 32-bit game coordinates. The transpose inverts an orthonormal
/// reference rotation; scale and shear are transposed rather than inverted.
/// Refresh coordinate caches before passing their `workm` matrices.
///
/// The matrices must be word-aligned. `out` may equal either complete input
/// matrix; otherwise it must be disjoint from both. Writes the nine rotation
/// elements and three translation words, leaving the alignment bytes untouched.
/// Requires an initialized scratch stack with 48 free bytes disjoint from the
/// matrices. Releases the reservation before returning and retains no pointers.
/// Changes GTE rotation and arithmetic state; does not update coordinate stamps.
void gfxMakeRelativeTransform(const MATRIX* reference, const MATRIX* target, MATRIX* out);

/// Builds a rotation whose local +Z axis faces along a supplied direction.
///
/// Normalizes the nonzero `direction` in its own frame, then writes
/// Ry(yaw) * Rx(-pitch) * Rz(roll) into `out` using GTE arithmetic. Yaw is
/// measured from +Z toward +X and pitch from the XZ plane toward +Y. Angles
/// count 4096 units per turn; roll is truncated to a signed halfword before
/// forming its rotation. Matrix elements use `ONE` (4096) for one unit.
/// Direction is read only; its pad word is ignored. Only the nine rotation
/// elements of `out` are written, leaving translation and alignment bytes
/// untouched. Does not update a containing coordinate's composition stamp.
/// Supply signed-halfword XYZ components whose squared length is in
/// 1..0x7FFFFFFF, as required by the SDK's GTE normalization.
/// Requires an initialized scratch stack with 76 free bytes disjoint from
/// both arguments, released before return. Changes GTE rotation, IR/MAC and
/// leading-sign-bit-count state; retains no pointers.
void gfxBuildDirectionRotation(const VECTOR* direction, MATRIX* out, s32 roll);

/// Queues a one-shot camera application on the selected task list; returns 1 on success.
///
/// Borrows the readable camera until the bank-0 camera task applies it and
/// exits. Keep the record and its owning overlay loaded until dispatch.
/// Returns 0 on task allocation failure and does not apply the camera directly.
s32 viewQueueCamera(const ViewCamera* camera);

/// Queues the current mapped camera and cached sprite-packet allocation on the selected list.
///
/// Requires valid populated 1-based session stage/area/room/view indices and
/// a nonzero mapped index within the loaded camera and sprite resources.
/// Keep those resources loaded until both one-shot tasks run. Camera application
/// precedes packet allocation at their equal priority. Spawn failures are ignored;
/// this does not queue image loading or change the selected list.
void viewQueueCurrentCameraAndPackets(void);

/// Borrows an area's camera selected by the live session's mapped view index.
///
/// `location` supplies only stage and area, both valid populated 1-based
/// directory indices. Room/view mapping always comes from the live session;
/// its nonzero result must fit the chosen area's camera array. Returns mutable
/// room-owned storage without copying it; keep the owning overlay loaded.
ViewCamera* viewGetMappedCamera(const GameLocationKey* location);

/// Destination for the optional cached sprite-packet task in `viewQueueCurrentCamera`.
enum {
    VIEW_PACKET_LIST_SELECTED = 0,
    VIEW_PACKET_LIST_DEFAULT  = 1,
    VIEW_PACKET_LIST_NONE     = 2
};

/// Queues the current mapped camera, optionally allocating cached sprite packets.
///
/// The camera always uses the selected execution list. packetListMode selects
/// the packet task's selected list (0), default list (1), or omission (2 and
/// every other value). The default-list option restores the previous selection.
/// Uses the resource bounds and borrowed lifetimes of `viewQueueCurrentCameraAndPackets`;
/// both spawn results are ignored. Does not queue image loading.
void viewQueueCurrentCamera(s32 packetListMode);

/// Monitors the saved logical view and gates readiness while a view transition starts.
///
/// Bank-0 slot 0x16 is persistent: spawnArg1.value remembers the last admitted
/// view. A changed view or viewDirty retries camera/packet setup and display-owned
/// loading unless an available scene payload is still loading. Successful loader
/// admission starts or refreshes a two-update counter; a single menu hold spans it.
/// The update admitting the loader also decrements the counter. Only its final
/// update publishes viewReady = 1; all other updates clear it, including idle ones.
/// The loader owns input unblock, viewDirty clearing and presentation handoff.
void viewTransitionGateTask(Task* task);

/// Task-bank selectors for play-time/HUD monitoring and the death sequence.
enum {
    PLAY_CLOCK_TASK_BANK = 0,
    PLAY_CLOCK_TASK_TYPE = 0x1D
};

/// Dispatches play-time/HUD updates and the death-to-session-restart sequence.
///
/// Bank 0 type 0x1D requires `state` 0..5; no bounds check is made. States
/// initialize task-owned clock/HUD work, update play, start and wait for death
/// presentation, restart the session, then kill the task. Gameplay and the live
/// session/player must remain available through the corresponding states.
/// A handler can release the task, which is not accessed after dispatch.
void playClockTask(Task* task);

#endif // GAMEPLAY_HUD_SPRITES_H
