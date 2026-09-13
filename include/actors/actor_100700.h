#ifndef ACTOR_100700_H
#define ACTOR_100700_H

#include "common.h"
#include "gameplay/3FB8.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

typedef struct Actor00700Obj2C {
    /* 0x00 */ byte           pad_0[8];
    /* 0x08 */ GsCOORDINATE2* field_8;
    /* 0x0C */ s16            field_C;
} Actor00700Obj2C;

/// Overlay-local view of the spawn parameter block. `field_F` indexes
/// `Actor00700_D06E50` for the state-2 random roll in `Actor00700_Fn00BC0`.
typedef struct Actor00700Params {
    /* 0x0 */ byte pad_0[0xF];
    /* 0xF */ u8   field_F;
} Actor00700Params;

typedef union Actor00700HitRecord {
    GpRec18 rec;
    struct {
        u32       header;
        GpFixed16 id;
    } hit;
} Actor00700HitRecord;

typedef union Actor00700ContactStorage {
    MATRIX matrix;
    struct {
        byte    pad_0[8];
        GpRec18 recs[3];
    } contacts;
} Actor00700ContactStorage;
STATIC_ASSERT_SIZEOF(Actor00700ContactStorage, 0x50);

typedef struct Actor00700Work {
    /* 0x000 */ byte                     pad_0[0x154];
    /* 0x154 */ Actor00700HitRecord      field_154;
    /* 0x16C */ byte                     pad_16C[0x20];
    /* 0x18C */ GpRec18                  field_18C;
    /* 0x1A4 */ byte                     pad_1A4[0x38];
    /* 0x1DC */ byte                     field_1DC[0x1E];
    /* 0x1FA */ u16                      field_1FA;
    /* 0x1FC */ byte                     field_1FC[0x18];
    /* 0x214 */ byte                     field_214[0x10];
    /* 0x224 */ GpEffArg                 field_224;
    /* 0x22C */ Actor00700ContactStorage field_22C;
    /* 0x27C */ byte                     field_27C[0x40];
    /* 0x2BC */ s32                      field_2BC;
    /* 0x2C0 */ s32                      field_2C0;
    /* 0x2C4 */ s32                      field_2C4;
    /* 0x2C8 */ byte                     pad_2C8[0x12];
    /* 0x2DA */ s16                      field_2DA;
    /* 0x2DC */ s16                      field_2DC;
    /* 0x2DE */ byte                     pad_2DE[4];
    /* 0x2E2 */ s16                      field_2E2;
    /* 0x2E4 */ byte                     pad_2E4[2];
    /* 0x2E6 */ s16                      field_2E6;
    /* 0x2E8 */ byte                     pad_2E8[0x14];
    /* 0x2FC */ byte                     field_2FC[0x1E];
    /* 0x31A */ u16                      field_31A;
    /* 0x31C */ byte                     pad_31C[0x20];
    /* 0x33C */ GsCOORDINATE2*           field_33C;
    /* 0x340 */ MATRIX                   field_340;
    /* 0x360 */ s32                      field_360;
    /* 0x364 */ s32                      field_364;
    /* 0x368 */ s32                      field_368;
    /* 0x36C */ byte                     pad_36C[4];
    /* 0x370 */ SVECTOR                  field_370;
    /* 0x378 */ s16                      field_378;
    /* 0x37A */ s16                      field_37A;
    /* 0x37C */ s16                      field_37C;
    /* 0x37E */ u16                      field_37E;
    /* 0x380 */ s16                      field_380;
    /* 0x382 */ u16                      field_382;
    /* 0x384 */ s16                      field_384;
    /* 0x386 */ s16                      field_386;
    /* 0x388 */ s16                      field_388;
    /* 0x38A */ u16                      field_38A;
    /* 0x38C */ u16                      field_38C;
    /* 0x38E */ u16                      field_38E;
    /* 0x390 */ s16                      field_390;
    /* 0x392 */ u16                      field_392;
    /* 0x394 */ s16                      field_394;
    /* 0x396 */ s16                      field_396;
    /* 0x398 */ s16                      field_398;
} Actor00700Work;

/// `Gp_UnlinkNode` list entry at +0x10 of `Actor00700Ctx`. `field_4` is the
/// flag byte previously named `field_14` (`sb` at 0x14).
typedef struct Actor00700Node {
    /* 0x0 */ struct Actor00700Node* next;
    /* 0x4 */ u8                     field_4;
    /* 0x5 */ byte                   pad_5[3];
} Actor00700Node;

typedef struct Actor00700Ctx {
    /* 0x00 */ byte              pad_0[0x8];
    /* 0x08 */ u16               field_8;
    /* 0x0A */ byte              pad_A[6];
    /* 0x10 */ Actor00700Node    node;
    /* 0x18 */ byte              pad_18[0x24];
    /* 0x3C */ Actor00700Params* field_3C;
    /* 0x40 */ s16               field_40;
    /* 0x42 */ byte              pad_42[0xA];
    /* 0x4C */ u8                field_4C;
    /* 0x4D */ byte              pad_4D[7];
    /* 0x54 */ s32               field_54;
} Actor00700Ctx;

typedef struct Actor00700 {
    /* 0x00 */ byte             pad_0[0x1C];
    /* 0x1C */ Actor00700Work*  field_1C;
    /* 0x20 */ Actor00700Ctx*   field_20;
    /* 0x24 */ byte             pad_24[8];
    /* 0x2C */ Actor00700Obj2C* field_2C;
    /* 0x30 */ s32              field_30;
} Actor00700;

typedef struct Actor00700StateFuncTable3 {
    void (*funcs[3])(Actor00700Ctx*, Actor00700*);
} Actor00700StateFuncTable3;

extern Actor00700StateFuncTable3 Actor00700_D00004;
extern Actor00700StateFuncTable3 Actor00700_D00054;

/// 0x18-byte frame this overlay allocates on the scratchpad stack; only the
/// `SVECTOR` at +0x10 is used by `Actor00700_Fn012E4`; the vector holds
/// the player displacement in `Actor00700_Fn02820`.
typedef struct Actor00700RotScratch {
    /* 0x00 */ VECTOR  vec;
    /* 0x10 */ SVECTOR rot;
} Actor00700RotScratch;
STATIC_ASSERT_SIZEOF(Actor00700RotScratch, 0x18);

extern s16 Actor00700_D06DF0[];
extern u16 Actor00700_D06E00[];
extern s16 Actor00700_D06E20[];
extern u16 Actor00700_D06E30[];
extern s16 Actor00700_D06E50[];
extern s16 Actor00700_D06E98[];

void Actor00700_Fn008B4(Actor00700* arg0);
void Actor00700_Fn00BC0(Actor00700* arg0);
void Actor00700_Fn012E4(Actor00700* arg0);
void Actor00700_Fn01434(Actor00700Ctx* arg0, Actor00700* arg1);
void Actor00700_Fn01830(Actor00700* arg0);
void Actor00700_Fn0188C(Actor00700Ctx* arg0, Actor00700* arg1);
void Actor00700_Fn01AB8(Actor00700* arg0);
void Actor00700_Fn02290(Actor00700Ctx* arg0, Actor00700* arg1);

#endif
