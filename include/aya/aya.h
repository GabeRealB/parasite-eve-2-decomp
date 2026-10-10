#ifndef INCLUDE_AYA_AYA_H
#define INCLUDE_AYA_AYA_H

#include "gameplay/gpu_image_upload.h"

#include "main/tmd_types.h"

/// One-part hand model for variant 3's first pair, attachment slot 0 at body joint 12.
///
/// Pair selector 0 chooses this source. Creation copies its read-only initial
/// pose and resolves draw-handler slots in its writable stream. Objects borrow
/// the record, geometry and stream; keep the package loaded until all referring
/// models are released and queued GPU draws finish.
extern TmdSource gAya10200FirstPairHand0Model;

/// One-part hand model for variant 3's second pair, attachment slot 0 at body joint 12.
///
/// Pair selector 1 chooses this separately stored copy of the first pair's
/// geometry. Its mutability and lifetime follow `gAya10200FirstPairHand0Model`.
extern TmdSource gAya10200SecondPairHand0Model;

/// One-part hand model for variant 3's first pair, attachment slot 1 at body joint 8.
///
/// Pair selector 0 chooses this source. Creation copies its read-only initial
/// pose and resolves draw-handler slots in its writable stream. Objects borrow
/// the record, geometry and stream; keep the package loaded until all referring
/// models are released and queued GPU draws finish.
extern TmdSource gAya10200FirstPairHand1Model;

extern TmdSource D_aya_10200_80116900;

/// One-part hand model for variant 4's first pair, attachment slot 0 at body joint 12.
///
/// Pair selector 0 chooses this source. Creation copies its read-only initial
/// pose and resolves draw-handler slots in its writable stream. Objects borrow
/// the record, geometry and stream; keep the package loaded until all referring
/// models are released and queued GPU draws finish.
extern TmdSource gAya10500FirstPairHand0Model;

/// One-part hand model for variant 4's second pair, attachment slot 0 at body joint 12.
///
/// Pair selector 1 chooses this separately stored copy of the first pair's
/// geometry. Its mutability and lifetime follow `gAya10500FirstPairHand0Model`.
extern TmdSource gAya10500SecondPairHand0Model;

/// One-part hand model for variant 4's first pair, attachment slot 1 at body joint 8.
///
/// Pair selector 0 chooses this source. Creation copies its read-only initial
/// pose and resolves draw-handler slots in its writable stream. Objects borrow
/// the record, geometry and stream; keep the package loaded until all referring
/// models are released and queued GPU draws finish.
extern TmdSource gAya10500FirstPairHand1Model;

extern TmdSource D_aya_10500_80116900;

/// Aya's nineteen-part player-body model for resource variant 2.
///
/// Objects borrow the writable geometry and command stream. Keep this package
/// loaded until all referring models are released and queued GPU work finishes.
extern TmdSource gAya10300BodyModel;

/// Aya's nineteen-part player-body model for resource variant 1.
///
/// Objects borrow the writable geometry and command stream. Keep this package
/// loaded until all referring objects are released and queued GPU draws finish.
extern TmdSource gAya10400BodyModel;

/// One-part hand model for variant 2's first pair, attachment slot 0 at body joint 12.
///
/// Pair selectors are 0 first and 1 second; slot selectors are 0 and 1.
/// The model borrows this package under `gAya10300BodyModel`'s lifetime contract.
extern TmdSource gAya10300FirstPairHand0Model;

/// One-part hand model shared by both pairs at attachment slot 0, body joint 12.
///
/// Resource variant 1 uses this same source for pair selectors 0 and 1.
/// It borrows this package under `gAya10400BodyModel`'s lifetime contract.
extern TmdSource gAya10400Hand0Model;

/// One-part hand model for variant 2's second pair, attachment slot 0 at body joint 12.
///
/// Pair selector 1 chooses this source. Creation copies the read-only initial
/// pose and resolves draw-handler slots in the writable stream. It borrows this
/// package under `gAya10300BodyModel`'s lifetime contract.
extern TmdSource gAya10300SecondPairHand0Model;

