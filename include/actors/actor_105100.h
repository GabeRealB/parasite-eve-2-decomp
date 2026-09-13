#ifndef ACTOR_105100_H
#define ACTOR_105100_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/3A34.h"

typedef struct Actor105100Obj2C {
    /* 0x00 */ byte           pad_0[8];
    /* 0x08 */ GsCOORDINATE2* field_8;
    /* 0x0C */ s16            field_C;
} Actor105100Obj2C;

typedef struct Actor105100Work {
    /* 0x000 */ GpObj obj0;
    /* 0x020 */ byte  pad_20[0x18];
    /* 0x038 */ GpObj obj38;
    /* 0x058 */ byte  pad_58[0x508];
    /* 0x560 */ s32   field_560;
    /* 0x564 */ byte  pad_564[0x1C];
    /* 0x580 */ s32   field_580;
    /* 0x584 */ s32   field_584;
    /* 0x588 */ s32   field_588;
    /* 0x58C */ byte  pad_58C[2];
    /* 0x58E */ u16   field_58E;
    /* 0x590 */ s16   field_590;
    /* 0x592 */ u16   field_592;
    /* 0x594 */ s16   field_594;
    /* 0x596 */ byte  pad_596[0xC];
    /* 0x5A2 */ s16   field_5A2;
    /* 0x5A4 */ byte  pad_5A4[4];
    /* 0x5A8 */ s16   field_5A8;
    /* 0x5AA */ byte  pad_5AA[0xE];
    /* 0x5B8 */ u16   field_5B8;
    /* 0x5BA */ byte  pad_5BA[2];
    /* 0x5BC */ s16   field_5BC;
} Actor105100Work;

/// List entry at +0x10 of `Actor105100Ctx`, linked by the state-0 setup.
/// `field_4` is the flag byte previously named `field_14` (`sb` at 0x14).
typedef struct Actor105100Node {
    /* 0x0 */ struct Actor105100Node* next;
    /* 0x4 */ u8                      field_4;
    /* 0x5 */ byte                    pad_5[3];
} Actor105100Node;
STATIC_ASSERT_SIZEOF(Actor105100Node, 0x8);

/// Task context (`Task::spawnArg2`), which is also `Actor105100::field_20`.
/// `field_8` carries the actor id in bits 12+ (the sound event ids are
/// `(field_8 >> 12) << 8 | 0x40330000`), `field_40` is HP and `field_4C` the
/// state flags the per-frame handlers test. `field_40` is unsigned in this
/// overlay's view: the heal in `func_actor_105100_80135FCC` and the damage in
/// `func_actor_105100_80135E54` both load it unsigned.
typedef struct Actor105100Ctx {
    /* 0x00 */ byte            pad_0[4];
    /* 0x04 */ MATRIX*         field_4;
    /* 0x08 */ u16             field_8;
    /* 0x0A */ byte            pad_A[6];
    /* 0x10 */ Actor105100Node node;
    /* 0x18 */ GsCOORDINATE2*  field_18;
    /* 0x1C */ s32             field_1C;
    /* 0x20 */ s32             field_20;
    /* 0x24 */ s32             field_24;
    /* 0x28 */ byte            pad_28[0x14];
    /* 0x3C */ byte            pad_3C[4];
    /* 0x40 */ u16             field_40;
    /* 0x42 */ byte            pad_42[6];
    /* 0x48 */ u8              field_48;
    /* 0x49 */ byte            pad_49[3];
    /* 0x4C */ u8              field_4C;
    /* 0x4D */ byte            pad_4D[3];
    /* 0x50 */ void*           field_50;
    /* 0x54 */ s32             field_54;
} Actor105100Ctx;
STATIC_ASSERT_SIZEOF(Actor105100Ctx, 0x58);

typedef struct Actor105100 {
    /* 0x00 */ byte              pad_0[0x1C];
    /* 0x1C */ Actor105100Work*  field_1C;
    /* 0x20 */ Actor105100Ctx*   field_20;
    /* 0x24 */ byte              pad_24[8];
    /* 0x2C */ Actor105100Obj2C* field_2C;
} Actor105100;

void func_actor_105100_80132AA0(Actor105100Ctx* arg0, Actor105100* arg1);

#endif
