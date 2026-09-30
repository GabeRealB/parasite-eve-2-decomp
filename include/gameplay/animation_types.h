#ifndef GAMEPLAY_ANIMATION_TYPES_H
#define GAMEPLAY_ANIMATION_TYPES_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

struct AnimationSet;

/// One normal-rate playback frame per tick, in sixteenths of a frame.
enum { ANIMATION_RATE_ONE = 0x10 };

/// Selects the slot's writable context-buffer pose instead of a bank keyframe.
enum { ANIMATION_SET_BUFFERED_POSE = 0x7FFF };

/// Results of the latest slot tick or track walk. Initialization and each tick clear them first.
enum {
    ANIMATION_SLOT_REACHED_END   = 1,    // End control, jump to the selected keyframe, or reverse track boundary
    ANIMATION_SLOT_FOLLOWED_JUMP = 2,    // Control jump taken this walk (stop clear; index becomes wordOffset), including a jump that also sets ANIMATION_SLOT_REACHED_END
    ANIMATION_SLOT_BOUNDARY_MASK = ANIMATION_SLOT_REACHED_END | ANIMATION_SLOT_FOLLOWED_JUMP,
    ANIMATION_SLOT_SETTLED       = 0x100 // The boundary pose is held
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
/// negative values step backwards, and zero holds time. Death playback halves
/// the step with signed rounding. The walk reports boundaries and jumps in
/// `flags`; `atEnd` holds the boundary until the endpoints change.
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
    u16                    flags;                 // Latest ANIMATION_SLOT_* walk results, cleared at each tick
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

#endif // GAMEPLAY_ANIMATION_TYPES_H
