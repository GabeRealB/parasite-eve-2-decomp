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
extern TmdSource gAyaMistFirstPairHand0Model;

/// One-part hand model for variant 3's second pair, attachment slot 0 at body joint 12.
///
/// Pair selector 1 chooses this separately stored copy of the first pair's
/// geometry. Its mutability and lifetime follow `gAyaMistFirstPairHand0Model`.
extern TmdSource gAyaMistSecondPairHand0Model;

/// One-part hand model for variant 3's first pair, attachment slot 1 at body joint 8.
///
/// Pair selector 0 chooses this source. Creation copies its read-only initial
/// pose and resolves draw-handler slots in its writable stream. Objects borrow
/// the record, geometry and stream; keep the package loaded until all referring
/// models are released and queued GPU draws finish.
extern TmdSource gAyaMistFirstPairHand1Model;

/// One-part hand model for variant 3's second pair, attachment slot 1 at body joint 8.
///
/// Pair selector 1 chooses this source, the equipped weapon's parent hand.
/// Creation copies its read-only initial pose and resolves draw-handler slots
/// in the writable stream. Objects borrow the record, geometry and stream;
/// keep the package loaded until all referring models are released and queued
/// GPU draws finish.
extern TmdSource gAyaMistSecondPairHand1Model;

/// One-part hand model for variant 4's first pair, attachment slot 0 at body joint 12.
///
/// Pair selector 0 chooses this source. Creation copies its read-only initial
/// pose and resolves draw-handler slots in its writable stream. Objects borrow
/// the record, geometry and stream; keep the package loaded until all referring
/// models are released and queued GPU draws finish.
extern TmdSource gAyaRangeFirstPairHand0Model;

/// One-part hand model for variant 4's second pair, attachment slot 0 at body joint 12.
///
/// Pair selector 1 chooses this separately stored copy of the first pair's
/// geometry. Its mutability and lifetime follow `gAyaRangeFirstPairHand0Model`.
extern TmdSource gAyaRangeSecondPairHand0Model;

/// One-part hand model for variant 4's first pair, attachment slot 1 at body joint 8.
///
/// Pair selector 0 chooses this source. Creation copies its read-only initial
/// pose and resolves draw-handler slots in its writable stream. Objects borrow
/// the record, geometry and stream; keep the package loaded until all referring
/// models are released and queued GPU draws finish.
extern TmdSource gAyaRangeFirstPairHand1Model;

/// One-part hand model for variant 4's second pair, attachment slot 1 at body joint 8.
///
/// Pair selector 1 chooses this source, the equipped weapon's parent hand.
/// Creation copies its read-only initial pose and resolves draw-handler slots
/// in the writable stream. Objects borrow the record, geometry and stream;
/// keep the package loaded until all referring models are released and queued
/// GPU draws finish.
extern TmdSource gAyaRangeSecondPairHand1Model;

/// Aya's nineteen-part player-body model for resource variant 2.
///
/// Objects borrow the writable geometry and command stream. Keep this package
/// loaded until all referring models are released and queued GPU work finishes.
extern TmdSource gAyaDryfieldBodyModel;

/// Aya's nineteen-part player-body model for resource variant 1.
///
/// Objects borrow the writable geometry and command stream. Keep this package
/// loaded until all referring objects are released and queued GPU draws finish.
extern TmdSource gAyaAkropolisBodyModel;

/// One-part hand model for variant 2's first pair, attachment slot 0 at body joint 12.
///
/// Pair selectors are 0 first and 1 second; slot selectors are 0 and 1.
/// The model borrows this package under `gAyaDryfieldBodyModel`'s lifetime contract.
extern TmdSource gAyaDryfieldFirstPairHand0Model;

/// One-part hand model shared by both pairs at attachment slot 0, body joint 12.
///
/// Resource variant 1 uses this same source for pair selectors 0 and 1.
/// It borrows this package under `gAyaAkropolisBodyModel`'s lifetime contract.
extern TmdSource gAyaAkropolisHand0Model;

/// One-part hand model for variant 2's second pair, attachment slot 0 at body joint 12.
///
/// Pair selector 1 chooses this source. Creation copies the read-only initial
/// pose and resolves draw-handler slots in the writable stream. It borrows this
/// package under `gAyaDryfieldBodyModel`'s lifetime contract.
extern TmdSource gAyaDryfieldSecondPairHand0Model;

/// One-part hand model for variant 1's first pair, attachment slot 1 at body joint 8.
///
/// Pair selector 0 chooses this source. The model borrows this package under
/// `gAyaAkropolisBodyModel`'s lifetime contract.
extern TmdSource gAyaAkropolisFirstPairHand1Model;

