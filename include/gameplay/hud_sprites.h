#ifndef GAMEPLAY_HUD_SPRITES_H
#define GAMEPLAY_HUD_SPRITES_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/view.h"

#include "main/session_types.h"
#include "main/task_types.h"

s32 Gp_IsDebugAttachRoom(void);

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

s32 Gp_TrySpawnViewTask(GpViewRec* arg0);

void Gp_ApplyView(GpViewRec* arg0);

void Gp_SpawnViewTasks(void);

GpViewRec* Gp_GetStageView(GameLocationKey* arg0);

void Gp_SpawnCurView(s32 arg0);

s32 Gp_SpendMp(s32 arg0);

void Gp_ApplyViewTask(Task* task);

void Gp_ViewGateTask(Task* task);

void func_800A77B4(Task* arg0);

void func_800A8654(Task* task);

#endif // GAMEPLAY_HUD_SPRITES_H
