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

/// Consumes one scene-image sector from the CD stream's gap-sector path.
///
/// Reads a 60-byte header before its payload, then advances the selected byte
/// cursor one 2048-byte sector per call. A reusable payload with zero
/// `forceReload` skips transfers while retaining the same cursor/count changes.
/// Empty headers stay in header state. A one-sector payload is marked available
/// on the next payload-state call. CD transfer failures are ignored; returns 0.
/// Both arguments are ignored and have no established original role.
///
/// Requires a selected live scene/audio descriptor and initialized sector state.
/// Nonempty sector counts must be 1..32768, and buffer kinds 0..4. The selected
/// decode, actor or external buffer must cover the complete payload and any
/// reserved VLC/timing prefix. Inputs and capacities are trusted, not checked.
/// Borrowed descriptors and payload storage must remain live through decoding.
s32 streamReadSceneImageSector(s32 unused0, s32 unused1);

/// Selects an exact scene/audio key and prepares its shared playback state.
///
/// All four selectors are exact 16-bit values; group 0 searches the stage-zero
/// table, otherwise the stage table. Returns its zero-based element index or
/// `CD_COMMAND_NO_SCENE_SLOT` when no loaded scene/audio descriptor matches.
/// Failure changes no playback state. Success borrows the selected descriptor,
/// clears cached image headers, requests buffer setup and resets scene loading
/// and VLC state. Keep the descriptor table live until playback finishes.
///
/// Saves the game LCG state and one SDK rand() result, then sets the LCG to 0
/// and seeds SDK rand() with 1 for deterministic playback. Finish the selected
/// scene before selecting another, so its saved RNG values are not overwritten.
/// Buffer allocation and playback start happen separately.
s16 streamSelectScene(u16 group, u16 id, u16 subId, u16 subId2);

void Gp_StepCdAudioCmd(void);

void Gp_ApplySndBankMasks(u16 arg0);

/// Marks scene streaming complete and restores the saved random values.
///
/// Marks image loading complete and the scene ended, then clears payload
/// availability, audio-start, buffer-request and timing-pacing flags. Restores
/// the saved game LCG word and reseeds SDK rand() with the saved draw.
/// Requires a prior successful scene selection. Releases no buffers or tasks
/// and does not cancel resident CD requests; their owners handle teardown.
void streamFinishScene(void);

/// Sets or clears the scene stream's game-halt request from an error code.
///
/// Narrows `errorCode` to a signed halfword. Nonzero stores that value as the
/// latest scene error and sets the stream halt bit; zero clears only that bit,
/// retaining the previous error. Other halt requests are preserved. Returns 0.
/// `unusedArgument` is ignored and has no established original role.
s32 streamSetSceneError(s32 errorCode, s32 unusedArgument);

/// Spawns a descriptor task with newly owned enemy work and a teardown parent.
///
/// `table[taskIndex]` must be a live non-terminator descriptor; the signed
/// element index is unchecked. `spawnArg` is copied unchanged to the task's
/// first payload word; its meaning belongs to that descriptor's callback.
/// `spawnArg` is a signed 32-bit value or packed argument bits.
/// Task-list, callback and borrowed model-resource lifetimes follow
/// `taskSpawnFromTable`. No callback is invoked during this call.
///
/// Allocates zeroed primary-heap `Enemy` work, installs its exit callback and
/// links the task under `parent->task`, or the live scene manager when parent
/// is NULL. Parent tasks and their child rings must be live. The new task owns
/// the returned work through its second payload word; do not free it separately.
/// Returns NULL on task/body/work allocation failure, tearing down an allocated
/// task if enemy allocation fails. No descriptor pointer is retained.
Enemy* enemySpawnFromTable(TaskDesc* table, s32 taskIndex, s32 spawnArg, Enemy* parent);

/// Copies a source frame and local offset into a coordinate-body task's world placement.
///
/// `task` may be NULL, in which case NULL is returned without accessing the other
/// arguments. Otherwise it must own a live `TASK_BODY_COORD` body with one writable
/// coordinate.
/// `localOffset` supplies xyz in the source frame's integer coordinate units.
/// The destination preserves the source rotation, moves its origin by the transformed
/// offset, is parented to `gGfxViewCoord`, and is marked dirty.
/// A world-parented source uses its local matrix directly; other sources are composed
/// through their live, acyclic parent chains and converted back from view space.
/// Returns `task`. Borrows all storage and reserves eight scratch bytes around
/// composition; no pointer is retained.
Task* actorRenderCopyCoordBodyTransform(Task* task, GfxCoord* sourceCoord, const SVECTOR* localOffset);

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

