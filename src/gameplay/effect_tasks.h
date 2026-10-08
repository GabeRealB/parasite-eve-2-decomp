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

/// Expands and fades the critical-hit ring, with optional randomized radial spikes.
///
/// spawnArg1 selects style 0..5. work->period holds its packed colour and
/// spike flag, step its radius increment, angle its current radius and scale
/// its brightness. Eight visible ticks advance before drawing, from radius
/// 32 and brightness 16, reducing brightness by two each tick. The outer
/// screen radius is radius * 256 / (SZ3 / 4 + 1) pixels; the inner is half.
/// Paused effects still advance; hidden effects wait. Completion or ordinary
/// effect cancellation releases the counted work and coordinate task.
void effectPolyTask9C(Task* task);

/// Attaches a handgun or SMG muzzle burst and ejects its cartridge model.
///
/// Requires the counted `EffectWork` and coordinate body supplied by `effectSpawn`,
/// and a live borrowed spawn parent. `spawnArg1`'s low byte is an unchecked
/// weapon-profile index 0..33; a nonzero signed high half suppresses `burstRequest`.
/// The NPC-pistol profile always suppresses it. `work->index` retains that flag;
/// `work->scale` becomes the last active age (2 for MP5A5, 4 otherwise).
/// The first update lights the spawn position, then attaches at the profile's
/// muzzle offset. P229 omits that flare; P229 and NPC pistol disable light slot
/// zero. Other profiles light it for two or four unpaused light updates.
/// A second-update spark is omitted for MP5A5. Every visible call, including
/// pause, advances age and shrinks the full-strength light radius by 400 world
/// units while it exceeds 400; the outer radius stays 4800. The next age after
/// the limit releases the work and task. Hidden and cancelled effects wait.
void effectControlTask2B(Task* task);

/// Emits an actor gun's muzzle flare and three drifting sparks.
///
/// Requires `effectSpawn`'s counted work, coordinate body and live borrowed parent.
/// `spawnArg1` is an unchecked profile index 0..33 into the muzzle-offset table.
/// The first visible update lights the original spawn position in transient
/// slot zero, then attaches at the muzzle and raises `burstRequest`. `work->scale`
/// retains a 0..511 size jitter and `work->move` a local negative-Z offset of
/// half that jitter. The following update emits sparks with one, two and three
/// ticks per frame. Five visible calls release the work and task; paused calls
/// still advance. Hidden and cancelled effects wait. The light's four unpaused
/// updates are independent; its inner radius shrinks by 400 per visible call
/// while above 400, with its 4800-world-unit outer radius fixed.
void effectControlTask6A(Task* task);

/// Emits a rifle muzzle burst with a flare model and transient point light.
///
/// Requires `effectSpawn`'s counted work, coordinate body and live borrowed parent.
/// `spawnArg1`'s low byte is an unchecked weapon-profile index 0..33; a nonzero
/// signed high half suppresses `burstRequest`, retained in `work->index`.
/// The first visible call lights the spawn position in shared slot zero, then
/// attaches at the profile's muzzle offset and emits two sprites and a model.
/// `work->move` retains the jittered local negative-Z flare offset; `work->scale`
/// then becomes the last active age: 1 for M249, 4 otherwise. The following
/// age releases the work and task. Paused calls still age and shrink the inner
/// light radius by 400 world units while above 400; its outer radius stays
/// 4800. Hidden and cancelled effects wait. The light expires separately.
void effectControlTask6B(Task* task);

void func_800ED42C(Task* arg0);

/// Emits a grenade-launcher flash, sparks and an optional delayed model piece.
///
/// Requires `effectSpawn`'s counted work, coordinate body and live borrowed parent.
/// `spawnArg1`'s low byte is an unchecked profile index 0..33; its signed high
/// half suppresses `burstRequest` when nonzero, retained in `work->index`.
/// The first visible call lights the spawn position, attaches at the grenade
/// muzzle offset, draws one randomly rotated flash and emits four spark pairs.
/// `work->scale` then holds the delayed ejection age: 16 normally, 24 for M4A1
/// with grenade launcher. That profile sets `work->angle` to the model-profile
/// bias 10. MM1 sets a four-update limit and skips delayed ejection. The delay
/// age emits three more sparks and model slot 0x91 at the ejection offset;
/// the next age releases the work and task. Paused visible calls still advance;
/// hidden and cancelled effects wait. Light slot zero expires separately and
/// its inner radius shrinks while the 4800-world-unit outer radius stays fixed.
void effectControlTask6C(Task* task);

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

