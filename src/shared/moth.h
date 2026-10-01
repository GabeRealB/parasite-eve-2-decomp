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

#include "main/task_types.h"

#include "gameplay/actor.h"
#include "gameplay/animation.h"
#include "gameplay/effects.h"

/// The 0x50-byte block at 0x22C: a `MATRIX` copied into the coordinate, or
/// three contact `WorldCollisionContact`s after an 8-byte header.
typedef union MothContactStorage {
    MATRIX matrix;
    struct {
        byte                  pad_0[8];
        WorldCollisionContact recs[3];
    } contacts;
    struct {
        /* 0x00 */ byte   pad_0[0x20];
        /* 0x20 */ MATRIX rotation;
    } quad;
} MothContactStorage;

typedef struct MothWork {
    /* 0x000 */ AnimationContext      anim;
    /* 0x014 */ byte                  pad_14[0x140];
    /* 0x154 */ WorldCollisionContact field_154;
    /* 0x16C */ byte                  pad_16C[0x20];
    /* 0x18C */ WorldCollisionContact field_18C;
    /* 0x1A4 */ byte                  pad_1A4[0x56];
    /* 0x1FA */ u16                   field_1FA;
    /* 0x1FC */ WorldCollisionContact sensorContacts[1]; // Single result for the player sensor
    /* 0x214 */ byte                  pad_214[0x10];
    /* 0x224 */ EffectSpawnArg        field_224;
    /* 0x22C */ MothContactStorage    field_22C;
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
    /* 0x2E8 */ byte                  pad_2E8[0x32];
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
} MothWork;

/// The 0x2F4-byte allocation `mothSpawn` makes with
/// `memCalloc` and stores in the task's work slot, then fills with the three
/// `WorldCollisionBody` collision bodies (object-list indices 2/2/3) and their `WorldCollisionContact`
/// tables. `MothWork` is the wider view the tick handlers use of the
/// same object.
typedef struct MothSpawnWork {
    /* 0x000 */ AnimationContext      anim;
    /* 0x014 */ AnimationSlot         slots[4];
    /* 0x0B4 */ byte                  field_B4[0x40]; // pose buffer, `func_800B3F84` arg3
    /* 0x0F4 */ MATRIX                field_F4;       // color matrix handed to the stream
    /* 0x114 */ MATRIX                field_114;      // light matrix handed to the stream
    /* 0x134 */ WorldCollisionBody    obj134;
    /* 0x154 */ WorldCollisionContact rec154;
    /* 0x16C */ WorldCollisionBody    obj16C;
    /* 0x18C */ WorldCollisionContact rec18C[4];
    /* 0x1EC */ WorldCollisionBody    obj1EC;
    /* 0x20C */ WorldCollisionContact rec20C;
    /* 0x224 */ void*                 field_224;
    /* 0x228 */ u16                   field_228;
    /* 0x22A */ u16                   field_22A;
    /* 0x22C */ byte                  pad_22C[0x80];
    /* 0x2AC */ s32                   field_2AC;
    /* 0x2B0 */ s32                   field_2B0;
    /* 0x2B4 */ s32                   field_2B4;
    /* 0x2B8 */ byte                  pad_2B8[0x1E];
    /* 0x2D6 */ u16                   field_2D6;
    /* 0x2D8 */ byte                  pad_2D8[4];
    /* 0x2DC */ u16                   field_2DC;
    /* 0x2DE */ byte                  pad_2DE[0x16];
} MothSpawnWork;
STATIC_ASSERT_SIZEOF(MothSpawnWork, 0x2F4);

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
