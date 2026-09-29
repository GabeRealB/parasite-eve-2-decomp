#ifndef GAMEPLAY_SCENE_RUNTIME_H
#define GAMEPLAY_SCENE_RUNTIME_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/enemy.h"

#include "main/coord.h"
#include "main/session_types.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

s32 func_800AF590(s32 unused0, s32 unused1);

s16 Gp_FindStreamSlot(u16 arg0, u16 arg1, u16 arg2, u16 arg3);

void Gp_StepCdAudioCmd(void);

void Gp_ApplySndBankMasks(u16 arg0);

void Gp_RestoreStreamRng(void);

s32 func_800B0118(s32 arg0, s32 arg1);

GpEnemy* Gp_SpawnEnemyFromTable(TaskDesc* table, s32 idx, s32 arg2, GpEnemy* parent);

void Gp_DestroyEnemy(GpEnemy* enemy, Task* task);

void Gp_EnemyTaskExit(Task* task);

/// Copies `arg1`'s matrix onto the coordinate at `Task::extra->field_8`,
/// adding `arg2` in that space. If `arg1->parent` is world (`gGfxViewCoord`),
/// copies `coord` and transforms in place; otherwise computes `workm`
/// via `Gp_UpdateCoord`, transforms there, and converts to local with
/// `Gp_WorldToLocal`. Always parents the dest to world and clears `composeStamp`.
/// Returns `arg0` (or NULL).
Task* Gp_CopyCoordOffset(Task* arg0, GfxCoord* arg1, SVECTOR* arg2);

void Gp_MtxToEuler(MATRIX* arg0, SVECTOR* arg1);

/// Extracts ZYX Euler angles from `arg1`'s rotation into `arg0`. Tries `vx`
/// and `vx ± 0x800` (the other Euler solution) and keeps the candidate with
/// the smaller sum of absolute angles. Returns `arg0`.
SVECTOR* Gp_ExtractEuler(SVECTOR* arg0, MATRIX* arg1);

/// Lerps the 3x3 rotation of `arg0` toward `arg1` by `arg3 / ONE`, then
/// orthonormalizes into `arg2`. Outer products of each interpolated row
/// pair pick the two most independent axes; `MatrixNormal_0` / `_1` / `_2`
/// reconstructs the missing row.
void Gp_LerpOrthonormal(MATRIX* arg0, MATRIX* arg1, MATRIX* arg2, s32 arg3);

/// Walks `arg0->parent` up to world (`gGfxViewCoord`), composing each node's
/// `coord` rotation into `arg1` and accumulating the rotated translation
/// into `arg2`. The world parent initializes `arg1` to identity and
/// `arg2` to zero.
void Gp_ComposeParentWorld(GfxCoord* arg0, MATRIX* arg1, SVECTOR* arg2);

void Gp_BlendRgb555Clut(u16* arg0, u16* arg1, s32 arg2, u16* arg3);

void func_800B3AA4(GpAnimCtx* arg0, GpAnimSlot* arg1, s32 arg2, s32 arg3, s32 arg4, s32 arg5);

/// Binds an animation context to a model allocation's coordinate tail and caller-owned playback data.
///
/// `model` must be the object returned by `Tmd_Create`; the context borrows its
/// `partCount` coordinates and the caller's set table and pose buffer. All stay
/// live while the context is used. Playback slots are bound separately.
void Gp_AnimInitCtx(GpAnimCtx* ctx, void* sets, TmdObject* model, void* poses);

void Gp_AnimInitSlot(GpAnimCtx* arg0, GpAnimSlot* arg1, s32 arg2, s32 arg3);

void Gp_AnimTickSlot(GpAnimCtx* arg0, GpAnimSlot* arg1);

/// Binds the model, set table, pose buffer and playback slots to an animation context.
///
/// The model allocation and borrowed-data lifetime requirements are those of
/// `Gp_AnimInitCtx`; `slots` supplies one playback slot per model part.
void Gp_AnimInitCtxSlots(GpAnimCtx* ctx, void* sets, TmdObject* model, void* poses, GpAnimSlot* slots);

/// Forwards to `Gp_AnimInitCtxSlots`, which most callers reach by this name
/// rather than its own.
void func_800B3F84(GpAnimCtx* arg0, void* arg1, TmdObject* arg2, void* arg3, GpAnimSlot* arg4);

