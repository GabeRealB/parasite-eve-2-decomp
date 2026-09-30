#ifndef GAMEPLAY_ANIMATION_H
#define GAMEPLAY_ANIMATION_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "gameplay/animation_types.h"

#include "main/coord.h"

/// Pose pair used by `Gp_AnimWritePoseBlend` / `Gp_AnimWritePoseCopy`. Translation is
/// GPF/GPL-blended (`Gp_AnimWritePoseBlend`) or copied (`Gp_AnimWritePoseCopy`) into
/// `GfxCoord.coord.t` when `AnimationSlot.poseEncoding == 1`; rotation is
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

/// Number of encoding-indexed pose-bank addresses stored in an animation set.
enum { ANIMATION_POSE_BANK_COUNT = 8 };

/// Borrowed clip data for the animation tracks of one model.
///
/// An animation id selects one set from a playback context's pointer table.
/// Each model-part track begins at its entry in `trackStartIndices` and reads
/// keyframes and control commands from the shared `records` array. Pose banks
/// are selected by the track's encoding; record offsets count four-byte words.
/// Encodings 1 and 4 use `AnimationPackedPose` and `AnimationPackedRotation`.
/// Other bank slots are unused by the supported encodings.
///
/// The descriptor stores no lengths. Track indices, record indices, jump targets
/// and complete encoded-pose reads must fit the supplied arrays. All data is
/// read-only during playback and must remain loaded while any slot or cached
/// record pointer refers to it; pose banks must be word-aligned.
typedef struct AnimationSet {
    const AnimationRecord* records;                              // Borrowed keyframe/control records shared by all tracks
    const u16*             trackStartIndices;                    // Borrowed absolute record indices, indexed by the model-part track
    const void*            poseBanks[ANIMATION_POSE_BANK_COUNT]; // Borrowed encoding-indexed banks (1 translation/rotation, 4 packed rotation); NULL if absent
} AnimationSet;
STATIC_ASSERT_SIZEOF(AnimationSet, 0x28);

/// Bytes reserved for one slot's encoded transition pose, independent of encoding.
enum { ANIMATION_POSE_BUFFER_BYTES = 16 };

/// Playback bindings and encoded transition poses for one model's part animations.
///
/// The context borrows the model allocation's coordinate tail, the animation-set
/// pointer table, writable slots and a word-aligned pose buffer. Their storage
/// and the sets' data must remain live while playback uses them. Slot setup
/// copies the context's set table; individual slots can retain different tables.
///
/// Indexed calls require a valid slot index and the corresponding buffer entry;
/// each slot's coordinate index must be below `partCount`. The buffer holds
/// `AnimationPackedPose` (12 bytes) or `AnimationPackedRotation` (4 bytes) at the
/// start of each 16-byte entry, selected by the slot's encoding. It does not hold
/// unpacked `GpAnimPose` values. Buffer and slot capacities are supplied by the
/// caller and are not stored or checked here. Slot-pointer tick helpers rebind
/// `slots` using the slot's track index, which must equal its array index.
typedef struct {
    AnimationSet** sets;                                       // Borrowed default set table copied into newly initialized slots
    GfxCoord*      coords;                                     // Borrowed mutable model-part transforms, indexed by each slot's coordIndex
    u8             (*poseBuffer)[ANIMATION_POSE_BUFFER_BYTES]; // Borrowed writable encoded poses, indexed by playback slot
    AnimationSlot* slots;                                      // Borrowed playback array; may be rebound by slot-pointer helpers
    s32            partCount;                                  // Number of model-part coordinates, copied from TmdObject.partCount
} AnimationContext;
STATIC_ASSERT_SIZEOF(AnimationContext, 0x14);

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
void animationTickSlotPose(AnimationContext* context, s32 slotIndex, GpAnimPose* unpackedDestination,
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
        struct AnimationSet* sets[ANIMATION_BANK_SET_CAPACITY];      // Borrowed set pointers, including script-installed clips
        s32                  addresses[ANIMATION_BANK_SET_CAPACITY]; // The same set addresses for word-copy messages
    } table;
} GpAnimBlk;
STATIC_ASSERT_SIZEOF(GpAnimBlk, 0x13C);

#endif // GAMEPLAY_ANIMATION_H