/// Emits reload sparks once and one model piece per update at a weapon offset.
///
/// Requires `effectSpawn`'s counted work, coordinate body and live borrowed parent.
/// `spawnArg1`'s low half is an unchecked profile index 0..33. If any bit 16..19
/// is set, the entire unsigned high half becomes the signed s16 duration in
/// `work->scale`; otherwise duration is 12. Ordinary callers use positive small
/// durations. The first call attaches at the profile's ejection offset plus
/// `work->pos`, in parent-local coordinate units, then emits three drifting sparks.
/// Every call emits model slot 0x91 and increments age; age greater than duration
/// minus one releases the work and task. Runs independently of `effectControl`.
void effectControlTask6E(Task* task);

/// Drops six falling cartridge models and releases the controller immediately.
///
/// Requires `effectSpawn`'s counted work and coordinate body. Keeps the body's
/// spawn translation and parent, replaces its rotation with Q12 identity and
/// composes before spawning six bullet-casing tasks with the Mongoose falling
/// profile. `spawnArg1` is ignored. Runs independently of `effectControl`, releases
/// its work and task in the same call, and leaves child lifetimes independent.
void effectControlTask6D(Task* task);

/// Moves and fades a pixel spark for eight ticks, independently of effectControl.
///
/// A zero spawnArg1 chooses a forward launch in the spawning frame, a one- or
/// two-pixel side (work->angle) and green-channel shift 1..3 (work->scale).
/// Nonzero chooses a small random drift but leaves size and shift at their
/// zeroed spawn values, so its queued tile has zero width and height. Motion
/// updates the next tick's composition; drawing uses the current world matrix.
void effectTileTaskA4(Task* task);

/// Draws a four-frame impact flash and starts six pixel-spark tasks.
///
/// Requires `effectSpawn`'s counted work and coordinate body. `spawnArg1` bits 0..11
/// supply size (zero selects 512). `work->angle` retains size and `work->scale` a
/// random rotation in 4096 units per turn. Its first visible call emits six
/// pixel sparks in nonzero drift mode; that mode retains zeroed tile size.
/// The flash's half-diagonal is size * 23 / (SZ3 / 4 + 1) pixels. Paused effects
/// draw without aging; hidden effects wait; age four or cancellation releases
/// the counted work and task. The borrowed spawn parent and offset are not read.
void effectControlTask3B(Task* task);

void Gp_EffSprTask5C(Task* arg0);

void func_800F289C(Task* arg0);

/// Draws the four-frame raw additive impact flash at a fixed screen-space rotation.
///
/// spawnArg1 bits 0..11 give size (zero selects 512). The first accepted
/// projection stores size in work->scale and a random rotation in work->angle,
/// in 4096 units per turn. Each frame's inclusive UV span (55 or 31 texels)
/// also scales the half-diagonal: span * size / (SZ3 / 4 + 1) pixels.
/// Age advances on every call, including rejected projections and paused or
/// hidden effects. Four calls release the counted work and coordinate task;
/// ordinary effectControl cancellation does not shorten this lifetime.
void effectSpriteTask76(Task* task);

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

/// Draws an eight-frame sand-tinted puff with a random rotation and slow drift.
///
/// Requires `effectSpawn`'s counted work and coordinate body. `spawnArg1` bits 0..11
/// supply size, bits 12..15 ticks per frame (zero selects one), and bit 31 enables
/// one smaller child puff for three of the four rotation residues modulo four.
/// Children have three ticks per frame and size floor(3 * size / 4), and do not
/// inherit bit 31. `work->scale` retains size, `work->angle` the random 4096-unit
/// rotation, `work->period` ticks per frame and `work->move` the displacement in
/// the body's view-parent frame. X/Z drift is -15..16 and Y is -15..0 coordinate
/// units per running tick. The half-diagonal is size * 31 / (SZ3 / 4) pixels;
/// depth has no added bias. Paused effects draw without motion or aging; hidden
/// effects wait. Eight frames or cancellation release the work and task.
/// The borrowed spawn parent and offset pointers are not read.
void effectSpriteTask54(Task* task);

/// Keeps the player's subtractive ground shadow beneath model part 1.
///
/// Parents its coordinate once the player task exists, then probes from its
/// composed view position and draws with half-size 448 coordinate units and
/// groundShadowShade. A disabled shade, hidden player model or missed probe
/// suppresses drawing. The task persists across absent-player ticks and does
/// not age or release its work in this callback.
void effectSpriteTask53(Task* task);

/// Grows, holds and fades the bank-6 ground decal in the coordinate's XZ plane.
///
/// spawnArg1 bits 0..11 give the final half-side in coordinate units and bits
/// 16..19 select its CLUT. EFFECT_GROUND_DECAL_START_DIM starts at full size
/// and brightness 64; EFFECT_GROUND_DECAL_START_FULL_BRIGHT takes precedence
/// and holds brightness 128. Otherwise size grows six units per running tick
/// before brightness dims to 64. work->angle holds final size, work->scale
/// current size, work->period brightness and work->step the CLUT selector.
/// EFFECT_GROUND_DECAL_STATE_FADE is externally writable while the task lives;
/// it subtracts four brightness units per running tick, then releases the task.
/// Nonrunning effects retain their draw without animation, including hidden
/// effects; cancellation performs teardown before that final draw.
void effectSpriteTask46(Task* task);