/// Blends two Q12 rotations and rebuilds an orthonormal basis from their strongest row pair.
///
/// `blendWeight` is in 1/4096 units, normally 0..`ONE`: zero selects
/// `fromRotation`, and `ONE` selects `toRotation` before normalization.
/// Row differences narrow to signed halfwords before the signed multiply and divide.
/// Cross-product lengths choose the two least parallel rows; the first pair wins
/// a tie. The input rotations must give a usable, nonzero basis after blending.
/// Only the nine rotation halfwords of `destination` are written; translation and
/// padding are untouched. The destination may alias either source: all source
/// rotation reads finish before the output is written. Changes GTE state.
void gfxBlendOrthonormalRotation(const MATRIX* fromRotation, const MATRIX* toRotation, MATRIX* destination, s32 blendWeight);

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

/// Starts a directly supplied track, blending from its current pose during demo scene 1.
///
/// Outside demo scene 1, performs the same normalized-set, matching-track/part
/// initialization as `animationInitDirectSlot`; `trackRecordOffset` and
/// `blendFrames` are ignored.
///
/// In demo scene 1 the slot must already be initialized, with its track index equal
/// to its playback-array index. Rebinds `context->slots` from that index, changes
/// only the destination coordinate index to `partIndex`, ticks and captures the
/// encoded pose in that slot's buffer, then makes the buffer the current endpoint.
/// `requestedSetIndex` is narrowed to u16 without normalization and must select a
/// loaded slot set, excluding the buffered-pose sentinel. `trackRecordOffset` counts
/// records from that set's existing track start; the sum is narrowed to u16 before
/// following controls. Jumps accumulate walk flags; a stop retains the capture
/// tick's next record index in the target set. The capture's flags and boundary
/// hold remain, and the buffered-rotation cache is invalidated.
///
/// `blendFrames` counts whole normal-rate frames, shifted to sixteenths and narrowed
/// through u16 into both time fields; 0..2047 keeps remaining time nonnegative.
/// Every slot, coordinate, buffer entry, visited record and encoded pose must fit
/// its live storage. The track-start sum must fit s32 and control chains must
/// terminate. The buffer must remain live while an endpoint refers to it.
/// Capture and GTE requirements are those of `animationTickSlotPose`.
void animationStartDirectSlot(AnimationContext* context, AnimationSlot* directSlot, s32 partIndex, s32 requestedSetIndex, s32 trackRecordOffset, s32 blendFrames);

/// Binds a model's coordinates, set table and encoded pose buffer without binding slots.
///
/// `model` is a live, non-NULL object returned by `tmdCreateModel`; its allocation
/// supplies `partCount` coordinates. `setTable` is a word-aligned native pointer
/// table with a loaded set for each index playback uses. `poseBuffer` provides
/// a writable, word-aligned entry of `ANIMATION_POSE_BUFFER_BYTES` for each slot
/// index whose transition pose is captured or reused. Entries contain encoded
/// poses, not unpacked `AnimationPose` values. Capacities are not stored or checked
/// and need not equal the model's coordinate count.
/// The context borrows these objects and the clip data for the lifetime of
/// playback. Leaves `context->slots` and all playback/buffer contents untouched;
/// initialize the slots separately, and bind their array before indexed ticking.
void animationBindModelContext(AnimationContext* context, AnimationSet** setTable, TmdObject* model,
                               u8 (*poseBuffer)[ANIMATION_POSE_BUFFER_BYTES]);

/// Initializes a directly supplied playback slot at a model-part track's first keyframe.
///
/// `setIndex` is normalized: zero selects set 1 and a negative index selects
/// its positive magnitude (INT_MIN is invalid). The resulting index must fit the
/// loaded context table and u16, excluding `ANIMATION_SET_BUFFERED_POSE`.
/// `partIndex` selects both the source track and destination coordinate; it must
/// fit u8 and both arrays. The slot borrows the context's set table and places
/// both endpoints at that track's start, selecting its initial pose encoding.
/// Sets normal rate, zero remaining time, and clears the status word and boundary
/// hold. Does not tick or write a model pose. Time span and buffered-pose cache
/// state remain untouched until playback updates them. All borrowed data must
/// remain live, and the initial record and complete encoded pose must fit the set.
void animationInitDirectSlot(AnimationContext* context, AnimationSlot* slot, s32 partIndex, s32 setIndex);