/// One-part hand model for variant 2's first pair, attachment slot 1 at body joint 8.
///
/// The model borrows this package under `gAyaDryfieldBodyModel`'s lifetime contract.
extern TmdSource gAyaDryfieldFirstPairHand1Model;

/// One-part hand model for variant 1's second pair, attachment slot 1 at body joint 8.
///
/// Pair selector 1 chooses this source, the equipped weapon's parent hand.
/// It borrows this package under `gAyaAkropolisBodyModel`'s lifetime contract.
extern TmdSource gAyaAkropolisSecondPairHand1Model;

/// One-part hand model for variant 2's second pair, attachment slot 1 at body joint 8.
///
/// This is the equipped weapon's parent hand. The model borrows this package
/// under `gAyaDryfieldBodyModel`'s lifetime contract.
extern TmdSource gAyaDryfieldSecondPairHand1Model;

/// Restores open eyes with one frame, then terminates (eye selector 1; reset command 0).
///
/// Variant 1's sequences hold read-only frame pointers to writable upload lists.
/// Eye frames contain 50x16 8-bit texels (25x16 VRAM words), placed at texture
/// U/V (0, 78), and advance every four actor ticks. Each list has one copy and
/// an operation terminator; playback rewrites its destination. Keep the package
/// loaded through playback and GPU transfer completion. The final NULL stops
/// playback and leaves the last frame's texels in place.
extern GpuImageUpload* const gAyaAkropolisEyesRestSequence[2];

/// Closes the eyes through open, half-closed and closed frames (eye selector 2; command 1).
///
/// Frame format, tick cadence and package lifetime follow `gAyaAkropolisEyesRestSequence`.
extern GpuImageUpload* const gAyaAkropolisEyesClosingSequence[4];

/// Opens the eyes through closed, half-closed and open frames (eye selector 3; command 2).
///
/// Frame format, tick cadence and package lifetime follow `gAyaAkropolisEyesRestSequence`.
extern GpuImageUpload* const gAyaAkropolisEyesOpeningSequence[4];

/// Blinks through open, half-closed, closed, half-closed and open (eye selector 4; command 3).
///
/// Frame format, cadence and package lifetime follow `gAyaAkropolisEyesRestSequence`.
extern GpuImageUpload* const gAyaAkropolisEyesBlinkSequence[6];

/// Restores the closed mouth with one frame (mouth selector 2; reset command 0 or command 5).
///
/// Read-only frame pointers refer to writable uploads of 28x20 8-bit texels
/// (14x20 VRAM words), placed at texture U/V (12, 104), with an eight-tick
/// cadence. The final NULL stops playback and leaves the texels in place.
/// Keep the package loaded through playback and GPU transfer completion.
extern GpuImageUpload* const gAyaAkropolisMouthRestSequence[2];

/// Selects the open mouth with one frame (mouth selector 1; command 4).
///
/// Frame format, cadence and package lifetime follow `gAyaAkropolisMouthRestSequence`.
extern GpuImageUpload* const gAyaAkropolisMouthOpenSequence[2];

/// Restores open eyes with one frame, then terminates (eye selector 1; reset command 0).
///
/// Variant 2's eye sequences hold read-only frame pointers to writable upload
/// lists. Each frame contains 50x16 8-bit texels (25x16 VRAM words), placed at
/// texture U/V (0,78), and frames advance every four actor ticks. Each upload
/// list has one copy and an operation terminator; playback rewrites its
/// destination. Keep the package loaded through playback and GPU completion.
/// The final NULL stops playback and leaves the last frame's texels in place.
extern GpuImageUpload* const gAyaDryfieldEyesRestSequence[2];

/// Closes the eyes through open, half-closed and closed frames (eye selector 2; command 1).
///
/// Bounds, tick cadence and package lifetime follow `gAyaDryfieldEyesRestSequence`.
extern GpuImageUpload* const gAyaDryfieldEyesClosingSequence[4];

/// Opens the eyes through closed, half-closed and open frames (eye selector 3; command 2).
///
/// Bounds, tick cadence and package lifetime follow `gAyaDryfieldEyesRestSequence`.
extern GpuImageUpload* const gAyaDryfieldEyesOpeningSequence[4];

/// Blinks through open, half-closed, closed, half-closed and open (eye selector 4; command 3).
///
/// Frame format, cadence and package lifetime follow `gAyaDryfieldEyesRestSequence`.
extern GpuImageUpload* const gAyaDryfieldEyesBlinkSequence[6];

