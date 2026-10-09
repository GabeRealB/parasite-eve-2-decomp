#ifndef INCLUDE_AYA_AYA_H
#define INCLUDE_AYA_AYA_H

#include "gameplay/gpu_image_upload.h"

#include "main/tmd_types.h"

/// Models named after the package that holds them.
extern TmdSource D_aya_10200_80115B90;

extern TmdSource D_aya_10200_80115FD0;

extern TmdSource D_aya_10200_801164C0;

extern TmdSource D_aya_10200_80116900;

extern TmdSource D_aya_10500_80115B90;

extern TmdSource D_aya_10500_80115FD0;

extern TmdSource D_aya_10500_801164C0;

extern TmdSource D_aya_10500_80116900;

/// Aya's nineteen-part player-body model for resource variant 2.
///
/// Objects borrow the writable geometry and command stream. Keep this package
/// loaded until all referring models are released and queued GPU work finishes.
extern TmdSource gAya10300BodyModel;

extern TmdSource D_aya_10400_8011B078;

/// One-part hand model for variant 2's first pair, attachment slot 0 at body joint 12.
///
/// Pair selectors are 0 first and 1 second; slot selectors are 0 and 1.
/// The model borrows this package under `gAya10300BodyModel`'s lifetime contract.
extern TmdSource gAya10300FirstPairHand0Model;

extern TmdSource D_aya_10400_8011B4CC;

extern TmdSource D_aya_10300_8011B568;

extern TmdSource D_aya_10400_8011B9BC;

/// One-part hand model for variant 2's first pair, attachment slot 1 at body joint 8.
///
/// The model borrows this package under `gAya10300BodyModel`'s lifetime contract.
extern TmdSource gAya10300FirstPairHand1Model;

extern TmdSource D_aya_10400_8011BE10;

/// One-part hand model for variant 2's second pair, attachment slot 1 at body joint 8.
///
/// This is the equipped weapon's parent hand. The model borrows this package
/// under `gAya10300BodyModel`'s lifetime contract.
extern TmdSource gAya10300SecondPairHand1Model;

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

extern TmdSource D_aya_10500_8011C2E4;

#endif // INCLUDE_AYA_AYA_H
