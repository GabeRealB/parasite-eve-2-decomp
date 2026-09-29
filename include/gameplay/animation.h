#ifndef GAMEPLAY_ANIMATION_H
#define GAMEPLAY_ANIMATION_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "main/coord.h"
#include "main/session_types.h"

struct GpAnimSet;

/// One playback frame per tick in the slots' sixteenths-of-a-frame units.
enum { ANIMATION_RATE_ONE = 0x10 };

/// Pose pair used by `Gp_AnimWritePoseBlend` / `Gp_AnimWritePoseCopy`. Translation is
/// GPF/GPL-blended (`Gp_AnimWritePoseBlend`) or copied (`Gp_AnimWritePoseCopy`) into
/// `GfxCoord.coord.t` when `GpAnimSlot.poseKind == 1`; rotation is
/// GPF/GPL-blended with the other pose and fed to `RotMatrix_gte`.
typedef struct _GpAnimPose {
    /* 0x00 */ SVECTOR trans;
    /* 0x08 */ SVECTOR rot;
} GpAnimPose;
STATIC_ASSERT_SIZEOF(GpAnimPose, 0x10);

/// One animation pose with a local translation and three Euler rotation angles.
///
/// Encoding 1 stores six signed halfwords without the padding of `SVECTOR`.
/// Translation uses the model coordinate's integer units; angles use 4096
/// units per turn. `AnimationRecord.wordOffset` addresses the bank in 4-byte words, so
/// consecutive poses begin three words apart. Buffered poses use this same
/// encoding at the start of each slot's 16-byte buffer entry.
typedef struct {
    s16 translationX; // Local X translation
    s16 translationY; // Local Y translation
    s16 translationZ; // Local Z translation
    s16 rotationX;    // Euler rotation about X, in 1/4096 turns
    s16 rotationY;    // Euler rotation about Y, in 1/4096 turns
    s16 rotationZ;    // Euler rotation about Z, in 1/4096 turns
} AnimationPackedPose;
STATIC_ASSERT_SIZEOF(AnimationPackedPose, 0xC);

/// Euler rotation for one animation keyframe, stored in one four-byte word.
///
/// Signed X, Y and Z components occupy bits 0-10, 11-20 and 21-31.
/// Each stored step is eight PsyQ angle units (4096 angle units per turn).
/// Encoding 4 contains no translation; the model part keeps its current offset.
/// The translation-and-rotation companion is `AnimationPackedPose`.
typedef struct {
    s32 rx : 11; // X rotation in eight-unit steps (-1024..1023)
    s32 ry : 10; // Y rotation in eight-unit steps (-512..511)
    s32 rz : 11; // Z rotation in eight-unit steps (-1024..1023)
} AnimationPackedRotation;
STATIC_ASSERT_SIZEOF(AnimationPackedRotation, 4);

/// Pose encodings selected by a track's initial keyframe.
enum {
    ANIMATION_POSE_TRANSLATION_ROTATION = 1, // Three words: packed translation and Euler angles
    ANIMATION_POSE_PACKED_ROTATION      = 4  // One word: packed Euler angles
};

/// Masks for the encoding, caller-defined cues and control commands in `AnimationRecord.flags`.
enum {
    ANIMATION_RECORD_POSE_KIND_MASK = 0x0F,
    ANIMATION_RECORD_CUE_1          = 0x10,
    ANIMATION_RECORD_CUE_2          = 0x20,
    ANIMATION_RECORD_CUE_MASK       = ANIMATION_RECORD_CUE_1 | ANIMATION_RECORD_CUE_2,
    ANIMATION_RECORD_STOP           = 0x40, // Ends the track only together with CONTROL
    ANIMATION_RECORD_CONTROL        = 0x80,
    ANIMATION_RECORD_END            = ANIMATION_RECORD_CONTROL | ANIMATION_RECORD_STOP
};

