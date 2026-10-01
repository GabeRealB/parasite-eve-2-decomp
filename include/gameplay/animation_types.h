#ifndef GAMEPLAY_ANIMATION_TYPES_H
#define GAMEPLAY_ANIMATION_TYPES_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "main/coord.h"

struct AnimationSet;

/// One normal-rate playback frame per tick, in sixteenths of a frame.
enum { ANIMATION_RATE_ONE = 0x10 };

/// Set-index sentinel selecting the slot's pose-buffer entry instead of a bank keyframe.
///
/// Stored in an endpoint's set index. Pose lookup then reads that slot's
/// context-buffer entry and ignores the record index. Stores leave the record
/// index in place, so whole-key compares and copies still include it. The value
/// is not a set-table index, and keyframe-record lookup of a buffered current
/// endpoint finds none. The reference is valid only for the slot whose buffer
/// captured the pose. Time walking indexes the set table without testing this
/// sentinel: a forward step uses the next endpoint's set index, and a backward
/// step uses the current one.
enum { ANIMATION_SET_BUFFERED_POSE = 0x7FFF };

/// Results of the latest slot tick or track walk. Initialization and each tick clear them first.
///
/// `ANIMATION_SLOT_SETTLED` means the latest pose tick is holding the boundary
/// pose. While `atEnd` is set and both endpoints still agree, that tick reports
/// this bit alone and does not advance time. The tick whose walk reaches a
/// boundary also reports `ANIMATION_SLOT_REACHED_BOUNDARY` and latches the hold.
/// A seek keeps that tick's flags and can add walk bits after moving the
/// endpoints, so a set bit after a seek is not a hold of the new segment.
/// The next pose tick clears the flags first. A zero `rate` outside a hold
/// leaves the segment where it is and leaves this bit clear.
enum {
    /// A track walk reported an end control, return jump, or reverse-start clamp.
    ///
    /// Forward walks set this for a stop control or a control jump whose target
    /// record index equals the slot's previously selected next record index;
    /// the jump comparison does not compare set indices. Reverse walks set it
    /// when the candidate record index falls below the track start.
    /// A pose tick clears it first and adds `ANIMATION_SLOT_SETTLED` when the
    /// walk reports this boundary. Later ticks holding that pose report only
    /// `ANIMATION_SLOT_SETTLED`. Seek/play walks retain the preceding pose
    /// tick's flags and may add this bit while selecting a new endpoint.
    ANIMATION_SLOT_REACHED_BOUNDARY = 1,
    ANIMATION_SLOT_FOLLOWED_JUMP    = 2,    // Control jump taken this walk (stop clear; index becomes wordOffset), including a jump that also sets ANIMATION_SLOT_REACHED_BOUNDARY
    ANIMATION_SLOT_BOUNDARY_MASK    = ANIMATION_SLOT_REACHED_BOUNDARY | ANIMATION_SLOT_FOLLOWED_JUMP,
    ANIMATION_SLOT_SETTLED          = 0x100 // Boundary pose held by the latest pose tick
};

/// Low record-index bits used by actor cue tests; playback uses the full index.
enum { ANIMATION_POSE_CUE_INDEX_MASK = 0x3FF };

/// An interpolation endpoint's bank keyframe or slot-local buffered pose.
///
/// For a bank pose, `indices.setIndex` selects the owning slot's borrowed set
/// table and `indices.recordIndex` is an absolute element index in that set's
/// records, including the track-start offset. Both indices must fit the loaded
/// tables; the reference carries no pointer, array length or pose encoding.
/// `ANIMATION_POSE_CUE_INDEX_MASK` extracts the low ten record-index bits used
/// by actor cue tests without narrowing the stored index or the playback lookup.
///
/// `ANIMATION_SET_BUFFERED_POSE` selects the owning slot's entry in its context's
/// writable pose buffer. Buffering retains the previous record index: pose
/// lookup ignores it, but whole-key equality and copies still include it.
/// Buffered references depend on that slot and buffer, so are not portable
/// between slots. Bank references likewise depend on the slot's set table.
///
/// The little-endian word view has the set index in bits 0-15 and the record
/// index in bits 16-31. `key` compares and transfers both indices together;
/// equal keys identify the same endpoint only within the same slot and tables.
typedef union {
    u32 key;             // Both indices as one word, including a buffered pose's retained record index
    struct {
        u16 setIndex;    // Owning slot's set-table index, or ANIMATION_SET_BUFFERED_POSE
        u16 recordIndex; // Absolute keyframe record index; unused only by buffered-pose lookup
    } indices;           // The two unsigned 16-bit indices used to resolve a bank pose
} AnimationPoseReference;
STATIC_ASSERT_SIZEOF(AnimationPoseReference, 4);

