#ifndef GAMEPLAY_ANIMATION_H
#define GAMEPLAY_ANIMATION_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "main/coord.h"
#include "main/session_types.h"

struct GpAnimSet;

/// Pose pair used by `Gp_AnimWritePoseBlend` / `Gp_AnimWritePoseCopy`. Translation is
/// GPF/GPL-blended (`Gp_AnimWritePoseBlend`) or copied (`Gp_AnimWritePoseCopy`) into
/// `GpCoord.coord.t` when `GpAnimSlot.poseKind == 1`; rotation is
/// GPF/GPL-blended with the other pose and fed to `RotMatrix_gte`.
typedef struct _GpAnimPose {
    /* 0x00 */ SVECTOR trans;
    /* 0x08 */ SVECTOR rot;
} GpAnimPose;
STATIC_ASSERT_SIZEOF(GpAnimPose, 0x10);

/// Translation and Euler rotation for one keyframe, without SVECTOR padding.
///
/// Encoding 1 occupies three four-byte words; banks and buffered poses are
/// word-aligned, and animation records index the banks in words, not poses.
typedef struct {
    s16 vx; // X translation in model-local coordinates
    s16 vy; // Y translation in model-local coordinates
    s16 vz; // Z translation in model-local coordinates
    s16 rx; // X rotation in PsyQ angle units (4096 per turn)
    s16 ry; // Y rotation in PsyQ angle units
    s16 rz; // Z rotation in PsyQ angle units
} GpPackedPose;
STATIC_ASSERT_SIZEOF(GpPackedPose, 0xC);

/// Euler rotation for one animation keyframe, stored in one four-byte word.
///
/// Signed X, Y and Z components occupy bits 0-10, 11-20 and 21-31.
/// Each stored step is eight PsyQ angle units (4096 angle units per turn).
/// Encoding 4 contains no translation; the model part keeps its current offset.
typedef struct {
    s32 rx : 11; // X rotation in eight-unit steps (-1024..1023)
    s32 ry : 10; // Y rotation in eight-unit steps (-512..511)
    s32 rz : 11; // Z rotation in eight-unit steps (-1024..1023)
} AnimationPackedRotation;
STATIC_ASSERT_SIZEOF(AnimationPackedRotation, 4);

/// One entry of an animation set's 4-byte record array (`GpAnimSet.recs`),
/// walked by the slot code to find the pose a clip is showing. A keyframe entry
/// names that pose, how many frames it is held, and how the pose is encoded;
/// the two cue bits in `flags` mark keyframes a frame handler wants to know
/// about, since it tests them and fires whatever cue it makes them mean.
///
/// A control entry is not shown at all: `flags` bit 7 marks it and the walk
/// follows it instead. With bit 6 clear it continues at `pose`, which is how a
/// clip loops; with bit 6 set it ends the clip and holds the pose reached. A
/// control entry's `duration` is written but never read.
///
/// `pose` counts **4-byte words, not poses**, in the pose bank and in the
/// record array alike: the record array and an `AnimationPackedRotation` bank are one word
/// per element, a `GpPackedPose` bank three, so the latter's poses sit at every
/// third offset.
typedef struct GpAnimRec {
    /* 0x00 */ u16 pose;     // word offset into the set's pose bank; a control entry's continuation record
    /* 0x02 */ u8  duration; // frames this keyframe is held
    /* 0x03 */ u8  flags;    // 0-3 pose encoding (0 control, 1 GpPackedPose, 4 AnimationPackedRotation), 4-5 cue bits, 7 control entry, 6 end of clip
} GpAnimRec;
STATIC_ASSERT_SIZEOF(GpAnimRec, 4);

/// One animation of a model: the clip data behind a single pointer of the table
/// at `GpAnimSlot.sets` (the same table as `GpAnimCtx.sets`), indexed
/// by animation id.
///
/// An animation carries one track per model part, each a run of `recs`
/// keyframes that begins at the record `trackStart` names, plus one pose bank
/// per pose encoding, which those records index into by 4-byte word.
typedef struct GpAnimSet {
    GpAnimRec* recs;         // keyframe records of every track, one run per model part
    u16*       trackStart;   // record index each track begins at, indexed by `GpAnimSlot.trackIndex`
    void*      poseBanks[8]; // Borrowed word-aligned banks (1 GpPackedPose, 4 AnimationPackedRotation); records give word offsets
} GpAnimSet;
STATIC_ASSERT_SIZEOF(GpAnimSet, 0x28);

/// One model's animation state: what its playback reads and the slots that walk
/// it.
///
/// A context is built once from the model body it animates and the animation
/// tables its slots index, and is handed to every later animation call on that
/// model. It borrows the model's own per-part coordinate array and part count,
/// so a slot tick needs nothing but the context.
///
/// The slots and the pose buffer are the caller's: one playback slot per model
/// part, and one pose record per slot, where a slot keeps a pose that no
/// keyframe supplies.
typedef struct {
    GpAnimSet** sets;      // Set table the slots index by animation id
    GpCoord*    coords;    // The model's per-part coordinate array: each slot writes the transform of the part it drives
    u8*         poses;     // Borrowed writable buffer: 16 bytes per slot, holding that slot's packed encoding
    GpAnimSlot* slots;     // Playback state, one slot per model part
    s32         partCount; // Parts the model is divided into, mirrored from `TmdObject.partCount`
} GpAnimCtx;
STATIC_ASSERT_SIZEOF(GpAnimCtx, 0x14);

/// Input for `Gp_MakeDirOffset`. `field_2` is the signed length subtracted
/// from `SquareRoot0(Gfx_ApplyMatrixNoSf(delta, delta))` (the difference
/// is then forced `<= 0`). `pos` is the far end of that delta.
typedef struct _GpDirSrc {
    /* 0x00 */ byte    pad_0[2];
    /* 0x02 */ s16     field_2;
    /* 0x04 */ byte    pad_4[4];
    /* 0x08 */ SVECTOR pos;
} GpDirSrc;

/// Persistent head-tracking state for `func_800B17D4`, allocated by the task
/// that drives the head turn and kept in its `Task::work`. `yawLimit` /
/// `pitchLimit` are the base clamps (widened to the head's current pose each
/// step), `rate` the per-step fraction of the remaining angle in `/ 0x1000`,
/// which the owner ramps, `lastPitch` the previous unwrapped pitch and
/// `inited` whether it is valid. Every owner allocates 12 bytes; nothing reads
/// the bytes after `inited`.
typedef struct _GpHeadAim {
    s16  yawLimit;
    s16  pitchLimit;
    s16  rate;
    s16  lastPitch;
    s8   inited;
    byte pad_9[0x3];
} GpHeadAim;
STATIC_ASSERT_SIZEOF(GpHeadAim, 0xC);

/// The fixed set table and the writable 32-word extension are present in each
/// selected weapon/companion resource. Messages transport set addresses as words.
typedef struct _GpAnimBlk {
    /* 0x00 */ union {
        struct GpAnimSet* sets[47];
        s32               addresses[47];
    } prefix;
    /* 0xBC */ s32 field_BC[32];
} GpAnimBlk;
STATIC_ASSERT_SIZEOF(GpAnimBlk, 0x13C);

#endif // GAMEPLAY_ANIMATION_H
