#ifndef ACTOR_202600_H
#define ACTOR_202600_H

#include "common.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"
#include "main/task.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

typedef struct Actor202600Obj2C {
    /* 0x00 */ byte           pad_0[8];
    /* 0x08 */ GsCOORDINATE2* field_8;
    /* 0x0C */ s16            field_C;
    /* 0x0E */ byte           pad_E[0xE];
    /* 0x1C */ MATRIX*        field_1C;
    /* 0x20 */ MATRIX*        field_20;
} Actor202600Obj2C;

/// Per-spawn parameters; `field_1` is the per-actor variant index and
/// `field_2` the spawn mode (`func_actor_202600_8014CE1C` splits it into a
/// tens digit and a units digit). `field_F` selects the row of the overlay's
/// per-actor data table.
typedef struct Actor202600Params {
    /* 0x00 */ byte pad_0[1];
    /* 0x01 */ u8   field_1;
    /* 0x02 */ u16  field_2;
    /* 0x04 */ byte pad_4[0xB];
    /* 0x0F */ u8   field_F;
} Actor202600Params;

typedef struct Actor202600Ctx {
    /* 0x00 */ byte               pad_0[8];
    /* 0x08 */ u16                field_8;
    /* 0x0A */ byte               pad_A[6];
    /* 0x10 */ s32                field_10;
    /* 0x14 */ u8                 field_14;
    /* 0x15 */ byte               pad_15[0x27];
    /* 0x3C */ Actor202600Params* field_3C;
    /* 0x40 */ s16                field_40;
    /* 0x42 */ byte               pad_42[0x12];
    /* 0x54 */ s32                field_54;
} Actor202600Ctx;

/// The work starts with a linked `GpObj` and its 0x18-byte collision record,
/// the same prefix `Actor105500Work` has: `func_actor_202600_8014C5A0` clears
/// `obj.flags` bit 0x8000 and wipes `rec` before re-linking, then counts
/// `field_38` frames of homing movement and decays `field_3A`.
typedef struct Actor202600Work {
    /* 0x000 */ GpObj     obj;
    /* 0x020 */ GpRec18   rec;
    /* 0x038 */ s16       field_38;
    /* 0x03A */ s16       field_3A;
    /* 0x03C */ byte      pad_3C[0x118];
    /* 0x154 */ byte      field_154[0x80];
    /* 0x1D4 */ MATRIX    field_1D4;
    /* 0x1F4 */ MATRIX    field_1F4;
    /* 0x214 */ GpObj     field_214;
    /* 0x234 */ GpRec18   field_234[4];
    /* 0x294 */ GpObj     field_294;
    /* 0x2B4 */ GpRec18   field_2B4[2];
    /* 0x2E4 */ GpObj     field_2E4;
    /* 0x304 */ GpRec18   field_304[1];
    /* 0x31C */ GpObj     field_31C;
    /* 0x33C */ GpRec18   field_33C[1];
    /* 0x354 */ GpEffArg  field_354;
    /* 0x35C */ VECTOR3   field_35C;
    /* 0x368 */ byte      pad_368[4];
    /* 0x36C */ TaskDesc* field_36C;
    /* 0x370 */ MATRIX    field_370;
    /* 0x390 */ s16       field_390;
    /* 0x392 */ s16       field_392;
    /* 0x394 */ s16       field_394;
    /* 0x396 */ u16       field_396;
    /* 0x398 */ s16       field_398;
    /* 0x39A */ s16       field_39A;
    /* 0x39C */ s16       field_39C;
    /* 0x39E */ s16       field_39E;
    /* 0x3A0 */ s16       field_3A0;
    /* 0x3A2 */ s16       field_3A2;
    /* 0x3A4 */ s16       field_3A4;
    /* 0x3A6 */ s16       field_3A6;
    /* 0x3A8 */ s16       field_3A8;
    /* 0x3AA */ s16       field_3AA;
    /* 0x3AC */ u16       field_3AC;
    /* 0x3AE */ byte      pad_3AE[2];
    /* 0x3B0 */ s16       field_3B0;
    /* 0x3B2 */ s16       field_3B2;
    /* 0x3B4 */ byte      pad_3B4[6];
    /* 0x3BA */ s16       field_3BA;
    /* 0x3BC */ s16       field_3BC;
    /* 0x3BE */ s16       field_3BE;
    /* 0x3C0 */ s16       field_3C0;
    /* 0x3C2 */ s16       field_3C2;
    /* 0x3C4 */ s16       field_3C4;
    /* 0x3C6 */ s16       field_3C6;
    /* 0x3C8 */ s16       field_3C8;
    /* 0x3CA */ s16       field_3CA;
    /* 0x3CC */ s16       field_3CC;
    /* 0x3CE */ s16       field_3CE;
    /* 0x3D0 */ s16       field_3D0;
} Actor202600Work;
STATIC_ASSERT_SIZEOF(Actor202600Work, 0x3D4);

/// The 0x38-byte scratch a push-back needs: `delta` is filled by the collision
/// walk, `unit` is its normal and `local` the same vector in the grid's space;
/// `rot` is the temporary `RotMatrix` source and `func_800FDB18` effect angle.
typedef struct Actor202600HitScratch {
    /* 0x00 */ GpDeltaScratch delta;
    /* 0x10 */ VECTOR         unit;
    /* 0x20 */ VECTOR         local;
    /* 0x30 */ SVECTOR        rot;
} Actor202600HitScratch;
STATIC_ASSERT_SIZEOF(Actor202600HitScratch, 0x38);

/// Animation view of the work prefix: the 0x14-byte context `func_800B3F84`
/// fills in, followed by the eight slots it is handed and `Gp_AnimResetSlot`
/// walks.
typedef struct Actor202600Anim {
    /* 0x000 */ GpAnimCtx  context;
    /* 0x014 */ GpAnimSlot slots[8];
} Actor202600Anim;

typedef struct Actor202600 {
    /* 0x00 */ byte              pad_0[0x1C];
    /* 0x1C */ Actor202600Work*  field_1C;
    /* 0x20 */ Actor202600Ctx*   field_20;
    /* 0x24 */ byte              pad_24[8];
    /* 0x2C */ Actor202600Obj2C* field_2C;
    /* 0x30 */ s32               field_30;
} Actor202600;

extern u32 Gp_LcgState;
typedef struct Actor202600RotScratch {
    /* 0x00 */ VECTOR  vec;
    /* 0x10 */ SVECTOR rot;
} Actor202600RotScratch;
STATIC_ASSERT_SIZEOF(Actor202600RotScratch, 0x18);

extern GpPairSrcE D_actor_202600_80152788;
extern SVECTOR    D_actor_202600_801527D0[];
extern s16        D_actor_202600_801527F0[];
extern SVECTOR    D_actor_202600_80152808[];
extern s16        D_actor_202600_80152828[];
extern TaskDesc   D_actor_202600_801528D4;
extern u8         D_actor_202600_801528EC[];
extern u16        D_actor_202600_80152798[];
extern u16        D_actor_202600_801527A8[];
extern u16        D_actor_202600_801527B8[];
extern s16        D_actor_202600_801527C8[];
extern s16        D_actor_202600_801527F8[];
extern s16        D_actor_202600_80152800[];
extern s16        D_actor_202600_80152836;
extern s16        D_actor_202600_80152838;
extern s16        D_actor_202600_80152850[][2];
extern s16        D_actor_202600_80152874[][2];
extern u8         D_801153F2[2];
extern s8         D_8011540E;
extern s8         D_80115412;
extern MATRIX*    D_80073B8C[1];
extern GpU16Pair  ActorsShared80135c4cPair;

#endif
