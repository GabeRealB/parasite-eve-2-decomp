#ifndef INCLUDE_ACTORS_ACTOR_510900_H
#define INCLUDE_ACTORS_ACTOR_510900_H

#include "main/task_types.h"

void func_actor_510900_80131F24(Task* arg0);

void func_actor_510900_801340E8(Task* arg0);

/// Draws No. 9's six-cell flame sprite with a fading tail and alternate-frame ground glow.
///
/// Bank-6 effect 0x45. `spawnArg1` bits 0..11 are its size numerator; bit 16
/// enables a random upward step of 0..47 coordinate units per running frame.
/// Initialization waits for an accepted projection and chooses the last age
/// in 0..15. Running calls advance age even if projection rejects the sprite.
/// Pausing keeps it visible without advancing it; hiding suppresses drawing.
/// `spawnArg2.pointer` owns a counted `EffectWork`, freed with the task on
/// cancellation or once age exceeds its limit. The coordinate body must live
/// until then. The scratch stack must fit an `EffectShapeScratch` and, when
/// drawing the glow, a nested `EffectQuadScratch`.
void actor510900FlameSpriteTask45(Task* task);

/// Draws No. 9's eight-frame additive flame-jet sprite with a palette per frame.
///
/// Bank-6 effect 0x4C. `spawnArg1` bits 0..11 give the size numerator, with
/// zero selecting 512; bit 16 enables a random upward step of 0..47 coordinate
/// units per running frame. Frames 0..7 use 32-by-48 texture cells and palettes
/// at VRAM Y=271. Initialization waits for an accepted projection, while age
/// advances on every running call. Pausing draws without advancing; hiding
/// suppresses drawing. Cancellation or age 8 frees the counted `EffectWork`
/// in `spawnArg2.pointer` and kills the task. Its coordinate body and room for
/// one scratch-stack `EffectShapeScratch` are required until it ends.
void actor510900FlameSpriteTask4C(Task* task);

/// Draws No. 9's twelve-frame flame-jet sprite with optional narrowing and downward motion.
///
/// Bank-6 effect 0x52. `spawnArg1` bits 0..11 give the size numerator, with
/// zero selecting 512. Bit 16 enables a random width shift in 0..47, also
/// gating a downward step of 56 coordinate units whenever that value is nonzero.
/// The target applies the low five bits of the shift. Frames 0..11 occupy
/// eight 32-by-48 cells on one texture row and four on the next; palettes
/// are columns 8..19 at VRAM Y=271. Initialization waits for an accepted
/// projection, while age advances on every running call. Pausing draws
/// without advancing; hiding suppresses drawing. Cancellation or age 12 frees
/// the counted `EffectWork` in `spawnArg2.pointer` and kills the task. Its
/// coordinate body and one scratch-stack `EffectShapeScratch` must be available.
void actor510900FlameSpriteTask52(Task* task);

/// Draws No. 9's rising, subtractive flame-jet sprite, holding each cell for two frames.
///
/// Bank-6 effect 0x59. `spawnArg1` bits 0..11 give the size numerator, with
/// zero selecting 512; a random upward step of 0..47 coordinate units per
/// running frame is always chosen. Ages 0..7 use four 32-by-32 cells and the
/// palette at VRAM (32,270). Initialization waits for an accepted projection,
/// while age advances on every running call. Pausing draws without advancing;
/// hiding suppresses drawing. Cancellation or age 8 frees the counted
/// `EffectWork` in `spawnArg2.pointer` and kills the task. Its coordinate body
/// and one scratch-stack `EffectShapeScratch` must be available until it ends.
void actor510900FlameSpriteTask59(Task* task);

/// Moves and draws one fading debris streak from No. 9's muzzle-flash burst.
///
/// Bank-6 effect 0x65. On the first running call, local-coordinate drift is
/// chosen independently: X/Z in -31..32, Y in -79..-16 units per frame. Gravity
/// adds 6 to Y drift each frame. The additive line joins the world positions
/// before and after that drift, sorted at the mean of its projected depths.
/// Lifetime is 16 frames with probability 3/4, otherwise 32; red dims each frame,
/// green uses a random right shift of 1 or 2, and blue is red divided by 8.
/// Non-running control suppresses drawing and motion; cancellation frees the
/// counted `EffectWork` in `spawnArg2.pointer` and kills the task. The coordinate
/// body must survive until termination. Every call reserves 36 scratch-stack
/// bytes; non-running calls retain that reservation until the frame resets it.
void actor510900DebrisStreakTask(Task* task);

void func_actor_510900_8013482C(Task* arg0);

void func_actor_510900_801346D4(Task* arg0);

#endif // INCLUDE_ACTORS_ACTOR_510900_H