/// Playback and interpolation state for one model-part animation track.
///
/// `currentPose` and `nextPose` identify the interpolation endpoints. Segment
/// timing counts sixteenths of a normal-rate frame: `timeLeft / timeSpan` is
/// the current endpoint's weight. Signed `rate` advances toward the next pose;
/// negative values step backwards, and zero holds time without by itself
/// reporting a boundary. Death playback halves the step with signed rounding.
/// The walk reports boundaries and jumps in `flags`. `ANIMATION_SLOT_SETTLED`
/// reports that the latest pose tick is holding the boundary pose; `atEnd`
/// keeps that hold until the endpoints change.
///
/// The caller owns the slot array and a writable, word-aligned 16-byte pose
/// buffer entry per slot. `sets` borrows loaded clip descriptors and their data
/// for as long as the slot uses them. Set and record indices must fit that
/// table and its records; `trackIndex` must fit the selected track-start table,
/// and `coordIndex` the context's coordinate array. Playback stores no lengths.
/// Pose encodings 1 and 4 read 12 and 4 bytes respectively from word-aligned
/// banks. Keyframe durations must be positive for the time walk to progress.
///
/// The source track and destination coordinate may differ. Helpers accepting
/// only a slot pointer recover the array as `slot - slot->trackIndex`, so they
/// require its array index to equal `trackIndex`; indexed calls can remap it.
typedef struct {
    AnimationPoseReference currentPose;           // Segment's starting pose
    AnimationPoseReference nextPose;              // Segment's destination pose
    byte                   unknown_8;             // No direct accesses; role unproven
    s8                     rate;                  // Signed sixteenth-frame step per tick (-128..127; 16 one frame)
    u8                     field_A;               // Cleared when crossing segments; role unproven
    u8                     poseEncoding;          // Initial keyframe's low flags nibble (1 translation/rotation, 4 packed rotation; 2 diagnostic only)
    s16                    timeLeft;              // Remaining sixteenth-frame time; may cross either segment boundary
    u16                    timeSpan;              // Segment duration in sixteenths; interpolation denominator (0 skips pose output)
    u16                    flags;                 // Latest ANIMATION_SLOT_* tick and walk results; a pose tick clears them first
    u16                    field_12;              // Cleared by initialization; role unproven
    u8                     coordIndex;            // Destination model-part coordinate index
    u8                     trackIndex;            // Source model-part track index
    u8                     atEnd;                 // Boundary hold (0 advancing, 1 holds while endpoints agree)
    u8                     usesBufferedPose;      // Latest blend endpoints (0 both bank poses, 1 either endpoint buffered)
    SVECTOR                bufferedRotationDelta; // Cached next * inverse(current) Euler rotation, in 1/4096 turns
    struct AnimationSet**  sets;                  // Borrowed animation-set pointer table
    byte                   unknown_24[4];         // No direct accesses; role unproven
} AnimationSlot;
STATIC_ASSERT_SIZEOF(AnimationSlot, 0x28);

/// Bytes reserved for one slot's encoded transition pose, independent of encoding.
///
/// This is the buffer-entry capacity and byte stride, not the complete buffer
/// length or an encoded pose's length. Slot `i` uses entry `i`: encoding 1
/// stores `AnimationPackedPose` (12 bytes) at its start, and encoding 4 stores
/// `AnimationPackedRotation` (4 bytes). Playback leaves the remaining bytes
/// untouched; their role is unproven. Entries do not hold unpacked
/// `AnimationPose` values, despite sharing their 16-byte size.
///
/// The caller supplies a writable, word-aligned entry for every valid slot
/// index and keeps it live while its context uses it. Playback stores no
/// capacity. The byte-array declaration itself does not guarantee alignment.
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
/// unpacked `AnimationPose` values. Buffer and slot capacities are supplied by the
/// caller and are not stored or checked here. Slot-pointer tick helpers rebind
/// `slots` using the slot's track index, which must equal its array index.
typedef struct {
    struct AnimationSet** sets;                                       // Borrowed default set table copied into newly initialized slots
    GfxCoord*             coords;                                     // Borrowed mutable model-part transforms, indexed by each slot's coordIndex
    u8                    (*poseBuffer)[ANIMATION_POSE_BUFFER_BYTES]; // Borrowed writable encoded poses, indexed by playback slot
    AnimationSlot*        slots;                                      // Borrowed playback array; may be rebound by slot-pointer helpers
    s32                   partCount;                                  // Number of model-part coordinates, copied from TmdObject.partCount
} AnimationContext;
STATIC_ASSERT_SIZEOF(AnimationContext, 0x14);

#endif // GAMEPLAY_ANIMATION_TYPES_H
