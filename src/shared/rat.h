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

#include "main/task_types.h"

#include "gameplay/actor.h"
#include "gameplay/animation.h"
#include "gameplay/effects.h"

typedef struct RatWork {
    /* 0x000 */ AnimationContext      anim;
    /* 0x014 */ byte                  pad_14[0x140];
    /* 0x154 */ WorldCollisionContact field_154;
    /* 0x16C */ byte                  pad_16C[0x20];
    /* 0x18C */ WorldCollisionContact field_18C;
    /* 0x1A4 */ byte                  pad_1A4[0x38];
    /* 0x1DC */ byte                  field_1DC[0x1E];
    /* 0x1FA */ u16                   field_1FA;
    /* 0x1FC */ WorldCollisionContact sensorContacts[1]; // Single result for the player sensor
    /* 0x214 */ byte                  field_214[0x10];
    /* 0x224 */ EffectSpawnArg        field_224;
    /* 0x22C */ byte                  pad_22C[8];
    /* 0x234 */ WorldCollisionContact hitContacts[3]; // Hits and obstacle overlaps of the body sphere, also published as `Enemy::recs`
    /* 0x27C */ byte                  field_27C[0x20];
    /* 0x29C */ byte                  pad_29C[0x10];
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
    /* 0x2E8 */ byte                  pad_2E8[0x14];
    /* 0x2FC */ byte                  field_2FC[0x1E];
    /* 0x31A */ u16                   field_31A;
    /* 0x31C */ WorldCollisionContact attackContacts[1]; // Single result for the paired attack body
    /* 0x334 */ EffectSpawnArg        field_334;         // record the hit's effect is spawned with
    /* 0x33C */ GfxCoord*             field_33C;
    /* 0x340 */ MATRIX                field_340;
    /* 0x360 */ s32                   field_360;
    /* 0x364 */ s32                   field_364;
    /* 0x368 */ s32                   field_368;
    /* 0x36C */ byte                  pad_36C[4];
    /* 0x370 */ SVECTOR               field_370;
    /* 0x378 */ s16                   field_378;
    /* 0x37A */ s16                   field_37A;
    /* 0x37C */ s16                   field_37C;
    /* 0x37E */ u16                   field_37E;
    /* 0x380 */ s16                   field_380;
    /* 0x382 */ u16                   field_382;
    /* 0x384 */ s16                   field_384;
    /* 0x386 */ s16                   field_386;
    /* 0x388 */ s16                   field_388;
    /* 0x38A */ u16                   field_38A;
    /* 0x38C */ u16                   field_38C;
    /* 0x38E */ u16                   field_38E;
    /* 0x390 */ s16                   field_390;
    /* 0x392 */ u16                   field_392;
    /* 0x394 */ s16                   field_394;
    /* 0x396 */ s16                   field_396;
    /* 0x398 */ s16                   field_398;
} RatWork;

/// The 0x39C-byte block `ratSpawn` allocates. Larger than
/// the Moth's `MothWork` and laid out differently: the pose buffer
/// `animationInitContext` fills sits at +0x12C instead of +0xB4, and the four
/// `WorldCollisionBody` collision bodies it links (object-list indices 3/2/2/3, each with its
/// own `WorldCollisionContact` table) start at +0x1DC rather than +0x134.
typedef struct RatInitWork {
    /* 0x000 */ AnimationContext      anim;
    /* 0x014 */ AnimationSlot         slots[7];
    /* 0x12C */ byte                  field_12C[0x70];
    /* 0x19C */ MATRIX                field_19C;
    /* 0x1BC */ MATRIX                field_1BC;
    /* 0x1DC */ WorldCollisionBody    obj1;
    /* 0x1FC */ WorldCollisionContact rec1;
    /* 0x214 */ WorldCollisionBody    obj2;
    /* 0x234 */ WorldCollisionContact rec2[3];
    /* 0x27C */ WorldCollisionBody    obj3;
    /* 0x29C */ WorldCollisionContact rec3[4];
    /* 0x2FC */ WorldCollisionBody    obj4;
    /* 0x31C */ WorldCollisionContact rec4;
    /* 0x334 */ GfxCoord*             field_334;
    /* 0x338 */ u16                   field_338;
    /* 0x33A */ u16                   field_33A;
    /* 0x33C */ byte                  pad_33C[0x42];
    /* 0x37E */ u16                   field_37E;
    /* 0x380 */ s16                   field_380;
    /* 0x382 */ byte                  pad_382[0x1A];
} RatInitWork;
STATIC_ASSERT_SIZEOF(RatInitWork, 0x39C);

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
