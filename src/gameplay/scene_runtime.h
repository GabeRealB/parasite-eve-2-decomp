#ifndef GAMEPLAY_PRIVATE_SCENE_RUNTIME_H
#define GAMEPLAY_PRIVATE_SCENE_RUNTIME_H

#include "types.h"

#include "gameplay/animation.h"
#include "area_flags.h"

#include "main/coord.h"
#include "main/session_types.h"
#include "main/task_types.h"

extern AreaObjectStage Gp_Bit2Banks[];

extern TaskDesc D_8010D1FC;

void func_800B25B0(void);

/// Queues a stage-zero PE sound file unless that file was already requested.
///
/// `fileIndex` must select a loaded category-5 entry (0..63). On a changed ID,
/// stops PE script instances, queues a normal file load and requests a seek
/// back to the current view, then caches the requested ID immediately. The cache
/// records a request, not successful completion. No idle/full-ring check is
/// made; the caller must respect the CD request ring's capacity.
void sndLoadEnqueuePeFile(u8 fileIndex);

/// Ticks a directly supplied player slot into its model coordinate.
///
/// `slot->trackIndex` must equal its index in a live playback array extending
/// back to slot - slot->trackIndex. Replaces and retains context->slots with
/// that base, then calls `animationTickSlotPose` with both optional outputs NULL.
/// Index, buffer, lifetime, scratch and GTE requirements follow that tick.
void animationTickPlayerSlot(AnimationContext* context, AnimationSlot* slot);

/// Captures a slot's current pose and starts a timed blend, optionally rebinding its clip table.
///
/// First advances playback using the slot's old set table and captures its
/// encoded pose in the context's buffer entry. `unpackedDestination` optionally
/// receives the same pose; when NULL, the tick updates the model coordinate.
/// Capture outputs and skipped writes follow `animationTickSlotPose`.
/// The buffer becomes the current endpoint, retaining its record index.
///
/// A non-NULL `replacementSetTable` replaces the context's default table and
/// this slot's table; other slots keep their bindings. NULL retains both tables.
/// The replacement is a word-aligned native `AnimationSet*` table, borrowed
/// without copying or freeing. Keep it and its loaded clip data live while any
/// context or slot uses them. Capturing finishes before the table changes.
///
/// `setIndex` selects a loaded set from the slot's resulting table, excluding
/// `ANIMATION_SET_BUFFERED_POSE`. `trackRecordOffset` counts records from that
/// set's start for the slot's existing track. The sum must fit s32 and is narrowed
/// to u16 before following controls. Jumps add walk flags; a stop retains the
/// capture tick's next record index within the target set. The capture tick's
/// flags and boundary latch remain, as do the slot's rate and pose encoding.
/// A later buffered blend refreshes its cached rotation delta.
///
/// `blendFrames` counts whole normal-rate frames, with zero selecting no
/// transition time. Its signed shift into sixteenths is narrowed through u16
/// into both time fields; 0..2047 keeps the signed remaining time nonnegative.
/// `unusedArgument` is ignored and has no established original role.
/// `slotIndex` must be nonnegative and fit both the writable slot array and its
/// word-aligned, 16-byte encoded pose-buffer entries. Track, coordinate, visited
/// record and complete pose accesses must fit their arrays, including a stop's
/// retained record index in the target set. Control chains must terminate and
/// the target bank must support the slot's existing encoding. No bounds are
/// checked. Storage, scratch capacity and GTE requirements are those of
/// `animationTickSlotPose`; keep the buffer live while an endpoint refers to it.
void animationPlaySlotWithBlend(AnimationContext* context, s32 slotIndex, AnimationPose* unpackedDestination,
                                u16 setIndex, s32 trackRecordOffset, s32 unusedArgument, s32 blendFrames,
                                AnimationSet** replacementSetTable);

/// Restores area actors' automatic model-buffer policy after movie decoding.
///
/// Requires a live registered scene task and stable circular child ring. Each
/// TMD child must own a live model and `Enemy` work with a live placement. The
/// live save's stage, area and variant must select a non-NULL resource table
/// ended by `AREA_PLACEMENT_END`; matching entries need a live task table.
/// Non-TMD children and placements with no resource match are left unchanged.
///
/// Uses descriptor entry zero, independently of the resource's spawn index.
/// Exactly `TASK_BODY_TMD` enables automatic buffer recovery; exactly that kind
/// with `TASK_DESC_SKIP_AUTO_MODEL_BUFFER` suppresses it. Other flags leave the
/// policy unchanged. Allocates and frees no buffer and does not change ownership.
void areaRestoreModelBufferPolicy(void);

/// Changes a coordinate parent while preserving its composed transform.
///
/// Both nodes and their existing parent chains must be live and acyclic;
/// `newParent` must not be the node or one of its descendants. An unchanged
/// parent is a no-op. Otherwise composes both nodes, converts the old composed
/// matrix into the new parent frame and marks the coordinate dirty. Retains
/// the borrowed parent pointer and does not allocate or release storage.
void gfxReparentCoord(GfxCoord* newParent, GfxCoord* coord);

/// Sets an area's saved-pose restoration bit when the key's variant agrees.
///
/// Reads stage, area and variant only; indexes must fit their tables. Missing
/// stage/saved-state tables or a variant mismatch are a no-op. Zero
/// `useSavedPoses` clears the bit and any nonzero value sets it; reset/map bits,
/// the key and the pose array remain unchanged. A pending reset can later clear
/// restoration again during spawn preparation.
void areaSetSavedPoseRestoreEnabled(s32 useSavedPoses, const GameLocationKey* key);

/// Requests an area's saved-pose reset, except for the nighttime Dryfield underpass.
///
/// Reads stage/area from a word-aligned live key; both indexes must fit their
/// tables. Missing stage/saved-state tables and the excluded area are a no-op.
/// Sets the pending reset bit without changing restoration, other flags, the key
/// or the pose array. Spawn preparation later applies the request. The visited
/// area caller issues it only on the first visit.
void areaRequestSavedPoseReset(const GameLocationKey* location);

/// Releases primitive buffers of the scene manager's direct TMD children for movie decoding.
///
/// The registered scene task and its circular child list must be live and stable,
/// each TMD child must own a live model, and GPU consumption must have finished.
/// Frees both primitive-buffer halves and suppresses automatic buffer recreation;
/// other body kinds and descendants are untouched. Models, coordinates and tasks
/// remain live. Empty child lists are a no-op; models can be enabled and allocated
/// again after movie decoding releases the auxiliary heap.
void sceneFreeActorPrimitiveBuffers(void);

#endif // GAMEPLAY_PRIVATE_SCENE_RUNTIME_H