/// Ticks a directly supplied slot after rebinding its context to the slot's playback array.
///
/// The slot's existing `trackIndex` must equal its array index; the complete live
/// array must extend backwards to `slot - slot->trackIndex`. Replaces
/// `context->slots` with that base and invokes `animationTickSlotPose` with
/// no unpacked or encoded output, applying the pose to the selected model coordinate.
/// Playback, buffer, index, scratch and GTE requirements follow that tick's contract.
/// The context retains the recovered array pointer after return.
void animationTickDirectSlot(AnimationContext* context, AnimationSlot* slot);

/// Restarts an indexed playback slot with independent source-track and destination-part indices.
///
/// `slotIndex` selects a writable context slot. `setIndex` selects a loaded
/// context set without normalization, including set zero; it must fit u16 and
/// exclude `ANIMATION_SET_BUFFERED_POSE`. `trackIndex` and `coordIndex` must
/// fit u8 and their respective track-start and model-coordinate arrays.
/// Binds the context's set table, selects the track start and its pose encoding,
/// sets normal rate and zero remaining time, and clears status and boundary hold.
/// The current record index is primed to zero rather than the track start;
/// the first forward tick replaces it before pose lookup. Does not write a pose.
/// The source set, records, encoded poses, slot and model coordinates must remain
/// live. This remapped slot must be ticked by its array index, not by a direct-slot
/// helper that reconstructs the array from the track index.
void animationResetRemappedSlot(AnimationContext* context, s32 slotIndex, s32 setIndex, s32 trackIndex, s32 coordIndex);

/// Applies a weighted blend of two unpacked poses to a slot's model coordinate.
///
/// `slotIndex` must select a live context slot whose `coordIndex` fits the model
/// coordinates. Encoding 1 blends both local translations; all other encodings
/// retain the coordinate's translation. Rotations always blend as signed Euler
/// components, in 4096 units per turn, without wrap correction. Both weights
/// use 1/4096 units, normally 0..`ONE` and summing to `ONE`. Translation uses model
/// integer units; the GTE blend saturates output components to signed halfwords.
/// Both poses remain readable through the call and may be the same pose.
/// Writes the local rotation matrix and marks the coordinate dirty; playback
/// state is unchanged. Requires initialized scratch with one free AnimationPose
/// plus nested matrix-conversion capacity, and clobbers GTE state. All scratch
/// reservations are released and no input pointer is retained.
void animationApplyBlendedPose(AnimationContext* context, s32 slotIndex, const AnimationPose* firstPose,
                               const AnimationPose* secondPose, s32 firstWeight, s32 secondWeight);

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

/// Captures a slot's ticked pose and starts a timed blend to a track-relative record.
///
/// Advances the old playback and writes its encoded pose-buffer entry.
/// `unpackedDestination` optionally receives that tick's unpacked pose; when
/// NULL the tick updates the model coordinate instead. Skipped capture writes
/// follow `animationTickSlotPose`, but the buffer still becomes the current
/// endpoint, retaining its record index. Retains rate, encoding, boundary hold
/// and capture flags; control jumps may add flags, and a stop retains the old
/// next-record index within the selected set. The next buffered blend refreshes
/// its cached rotation delta. Neither the context nor slot set table is replaced.
///
/// `slotIndex` must fit the writable slot and word-aligned encoded-buffer arrays.
/// `setIndex` selects a loaded slot set, excluding `ANIMATION_SET_BUFFERED_POSE`.
/// `trackRecordOffset` counts records from its start for the slot's existing
/// track; the s32 sum must be representable and narrows to u16 before following
/// controls. Track, coordinate, visited record, fallback index and complete pose
/// reads must fit their arrays. The control chain must terminate and the target
/// bank must support the slot's existing encoding. Keep borrowed data and the
/// buffer live while an endpoint refers to them. Scratch/GTE and optional-output
/// requirements are those of `animationTickSlotPose`; no bounds are checked.
///
/// `blendFrames` counts whole normal-rate frames. Its signed shift into sixteenths
/// narrows through u16 into both time fields; 0..2047 keeps signed remaining time
/// nonnegative, and zero starts with no transition time. `unusedArgument` is
/// ignored and has no established original role.
void animationCaptureSlotWithBlend(AnimationContext* context, s32 slotIndex, AnimationPose* unpackedDestination,
                                   u16 setIndex, s32 trackRecordOffset, s32 unusedArgument, s32 blendFrames);