/// One-part hand model for variant 1's first pair, attachment slot 1 at body joint 8.
///
/// Pair selector 0 chooses this source. The model borrows this package under
/// `gAya10400BodyModel`'s lifetime contract.
extern TmdSource gAya10400FirstPairHand1Model;

/// One-part hand model for variant 2's first pair, attachment slot 1 at body joint 8.
///
/// The model borrows this package under `gAya10300BodyModel`'s lifetime contract.
extern TmdSource gAya10300FirstPairHand1Model;

/// One-part hand model for variant 1's second pair, attachment slot 1 at body joint 8.
///
/// Pair selector 1 chooses this source, the equipped weapon's parent hand.
/// It borrows this package under `gAya10400BodyModel`'s lifetime contract.
extern TmdSource gAya10400SecondPairHand1Model;

/// One-part hand model for variant 2's second pair, attachment slot 1 at body joint 8.
///
/// This is the equipped weapon's parent hand. The model borrows this package
/// under `gAya10300BodyModel`'s lifetime contract.
extern TmdSource gAya10300SecondPairHand1Model;

/// Restores open eyes with one frame, then terminates (eye selector 1; reset command 0).
///
/// Variant 1's sequences hold read-only frame pointers to writable upload lists.
/// Eye frames contain 50x16 8-bit texels (25x16 VRAM words), placed at texture
/// U/V (0, 78), and advance every four actor ticks. Each list has one copy and
/// an operation terminator; playback rewrites its destination. Keep the package
/// loaded through playback and GPU transfer completion. The final NULL stops
/// playback and leaves the last frame's texels in place.
extern GpuImageUpload* const gAya10400EyesRestSequence[2];

/// Closes the eyes through open, half-closed and closed frames (eye selector 2; command 1).
///
/// Frame format, tick cadence and package lifetime follow `gAya10400EyesRestSequence`.
extern GpuImageUpload* const gAya10400EyesClosingSequence[4];

/// Opens the eyes through closed, half-closed and open frames (eye selector 3; command 2).
///
/// Frame format, tick cadence and package lifetime follow `gAya10400EyesRestSequence`.
extern GpuImageUpload* const gAya10400EyesOpeningSequence[4];

/// Restores open eyes with one frame, then terminates (eye selector 1; reset command 0).
///
/// Variant 2's eye sequences hold read-only frame pointers to writable upload
/// lists. Each frame contains 50x16 8-bit texels (25x16 VRAM words), placed at
/// texture U/V (0,78), and frames advance every four actor ticks. Each upload
/// list has one copy and an operation terminator; playback rewrites its
/// destination. Keep the package loaded through playback and GPU completion.
/// The final NULL stops playback and leaves the last frame's texels in place.
extern GpuImageUpload* const gAya10300EyesRestSequence[2];

/// Closes the eyes through open, half-closed and closed frames (eye selector 2; command 1).
///
/// Bounds, tick cadence and package lifetime follow `gAya10300EyesRestSequence`.
extern GpuImageUpload* const gAya10300EyesClosingSequence[4];

/// Opens the eyes through closed, half-closed and open frames (eye selector 3; command 2).
///
/// Bounds, tick cadence and package lifetime follow `gAya10300EyesRestSequence`.
extern GpuImageUpload* const gAya10300EyesOpeningSequence[4];

/// Aya's nineteen-part body model for player resource variant 3.
///
/// Objects borrow the geometry and writable command stream; this package must
/// remain loaded until all referring objects and queued draws are finished.
extern TmdSource gAya10200BodyModel;

