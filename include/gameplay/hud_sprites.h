#ifndef GAMEPLAY_HUD_SPRITES_H
#define GAMEPLAY_HUD_SPRITES_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/view.h"

#include "main/session_types.h"
#include "main/task_types.h"

void Gp_TriggerPeIfArmed(void);

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

void Gp_SpawnViewTasks(void);

/// Borrows an area's camera selected by the live session's mapped view index.
///
/// `location` supplies only stage and area, both valid populated 1-based
/// directory indices. Room/view mapping always comes from the live session;
/// its nonzero result must fit the chosen area's camera array. Returns mutable
/// room-owned storage without copying it; keep the owning overlay loaded.
ViewCamera* viewGetMappedCamera(const GameLocationKey* location);

void Gp_SpawnCurView(s32 arg0);

void Gp_ViewGateTask(Task* task);

void func_800A77B4(Task* arg0);

#endif // GAMEPLAY_HUD_SPRITES_H
