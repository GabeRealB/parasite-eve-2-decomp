/* The Moth, the second enemy of actor_00700 (actor_100700/actor_200700) and
 * the first of actor_300700. It wanders within a box around its spawn point
 * with random jitter, random yaw and pitch steps and two model parts
 * oscillated about Z, until any moth's death raises a combat-state alert; then
 * it steers toward the player and flies along its facing at a per-place-row
 * speed, holding its height relative to the player, with an occasional random
 * sound. Any hit kills it: the death spins and squashes the model, briefly
 * enables its attack sphere, draws an animated burst sprite and destroys the
 * enemy after the sequence.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_MOTH_H
#define SRC_SHARED_MOTH_H

#include "types.h"

#include "actors/actor.h"

#include "main/task_types.h"

#include "gameplay/actor.h"
#include "gameplay/animation.h"
#include "gameplay/effects.h"

/// Work block of a moth: the zeroed allocation its spawn handler makes and
/// keeps at `Task::work` until the enemy is destroyed.
///
/// It opens with the playback storage of the model's four parts and the two
/// matrices the model is lit with, then the three collision spheres the spawn
/// links at the model's root, each followed by its own contact table, and ends
/// with the state the per-frame handlers share. Angles are 4096 to a turn,
/// positions are world units and timers count frames.
typedef struct {
    ActorAnimRig4         rig;               // Playback storage of the model's four parts; slots 1 to 3 are seeded
    MATRIX                colorMtx;          // Colour matrix the model is lit with
    MATRIX                lightMtx;          // Light matrix the model is lit with
    WorldCollisionBody    hitBody;           // Sphere at the root that takes hits; pairs until the death begins
    WorldCollisionContact hitContacts[1];    // The one contact of `hitBody`; also the enemy's hit records. A contact of kind 1 or 2 there starts the death
    WorldCollisionBody    gridBody;          // Sphere at the root tested against the room's grid until the death begins
    WorldCollisionContact gridContacts[4];   // Contacts of `gridBody`; their averaged push-back is applied to the root each frame
    WorldCollisionBody    attackBody;        // Larger sphere at the root keyed with the moth's attack; pairs only from the start of the death until the spheres are unlinked
    WorldCollisionContact attackContacts[1]; // The one contact of `attackBody`; initialized and never read
    EffectSpawnArg        hitEffectArg;      // Argument of the effect a hit spawns, hung off the model's root coordinate
    MATRIX                savedRootMtx;      // Root matrix when the death began, sunk 0x18 a frame; each squash frame rescales a copy of it
    MATRIX                burstRollMtx;      // Rotation of the death burst sprite's quad: a random roll about Z picked on the death's first frame
    byte                  field_26C[0x40];   // Never read or written. Role unproven
    VECTOR                homePos;           // Root position at spawn: centre of the box the wander is held to before the alert
    VECTOR                prevPos;           // Root position before the frame's move, restored when the grid contacts oppose each other
    byte                  field_2CC[8];      // Never read or written. Role unproven
    s16                   flapFast;          // Wing beat, rerolled every 16 frames (0 slow: `flapAngle` steps through a sweep and the moth sinks, 1 fast: full swing reversed every frame and the moth climbs)
    s16                   flapSign;          // Sense of the wing swing, 1 or -1; reversed at each end of a slow sweep and on every fast frame
    s16                   flapAngle;         // Position in the wing swing, -0x100 to 0x100; times `flapSign` it is the Z rotation of model part 2, and negated that of part 3
    s16                   pitch;             // Rotation of the root about X: a random walk held to +-0x100 while alive, spun by `deathSpinRate` in the death
    s16                   yaw;               // Heading of the root, seeded from the placement: a random walk before the alert, turned toward the player after it, spun in the death
    s16                   deathStep;         // Step of the death (0 begin, 1 squash, spin and burst, 2 wait before the enemy is destroyed)
    s16                   timer;             // Alive: frames since `flapFast` was rerolled, 0 to 15. Dying: counts up from 1 to 30 through step 1, picking the burst's frame, then back down to 0
    s16                   squashScale;       // Dying: vertical scale of the root, 0x1000 shrinking to 0x200
    s16                   deathSpinRate;     // Dying: angle added to `pitch` and `yaw` each frame, a random -255 to 255
    s16                   alerted;           // Set once any moth of the scene has begun to die; the moth then flies at the player instead of wandering
    byte                  field_2E8[0xC];    // Never read or written. Role unproven
} MothWork;
STATIC_ASSERT_SIZEOF(MothWork, 0x2F4);

void mothSpawn(Enemy* arg0, Task* arg1);
void mothUpdate(Enemy* arg0, Task* arg1);
void mothContacts(Task* arg0);
void mothOscillateParts(Task* arg0);
void mothSteer(Task* arg0);
void mothDrift(Task* arg0);
void mothDeath(Enemy* arg0, Task* arg1);
void mothDrawBurst(Task* arg0);
void mothTask(Task* arg0);
void mothSquash(Task* arg0);

/* Defined by each package. */

void mothUpdateColor(Task* arg0);

#endif /* SRC_SHARED_MOTH_H */