/// Borrows the current keyframe record of a playback slot.
///
/// A buffered current pose (`ANIMATION_SET_BUFFERED_POSE`) has no record and
/// returns NULL. Otherwise the current set and absolute record index must be
/// valid in the slot's loaded set table. The pointer stays valid while that
/// resource is loaded; callers may compare its identity to gate keyframe cues.
/// This query changes no playback state. `unusedContext` is ignored and may be NULL.
const AnimationRecord* animationGetCurrentRecord(const AnimationContext* unusedContext, const AnimationSlot* slot);

/// Retains the first saved pose and resume state for an enemy's placement key.
///
/// Requires a live enemy/task, a TMD model with a root coordinate in world
/// space and a nonzero packed placement key. Normalizes a zero live spawn
/// state to the default resume state even when the key is already saved.
/// An existing key keeps its first pose and state; otherwise the first free
/// entry receives the root position narrowed to signed halfwords and Euler
/// angles quantized to their high bytes (16 units per turn).
///
/// A full table removes the first entry from another saved stage/area and
/// shifts later entries left; if all entries share that area, replaces the
/// final entry. Uses one scratch SVECTOR, released before returning.
void areaSaveEnemyPose(Enemy* enemy);

/// Spawns the placement/resource layout selected by stage, area and variant.
void Gp_SpawnArea(GameLocationKey* location);

/// Returns the first scene child's enemy work with the packed placement key, or NULL.
///
/// `placeKey` packs area (bits 0..7), stage (8..11), and instance index (12..15).
/// Requires a live scene manager and live `Enemy` work on every child; no work-bank
/// filter is applied. The result is borrowed and becomes invalid when that child
/// exits or releases its enemy work. Does not create or retain a reference.
Enemy* sceneFindEnemyByPlaceKey(u16 placeKey);

/// Finds the placed actor at an instance index in the current stage and area.
///
/// `placementIndex` is 0..15, packed into the high nibble of the placement key;
/// it is not range-checked or narrowed. Requires the initialized scene manager.
/// Returns a borrowed `Task*`, or NULL if no placed actor matches. The caller
/// must keep the actor live while using the result.
Task* sceneFindPlacedActor(s32 placementIndex);

/// Changes a model's encoded texture-page and CLUT-row displacements and refreshes both buffer halves.
///
/// Both arguments narrow to signed bytes; -128..127 preserves their values.
/// `texturePageOffset` is added to encoded page words, while one `clutRowOffset`
/// unit adds 64 to encoded CLUT words. A live model and source are required.
/// When a buffer exists, it must hold both writable halves: two builds update
/// them and restore the original next-half selector. With no buffer only the
/// model's offsets change. Storage ownership and lifetime are unchanged.
void tmdSetTextureOffsets(TmdObject* model, s32 texturePageOffset, s32 clutRowOffset);

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

/// Draws a subtractive textured ground-shadow square in a model coordinate's local XZ plane.
///
/// `side` is the full side length in that frame's integer coordinate units.
/// `centreOffset` supplies the local centre, or NULL selects the origin;
/// corner coordinates narrow to signed halfwords. `frame` is composed through
/// its live, acyclic parent chain before projection. Only the last corner's
/// projection flags decide rejection, and the largest corner depth selects
/// the ordering-table entry with a fixed four-bit shift.
/// Borrows a POLY_FT4 packet from the current frame arena and releases its scratch
/// workspace before returning. The packet lives through drawing that frame.
/// The current ordering table must provide 1024 entries. Changes GTE state.
void actorRenderDrawGroundShadow(GfxCoord* frame, u32 side, const SVECTOR* centreOffset);

/// Computes a world-axis separation offset from a contact and a queried view-space position.
///
/// `position` and `contact->point` must be in the same composed view frame, in
/// integer coordinate units; the contact distance uses the same length units.
/// Their difference narrows to signed halfwords before its length is measured.
/// The signed scale is `-abs(length - contact->distance)`, including when the
/// queried point lies beyond that distance. The normalized delta is rotated by
/// the transpose of the view rotation into world axes, then scaled with the GTE;
/// the scale passes through signed IR0 and output components saturate to halfwords.
/// Requires live inputs, writable xyz output and an initialized scratch stack.
/// No pointer is retained; input storage is not modified.
void worldCollisionCalcContactWorldOffset(const SVECTOR* position, const WorldCollisionContact* contact, SVECTOR* offset);

