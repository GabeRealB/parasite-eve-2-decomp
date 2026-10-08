/* The Beta Generator (actor_105300) and the Proto Generator (actor_105400) of
 * the Neo Ark power plant rooms: a stationary body with a separate Life
 * Support system part task. While the Life Support system lives, the body
 * takes a tenth of the damage, cannot drop below 1 HP and regenerates a point
 * every five frames. Destroying the Life Support system toggles the power plant room
 * state and a per-variant game flag (0x147/0x148). The body's scale pulses
 * from clip tables and flickers on hits. On death it shrinks to an eighth
 * while spewing effects, turns semi-transparent, and waits for message bits to
 * release scripted state before being destroyed.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_GENERATOR_H
#define SRC_SHARED_GENERATOR_H

#include "types.h"

#include "actors/actor.h"

#include "main/task_types.h"

/* Each package builds the library for one kind, selected before this header
 * is included; the kind also indexes the per-kind tables. */
#define GENERATOR_BETA  0
#define GENERATOR_PROTO 1
#ifndef GENERATOR_KIND
#error "define GENERATOR_KIND (GENERATOR_BETA or GENERATOR_PROTO) before including generator.h"
#endif
#define GENERATOR_COLLISION_KEY (0x30035 + GENERATOR_KIND)

extern TaskMessageEntry gGeneratorMessages[];

/// State indices of the body and Life Support task dispatchers.
enum {
    GENERATOR_TASK_SPAWN    = 0,
    GENERATOR_TASK_ACTIVE   = 1,
    GENERATOR_TASK_TEARDOWN = 2
};

/// Animation sets of the body, the values of `GeneratorWork::animSet`: indexes
/// into the package's animation-set table, whose entry 0 is empty.
enum {
    GENERATOR_ANIM_IDLE  = 1, // played from the spawn, and again once a hit reaction has run
    GENERATOR_ANIM_HIT   = 2, // reaction to a hit the body survives
    GENERATOR_ANIM_DEATH = 3  // played from the killing hit on
};

/// Values of `GeneratorWork::pulseState`.
enum {
    GENERATOR_PULSE_IDLE        = 0, // counting `pulseTimer` down, then playing the idle pulse clip
    GENERATOR_PULSE_HIT         = 1, // playing the hit pulse clip
    GENERATOR_PULSE_HIT_RECOVER = 2  // hit pulse over; waiting for the hit animation before returning to idle
};

/// Values of `GeneratorWork::deathState`, in the order a death passes through
/// them: wait, start, shrink, done.
enum {
    GENERATOR_DEATH_START  = 0, // drops the body's target node and collision bodies, then shrinks
    GENERATOR_DEATH_SHRINK = 1, // the 0x78-frame shrink, flickering and spewing effects
    GENERATOR_DEATH_DONE   = 2, // sequence over
    GENERATOR_DEATH_WAIT   = 3  // killed; spews effects until `GENERATOR_RELEASE_DEATH` arrives
};

/// Values of `GeneratorWork::battleExitState`.
enum {
    GENERATOR_BATTLE_EXIT_DUE  = 0, // leave the battle on the next death tick
    GENERATOR_BATTLE_EXIT_DONE = 1, // battle reference dropped and rewards credited
    GENERATOR_BATTLE_EXIT_HELD = 2  // killed; waits for `GENERATOR_RELEASE_BATTLE_EXIT`
};

/// Bits of `GeneratorWork::releaseBits`. An `ACTOR_COMMAND_MESSAGE_APPLY`
/// command carries the same values, so command 3 raises both.
enum {
    GENERATOR_RELEASE_DEATH       = 1, // lets a killed body leave `GENERATOR_DEATH_WAIT`
    GENERATOR_RELEASE_BATTLE_EXIT = 2  // lets it leave `GENERATOR_BATTLE_EXIT_HELD`
};

