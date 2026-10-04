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

/// The moth's work block: the 0x2F4 bytes `mothSpawn` allocates with
/// `memCalloc` and stores in the task's work slot. It opens with the animation
/// context, its four slots and pose buffer, then the two matrices handed to
/// the model stream and the three `WorldCollisionBody` collision bodies
/// (object-list indices 2/2/3) with their `WorldCollisionContact` tables.
/// The tick handlers' state follows from 0x224.
typedef struct MothWork {
    /* 0x000 */ ActorAnimRig4         rig;       // playback storage of the model's parts; slots 1 to 3 are seeded
    /* 0x0F4 */ MATRIX                field_F4;  // color matrix handed to the stream
    /* 0x114 */ MATRIX                field_114; // light matrix handed to the stream
    /* 0x134 */ WorldCollisionBody    obj134;
    /* 0x154 */ WorldCollisionContact field_154;
    /* 0x16C */ WorldCollisionBody    obj16C;
    /* 0x18C */ WorldCollisionContact rec18C[4];
    /* 0x1EC */ WorldCollisionBody    obj1EC;
    /* 0x20C */ WorldCollisionContact rec20C;
    /* 0x224 */ EffectSpawnArg        field_224;
    /* 0x22C */ MATRIX                savedRootMtx; // root transform when the death began, sunk 0x18 a frame; each squash frame rescales a copy of it
    /* 0x24C */ MATRIX                burstRollMtx; // rotation of the death burst sprite's quad: a random roll about Z picked on the death's first frame
    /* 0x26C */ byte                  field_26C[0x10];
    /* 0x27C */ byte                  field_27C[0x30];
    /* 0x2AC */ s32                   field_2AC;
    /* 0x2B0 */ s32                   field_2B0;
    /* 0x2B4 */ s32                   field_2B4;
    /* 0x2B8 */ byte                  pad_2B8[4];
    /* 0x2BC */ s32                   field_2BC;
    /* 0x2C0 */ s32                   field_2C0;
    /* 0x2C4 */ s32                   field_2C4;
    /* 0x2C8 */ byte                  pad_2C8[0xC];
    /* 0x2D4 */ s16                   field_2D4;
    /* 0x2D6 */ s16                   field_2D6;
    /* 0x2D8 */ s16                   field_2D8;
    /* 0x2DA */ s16                   field_2DA;
    /* 0x2DC */ s16                   field_2DC;
    /* 0x2DE */ s16                   field_2DE;
    /* 0x2E0 */ s16                   field_2E0;
    /* 0x2E2 */ s16                   field_2E2;
    /* 0x2E4 */ s16                   field_2E4;
    /* 0x2E6 */ s16                   field_2E6;
    /* 0x2E8 */ byte                  pad_2E8[0xC];
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