void Gp_AnimResetSlot(GpAnimCtx* arg0, s32 arg1, s32 arg2);

void Gp_AnimResetSlotEx(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

void Gp_AnimWritePoseBlend(GpAnimCtx* arg0, s32 arg1, GpAnimPose* arg2, GpAnimPose* arg3, s32 arg4,
                           s32 arg5);

void Gp_AnimWritePoseCopy(GpAnimCtx* arg0, s32 arg1, GpAnimPose* arg2, GpAnimPose* arg3, s32 arg4,
                          s32 arg5);

void Gp_AnimTickIndex(GpAnimCtx* arg0, s32 arg1);

void func_800B4538(GpAnimCtx* arg0, s32 arg1, GpAnimPose* arg2, u16 arg3, s32 arg4, s32 arg5,
                   s32 arg6);

/// Returns the current keyframe record, or `NULL` when the slot uses a buffered pose (set 0x7FFF).
///
/// `slot` must have a valid current set and record index. The returned pointer
/// borrows the set's record array and remains valid while that resource is loaded;
/// callers may compare it across ticks to detect a new keyframe. `unusedContext`
/// is ignored and may be `NULL`.
AnimationRecord* Gp_AnimGetRec(GpAnimCtx* unusedContext, GpAnimSlot* slot);

void Gp_SaveEnemyPose(GpEnemy* arg0);

void Gp_SpawnArea(GpAreaKey* arg0);

GpWorkObj* Gp_FindWorkById(u16 arg0);

void Gp_SetTmdBytes(TmdObject* arg0, s32 arg1, s32 arg2);

s32 Gp_GetAreaFlag2(GpAreaKey* arg0);

void Gp_SetAreaObjId(GpAreaKey* arg0, s32 arg1, s32 arg2);

GpAreaVariant* Gp_GetNestedAreaRec(GpAreaKey* arg0);

void Gp_SyncAreaKeyIndex(GpAreaKey* arg0);

/// Draws a semi-transparent textured square of side `arg1` on the XZ plane,
/// anchored at `arg2` (or at the coordinate's own origin when `arg2` is
/// `NULL`), transformed by `arg0->workm` and linked into `gGpuCurrentOt`
/// at the largest corner `otz`.
void Gp_DrawFloorQuad(GfxCoord* arg0, u32 arg1, SVECTOR* arg2);

/// Builds a camera-space offset from `arg0` toward `arg1->pos`, scaled
/// by `-abs(length - arg1->field_2)`, and writes it to `arg2`.
void Gp_MakeDirOffset(SVECTOR* arg0, GpDirSrc* arg1, SVECTOR* arg2);

Task* func_800B2968(void);

void Gp_SetStreamBuf(void* arg0);

void func_800B0928(Task* arg0, Task* arg1, s32 arg2, s32 arg3, s32 arg4);

/// Turns the slot-3 skeleton's head toward the world point in `arg1`'s
/// translation (`coord.t`). Sums the first five `GfxCoord` transforms of
/// `arg0->extra` to get the head's own position and orientation, takes the
/// offset to the target through `ratan2` as a yaw/pitch pair, steps toward it
/// by `arg4 / 0x1000` of the remaining angle and clamps the result to `arg2`
/// yaw and `arg3` pitch before writing the rotation with `RotMatrix`.
void func_800B0CF4(Task* arg0, GfxCoord* arg1, s32 arg2, s32 arg3, s32 arg4);

/// `func_800B0928` with the limits and step taken from `arg2` and the target
/// being `arg1`'s head: composes the first five `GfxCoord` transforms of
/// both tasks (plus the `D_80093A28` head offset) to get each head's world
/// position, takes the offset in `arg0`'s head frame through `ratan2`, unwraps
/// the pitch against `arg2->lastPitch` when it jumps by more than 0x800, steps
/// toward it by `arg2->rate / 0x1000`, clamps, and writes the head rotation.
void func_800B17D4(Task* arg0, Task* arg1, GpHeadAim* arg2);

void Gp_EnemyDispatch(Task* arg0);

void Gp_FadeWorkTask(Task* arg0);

void func_800B2910(Task* arg0);

void func_800B5DB8(Task* arg0);

void func_800B60C0(Task* arg0);

/// Seek a playback slot; the animation-set index uses the low 16 bits of arg2.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

#endif // GAMEPLAY_SCENE_RUNTIME_H
