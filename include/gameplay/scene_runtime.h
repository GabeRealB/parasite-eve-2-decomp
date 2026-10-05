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

Enemy* Gp_SpawnEnemyFromTable(TaskDesc* table, s32 idx, s32 arg2, Enemy* parent);

/// Copies `arg1`'s matrix onto the coordinate at `Task::extra.coordBody->coord`,
/// adding `arg2` in that space. If `arg1->parent` is world (`gGfxViewCoord`),
/// copies `coord` and transforms in place; otherwise computes `workm`
/// via `actorRenderComposeCoord`, transforms there, and converts to local with
/// `gfxMakeRelativeTransform`. Always parents the dest to world and clears `composeStamp`.
/// Returns `arg0` (or NULL).
Task* Gp_CopyCoordOffset(Task* arg0, GfxCoord* arg1, SVECTOR* arg2);

/// Extracts one XYZ Euler solution by cancelling the transformed +Z and +Y axes.
///
/// `matrix` supplies a Q12 rotation and is not modified; translation is ignored.
/// `angles` receives signed X, Y and Z angles in 4096 units per turn. Its fourth
/// halfword is untouched. +Z gives pitch/yaw; inverse `RotMatrixZYX` cancellation
/// of that pair from +Y gives roll. Uses no scratch-stack reservation.
void gfxExtractEulerAngles(const MATRIX* matrix, SVECTOR* angles);

/// Writes the smaller-magnitude XYZ Euler angles of `matrix` into `angles`.
///
/// `vx`, `vy` and `vz` are signed X, Y and Z angles, 4096 units per turn, for
/// the product Rx(x) * Ry(y) * Rz(z) that `RotMatrix` builds. Translation is
/// not read and `matrix` is not modified. `gfxMatrixToEuler` reports one
/// solution of that product; this routine also forms the solution whose X
/// differs by half a turn and stores the candidate with the smaller sum of
/// absolute components. A tie keeps the half-turn solution.
///
/// The store replaces all eight bytes of `angles`. Only xyz are defined
/// angles; the fourth halfword is not an angle and has no defined value.
/// `matrix` must be halfword-aligned. Returns `angles`. Does not reserve
/// scratch.
SVECTOR* gfxExtractSmallestEuler(SVECTOR* angles, const MATRIX* matrix);

/// Lerps the 3x3 rotation of `arg0` toward `arg1` by `arg3 / ONE`, then
/// orthonormalizes into `arg2`. Outer products of each interpolated row
/// pair pick the two most independent axes; `MatrixNormal_0` / `_1` / `_2`
/// reconstructs the missing row.
void Gp_LerpOrthonormal(MATRIX* arg0, MATRIX* arg1, MATRIX* arg2, s32 arg3);

/// Composes a coordinate node and its ancestors into world rotation and translation.
///
/// `node` must be a non-world node in an acyclic chain ending at `gGfxViewCoord`.
/// The world node itself is excluded. `worldRotation` receives only the Q12 3x3
/// rotation; its translation and padding are untouched. `worldTranslation`
/// receives xyz in integer coordinate units; its fourth halfword is untouched.
/// Each local translation is truncated to signed halfwords before rotation, and
/// accumulation truncates at each hierarchy level. No composed cache is updated.
void gfxComposeNodeWorldTransform(const GfxCoord* node, MATRIX* worldRotation, SVECTOR* worldTranslation);

/// Number of packed colours processed by one CLUT-row interpolation.
enum { GPU_RGB555_CLUT_ROW_COLORS = 16 };

/// Interpolates sixteen RGB555 colours from `second` toward `first`.
///
/// `firstWeight` is in 1/4096 units, normally 0..`ONE`; the second weight is
/// `ONE - firstWeight`. Each output keeps bit 15 when either source has it.
/// Both inputs must contain sixteen live halfwords and the destination sixteen
/// writable halfwords, without overlapping either input. Uses temporary scratch
/// storage per colour and retains no pointers.
void gpuBlendRgb555ClutRow(const u16* first, const u16* second, s32 firstWeight, u16* destination);

void func_800B3AA4(AnimationContext* context, AnimationSlot* arg1, s32 arg2, s32 arg3, s32 arg4, s32 arg5);

/// Binds an animation context to a model allocation's coordinate tail and caller-owned playback data.
///
/// `model` must be the object returned by `tmdCreateModel`; the context borrows its
/// `partCount` coordinates and the caller's set table and pose buffer. All stay
/// live while the context is used. Playback slots are bound separately.
void Gp_AnimInitCtx(AnimationContext* ctx, void* sets, TmdObject* model, void* poses);

void Gp_AnimInitSlot(AnimationContext* context, AnimationSlot* arg1, s32 arg2, s32 arg3);

void Gp_AnimTickSlot(AnimationContext* context, AnimationSlot* arg1);