/// Restores open eyes with one frame, then terminates (eye selector 1; reset command 0).
///
/// This package's facial sequences are NULL-terminated arrays of writable upload
/// lists. Use them only while player resource variant 3's package is loaded.
/// Eye frames contain 50x16 8-bit texels (25x16 VRAM words), placed at texture
/// U/V (0, 78), and advance every four actor ticks. Mouth frames contain 28x20
/// texels (14x20 words), placed at (12, 104), and advance every eight ticks.
/// Each upload list has one copy and an operation terminator. Playback rewrites
/// the destination; pixels must remain valid until the queued GPU copy completes.
/// Reaching NULL stops playback and leaves the last frame's texels in place.
extern GpuImageUpload* gAya10200EyesRestSequence[2];

/// Closes the eyes through open, half-closed and closed frames (eye selector 2; command 1).
extern GpuImageUpload* gAya10200EyesClosingSequence[4];

/// Opens the eyes through closed, half-closed and open frames (eye selector 3; command 2).
extern GpuImageUpload* gAya10200EyesOpeningSequence[4];

/// Blinks through open, half-closed, closed, half-closed and open (eye selector 4; command 3).
extern GpuImageUpload* gAya10200EyesBlinkSequence[6];

/// Restores the default mouth frame (mouth selector 2; reset command 0 or command 5).
extern GpuImageUpload* gAya10200MouthRestSequence[2];

/// Selects the alternate mouth frame (mouth selector 1; command 4), identical here to the default.
extern GpuImageUpload* gAya10200MouthAlternateSequence[2];

/// Aya's nineteen-part player-body model for resource variant 4.
///
/// Objects borrow this record, its writable geometry and its command stream.
/// Keep the package loaded until all referring models are released and queued
/// GPU work finishes. Creation copies the read-only initial skeleton; the
/// shared stream's draw-handler slots are resolved in place on first creation.
extern TmdSource gAya10500BodyModel;

/// Restores open eyes with one frame, then terminates (eye selector 1; reset command 0).
///
/// Variant 4's sequences hold read-only frame pointers to writable upload lists.
/// Eye frames contain 50x16 8-bit texels (25x16 VRAM words), placed at texture
/// U/V (0, 78), and advance every four actor ticks. Each upload list contains
/// one copy and an operation terminator; playback rewrites its destination.
/// Keep the package loaded through playback and GPU transfer completion.
/// The final NULL stops playback and leaves the last frame's texels in place.
extern GpuImageUpload* const gAya10500EyesRestSequence[2];

/// Closes the eyes through open, half-closed and closed frames (eye selector 2; command 1).
///
/// Frame format, cadence and lifetime follow `gAya10500EyesRestSequence`.
extern GpuImageUpload* const gAya10500EyesClosingSequence[4];

/// Opens the eyes through closed, half-closed and open frames (eye selector 3; command 2).
///
/// Frame format, cadence and lifetime follow `gAya10500EyesRestSequence`.
extern GpuImageUpload* const gAya10500EyesOpeningSequence[4];

/// Blinks through open, half-closed, closed, half-closed and open (eye selector 4; command 3).
///
/// Frame format, cadence and lifetime follow `gAya10500EyesRestSequence`.
extern GpuImageUpload* const gAya10500EyesBlinkSequence[6];

/// Restores the resting mouth with one frame (mouth selector 2; reset command 0 or command 5).
///
/// Read-only frame pointers refer to writable uploads of 28x20 8-bit texels
/// (14x20 VRAM words), placed at texture U/V (12, 104), with an eight-tick
/// cadence. The final NULL stops playback and leaves the texels in place.
/// Package residency and transfer lifetime follow `gAya10500EyesRestSequence`.
extern GpuImageUpload* const gAya10500MouthRestSequence[2];

/// Selects the alternate mouth frame (mouth selector 1; command 4), identical here to the resting mouth.
///
/// Frame format, cadence and lifetime follow `gAya10500MouthRestSequence`.
extern GpuImageUpload* const gAya10500MouthAlternateSequence[2];

#endif // INCLUDE_AYA_AYA_H
