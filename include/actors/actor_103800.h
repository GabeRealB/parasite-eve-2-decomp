#ifndef ACTOR_103800_H
#define ACTOR_103800_H

#include "common.h"
#include "gameplay/1BC.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

typedef struct Actor103800Obj2C {
    /* 0x00 */ byte           pad_0[0x8];
    /* 0x08 */ GsCOORDINATE2* field_8;
    /* 0x0C */ u16            field_C;
    /* 0x0E */ byte           pad_E[0xE];
    /* 0x1C */ MATRIX*        field_1C;
    /* 0x20 */ MATRIX*        field_20;
} Actor103800Obj2C;

/// A `MATRIX` plus the word-wise view `Actor03800_Fn003B8` splats an identity
/// rotation through: five aligned stores rather than nine halfword ones (the
/// same idiom as `Actor342000MatWords`).
typedef union Actor103800MatWords {
    MATRIX mat;
    struct {
        /* 0x00 */ s32 m00_m01;
        /* 0x04 */ s32 m02_m10;
        /* 0x08 */ s32 m11_m12;
        /* 0x0C */ s32 m20_m21;
        /* 0x10 */ s16 m22;
    } ident;
} Actor103800MatWords;
STATIC_ASSERT_SIZEOF(Actor103800MatWords, 0x20);

typedef struct Actor103800Work {
    /* 0x000 */ GpAnimCtx  anim;
    /* 0x014 */ GpAnimSlot slots[6];
    /* 0x104 */ byte       field_104[0x60];
    /* 0x164 */ MATRIX     field_164;
    /* 0x184 */ MATRIX     field_184;
    /* 0x1A4 */ byte       field_1A4[8];
    /* 0x1AC */ void*      field_1AC;
    /* 0x1B0 */ GpRec18*   field_1B0;
    /* 0x1B4 */ u16        field_1B4;
    /* 0x1B6 */ s16        field_1B6;
    /* 0x1B8 */ u16        field_1B8;
    /* 0x1BA */ byte       pad_1BA[2];
    /* 0x1BC */ u32        field_1BC;
    /* 0x1C0 */ u16        field_1C0;
    /* 0x1C2 */ u16        field_1C2;
    /* 0x1C4 */ GpRec18    field_1C4[3];
    /* 0x20C */ byte       field_20C[8];
    /* 0x214 */ void*      field_214;
    /* 0x218 */ GpRec18*   field_218;
    /* 0x21C */ u16        field_21C;
    /* 0x21E */ s16        field_21E;
    /* 0x220 */ u16        field_220;
    /* 0x222 */ byte       pad_222[2];
    /* 0x224 */ u32        field_224;
    /* 0x228 */ s16        field_228;
    /* 0x22A */ u16        field_22A;
    /* 0x22C */ GpRec18    field_22C[4];
    /* 0x28C */ byte       field_28C[8];
    /* 0x294 */ void*      field_294;
    /* 0x298 */ GpRec18*   field_298;
    /* 0x29C */ u16        field_29C;
    /* 0x29E */ s16        field_29E;
    /* 0x2A0 */ u16        field_2A0;
    /* 0x2A2 */ byte       pad_2A2[2];
    /* 0x2A4 */ u32        field_2A4;
    /* 0x2A8 */ u16        field_2A8;
    /* 0x2AA */ u16        field_2AA;
    /* 0x2AC */ GpRec18    field_2AC[1];
    /* 0x2C4 */ GpEffArg   field_2C4; // record the death effect is spawned with
    /* 0x2CC */ MATRIX     field_2CC;
    /* 0x2EC */ s16        field_2EC;
    /* 0x2EE */ s16        field_2EE;
    /* 0x2F0 */ s16        field_2F0;
    /* 0x2F2 */ byte       pad_2F2[2];
    /// Coordinate node `Actor03800_Fn003B8` publishes on `field_344` for the
    /// detached modes (spawn kinds 1 and 2): it is seeded from the model's own
    /// `Actor103800Obj2C::field_8`, parented to `gGfxViewCoord` and then turned
    /// by 0x400 / 0x800 about X. `coord.coord.t` is the saved world translation
    /// the idle and detach ticks restore after rebuilding the rotation.
    /* 0x2F4 */ GsCOORDINATE2  coord;
    /* 0x344 */ GsCOORDINATE2* field_344;
    /* 0x348 */ u16            field_348;
    /* 0x34A */ s16            field_34A;
    /* 0x34C */ u16            field_34C;
    /* 0x34E */ s16            field_34E;
    /* 0x350 */ s16            field_350;
    /* 0x352 */ s16            field_352;
    /* 0x354 */ s16            field_354;
    /* 0x356 */ s16            field_356;
    /* 0x358 */ s16            field_358;
    /* 0x35A */ s16            field_35A;
    /* 0x35C */ s16            field_35C;
    /* 0x35E */ s16            field_35E;
    /* 0x360 */ s16            field_360;
    /* 0x362 */ s16            field_362;
    /* 0x364 */ s16            field_364;
    /* 0x366 */ s16            field_366;
    /* 0x368 */ s16            field_368;
    /* 0x36A */ s16            field_36A;
    /* 0x36C */ s16            field_36C;
    /* 0x36E */ s16            field_36E;
    /* 0x370 */ s16            field_370;
    /* 0x372 */ s16            field_372;
    /* 0x374 */ s16            field_374;
    /* 0x376 */ byte           pad_376[2];
    /* 0x378 */ s16            field_378;
    /* 0x37A */ s16            field_37A;
    /* 0x37C */ s16            field_37C;
    /* 0x37E */ s16            field_37E;
} Actor103800Work;
STATIC_ASSERT_SIZEOF(Actor103800Work, 0x380);

