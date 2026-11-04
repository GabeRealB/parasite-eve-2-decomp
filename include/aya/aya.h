#ifndef INCLUDE_AYA_AYA_H
#define INCLUDE_AYA_AYA_H

#include "gameplay/gpu_image_upload.h"

#include "main/tmd_types.h"

/*
 * What Aya's four outfit packages publish: her body, her hands and the facial animation each one carries. They are
 * the player's resource variants and load one at a time into Aya's slot: variant 1 is `aya_akropolis`, 2
 * `aya_dryfield`, 3 `aya_mist` and 4 `aya_range`. Everything an outfit publishes is borrowed from its package, so the
 * package stays loaded until every model made from it is released, its facial playback has stopped, and the GPU work
 * queued from either has finished.
 *
 * Body. A nineteen-part model. Creation copies its read-only initial skeleton; the writable command stream's
 * draw-handler slots are resolved in place on first creation.
 *
 * Hands. One-part models in two pairs, chosen by the pair selector, 0 for the first and 1 for the second. In each
 * pair, attachment slot 0 is at body joint 12 and slot 1 at body joint 8; the second pair's slot 1 is the equipped
 * weapon's parent hand. Creation copies a hand's initial pose and resolves the draw-handler slots in its stream.
 *
 * Face. Each sequence is a NULL-terminated list of frames, each frame an upload list of one copy and its terminator.
 * Playback writes the destination: eye frames go to texture U/V (0, 78) and advance every four actor ticks, mouth
 * frames to (12, 104) every eight. A frame's size is its image's, and its pixels stay valid until the queued copy has
 * completed. Reaching NULL stops playback and leaves the last frame's texels in place. Each sequence answers one
 * selector and command of the player's facial animation, the same in every outfit.
 */

// Variant 1, at the Akropolis.

extern TmdSource gAyaAkropolisBodyModel;
/// Both pairs' slot 0: one source serves pair selectors 0 and 1.
extern TmdSource gAyaAkropolisHand0Model;
/// First pair, slot 1.
extern TmdSource gAyaAkropolisFirstPairHand1Model;
/// Second pair, slot 1: the weapon hand.
extern TmdSource gAyaAkropolisSecondPairHand1Model;

/// Open eyes, then stop: eye selector 1, reset command 0.
extern GpuImageUpload* gAyaAkropolisEyesRestSequence[];
/// Open, half-closed and closed: eye selector 2, command 1.
extern GpuImageUpload* gAyaAkropolisEyesClosingSequence[];
/// Closed, half-closed and open: eye selector 3, command 2.
extern GpuImageUpload* gAyaAkropolisEyesOpeningSequence[];
/// Open, half-closed, closed, half-closed and open: eye selector 4, command 3.
extern GpuImageUpload* gAyaAkropolisEyesBlinkSequence[];
/// The closed mouth: mouth selector 2, reset command 0 or command 5.
extern GpuImageUpload* gAyaAkropolisMouthRestSequence[];
/// The open mouth: mouth selector 1, command 4.
extern GpuImageUpload* gAyaAkropolisMouthOpenSequence[];

// Variant 2, at Dryfield.

extern TmdSource gAyaDryfieldBodyModel;
/// First pair, slot 0.
extern TmdSource gAyaDryfieldFirstPairHand0Model;
/// Second pair, slot 0.
extern TmdSource gAyaDryfieldSecondPairHand0Model;
/// First pair, slot 1.
extern TmdSource gAyaDryfieldFirstPairHand1Model;
/// Second pair, slot 1: the weapon hand.
extern TmdSource gAyaDryfieldSecondPairHand1Model;

/// Open eyes, then stop: eye selector 1, reset command 0.
extern GpuImageUpload* gAyaDryfieldEyesRestSequence[];
/// Open, half-closed and closed: eye selector 2, command 1.
extern GpuImageUpload* gAyaDryfieldEyesClosingSequence[];
/// Closed, half-closed and open: eye selector 3, command 2.
extern GpuImageUpload* gAyaDryfieldEyesOpeningSequence[];
/// Open, half-closed, closed, half-closed and open: eye selector 4, command 3.
extern GpuImageUpload* gAyaDryfieldEyesBlinkSequence[];
/// The resting mouth: mouth selector 2, reset command 0 or command 5.
extern GpuImageUpload* gAyaDryfieldMouthRestSequence[];
/// The open mouth: mouth selector 1, command 4.
extern GpuImageUpload* gAyaDryfieldMouthOpenSequence[];

// Variant 3, Aya's clothes at the MIST.

extern TmdSource gAyaMistBodyModel;
/// First pair, slot 0.
extern TmdSource gAyaMistFirstPairHand0Model;
/// Second pair, slot 0: a separately stored copy of the first pair's.
extern TmdSource gAyaMistSecondPairHand0Model;
/// First pair, slot 1.
extern TmdSource gAyaMistFirstPairHand1Model;
/// Second pair, slot 1: the weapon hand.
extern TmdSource gAyaMistSecondPairHand1Model;

/// Open eyes, then stop: eye selector 1, reset command 0.
extern GpuImageUpload* gAyaMistEyesRestSequence[];
/// Open, half-closed and closed: eye selector 2, command 1.
extern GpuImageUpload* gAyaMistEyesClosingSequence[];
/// Closed, half-closed and open: eye selector 3, command 2.
extern GpuImageUpload* gAyaMistEyesOpeningSequence[];
/// Open, half-closed, closed, half-closed and open: eye selector 4, command 3.
extern GpuImageUpload* gAyaMistEyesBlinkSequence[];
/// The resting mouth: mouth selector 2, reset command 0 or command 5.
extern GpuImageUpload* gAyaMistMouthRestSequence[];
/// The alternate mouth, here the resting one again: mouth selector 1, command 4.
extern GpuImageUpload* gAyaMistMouthAlternateSequence[];

// Variant 4, the same clothes with ear protection, for the new game and the shooting range.

extern TmdSource gAyaRangeBodyModel;
/// First pair, slot 0.
extern TmdSource gAyaRangeFirstPairHand0Model;
/// Second pair, slot 0: a separately stored copy of the first pair's.
extern TmdSource gAyaRangeSecondPairHand0Model;
/// First pair, slot 1.
extern TmdSource gAyaRangeFirstPairHand1Model;
/// Second pair, slot 1: the weapon hand.
extern TmdSource gAyaRangeSecondPairHand1Model;

/// Open eyes, then stop: eye selector 1, reset command 0.
extern GpuImageUpload* gAyaRangeEyesRestSequence[];
/// Open, half-closed and closed: eye selector 2, command 1.
extern GpuImageUpload* gAyaRangeEyesClosingSequence[];
/// Closed, half-closed and open: eye selector 3, command 2.
extern GpuImageUpload* gAyaRangeEyesOpeningSequence[];
/// Open, half-closed, closed, half-closed and open: eye selector 4, command 3.
extern GpuImageUpload* gAyaRangeEyesBlinkSequence[];
/// The resting mouth: mouth selector 2, reset command 0 or command 5.
extern GpuImageUpload* gAyaRangeMouthRestSequence[];
/// The alternate mouth, here the resting one again: mouth selector 1, command 4.
extern GpuImageUpload* gAyaRangeMouthAlternateSequence[];

#endif // INCLUDE_AYA_AYA_H
