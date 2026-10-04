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

/// One row of that enemy's clip tables: a nonzero `field_0` ends the clip and
/// `field_2` is the row's scale.
typedef struct GeneratorClip {
    s16 field_0;
    u16 field_2;
} GeneratorClip;
STATIC_ASSERT_SIZEOF(GeneratorClip, 0x4);

/// Part object that enemy's spawn allocates and keeps at the part task's
/// `Task::work`: a linked collision node with its single record, and the
/// record the part's death effect is spawned with.
typedef struct GeneratorPart {
    WorldCollisionBody    obj;
    WorldCollisionContact rec18[1];
    EffectSpawnArg        field_38; // record this part's death effect is spawned with
    s16                   field_40;
    u16                   field_42;
    s16                   field_44;
    s16                   field_46;
} GeneratorPart;
STATIC_ASSERT_SIZEOF(GeneratorPart, 0x48);

/// Scratch-pad block of that enemy's hit handler: the offset from the player
/// and the offset the hit effect is spawned at.
typedef struct GeneratorScratch {
    VECTOR  delta;
    SVECTOR ofs;
} GeneratorScratch;
STATIC_ASSERT_SIZEOF(GeneratorScratch, 0x18);

/// A spawn position of that enemy, one per sub-state.
typedef struct GeneratorSpawnPos {
    s16 x;
    s16 y;
    s16 z;
} GeneratorSpawnPos;
STATIC_ASSERT_SIZEOF(GeneratorSpawnPos, 0x6);

/// One row of that enemy's per-area sound table: the two parameters its
/// sound event is queued with.
typedef struct GeneratorSndRow {
    s8 field_0;
    s8 pad_1;
    s8 field_2;
    s8 pad_3;
} GeneratorSndRow;
STATIC_ASSERT_SIZEOF(GeneratorSndRow, 0x4);

void generatorSpawn(Enemy* arg0, Task* arg1);
void generatorBodyHit(Task* arg0);
void generatorPulse(Task* arg0);
void generatorDeathState(Enemy* arg0, Task* arg1);
void generatorLifeSupportSpawn(Enemy* arg0, Task* arg1);
void generatorLifeSupportHit(Enemy* arg0, Task* arg1);
void generatorTickState(Enemy* arg0, Task* arg1);
void generatorRegenerate(Task* arg0);
void generatorTickPose(Task* arg0);
void generatorLifeSupportTeardown(Enemy* arg0, Task* arg1);
s32  generatorSetReleaseBits(Task* task, s32 msgId, ActorCommand* msg, s32 arg3);

static inline void generatorTickPoseInline(Task* task);

void generatorUpdateColor(Task* arg0);
void generatorLifeSupportTask(Task* arg0);
s32  generatorIsAlive(Task* arg0, s32 msgId, s32 arg2, s32 arg3);
void generatorTask(Task* arg0);

#endif /* SRC_SHARED_GENERATOR_H */