/// Animates the eight-cell hit puff with randomized launch and gravity.
///
/// spawnArg1 bits 0..11 give size (zero means 512), bits 12..15 the signed
/// four-bit frame period (zero means one), bits 16..17 the GPU blend mode,
/// bit 20 randomized velocity, bit 24 Q12 velocity scaling by size / 1024,
/// and bit 28 one of two CLUTs. work->angle is a 4096-unit rotation; scale is
/// size, period ticks per cell, step blend mode and index palette. The screen
/// half-diagonal is size * 31 / (SZ3 / 4 + 1) pixels. Initialization waits for
/// an accepted projection. Running ticks move by work->move, add six to Y
/// velocity and advance age; the effect ends after eight cell periods.
/// Paused effects draw frozen; hidden effects wait and cancellation releases it.
void effectSpriteTask55(Task* task);

/// Animates the eight-cell trail puff with optional random velocity and gravity.
///
/// spawnArg1 bits 0..11 give size (zero means 512), bits 12..15 the signed
/// four-bit cell period (zero means one), bits 16..17 the GPU blend mode,
/// bit 20 randomized velocity and bit 24 Q12 velocity scaling by size / 1024.
/// work->scale holds size, angle a 4096-unit rotation, period ticks per cell
/// and step blend mode. The screen half-diagonal is size * 15 / (SZ3 / 4 + 1)
/// pixels. Initialization waits for projection; running ticks move, add six to
/// Y velocity and advance age, releasing after eight cell periods. Paused
/// effects draw frozen; hidden effects wait and cancellation releases it.
void effectSpriteTask42(Task* task);

/// Orbits a PE-charge billboard about the player, then contracts or falls away.
///
/// Parents to the player's root coordinate; age advances on every call,
/// independently of ordinary effectControl. work->scale is the random phase,
/// step the orbit radius, move.vz the elevation angle, move.vx the horizontal
/// radius and move.vy the local height. Angles use 4096 units per turn.
/// work->angle is billboard size and period its fading brightness. The live
/// attachment duration starts contraction below eight frames; cancellation
/// makes an established particle fall in local +Y and fade. The terminal
/// state draws once, then releases the counted work and coordinate task.
void effectSpriteTask32(Task* task);

/// Drives the player-joint charge glow and its attachment-selected sound.
///
/// spawnArg1 initially supplies a nonzero divisor for the 256-unit brightness
/// ramp, then stores the sound id selected by the live three-digit attachment
/// id (hundreds 1..6, tens and ones 1..3). Parents to player joint 12. work->scale is inner glow
/// brightness, angle its radius, step the brightness increment and period the
/// outer-band brightness. A charge draws two discs and then a growing band;
/// release shrinks the discs, while cancellation or leaving battle stops the
/// sound and fades them. Ordinary effectControl does not pause this task.
/// The terminal phase releases the counted work and coordinate task.
void effectControlTaskAE(Task* task);

/// Expands and fades a colored Gouraud band while PE effects are running.
///
/// spawnArg1 bits 0..11 give local Z rotation (4096 units per turn) and bits
/// 16..17 select one of four packed RGB-nibble colors. work->period stores
/// that color, scale its brightness and angle the inner radius. Each running
/// tick draws a band 256 coordinate units wide, expands its radius by 128 and
/// dims brightness by eight. Other PE control values wait; cancellation and
/// brightness below nine release the counted work and coordinate task.
void effectPolyTaskC1(Task* task);

/// Raises an eight-cell energy spark with a fixed or randomly chosen palette.
///
/// spawnArg1 bits 0..11 give size; bits 12..15 are the spinning billboard's
/// palette selector, with bit 15 enabling a random 0/1 selector each draw.
/// work->angle holds size, scale a random 4096-unit rotation, period palette
/// bits, index the frame and move.vy a local Y displacement from -16 to -79.
/// Running PE ticks move upward and advance one cell per four calls. Other
/// noncancelled PE control values draw with a fresh random palette unless the
/// player model suppresses drawing. Cancellation or frame eight releases it.
void effectSpriteTaskF4(Task* task);

