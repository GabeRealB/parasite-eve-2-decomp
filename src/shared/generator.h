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

typedef struct GeneratorMsgEntry {
    s32 id;
    union {
        s16 (*call0)(Task*);
        s32 (*call1)(Task*, s32, ActorCommand* request);
    } handler;
} GeneratorMsgEntry;

extern GeneratorMsgEntry gGeneratorMessages[];

/// Work block of the enemy whose code both actor_105300 and actor_105400
/// carry, kept at `Task::work`: the animation context with its slots and
/// poses, the collision nodes and records, and the state the per-frame
/// handlers drive.
typedef struct GeneratorWork {
    AnimationContext      anim;
    AnimationSlot         slots[10];
    AnimationPose         poses[10];
    MATRIX                field_244;
    MATRIX                field_264;
    WorldCollisionBody    node0;
    WorldCollisionBody    node1;
    WorldCollisionContact rec18[2];
    EffectSpawnArg        field_2F4;
    MATRIX                field_2FC;
    s32                   field_31C;
    u16                   field_320;
    s16                   field_322;
    u16                   field_324;
    u16                   field_326;
    u16                   field_328;
    u16                   field_32A;
    u16                   field_32C;
    u16                   field_32E;
    u16                   field_330;
    s16                   field_332;
    s16                   kind;
    s16                   field_336;
    s16                   field_338;
    s16                   field_33A;
    s16                   field_33C;
    s16                   field_33E;
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
s32  generatorSetReleaseBits(Task* task, s32 msgId, ActorCommand* msg);

static inline void generatorTickPoseInline(Task* task);

void generatorUpdateColor(Task* arg0);
void generatorLifeSupportTask(Task* arg0);
s16  generatorIsAlive(Task* arg0);
void generatorTask(Task* arg0);

#endif /* SRC_SHARED_GENERATOR_H */