/// Restores the resting mouth with one frame (mouth selector 2; reset command 0 or command 5).
///
/// Read-only frame pointers refer to writable uploads of 28x20 8-bit texels
/// (14x20 VRAM words), placed at texture U/V (12, 104), with an eight-tick
/// cadence. The final NULL stops playback and leaves the texels in place.
/// Keep the package loaded through playback and GPU transfer completion.
extern GpuImageUpload* const gAyaDryfieldMouthRestSequence[2];

/// Selects the open mouth with one frame (mouth selector 1; command 4).
///
/// Frame format, cadence and package lifetime follow `gAyaDryfieldMouthRestSequence`.
extern GpuImageUpload* const gAyaDryfieldMouthOpenSequence[2];

/// Aya's nineteen-part body model for player resource variant 3.
///
/// Objects borrow the geometry and writable command stream; this package must
/// remain loaded until all referring objects and queued draws are finished.
extern TmdSource gAyaMistBodyModel;

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
extern GpuImageUpload* gAyaMistEyesRestSequence[2];

/// Closes the eyes through open, half-closed and closed frames (eye selector 2; command 1).
extern GpuImageUpload* gAyaMistEyesClosingSequence[4];

/// Opens the eyes through closed, half-closed and open frames (eye selector 3; command 2).
extern GpuImageUpload* gAyaMistEyesOpeningSequence[4];

/// Blinks through open, half-closed, closed, half-closed and open (eye selector 4; command 3).
extern GpuImageUpload* gAyaMistEyesBlinkSequence[6];

/// Restores the default mouth frame (mouth selector 2; reset command 0 or command 5).
extern GpuImageUpload* gAyaMistMouthRestSequence[2];

/// Selects the alternate mouth frame (mouth selector 1; command 4), identical here to the default.
extern GpuImageUpload* gAyaMistMouthAlternateSequence[2];

/// Aya's nineteen-part player-body model for resource variant 4.
///
/// Objects borrow this record, its writable geometry and its command stream.
/// Keep the package loaded until all referring models are released and queued
/// GPU work finishes. Creation copies the read-only initial skeleton; the
/// shared stream's draw-handler slots are resolved in place on first creation.
extern TmdSource gAyaRangeBodyModel;

/// Restores open eyes with one frame, then terminates (eye selector 1; reset command 0).
///
/// Variant 4's sequences hold read-only frame pointers to writable upload lists.
/// Eye frames contain 50x16 8-bit texels (25x16 VRAM words), placed at texture
/// U/V (0, 78), and advance every four actor ticks. Each upload list contains
/// one copy and an operation terminator; playback rewrites its destination.
/// Keep the package loaded through playback and GPU transfer completion.
/// The final NULL stops playback and leaves the last frame's texels in place.
extern GpuImageUpload* const gAyaRangeEyesRestSequence[2];

/// Closes the eyes through open, half-closed and closed frames (eye selector 2; command 1).
///
/// Frame format, cadence and lifetime follow `gAyaRangeEyesRestSequence`.
extern GpuImageUpload* const gAyaRangeEyesClosingSequence[4];

/// Opens the eyes through closed, half-closed and open frames (eye selector 3; command 2).
///
/// Frame format, cadence and lifetime follow `gAyaRangeEyesRestSequence`.
extern GpuImageUpload* const gAyaRangeEyesOpeningSequence[4];

/// Blinks through open, half-closed, closed, half-closed and open (eye selector 4; command 3).
///
/// Frame format, cadence and lifetime follow `gAyaRangeEyesRestSequence`.
extern GpuImageUpload* const gAyaRangeEyesBlinkSequence[6];

/// Restores the resting mouth with one frame (mouth selector 2; reset command 0 or command 5).
///
/// Read-only frame pointers refer to writable uploads of 28x20 8-bit texels
/// (14x20 VRAM words), placed at texture U/V (12, 104), with an eight-tick
/// cadence. The final NULL stops playback and leaves the texels in place.
/// Package residency and transfer lifetime follow `gAyaRangeEyesRestSequence`.
extern GpuImageUpload* const gAyaRangeMouthRestSequence[2];

/// Selects the alternate mouth frame (mouth selector 1; command 4), identical here to the resting mouth.
///
/// Frame format, cadence and lifetime follow `gAyaRangeMouthRestSequence`.
extern GpuImageUpload* const gAyaRangeMouthAlternateSequence[2];

#endif // INCLUDE_AYA_AYA_H
