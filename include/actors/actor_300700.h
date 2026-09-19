#ifndef ACTOR_300700_H
#define ACTOR_300700_H

#include "common.h"
#include "gameplay/3FB8.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

typedef struct Actor300700Obj2C {
    /* 0x00 */ byte           pad_0[8];
    /* 0x08 */ GsCOORDINATE2* field_8;
    /* 0x0C */ s16            field_C;
    /* 0x0E */ byte           pad_E[0x16];
    /* 0x24 */ s8             field_24;
    /* 0x25 */ s8             field_25;
} Actor300700Obj2C;

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

/// Yaw scratch on the scratchpad: the `SVECTOR` handed to `RotMatrix` next to
/// the `VECTOR` slot keeping the block at the size both allocations expect.
typedef struct Actor300700RotScratch {
    /* 0x00 */ VECTOR  vec;
    /* 0x10 */ SVECTOR rot;
} Actor300700RotScratch;
STATIC_ASSERT_SIZEOF(Actor300700RotScratch, 0x18);

/// Four rotated corners and the projected center/depth on the scratchpad.
typedef struct Actor300700QuadScratch {
    /* 0x00 */ SVECTOR v[4];
    /* 0x20 */ s32     sxy;
    /* 0x24 */ s32     otz;
} Actor300700QuadScratch;
STATIC_ASSERT_SIZEOF(Actor300700QuadScratch, 0x28);

typedef struct Actor300700TexEntry {
    /* 0x0 */ u8 u;
    /* 0x1 */ u8 pad_1;
    /* 0x2 */ u8 v;
    /* 0x3 */ u8 pad_3;
} Actor300700TexEntry;
STATIC_ASSERT_SIZEOF(Actor300700TexEntry, 4);

extern Actor300700TexEntry D_actor_300700_80165B9C[];

/// Spawn parameter block reached through `Actor300700Ctx.field_3C`; `field_A`
/// is the halfword `func_actor_300700_80161E80` copies into
/// `Actor300700SpawnWork.field_2DC`.
typedef struct Actor300700Params {
    /* 0x0 */ byte pad_0[0xA];
    /* 0xA */ u16  field_A;
    /* 0xC */ byte pad_C[3];
    /* 0xF */ u8   field_F;
} Actor300700Params;

/// Damage record the actor pushes hits into. Same object family as
/// `GpObj5C`: `field_4C` carries the generic hit-flag bits, `field_40` the
/// remaining hit points and `field_10` the anchor `func_800DA6E8` binds a
/// damage slot to.
typedef struct Actor300700Ctx {
    /* 0x00 */ byte               pad_0[8];
    /* 0x08 */ u16                field_8;
    /* 0x0A */ byte               pad_A[6];
    /* 0x10 */ byte               field_10[4];
    /* 0x14 */ u8                 field_14;
    /* 0x15 */ byte               pad_15[0x27];
    /* 0x3C */ Actor300700Params* field_3C;
    /* 0x40 */ u16                field_40;
    /* 0x42 */ byte               pad_42[0xA];
    /* 0x4C */ u8                 field_4C;
} Actor300700Ctx;

typedef struct Actor300700 {
    /* 0x00 */ byte              pad_0[0x1C];
    /* 0x1C */ Actor300700Work*  field_1C;
    /* 0x20 */ Actor300700Ctx*   field_20;
    /* 0x24 */ byte              pad_24[8];
    /* 0x2C */ Actor300700Obj2C* field_2C;
    /* 0x30 */ s32               field_30;
} Actor300700;

/// Per-state animation id handed to `func_800B4114`, indexed by `field_37E`.
extern s16 D_actor_300700_801693E4[];

void func_actor_300700_80164D3C(Actor300700Ctx* arg0, Actor300700* arg1);

#endif
