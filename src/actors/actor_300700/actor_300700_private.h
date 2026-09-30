#ifndef SRC_ACTORS_ACTOR_300700_ACTOR_300700_PRIVATE_H
#define SRC_ACTORS_ACTOR_300700_ACTOR_300700_PRIVATE_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/animation.h"
#include "gameplay/collision.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/geometry.h"
#include "gameplay/pairsrc.h"

#include "main/coord.h"
#include "main/session_types.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

/// The 0x50-byte block at 0x22C: a `MATRIX` copied into the coordinate, or
/// three contact `WorldCollisionContact`s after an 8-byte header.
typedef union Actor300700ContactStorage {
    MATRIX matrix;
    struct {
        byte                  pad_0[8];
        WorldCollisionContact recs[3];
    } contacts;
    struct {
        /* 0x00 */ byte   pad_0[0x20];
        /* 0x20 */ MATRIX rotation;
    } quad;
} Actor300700ContactStorage;

typedef struct Actor300700Work {
    /* 0x000 */ AnimationContext          anim;
    /* 0x014 */ byte                      pad_14[0x140];
    /* 0x154 */ WorldCollisionContact     field_154;
    /* 0x16C */ byte                      pad_16C[0x20];
    /* 0x18C */ WorldCollisionContact     field_18C;
    /* 0x1A4 */ byte                      pad_1A4[0x56];
    /* 0x1FA */ u16                       field_1FA;
    /* 0x1FC */ WorldCollisionContact     sensorContacts[1]; // Single result for the player sensor
    /* 0x214 */ byte                      pad_214[0x10];
    /* 0x224 */ EffectSpawnArg            field_224;
    /* 0x22C */ Actor300700ContactStorage field_22C;
    /* 0x27C */ byte                      field_27C[0x30];
    /* 0x2AC */ s32                       field_2AC;
    /* 0x2B0 */ s32                       field_2B0;
    /* 0x2B4 */ s32                       field_2B4;
    /* 0x2B8 */ byte                      pad_2B8[4];
    /* 0x2BC */ s32                       field_2BC;
    /* 0x2C0 */ s32                       field_2C0;
    /* 0x2C4 */ s32                       field_2C4;
    /* 0x2C8 */ byte                      pad_2C8[0xC];
    /* 0x2D4 */ s16                       field_2D4;
    /* 0x2D6 */ s16                       field_2D6;
    /* 0x2D8 */ s16                       field_2D8;
    /* 0x2DA */ s16                       field_2DA;
    /* 0x2DC */ s16                       field_2DC;
    /* 0x2DE */ s16                       field_2DE;
    /* 0x2E0 */ s16                       field_2E0;
    /* 0x2E2 */ s16                       field_2E2;
    /* 0x2E4 */ s16                       field_2E4;
    /* 0x2E6 */ s16                       field_2E6;
    /* 0x2E8 */ byte                      pad_2E8[0x32];
    /* 0x31A */ u16                       field_31A;
    /* 0x31C */ WorldCollisionContact     attackContacts[1]; // Single result for the paired attack body
    /* 0x334 */ EffectSpawnArg            field_334;         // record the hit's effect is spawned with
    /* 0x33C */ GfxCoord*                 field_33C;
    /* 0x340 */ MATRIX                    field_340;
    /* 0x360 */ s32                       field_360;
    /* 0x364 */ s32                       field_364;
    /* 0x368 */ s32                       field_368;
    /* 0x36C */ byte                      pad_36C[4];
    /* 0x370 */ SVECTOR                   field_370;
    /* 0x378 */ s16                       field_378;
    /* 0x37A */ s16                       field_37A;
    /* 0x37C */ s16                       field_37C;
    /* 0x37E */ u16                       field_37E;
    /* 0x380 */ s16                       field_380;
    /* 0x382 */ u16                       field_382;
    /* 0x384 */ s16                       field_384;
    /* 0x386 */ s16                       field_386;
    /* 0x388 */ s16                       field_388;
    /* 0x38A */ u16                       field_38A;
    /* 0x38C */ u16                       field_38C;
    /* 0x38E */ u16                       field_38E;
    /* 0x390 */ s16                       field_390;
    /* 0x392 */ u16                       field_392;
    /* 0x394 */ s16                       field_394;
    /* 0x396 */ s16                       field_396;
    /* 0x398 */ s16                       field_398;
} Actor300700Work;

extern TmdSource D_actor_300700_80167400;

extern DamageAttack D_actor_300700_80169328;

extern GpPairSrcE D_actor_300700_8016932C;

extern u32 D_actor_300700_801693B8;

/// Second variant's spawn: allocates its 0x39C-byte work block, binds the two
/// pose matrices into the TMD object, then hangs the four render nodes on
/// their global lists with the record tables `Gp_InitRec18Table` zeroes.
void func_actor_300700_80163510(GpEnemy* arg0, Task* arg1);

#endif // SRC_ACTORS_ACTOR_300700_ACTOR_300700_PRIVATE_H