void Gp_AnimResetSlotEx(AnimationContext* context, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

void Gp_AnimWritePoseBlend(AnimationContext* context, s32 arg1, AnimationPose* arg2, AnimationPose* arg3, s32 arg4,
                           s32 arg5);

/// Applies one pose's translation and a weighted blend of two poses' rotations.
///
/// `slotIndex` must address a live context slot, whose `coordIndex` selects a live
/// model coordinate. Encoding 1 copies `sourcePose.translation`; all other
/// encodings leave the coordinate's translation unchanged. `rotationPose` is
/// read only for rotation. Angles use 4096 units per turn and weights 1/4096 units,
/// normally complementary weights summing to `ONE`. Angles blend as signed
/// components without wrap correction. Both borrowed poses must remain live
/// through this call. Marks the coordinate dirty and releases its scratch pose.
void animationApplyPoseWithBlendedRotation(AnimationContext* context, s32 slotIndex, const AnimationPose* sourcePose, const AnimationPose* rotationPose, s32 sourceWeight,
                                           s32 rotationWeight);

void func_800B4538(AnimationContext* context, s32 arg1, AnimationPose* arg2, u16 arg3, s32 arg4, s32 arg5,
                   s32 arg6);

/// Borrows the current keyframe record of a playback slot.
///
/// A buffered current pose (`ANIMATION_SET_BUFFERED_POSE`) has no record and
/// returns NULL. Otherwise the current set and absolute record index must be
/// valid in the slot's loaded set table. The pointer stays valid while that
/// resource is loaded; callers may compare its identity to gate keyframe cues.
/// This query changes no playback state. `unusedContext` is ignored and may be NULL.
const AnimationRecord* animationGetCurrentRecord(const AnimationContext* unusedContext, const AnimationSlot* slot);

/// Records an enemy's state and world pose under its packed placement key.
///
/// Requires the enemy's model and task to remain live. An existing record keeps
/// its pose. A full list evicts a pose from another saved area, with the final
/// slot as the fallback; positions and angles retain the save format's widths.
void Gp_SaveEnemyPose(Enemy* enemy);

/// Spawns the placement/resource layout selected by stage, area and variant.
void Gp_SpawnArea(GameLocationKey* location);

/// Returns the first scene child's enemy work with the packed placement key, or NULL.
///
/// `placeKey` packs area (bits 0..7), stage (8..11), and instance index (12..15).
/// Requires a live scene manager and live `Enemy` work on every child; no work-bank
/// filter is applied. The result is borrowed and becomes invalid when that child
/// exits or releases its enemy work. Does not create or retain a reference.
Enemy* sceneFindEnemyByPlaceKey(u16 placeKey);

void Gp_SetTmdBytes(TmdObject* arg0, s32 arg1, s32 arg2);

s32 Gp_GetAreaFlag2(GameLocationKey* key);

/// Controls when changing a placement variant discards saved enemy poses.
enum {
    AREA_VARIANT_RESET_IF_CHANGED = -1,
    AREA_VARIANT_SKIP_POSE_RESET  = 0,
    AREA_VARIANT_RESET_ALWAYS     = 1
};

/// Sets an area's saved placement variant and updates its saved-pose state.
///
/// `resetMode` is -1 to reset poses only on a variant change, 0 to clear the
/// reset request, or other nonzero values to force it. A variant of 0 always
/// initializes layout 1 and resets poses. Only stage and area are read from `key`;
/// the selected variant must be valid for that area's placement/resource table.
void areaSetPlacementVariant(GameLocationKey* key, s32 variant, s32 resetMode);

/// Returns the stage/area/variant layout, or NULL when its tables are absent.
AreaVariant* Gp_GetNestedAreaRec(GameLocationKey* key);

/// Copies the area's saved placement variant into `key->variant`.
///
/// Defaults the key to layout 1 when its tables are absent. A saved selector
/// of 0 is initialized to 1 and requests removal of that area's saved poses.
/// Only stage and area need to be initialized before this call.
/// An existing layout table requires its saved area-state record to exist too.
void areaSyncLocationVariant(GameLocationKey* key);

/// Draws a semi-transparent textured square of side `arg1` on the XZ plane,
/// anchored at `arg2` (or at the coordinate's own origin when `arg2` is
/// `NULL`), transformed by `arg0->workm` and linked into `gGpuCurrentOt`
/// at the largest corner `otz`.
void Gp_DrawFloorQuad(GfxCoord* arg0, u32 arg1, SVECTOR* arg2);

/// Contact offset in the view coordinate frame, in signed world-coordinate units.
///
/// `position` and `contact->point` are world positions. Their delta is
/// truncated to signed halfwords before measuring its length. The offset
/// magnitude is `-abs(length - contact->distance)`, including when the point
/// lies beyond that distance; `offset` receives the scaled normalized delta
/// transformed by the transpose of the view coordinate's world rotation.
void worldCollisionCalcContactViewOffset(SVECTOR* position, WorldCollisionContact* contact, SVECTOR* offset);

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
void func_800B17D4(Task* arg0, Task* arg1, AnimationHeadAim* arg2);

void Gp_EnemyDispatch(Task* arg0);

void Gp_FadeWorkTask(Task* arg0);

void func_800B2910(Task* arg0);

void func_800B5DB8(Task* arg0);

void func_800B60C0(Task* arg0);

#endif // GAMEPLAY_SCENE_RUNTIME_H