Task* func_800B2968(void);

/// Publishes caller-owned byte storage for external scene-image payloads.
///
/// Scene headers with `STREAM_SCENE_BUFFER_EXTERNAL` load their payload here,
/// after stripping the first sector's header; offsets are relative to this base.
/// The storage must be word-aligned and cover each selected payload's complete
/// sector transfer and decoded offset accesses. Its capacity is not supplied or
/// checked. Keep it live and unmodified while loading, decoding or cached reuse
/// can refer to it; replacing the pointer neither invalidates a cache nor releases
/// the old buffer. NULL is only usable while no external payload is selected.
/// This setter allocates, clears and frees no storage.
void streamSetExternalScenePayloadBuffer(u8* payloadBuffer);

/// Blends a model's part-4 head rotation toward another model's accumulated chain position.
///
/// Both tasks must own live TMD models with at least five coordinates arranged
/// in the expected root-to-head order. Position sums include transformed
/// translations of parts 0..3; part 4 is transformed separately but its result
/// is excluded. Each accumulated rotation is updated as local * accumulated.
/// The target delta narrows to signed halfwords and is rotated into the
/// subject's pre-head frame; pitch uses the absolute forward component.
///
/// Angles and nonnegative `maxYaw` / `maxPitch` use 4096 units per turn.
/// `blendWeight` is a signed 1/4096 interpolation weight, normally 0..`ONE`,
/// with signed division toward zero and no angle-wrap correction. Each limit
/// is widened to the magnitude of the existing head angle before clamping,
/// so an existing out-of-limit pose is not forced inward. Existing roll is
/// preserved. Writes part 4's rotation and marks that coordinate dirty.
/// Borrows all model storage, retains no pointers and changes GTE state.
void animationAimHeadAtTask(Task* subject, Task* targetTask, s32 maxYaw, s32 maxPitch, s32 blendWeight);

/// Blends a model's part-4 head rotation toward a world point carried in a coordinate's translation.
///
/// `subject` must own a live TMD model with at least five coordinates in the
/// expected root-to-head order. Sums transformed translations of parts 0..4,
/// updating the accumulated rotation as accumulated * local, then takes the
/// target delta through the inverse accumulated rotation. The delta narrows
/// to signed halfwords. Only `targetPointFrame->coord.t[0..2]` is read; the
/// other coordinate fields need not be initialized and no target composition occurs.
///
/// Angles and nonnegative `maxYaw` / `maxPitch` use 4096 units per turn.
/// `blendWeight` is in 1/4096 units, normally 0..`ONE`, using signed division
/// toward zero without angle-wrap correction. Each limit is widened to the
/// magnitude of the existing head angle before clamping; existing roll remains.
/// Writes only part 4's rotation. Its composition stamp is left unchanged, so
/// the owner must invalidate the coordinate before a later composition uses it.
/// Borrows its inputs, retains no pointers and changes GTE state.
void animationAimHeadAtPoint(Task* subject, const GfxCoord* targetPointFrame, s32 maxYaw, s32 maxPitch, s32 blendWeight);

/// Blends a model's head toward another head, retaining pitch-wrap history.
///
/// Both tasks must own live TMD models with at least five coordinates in the
/// expected root-to-head order. Accumulates transformed translations and
/// rotations of parts 0..4 as accumulated * local, including a head-local
/// offset of (0, -100, 0) integer units for each model. The world delta narrows
/// to signed halfwords before rotation into the subject's current head frame.
///
/// `aim` must be writable and retained by the caller across ticks. Angles and
/// nonnegative pitch/yaw limits use 4096 units per turn; rate is a signed
/// fraction of ONE, normally 0..ONE. Selects a pitch branch using world-delta
/// Y, then wraps to [-2048, 2048) when it differs from valid history by more
/// than half a turn. Stores that aim pitch before interpolation and clamping.
/// Limits widen to each existing head-angle magnitude; roll is preserved.
/// Writes part 4's rotation, marks it dirty and changes GTE state. Retains no
/// task or model pointer and allocates no storage.
void animationAimHeadAt(Task* subject, Task* targetTask, AnimationHeadAim* aim);

