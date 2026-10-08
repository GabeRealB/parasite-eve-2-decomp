/* Moth enemy behavior shared by actor_00700 and actor_300700.
 * The carriers declare their private handlers in their source prologues and
 * include each fragment at its function's position. Living moths wander or
 * pursue the player after another moth begins dying. Contact starts a death
 * that squashes the saved root pose, briefly enables the attack sphere and
 * draws a burst before releasing the enemy.
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

/// Indices of the moth task's three state handlers.
enum {
    MOTH_TASK_SPAWN  = 0,
    MOTH_TASK_UPDATE = 1,
    MOTH_TASK_DEATH  = 2
};

/// Placement index occupies the sound script instance byte.
enum { MOTH_SOUND_PLACE_INDEX_SHIFT = 8 };

/// The death burst advances through eight texture cells, three timer ticks per cell.
enum { MOTH_BURST_CELL_COUNT     = 8,
       MOTH_BURST_TICKS_PER_CELL = 3 };

#endif /* SRC_SHARED_MOTH_H */
