#ifndef GAMEPLAY_ANIMATION_H
#define GAMEPLAY_ANIMATION_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "gameplay/animation_types.h"

#include "main/coord.h"

/// Unpacked local transform of one animated model part.
///
/// Playback writes one when a caller wants the blended pose instead of an
/// update to the part's model coordinate, and blend and copy routines read a
/// pair of them to produce that coordinate. Each `SVECTOR` keeps its trailing
/// pad, so the pose is 16 bytes. `translation` uses the model coordinate's
/// integer units. Each component of `rotation` is an Euler angle in 4096 units
/// per turn. A translation-and-rotation track fills both; a rotation-only
/// track leaves `translation` unchanged. The keyframe forms are
/// `AnimationPackedPose` and `AnimationPackedRotation`.
typedef struct {
    SVECTOR translation; // Local X/Y/Z in model integer units
    SVECTOR rotation;    // Euler angles about X/Y/Z, in 1/4096 turns
} AnimationPose;
STATIC_ASSERT_SIZEOF(AnimationPose, 0x10);

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

/// Masks for caller-defined cues and control commands in `AnimationRecord.flags`.
enum {
    ANIMATION_RECORD_CUE_1    = 0x10,
    ANIMATION_RECORD_CUE_2    = 0x20,
    ANIMATION_RECORD_CUE_MASK = ANIMATION_RECORD_CUE_1 | ANIMATION_RECORD_CUE_2,
    ANIMATION_RECORD_STOP     = 0x40, // Ends the track only together with CONTROL
    ANIMATION_RECORD_CONTROL  = 0x80
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

/// Restarts a slot on the same-numbered model-part track and coordinate.
///
/// Rebinds the slot to the context's borrowed set table, selects the track's
/// first keyframe and encoding, clears its boundary state and result flags,
/// and sets normal playback rate (`ANIMATION_RATE_ONE`), replacing any prior rate.
/// The current record is left at zero and the segment duration is unchanged;
/// the first forward pose tick establishes the interpolation segment.
/// This call does not write a model coordinate or capture a transition pose.
///
/// `slotIndex` must be in 0..255 and fit the slot array, model coordinates and
/// selected set's track-start table. `setIndex` must be a loaded table index
/// representable in `u16`, excluding `ANIMATION_SET_BUFFERED_POSE`; zero is used
/// unchanged. The selected track start must name a keyframe in the record array.
/// The caller owns the writable slot; the set table and clip data must remain
/// live during playback. Capacities are neither stored nor checked here.
void animationResetSlot(AnimationContext* context, s32 slotIndex, s32 setIndex);

/// Captures a slot's ticked pose and starts a timed blend toward a selected track record.
///
/// First advances playback and writes the model coordinate and this slot's
/// encoded pose-buffer entry, as in `animationTickSlotPose`. The buffered pose
/// becomes the current endpoint, retaining its record index. A skipped pose
/// write leaves the buffer unchanged, but still installs that endpoint.
/// The rate, pose encoding, set table, capture tick's flags and boundary latch
/// are retained; a later buffered blend refreshes its cached rotation delta.
///
/// `slotIndex` is a nonnegative element index into the context's writable slots
/// and corresponding word-aligned, 16-byte pose-buffer entries. The exported
/// `setIndex` is narrowed to its low 16 bits before selecting a loaded set from
/// the slot's table; the narrowed value must exclude `ANIMATION_SET_BUFFERED_POSE`.
/// `trackRecordOffset` counts records from that set's start for the slot's
/// existing track. The sum must fit `s32` and is narrowed to `u16` before
/// following control records. Jumps add walk flags; a stop retains the capture
/// tick's next record index within the newly selected set.
///
/// `blendFrames` counts whole normal-rate frames, with zero requesting no
/// transition time. Its signed shift into sixteenths is narrowed through `u16`
/// into both time fields; 0..2047 keeps the signed remaining time nonnegative.
/// No argument bounds are checked. Slot, coordinate, track, record and complete
/// pose accesses must fit their arrays; the target must support the slot's
/// existing encoding and control walks must terminate. Borrowed storage, loaded
/// clip data, scratch-stack capacity and GTE requirements are those of
/// `animationTickSlotPose`. The buffer must remain live while either endpoint
/// refers to it.
void animationSeekSlotWithBlend(AnimationContext* context, s32 slotIndex, s32 setIndex,
                                s32 trackRecordOffset, s32 blendFrames);

/// Advances one playback slot and writes its interpolated pose.
///
/// Clears the slot's result flags, consumes its signed `rate` in sixteenths of
/// a frame, and crosses as many segments as needed. Death playback consumes
/// half that rate, rounded toward positive infinity. Forward playback follows
/// control jumps and stops; reverse playback steps contiguous records toward
/// the track start. A boundary latches `atEnd` and reports
/// `ANIMATION_SLOT_SETTLED`. A tick that starts with this latch does not consume
/// time: equal endpoint keys retain the hold, and unequal keys release it.
///
/// `slotIndex` must fit the context's slot and pose-buffer arrays; the slot's
/// coordinate and track indices must fit their respective arrays. Borrowed
/// sets, records and banks must remain loaded. Every visited keyframe must have
/// a positive duration, and control walks must reach a keyframe or stop. Time
/// walking requires a bank-backed next endpoint when advancing and a bank-backed
/// current endpoint when reversing; the buffered set sentinel is only resolved
/// during pose lookup. Reverse stepping must not wrap the unsigned record index
/// or enter a control record, and the segment duration must be positive. The
/// encoding must fit the set's eight-entry bank table, even when unsupported.
/// Playback stores and checks no array lengths.
///
/// Encodings 1 and 4 produce translation/rotation and rotation-only poses.
/// With `unpackedDestination == NULL`, the result updates the slot's model
/// coordinate and marks it dirty; otherwise it writes the unpacked pose and
/// leaves the coordinate unchanged. Rotation-only output preserves translation.
/// The independent optional `encodedDestination` must hold a word-aligned
/// `AnimationPackedPose` (12 bytes) or `AnimationPackedRotation` (4 bytes),
/// according to the slot's encoding. It may alias either encoded endpoint,
/// including this slot's 16-byte buffer entry; both are decoded before writing.
/// Banks must cover the complete 12-byte or 4-byte pose at each word offset.
/// The unpacked output must be separate from encoded endpoint storage. All
/// outputs are borrowed for this call only; zero `timeSpan` skips pose writes.
///
/// Encoding 2 processes playback state and resolves both endpoint addresses,
/// including bank-2 word offsets, then reports an unsupported-encoding error
/// without decoding or writing a pose. Its indices and bank addresses must still
/// be valid. Other unsupported encodings likewise write no pose, without a
/// diagnostic. Pose banks must be word-aligned. The shared scratch stack must
/// be initialized with room for the tick, pose blend and nested matrix conversion;
/// all reservations are released before returning. Pose blending clobbers the GTE.
void animationTickSlotPose(AnimationContext* context, s32 slotIndex, AnimationPose* unpackedDestination,
                           void* encodedDestination);

/// Advances one animation slot and applies its blended pose to the model coordinate.
///
/// Consumes the slot's signed `rate` in sixteenths of a frame, with the death
/// playback adjustment and boundary flags described by `animationTickSlotPose`.
/// Encoding 1 writes local translation and rotation; encoding 4 writes rotation
/// and preserves translation. Both mark the coordinate dirty. Zero `timeSpan`
/// and unsupported encodings skip pose writes. No transition pose is captured.
///
/// `slotIndex` is a nonnegative element index into `context->slots` and its
/// corresponding encoded pose-buffer entry; zero is valid. The slot's
/// `coordIndex` selects the destination and must be below `context->partCount`.
/// The context's bindings are retained. Borrowed storage, loaded clip data,
/// record bounds, scratch-stack capacity and GTE requirements are those of
/// `animationTickSlotPose`; no array lengths are checked here.
void animationTickSlot(AnimationContext* context, s32 slotIndex);

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