/// Runs the bodyless-enemy teardown delay (bank 1, type 0xB).
///
/// Requires state 0 (start), 1 (countdown) or 2 (destroy); dispatch performs
/// no bounds check. `spawnArg2.pointer` must be the live primary-heap `Enemy`
/// owned by this bodyless task. Start sets `waitTicks` to `ENEMY_WAIT_FRAMES`;
/// 120 subsequent countdown invocations advance to destruction on the next
/// invocation. Destruction releases target tracking, actor locks and enemy
/// storage, then tears down the task. Neither argument is used afterwards.
/// Gameplay must remain loaded while the task can dispatch.
void enemyTeardownDelayTask(Task* task);

/// Runs an owner-controlled full-screen fade (bank 1, type 0x31).
///
/// `spawnArg2.pointer` borrows a writable `ScreenFade` for the task's lifetime.
/// State 0 starts, 1 ramps coverage up, 2 holds and 3 ramps it down. A return
/// request during ramp-up waits for the hold. A nonpositive start length selects
/// `SCREEN_FADE_DEFAULT_FRAMES`; the length must stay positive and unchanged
/// during each ramp. The owner may replace it when requesting return in the hold.
/// Zero blend subtracts toward black; nonzero adds toward white. Intensity uses
/// signed division with four fractional bits, then narrows to each colour byte.
/// Drawing precedes the counter step; the final return packet is emitted before
/// setting `SCREEN_FADE_DONE` and tearing down the task. The record is not freed.
///
/// `spawnArg1.value` is a signed ordering-table tag index. A nonzero index must
/// address a live tag. Zero selects tag 0 at either presentation-table
/// root, otherwise tag -10 relative to the current pointer, requiring that prefix
/// to exist. Requires the current frame arena for a `TILE` and `DR_TPAGE`.
/// The blend command executes before the tile, which compensates vertical shake.
/// Gameplay must remain loaded; packets are borrowed until that frame is drawn.
void fadeScreenTask(Task* task);

/// Runs the event-script full-screen pulse task (bank 1, type 0x19).
///
/// Requires a live task with state 0 (start), 1 (pulse) or 2 (exit); dispatch
/// performs no bounds check. Start draws the first frame immediately. A positive
/// `spawnArg1.value` holds the peak for that many frames between the fixed ramps;
/// a nonpositive value takes the return path immediately and advances to exit
/// after the first draw. Zero `spawnArg2.value` subtracts toward black; nonzero
/// adds toward white. The pulse consumes the hold word and `killCountdown`.
/// Requires the current frame's GPU arena and ordering table. Exit invokes the
/// task's teardown callback, which may release the task. Gameplay must stay loaded.
void fadePulseTask(Task* task);

/// Runs scene-manager registration and per-frame world-target drawing (bank 1, type 0x23).
///
/// Requires a live task with state 0 (initialize) or 1 (running); dispatch
/// performs no bounds check. Initialize registers `GAME_TASK_SLOT_SCENE`, installs
/// actor-message routing and advances to running. Running draws and updates the
/// world-target overlay without advancing state. Its view, target, scratch and
/// GPU requirements are those of `worldTargetDrawOverlay`. The owner keeps the
/// registered task live while scene children and slot consumers use it.
void sceneManagerTask(Task* sceneTask);

/// Runs previous-frame blending over the current frame (bank 1, type 0x2D).
///
/// Requires a live task with state 0 (initialize), 1 (redraw) or 2 (exit); dispatch
/// performs no bounds check. `spawnArg1.value` exactly equal to 16 redraws twice;
/// other values redraw once. Bit 0 independently enables a black fade after 60
/// redraw ticks, adding eight intensity units per tick up to 255. Initialize
/// clears `killCountdown` only for that fade mode and starts redraw on the next
/// task tick. Saved demo scene 1 suppresses redraw and the fade counter.
/// Redraw remains in state 1 until the owner exits it. Requires initialized
/// framebuffers, the current GPU arena and ordering table. Exit invokes the
/// task's teardown callback, which may release it. Gameplay must stay loaded.
void displayBlendPreviousFrameTask(Task* task);

/// Sets the draw mode of a placed actor in the current stage and area, if found.
///
/// `placeIndex` must fit the placement key's four bits (0..15); stage and area
/// come from the live session. The scene task and its placed children must be
/// live. The lookup writes a borrowed Task* synchronously; a missing actor does
/// nothing. Draw modes 0/1 hide/show; other modes belong to the receiver's
/// `ACTOR_MESSAGE_SET_MODEL_DRAW` handler. The forwarding result is discarded.
void sceneSetPlacedActorDrawMode(s32 placeIndex, s32 drawMode);

#endif // GAMEPLAY_SCENE_RUNTIME_H