/// Four-byte keyframe or control record in a model-part animation track.
///
/// Keyframes select a pose and give the positive duration, in normal-rate frames,
/// of the segment interpolating toward it. The initial keyframe's low flags
/// nibble selects the track's pose encoding; cue bits are interpreted by the
/// caller as sounds, effects or action markers.
///
/// A control record has `ANIMATION_RECORD_CONTROL` set. Without
/// `ANIMATION_RECORD_STOP`, its `wordOffset` is an absolute index in the set's
/// record array. With both bits set, advancement ends at the slot's previously
/// selected keyframe and ignores `wordOffset`. Control records never use
/// `durationFrames`.
///
/// Pose offsets count four-byte words: `AnimationPackedPose` reads three words and
/// `AnimationPackedRotation` reads one. Track starts, jump targets and complete pose reads
/// must stay within the loaded set's data; playback carries no array lengths.
typedef struct AnimationRecord {
    u16 wordOffset;     // Pose-bank word offset or absolute jump record index; ignored on end records
    u8  durationFrames; // Normal-rate segment duration (1..255 frames); ignored on control records
    u8  flags;          // Bits 0-3 encoding (1 translation/rotation, 4 packed rotation), 4-5 cues, 7 control, 6 stop control
} AnimationRecord;
STATIC_ASSERT_SIZEOF(AnimationRecord, 4);

/// One animation of a model: the clip data behind a single pointer of the table
/// at `GpAnimSlot.sets` (the same table as `GpAnimCtx.sets`), indexed
/// by animation id.
///
/// An animation carries one track per model part, each a run of `recs`
/// keyframes that begins at the record `trackStart` names, plus one pose bank
/// per pose encoding, which those records index into by 4-byte word.
typedef struct GpAnimSet {
    AnimationRecord* recs;         // keyframe records of every track, one run per model part
    u16*             trackStart;   // record index each track begins at, indexed by `GpAnimSlot.trackIndex`
    void*            poseBanks[8]; // Borrowed word-aligned banks (1 AnimationPackedPose, 4 AnimationPackedRotation); records give word offsets
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
    GfxCoord*   coords;    // The model's per-part coordinate array: each slot writes the transform of the part it drives
    u8*         poses;     // Borrowed writable buffer: 16 bytes per slot, holding that slot's packed encoding
    GpAnimSlot* slots;     // Playback state, one slot per model part
    s32         partCount; // Parts the model is divided into, mirrored from `TmdObject.partCount`
} GpAnimCtx;
STATIC_ASSERT_SIZEOF(GpAnimCtx, 0x14);

/// Advances one playback slot and writes its interpolated pose.
///
/// `slotIndex` must name a slot in the context's per-part array. With
/// `unpackedDestination == NULL`, playback updates the slot's model coordinate;
/// otherwise it writes that pose and leaves the coordinate unchanged.
/// Rotation-only tracks leave the destination's translation unchanged.
/// `encodedDestination` is optional and must hold the slot's encoding:
/// `AnimationPackedPose` (12 bytes) or `AnimationPackedRotation` (4 bytes). Either output
/// is borrowed for this call only; a zero-duration segment writes neither.
/// Banks and encoded outputs must be word-aligned. The encoded output may
/// alias the current slot's 16-byte buffer entry, which is read before writing.
void animationTickSlotPose(GpAnimCtx* context, s32 slotIndex, GpAnimPose* unpackedDestination,
                           void* encodedDestination);

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

/// Bounds of the base animation sets and the script-writable extension.
enum {
    ANIMATION_BANK_BASE_SET_COUNT     = 47,
    ANIMATION_BANK_EXTENSION_CAPACITY = 32,
    ANIMATION_BANK_SET_CAPACITY       = ANIMATION_BANK_BASE_SET_COUNT + ANIMATION_BANK_EXTENSION_CAPACITY,
};

/// Animation-set storage borrowed from a selected weapon or companion resource.
///
/// Base clips occupy the first 47 entries; scripts can copy up to 32 set
/// addresses into the following entries and play them by their extended ids.
/// The word view preserves the message ABI's address representation. Both
/// views cover the complete established storage span without a subarray boundary.
typedef struct {
    union {
        struct GpAnimSet* sets[ANIMATION_BANK_SET_CAPACITY];      // Borrowed set pointers, including script-installed clips
        s32               addresses[ANIMATION_BANK_SET_CAPACITY]; // The same set addresses for word-copy messages
    } table;
} GpAnimBlk;
STATIC_ASSERT_SIZEOF(GpAnimBlk, 0x13C);

#endif // GAMEPLAY_ANIMATION_H