/// Animates an attached eight-cell wisp that accelerates upward in its parent frame.
///
/// spawnArg1 bits 0..11 give the size numerator and bits 12..13 give one to
/// four ticks per cell. Parents at the spawning coordinate's origin; that
/// parent must remain live through the effect's lifetime. work->angle holds
/// size and scale a random 4096-unit rotation. move.vy starts in -7..0 and
/// decreases on odd ages. The screen half-diagonal is size * 31 / (SZ3 / 4 + 1)
/// pixels. Paused effects draw frozen; hidden effects wait and cancellation
/// releases the task, as does completion of its eight-cell animation.
void effectSpriteTaskA7(Task* task);

/// Draws a growing six-cell additive puff, optionally rising before it fades.
///
/// spawnArg1 bits 0..11 give size (zero means 512); bit 16 chooses a random
/// upward speed in 0..39 coordinate units per running tick. work->scale is
/// final size, period current size, angle the last live age (12..23) and step
/// upward speed. Size grows over 12 ticks, the texture repeats every six, and
/// the last eight ages modulate brightness. The screen half-side is current
/// size * 31 / (SZ3 / 4 + 1) pixels. Initialization precedes projection.
/// Nonrunning effects still draw, including hidden effects. Running age past
/// the selected lifetime or cancellation releases the counted work and task.
void effectSpriteTask80(Task* task);

/// Draws an eight-cell growing fire burst that rises and fades.
///
/// spawnArg1 bits 0..11 give size (zero means 512). work->scale is final size,
/// period current size, angle the last live age (16..23) and step a random
/// upward speed in 0..47 coordinate units per running tick. Size grows over
/// 12 ticks, the texture repeats every eight and the last eight ages fade.
/// The screen half-side is current size * 23 / (SZ3 / 4 + 1) pixels.
/// Initialization waits for projection, while running age advances offscreen.
/// Nonrunning effects still draw, including hidden effects. Running age past
/// the selected lifetime or cancellation releases the counted work and task.
void effectSpriteTask8D(Task* task);

/// Animates an eight-cell rotated particle with free drift or parent-relative rise.
///
/// spawnArg1 bits 0..11 give size, bits 12..13 give two to five ticks per cell
/// and bit 16 selects attachment at the spawning coordinate's origin. An
/// attached parent must remain live through the effect's lifetime. work->scale
/// is size, angle a random 4096-unit rotation, period ticks per cell and index
/// the attachment flag. Free particles drift by work->move with extra local
/// -Y movement of size / 736; attached particles move only along parent Y.
/// Initialization waits for projection. Nonrunning effects draw frozen even
/// when hidden; animation completion or cancellation releases the task.
void effectSpriteTask3F(Task* task);

/// Draws the six-cell rotated additive flash burst, one cell per running tick.
///
/// spawnArg1 bits 0..11 give base size, enlarged by a random 0..255 units;
/// the signed high half selects CLUT X = 96 * palette, Y = 266 + palette.
/// work->scale holds size, angle a random 4096-unit rotation and step palette.
/// The screen half-diagonal is size * 39 / max(SZ3 / 4 - 32, 16) pixels.
/// Initialization waits for projection but running age advances offscreen.
/// Paused effects draw frozen; hidden effects wait and cancellation or the
/// sixth completed cell releases the counted work and coordinate task.
void effectSpriteTaskE0(Task* task);

/// Draws the eight-cell rotated spark fade, one cell per running tick.
///
/// spawnArg1 bits 0..11 give base size, enlarged by a random 0..255 units;
/// the signed high half selects CLUT X = 192 - 80 * palette, Y = 268 - palette.
/// work->scale holds size, angle a random 4096-unit rotation and step palette.
/// The screen half-diagonal is size * 23 / max(SZ3 / 4 - 32, 16) pixels.
/// Initialization precedes projection. Paused effects draw frozen; hidden
/// effects wait and cancellation or the eighth cell releases the work and task.
void effectSpriteTaskE1(Task* task);

/// Blinks a six-cell additive spark burst on alternate running ticks.
///
/// Bank-6 slot 0xE2 borrows its coordinate body and owns counted `EffectWork`
/// in `spawnArg2.pointer`, initialized with age zero. Spawn bits 0..11 give
/// base size, enlarged by random 0..255; size narrows to s16 and then masks to
/// 12 bits in the drawer. A negative spawn word attaches an identity-oriented
/// local placement at `work->pos` under the borrowed `work->parent`, which must
/// remain live. Otherwise the spawning placement is kept.
///
/// Draws cells 0..5 at ages 0,2,..10, at one random 4096-unit rotation and
/// palette zero on page 0x2A. Visible paused effects draw only at even age and
/// do not age; hidden effects wait. Twelve running ticks or cancellation
/// release the counted work and task. Requires the drawer's GTE, scratch and
/// frame-arena resources; queued packets remain live until GPU use ends.
void effectSpriteTaskE2(Task* task);

#endif // GAMEPLAY_PRIVATE_EFFECT_TASKS_H
