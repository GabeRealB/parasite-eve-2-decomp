#ifndef GAMEPLAY_PRIVATE_EFFECT_TASKS_H
#define GAMEPLAY_PRIVATE_EFFECT_TASKS_H

#include "types.h"

#include "main/task_types.h"
#include "main/tmd_types.h"

/// Six CLUT X coordinates (0x20, 0x30, 0xC0, 0xD0, 0xE0, 0xF0) selected by
/// bits 12..15 of `effectDrawSpinningBillboard`'s packed angle/palette argument and paired with CLUT
/// Y 0x10B.
extern u16 Gp_QuadClutX[];

extern u16 D_80111EB4[];

/// 8 packed RGB-nibble colors. Index is `cln(spawnArg1 << 12) / 2839 & 7`.
/// High nibble is the `effectDrawScreenTint` blend; low three nibbles are R, G, B.
extern u16 Gp_FadeQuadColors[];

extern TmdSource D_80111FC8;

extern TmdSource D_801120E4;

extern TmdSource D_80112200;

extern TmdSource D_8011231C;

extern TmdSource D_801124B8;

void Gp_EffPolyTask9C(Task* arg0);

void Gp_EffCtlTask2B(Task* arg0);

void Gp_EffCtlTask6A(Task* arg0);

void Gp_EffCtlTask6B(Task* arg0);

void func_800ED42C(Task* arg0);

void Gp_EffCtlTask6C(Task* arg0);

/// Draws the two-frame muzzle flare with a random screen-space rotation.
///
/// spawnArg1's signed low half is the size numerator; work->angle retains
/// that size and work->scale the 4096-unit rotation. The half-diagonal is
/// size * 23 / (SZ3 / 4 + 1) pixels despite the 40-texel texture cells.
/// Visible paused effects draw without aging; hidden effects wait and cancel
/// requests release the effect's work and task.
void effectSpriteTask34(Task* task);

/// Draws the two-frame additive muzzle flare at one of two opposite diagonals.
///
/// spawnArg1's signed low half is the size numerator; work->angle retains
/// it and work->scale the rotation (-512 or 1536, 4096 units per turn).
/// The half-diagonal is size * 31 / (SZ3 / 4 + 1) pixels. Paused effects draw
/// without aging; hidden effects wait and cancel requests release the task.
void effectSpriteTask72(Task* task);

/// Draws a four-frame shotgun spark, black at its origin and fading at its tip.
///
/// work->move holds the random launch direction in the spawning coordinate's
/// frame; the tip is scaled by 1 + age / 2 in Q12 after rotation into the view
/// frame. work->scale is the green-channel shift (1..3). Paused effects draw
/// without aging; hidden effects wait and cancel requests release the task.
void effectLineTaskA3(Task* task);

/// Animates a four-frame muzzle spark with optional random drift.
///
/// spawnArg1 bits 0..11 supply size, bit 12 enables random drift and bits
/// 16..19 supply ticks per frame (zero means one). work->angle holds size,
/// work->scale the random 4096-unit rotation and work->period ticks per frame.
/// The half-diagonal is size * 23 / (SZ3 / 4) pixels. Paused effects draw
/// without motion or aging; hidden effects wait and cancellation releases it.
void effectSpriteTask35(Task* task);

/// Animates and moves the eight-frame spark thrown from a weapon muzzle.
///
/// spawnArg1 bits 0..11 supply size. work->angle holds size, work->scale a
/// random 4096-unit rotation and work->period one or two ticks per frame.
/// work->move is the randomized displacement rotated out of the spawning
/// coordinate's frame. The half-diagonal is size * 31 / (SZ3 / 4) pixels.
/// Paused effects draw without motion or aging; hidden effects wait and cancel
/// requests release the work and task.
void effectSpriteTask6F(Task* task);

/// Throws, spins and blinks a short-lived model piece, with room-grid rebounds.
///
/// Shared by bank-6 slots 0x36, 0x66, 0x67, 0x68 and 0x91. spawnArg1 selects
/// the weapon's launch profile; work->move is a Q12 direction, work->scale
/// its speed in coordinate units per tick, work->pos the Euler spin per tick
/// (4096 units per turn), and work->angle the age after which blinking starts.
/// A rebound reduces speed to two thirds; a miss adds 384 to direction Y.
/// The task releases its counted work after twice the blink age. Paused or
/// hidden effects wait; cancellation releases the work and model task.
void effectThrownModelTask(Task* task);

void Gp_EffCtlTask6E(Task* arg0);

void Gp_EffCtlTask6D(Task* arg0);

/// Moves and fades a pixel spark for eight ticks, independently of effectControl.
///
/// A zero spawnArg1 chooses a forward launch in the spawning frame, a one- or
/// two-pixel side (work->angle) and green-channel shift 1..3 (work->scale).
/// Nonzero chooses a small random drift but leaves size and shift at their
/// zeroed spawn values, so its queued tile has zero width and height. Motion
/// updates the next tick's composition; drawing uses the current world matrix.
void effectTileTaskA4(Task* task);

void Gp_EffCtlTask3B(Task* arg0);

void Gp_EffSprTask5C(Task* arg0);

void func_800F289C(Task* arg0);

void Gp_EffSprTask76(Task* arg0);

/// Moves and spins a six-frame additive spark, fading during its last seven ticks.
///
/// spawnArg1 bits 0..11 supply size (zero selects 512), and bits 12..15
/// ticks per frame (zero selects one). work->scale holds size, work->angle
/// the 4096-unit rotation, work->step its increment, and work->move the
/// displacement in the view coordinate's local frame. The half-diagonal is
/// size * 15 / (SZ3 / 4 + 1) pixels. Motion adds five to Y velocity per tick;
/// crossing the probed floor reverses and halves Y and halves X/Z velocity.
/// Paused effects draw without aging; hidden effects wait; age 31 or
/// cancellation releases the counted work and task.
void effectSpriteTask7C(Task* task);

void func_800F4308(Task* arg0);

/// Moves a fading orange or blue spark streak for eight or sixteen ticks.
///
/// Zero spawnArg1 chooses orange, nonzero blue. work->move is the random
/// displacement, work->scale the fade-rate selector (1 or 2), work->angle the
/// green-channel shift (1 or 2), and work->pos the last successfully drawn
/// world position. A rejected projection leaves that saved endpoint unchanged.
/// This task advances independently of effectControl and owns its work block.
void effectLineTask92(Task* task);

/// Draws a red ground glow at random yaw for 1024 ticks.
///
/// spawnArg1 bits 0..11 choose the square's half-side, with zero selecting
/// 1024 coordinate units. work->scale holds that size and work->angle the
/// final age 1023. Red wraps the signed brightness into a byte; green and blue
/// use a quarter of that wrapped byte. This task ignores effectControl.
void effectSpriteTask9E(Task* task);

void Gp_EffSprTask54(Task* arg0);

/// Keeps the player's subtractive ground shadow beneath model part 1.
///
/// Parents its coordinate once the player task exists, then probes from its
/// composed view position and draws with half-size 448 coordinate units and
/// groundShadowShade. A disabled shade, hidden player model or missed probe
/// suppresses drawing. The task persists across absent-player ticks and does
/// not age or release its work in this callback.
void effectSpriteTask53(Task* task);

#endif // GAMEPLAY_PRIVATE_EFFECT_TASKS_H
