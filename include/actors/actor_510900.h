#ifndef INCLUDE_ACTORS_ACTOR_510900_H
#define INCLUDE_ACTORS_ACTOR_510900_H

#include "main/task_types.h"

/// Emits the GOLEM's mode-controlled flame jet and orange transient light.
///
/// Bank-6 slot 0x43 borrows its parent's coordinate and owns the EffectWork in
/// spawnArg2.pointer. The actor supplies modes 0 off, 1 burning, 2 blast,
/// 3 dying or 4 released in spawnArg1.value; its current controllers never
/// request blast. Burning grows the size numerator to 256; dying emits flame
/// while age is below 30 and residual particles while below 60. Adopted particles
/// share the emitter's task lifetime. Slot 2 is shared light storage: this task
/// also decrements its lifetime and contracts the inner radius by 400 game
/// units while above 400, retaining the outer radius. Pausing stops emission;
/// cancellation disables the light, while released mode frees work and task
/// even during non-running control. Parent and coordinate body must stay live.
void actor510900FlameJetTask(Task* task);

/// Emits No. 9's muzzle flash, debris streaks, pixel sparks and orange light.
///
/// Bank-6 slot 0x44 runs once at the EffectWork's parent-relative position.
/// Running control emits one impact spark and six pairs of debris/pixel
/// particles, then places shared transient light slot 3 for four ticks with
/// inner/outer radii 4000/4800 game units. Every control mode frees the owned
/// work in spawnArg2.pointer and kills the task on this call. Spawned particles
/// are independent; the borrowed parent and coordinate body must remain live
/// through placement and spawning.
void actor510900MuzzleFlashTask44(Task* task);

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

/// Draws an expanding twelve-frame explosion fireball and optional child copies.
///
/// Bank-6 slot 0x184: spawnArg1 bits 0..11 give the size numerator (0 selects
/// 768), bits 12..15 the ticks per frame (0 selects 2), and bits 24..27 the
/// parent-basis drift multiplier. With zero drift and a zero high nibble,
/// initialization adopts animFrame modulo 4 fast children and modulo 2 slow
/// children at half size. Angles use 4096 units per turn; size grows by its
/// initial signed-halfword value divided by 128 with an arithmetic shift.
/// Atlas frames 0..10 last a full period; frame 11 is drawn on the terminating
/// call. Pausing suppresses
/// drawing and motion; cancellation or the final frame frees owned work in
/// spawnArg2.pointer and kills the task. Parent and coordinate body must live
/// through teardown, and the shared sprite drawer needs its scratch/GTE state.
void actor510900ExplosionFireballTask184(Task* task);

/// Sequences the GOLEM grenade's fireball, main smoke and trailing smoke.
///
/// Bank-6 slot 0x185 requires task state 0..4 and a live coordinate body.
/// Running calls advance the owned EffectWork's age: state 0 adopts a size-1152
/// fireball, state 1 waits through age 9, state 2 emits main smoke through age
/// 51, state 3 emits trailing smoke through age 61, and state 4 frees the work
/// in spawnArg2.pointer and kills the task. State changes emit on the following
/// call. Smoke tasks are independent; pausing holds the sequence, cancellation
/// ends it, and finished state also ends while paused.
void actor510900ExplosionTask185(Task* task);

#endif // INCLUDE_ACTORS_ACTOR_510900_H