/// Work block of a Generator body.
///
/// The body's spawn handler allocates it zeroed and keeps it at `Task::work`;
/// the Life Support part task reaches it through its parent task. It holds the
/// animation context and its storage, the model's matrices, the two collision
/// spheres with their shared contact records, and the state the per-frame
/// handlers drive: the scale pulses while alive, and after the killing hit the
/// death sequence and the exit from the battle, each held until a message
/// releases it.
typedef struct {
    AnimationContext      anim;                                   // animation playback of the model
    AnimationSlot         slots[10];                              // one per model part; 1..9 play `animSet`, slot 0 is never started
    u8                    poses[10][ANIMATION_POSE_BUFFER_BYTES]; // blend pose of each slot
    MATRIX                colorMtx;                               // storage for the model's `TmdObject::colorMtx`
    MATRIX                lightMtx;                               // storage for the model's `TmdObject::lightMtx`
    WorldCollisionBody    rootBody;                               // sphere of radius 1500 at the model's root; the Life Support part copies its key
    WorldCollisionBody    targetBody;                             // sphere of radius 300 at the enemy's `Enemy::bodyPos`; shares `contacts`
    WorldCollisionContact contacts[2];                            // contacts of both bodies; also the enemy's hit records
    EffectSpawnArg        effectArg;                              // argument record of the effects its hits spawn, hung off the root
    MATRIX                unscaledMtx;                            // root matrix at the spawn, taken again when the death shrink starts; each frame rescales a copy of it
    s32                   runningSoundId;                         // sound started at the spawn: re-panned for the view each frame, stopped when the Life Support part is destroyed
    s16                   animSet;                                // requested animation, `GENERATOR_ANIM_*`; 0 until the first request
    s16                   appliedAnimSet;                         // animation last applied to the slots
    s16                   animFrames;                             // frames since `animSet` was applied
    s16                   shrinkScale;                            // Y scale of the death shrink, 0x1000 = 1.0; falls to 0x200
    s16                   stateFrames;                            // row of the pulse clip being played while alive; frames of the death wait and of the shrink afterwards
    s16                   pulseTimer;                             // frames until the next idle pulse or death flicker; the hit pulse clip's row during a death flicker
    s16                   pulseState;                             // `GENERATOR_PULSE_*`; the death shrink reuses IDLE and HIT for its flicker
    s16                   deathState;                             // `GENERATOR_DEATH_*`
    s16                   battleExitState;                        // `GENERATOR_BATTLE_EXIT_*`
    s16                   hitCooldown;                            // frames before another hit is taken; set from the hit's id parameter 2
    s16                   kind;                                   // `GENERATOR_KIND` of the package; indexes the per-kind tables
    s16                   lifeSupportDestroyed;                   // 1 once the Life Support part has been destroyed: full damage, no regeneration, and the body can die
    s16                   alive;                                  // 1 from the spawn until the killing hit; the answer to `ACTOR_MESSAGE_IS_PRESENT`
    s16                   releaseBits;                            // `GENERATOR_RELEASE_*` bits received by message
    s16                   hpCeiling;                              // hit points regeneration stops at: the kind's `EnemyParams::hpMax`
    s16                   regenTimer;                             // frames until the next regenerated hit point, five apart
} GeneratorWork;
STATIC_ASSERT_SIZEOF(GeneratorWork, 0x340);

/// One frame of a scale pulse clip of the body.
///
/// A clip is an array of these played a row per frame; the body's root matrix
/// is rescaled by each row in turn. The row flagged `last` is still applied.
typedef struct {
    s16 last;  // nonzero on the clip's final frame
    s16 scale; // uniform scale of the body for this frame, 0x1000 = 1.0
} GeneratorPulseFrame;
STATIC_ASSERT_SIZEOF(GeneratorPulseFrame, 0x4);

/// Work block of the Life Support part.
///
/// The part's spawn handler allocates it zeroed and keeps it at `Task::work`.
/// The part has no model of its own: it is a collision sphere on the task's
/// coordinate with its one contact record, the argument record of its hit
/// effects, and the timers of its hit and teardown handlers.
typedef struct {
    WorldCollisionBody    body;              // sphere of radius 200 at the part's coordinate; takes the key of the body's `GeneratorWork::rootBody`
    WorldCollisionContact contacts[1];       // contact of `body`; also the part enemy's hit records
    EffectSpawnArg        effectArg;         // argument record of the effects a hit the part survives spawns
    s16                   hitCooldown;       // frames before another hit is taken; set from the hit's id parameter 2
    s16                   teardownFrames;    // frames since the part was destroyed; its task ends at 0x3D
    s16                   hitEffectCooldown; // frames before a hit spawns its effects again, ten after each
    s16                   kind;              // `GeneratorWork::kind` of the body, taken at the spawn
} GeneratorLifeSupportWork;
STATIC_ASSERT_SIZEOF(GeneratorLifeSupportWork, 0x48);

/// Position of the Life Support part of one generator kind.
typedef struct {
    s16 x; // world X of the part's coordinate
    s16 y; // world Y
    s16 z; // world Z
} GeneratorLifeSupportPos;
STATIC_ASSERT_SIZEOF(GeneratorLifeSupportPos, 0x6);

/// Placement of the body's running sound in one view of the room.
///
/// The body never moves and the room's cameras are fixed, so the sound's
/// position is tabulated per view rather than projected: a table of these is
/// indexed by the session's 1-based view number.
typedef struct {
    s16 panOffset;   // stereo pan offset of the sound in this view; only the low byte is read
    s16 attenuation; // attenuation of the sound in this view; only the low byte is read
} GeneratorViewSound;
STATIC_ASSERT_SIZEOF(GeneratorViewSound, 0x4);

void        generatorDeathState(Enemy* arg0, Task* arg1);
static void _generatorLifeSupportSpawn(Enemy* enemy, Task* task);
static void _generatorLifeSupportTeardown(Enemy* enemy, Task* task);

#endif /* SRC_SHARED_GENERATOR_H */
