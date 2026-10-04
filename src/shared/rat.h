/* The Rat, the first enemy of actor_00700 (actor_100700/actor_200700) and the
 * second of actor_300700. Its per-frame update reads the reaction flags, runs
 * a contact pass and a five-mode behaviour machine - idle wandering with timed
 * moves rolled from per-place-row chance tables, a sensor-triggered approach
 * and attack with a short-lived attack sphere, a knock-back stagger, a build-
 * up hold, and a hurt pause - then turns toward the wanted heading, steps
 * along its facing and plays its six-slot animation, with a randomly timed
 * idle sound and a ground shadow. On death it unlinks its node and four
 * collision spheres, squashes the model flat over 60 frames and is destroyed.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_RAT_H
#define SRC_SHARED_RAT_H

#include "types.h"

#include "actors/actor.h"

#include "main/task_types.h"

#include "gameplay/actor.h"
#include "gameplay/animation.h"
#include "gameplay/effects.h"

/// Top-level behaviour of a rat, kept in `RatWork::mode`.
enum {
    RAT_MODE_IDLE    = 0, // Wanders in timed moves until `attackRequested`
    RAT_MODE_ATTACK  = 1, // Closes on `targetCoord`, bites and backs off
    RAT_MODE_STAGGER = 2, // Knocked back from the player by a stagger reaction
    RAT_MODE_BUILDUP = 3, // Held down for as long as the build-up reaction lasts
    RAT_MODE_HURT    = 4, // Flinches after a hit it survived
    RAT_MODE_DEAD    = 5, // No behaviour: the task's death state has taken over
};

/// Animations of a rat, kept in `RatWork::animId`: indices into the package's
/// animation-set and blend tables, named for when each is played. Entry 0 of
/// the set table is empty.
enum {
    RAT_ANIM_IDLE            = 1,  // Standing still
    RAT_ANIM_RUN             = 2,  // The fast idle move, and the approach to the target
    RAT_ANIM_BACK_OFF        = 3,  // Retreat after a bite
    RAT_ANIM_ATTACK          = 4,  // The bite
    RAT_ANIM_HURT            = 5,  // Flinch
    RAT_ANIM_COLLAPSE        = 6,  // Going down: the start of a build-up hold, and the death
    RAT_ANIM_WALK            = 7,  // The slow idle move
    RAT_ANIM_BUILDUP_HOLD    = 8,  // Held down by the build-up reaction
    RAT_ANIM_STAGGER_RECOVER = 9,  // Getting back up after a stagger
    RAT_ANIM_STAGGER         = 10, // Knocked back
};

/// Work block of a rat: the zeroed allocation its spawn handler makes and
/// keeps at `Task::work` until the enemy is destroyed.
///
/// It opens with the playback storage of the model's seven parts and the two
/// matrices the model is lit with, then the four collision spheres the spawn
/// links, each followed by its own contact table, and ends with the state the
/// per-frame handlers share. Angles are 4096 to a turn, speeds are world
/// units a frame and timers count frames.
typedef struct {
    ActorAnimRig7         rig;               // Playback storage of the model's seven parts; slots 1 to 6 are driven
    MATRIX                colorMtx;          // Colour matrix the model is lit with
    MATRIX                lightMtx;          // Light matrix the model is lit with
    WorldCollisionBody    sensorBody;        // Keyless sphere ahead of the root; pairs while the rat idles, to notice a player body
    WorldCollisionContact sensorContacts[1]; // The one contact of `sensorBody`; a player-body contact there sets `targetCoord` and `attackRequested`
    WorldCollisionBody    hitBody;           // Sphere on the model's fifth coordinate that takes hits and is pushed off other bodies
    WorldCollisionContact hitContacts[3];    // Contacts of `hitBody`; also the enemy's hit records
    WorldCollisionBody    gridBody;          // Sphere above the root tested against the room's grid and floor
    WorldCollisionContact gridContacts[4];   // Contacts of `gridBody`; their averaged push-back is applied to the root each frame
    WorldCollisionBody    attackBody;        // Sphere ahead of the root that deals the bite; pairs only on the bite's hit frames
    WorldCollisionContact attackContacts[1]; // The one contact of `attackBody`; a contact there ends the bite's hit frames
    EffectSpawnArg        hitEffectArg;      // Argument of the effect a hit spawns, hung off the model's root coordinate
    GfxCoord*             targetCoord;       // Borrowed root coordinate of the player actor being attacked; NULL until the sensor or the attack picks one
    MATRIX                savedRootMtx;      // Root matrix when the death began; each squash frame rescales a copy of it
    VECTOR                prevPos;           // Root position before the frame's step, restored when the grid contacts oppose each other
    SVECTOR               staggerDir;        // Unit vector (4096 = 1) from the rat to the player when the stagger began; the knock-back moves against it
    s16                   hitCooldown;       // Frames before another hit is taken; set from the hit's id parameter 2
    s16                   mode;              // Top-level behaviour, a `RAT_MODE_` value
    s16                   step;              // Step within the running mode, or within the death, counted from 0
    s16                   animId;            // Animation requested, a `RAT_ANIM_` value
    s16                   appliedAnimId;     // Animation the slots were last seeded with; a different `animId` reseeds them, and a handler resets this to restart its animation
    s16                   animFrame;         // Frames since the slots were last reseeded
    s16                   forwardSpeed;      // Speed along the facing; negative backs away
    s16                   turnRate;          // Angle the root turns toward `targetYaw` each frame; 0 holds the heading
    s16                   yaw;               // Current heading: each turn reads it back from the root's rotation and steps it toward `targetYaw`
    u16                   targetYaw;         // Heading the root turns toward, 0..0xFFF
    s16                   timer;             // Frame count of the running step: counts up between idle move rolls and through the death, down otherwise
    s16                   wanderTimer;       // Frames before the idle mode picks its next random heading
    s16                   squashScale;       // Dying: vertical scale of the root, 0x1000 shrinking to 0x200
    s16                   idleSoundTimer;    // Frames before the idle mode's next sound
    s16                   attackRequested;   // Set by a player-body contact on `sensorBody` and when a stagger, build-up or flinch ends; the idle mode then attacks
    s16                   knockedDown;       // Set from the start of a stagger or build-up until it ends; a build-up entered with it set skips the collapse
    s16                   buildupHeld;       // Set when the build-up reaction interrupts another mode, until the hold ends; a survived hit then causes no flinch
} RatWork;
STATIC_ASSERT_SIZEOF(RatWork, 0x39C);

void ratSpawn(Enemy* ctx, Task* actor);
void ratIdle(Task* arg0);
void ratAttack(Task* arg0);
void ratStagger(Task* arg0);
void ratBuildup(Task* arg0);
void ratTurn(Task* arg0);
void ratDeath(Enemy* arg0, Task* arg1);
void ratTask(Task* arg0);
void ratUpdate(Enemy* arg0, Task* arg1);
void ratReactions(Task* arg0);
void ratIdleSound(Task* arg0);
void ratHurt(Task* arg0);
void ratStep(Task* arg0);
void ratAnimate(Task* arg0);
void ratUpdateColor(Task* arg0);
void ratShadow(Task* arg0);
void ratSquash(Task* arg0);

/* Defined by each package. */
void ratContacts(Task* actor);
void ratBehavior(Task* arg0);

#endif /* SRC_SHARED_RAT_H */