typedef struct Actor103800Ctx {
    /* 0x00 */ byte       pad_0[8];
    /* 0x08 */ u16        field_8;
    /* 0x0A */ byte       pad_A[6];
    /* 0x10 */ GpLinkNode node;
    /* 0x18 */ byte       pad_18[0x24];
    /// `GpAreaPlace*` placement record (`GpEnemy::place`); `field_2` is the
    /// spawn variant `Actor03800_Fn003B8` splits into `field_350` / `field_352`.
    /* 0x3C */ GpAreaPlace* field_3C;
    /* 0x40 */ s16          field_40;
    /* 0x42 */ byte         pad_42[0xA];
    /* 0x4C */ u8           field_4C;
    /* 0x4D */ byte         pad_4D[7];
    /* 0x54 */ s32          field_54;
} Actor103800Ctx;
STATIC_ASSERT_SIZEOF(Actor103800Ctx, 0x58);

typedef struct Actor103800 {
    /* 0x00 */ byte              pad_0[0x1C];
    /* 0x1C */ Actor103800Work*  field_1C;
    /* 0x20 */ Actor103800Ctx*   field_20;
    /* 0x24 */ byte              pad_24[8];
    /* 0x2C */ Actor103800Obj2C* field_2C;
    /* 0x30 */ s32               field_30;
} Actor103800;
STATIC_ASSERT_SIZEOF(Actor103800, 0x34);

typedef struct Actor03800TurnScratch {
    /* 0x00 */ SVECTOR rotation;
    /* 0x08 */ MATRIX  matrix;
} Actor03800TurnScratch;
STATIC_ASSERT_SIZEOF(Actor03800TurnScratch, 0x28);

typedef struct Actor03800MoveScratch {
    VECTOR  delta;
    SVECTOR normal;
} Actor03800MoveScratch;
STATIC_ASSERT_SIZEOF(Actor03800MoveScratch, 0x18);

typedef struct Actor103800StateFuncTable3 {
    void (*funcs[3])(Actor103800Ctx*, Actor103800*);
} Actor103800StateFuncTable3;
STATIC_ASSERT_SIZEOF(Actor103800StateFuncTable3, 0xC);

extern Actor103800StateFuncTable3 Actor03800_D00004;

extern void* D_80067704[1];
extern u8    Actor03800_D0459C[];
extern u8    Actor03800_D046A0[];
extern u8    Actor03800_D047A4[];
extern u8    Actor03800_D04868[];
extern u8    Actor03800_D0492C[];

extern s8  D_80115410;
extern s16 Actor03800_D05F90[];
extern s16 Actor03800_D05FA8[];
extern u16 Actor03800_D05F40;
extern u16 Actor03800_D05F42;
extern u16 Actor03800_D05F48;

void Actor03800_Fn03008(Actor103800* actor, u32 variant);
void Actor03800_Fn02998(Actor103800Ctx* arg0, Actor103800* arg1);
void Actor03800_Fn0315C(Actor103800* arg0);
void Actor03800_Fn031B8(Actor103800Ctx* arg0, Actor103800* arg1);
void Actor03800_Fn032D8(Actor103800* arg0);

#endif
