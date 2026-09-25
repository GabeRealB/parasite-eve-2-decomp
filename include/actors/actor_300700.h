#ifndef ACTOR_300700_H
#define ACTOR_300700_H

#include "common.h"
#include "actors/actor.h"
#include "gameplay/areaplace.h"
#include "gameplay/3FB8.h"
#include "gameplay/1BC.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

/// The first collision record of the work block, viewed both as a plain
/// `GpRec18` and as the raw id pair the tick handler reads back out of it.
typedef union Actor300700HitRecord {
    GpRec18 rec;
    struct {
        u32       header;
        GpFixed16 id;
    } hit;
} Actor300700HitRecord;

/// The 0x50-byte block at 0x22C: a `MATRIX` copied into the coordinate, or
/// three contact `GpRec18`s after an 8-byte header.
typedef union Actor300700ContactStorage {
    MATRIX matrix;
    struct {
        byte    pad_0[8];
        GpRec18 recs[3];
    } contacts;
    struct {
        /* 0x00 */ byte   pad_0[0x20];
        /* 0x20 */ MATRIX rotation;
    } quad;
} Actor300700ContactStorage;

typedef struct Actor300700Work {
    /* 0x000 */ byte                      pad_0[0x154];
    /* 0x154 */ Actor300700HitRecord      field_154;
    /* 0x16C */ byte                      pad_16C[0x20];
    /* 0x18C */ GpRec18                   field_18C;
    /* 0x1A4 */ byte                      pad_1A4[0x56];
    /* 0x1FA */ u16                       field_1FA;
    /* 0x1FC */ byte                      field_1FC[0x18];
    /* 0x214 */ byte                      pad_214[0x10];
    /* 0x224 */ GpEffArg                  field_224;
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
    /* 0x31C */ byte                      pad_31C[0x18];
    /* 0x334 */ GpEffArg                  field_334; // record the hit's effect is spawned with
    /* 0x33C */ GsCOORDINATE2*            field_33C;
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

void func_actor_300700_80164D3C(GpEnemy* arg0, Task* arg1);

/// The second variant's state handlers: spawn, per-frame update and the
/// handler for state 2.
extern const GpEnemyTaskFuncTable3 D_actor_300700_80161E30;

#endif
