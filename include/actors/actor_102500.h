#ifndef ACTOR_102500_H
#define ACTOR_102500_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

/// 0x18-byte slot record; `Gp_InitRec18Table` zeroes `count` of them.
typedef struct Actor02500Rec18 {
    /* 0x00 */ byte pad_0[0x18];
} Actor02500Rec18;
STATIC_ASSERT_SIZEOF(Actor02500Rec18, 0x18);

/// Collision/proximity list node this overlay embeds four times in
/// `Actor02500Work`. `Gp_LinkObj` appends it to one of the global object
/// lists and `Gp_UnlinkObj` takes it back off; `field_C` points at the
/// `Actor02500Rec18` table that follows the node in the work block.
typedef struct Actor02500Obj {
    /* 0x00 */ struct Actor02500Obj* next;
    /* 0x04 */ struct Actor02500Obj* prev;
    /* 0x08 */ void*                 field_8;
    /* 0x0C */ Actor02500Rec18*      field_C;
    /* 0x10 */ s16                   field_10;
    /* 0x12 */ s16                   field_12;
    /* 0x14 */ s16                   field_14;
    /* 0x16 */ byte                  pad_16[2];
    /* 0x18 */ s32                   field_18;
    /* 0x1C */ s16                   field_1C;
    /* 0x1E */ u16                   flags;
} Actor02500Obj;
STATIC_ASSERT_SIZEOF(Actor02500Obj, 0x20);

/// The five 0x28-byte animation slots at `Actor02500Work + 0x14`; the work
/// block opens with the 0x14-byte animation context `func_800B3F84` fills in,
/// so `Gp_AnimResetSlot` reaches these through it.
typedef struct Actor02500AnimSlots {
    /* 0x00 */ byte pad_0[0xC8];
} Actor02500AnimSlots;
STATIC_ASSERT_SIZEOF(Actor02500AnimSlots, 0xC8);

typedef struct Actor02500Obj2C {
    /* 0x00 */ byte           pad_0[8];
    /* 0x08 */ GsCOORDINATE2* field_8;
    /* 0x0C */ s16            field_C;
    /* 0x0E */ byte           pad_E[0xE];
    /* 0x1C */ void*          field_1C;
    /* 0x20 */ void*          field_20;
} Actor02500Obj2C;

/// 0x348-byte work block `Actor02500_Fn00078` allocates and hangs off
/// `Actor02500.field_1C`. It opens with the animation context (`func_800B3F84`
/// arg0) and its five slots, and carries the four list nodes plus their
/// `Actor02500Rec18` tables.
typedef struct Actor02500Work {
    /* 0x000 */ byte                pad_0[0x14];
    /* 0x014 */ Actor02500AnimSlots field_14;
    /* 0x0DC */ byte                field_DC[0x50];
    /* 0x12C */ byte                field_12C[0x20];
    /* 0x14C */ byte                field_14C[0x20];
    /* 0x16C */ Actor02500Obj       field_16C;
    /* 0x18C */ Actor02500Rec18     field_18C[1];
    /* 0x1A4 */ Actor02500Obj       field_1A4;
    /* 0x1C4 */ Actor02500Rec18     field_1C4[3];
    /* 0x20C */ Actor02500Obj       field_20C;
    /* 0x22C */ Actor02500Rec18     field_22C[5];
    /* 0x2A4 */ Actor02500Obj       field_2A4;
    /* 0x2C4 */ Actor02500Rec18     field_2C4[1];
    /* 0x2DC */ GsCOORDINATE2*      field_2DC;
    /* 0x2E0 */ s16                 field_2E0;
    /* 0x2E2 */ s16                 field_2E2;
    /* 0x2E4 */ MATRIX              field_2E4;
    /* 0x304 */ byte                pad_304[0x10];
    /* 0x314 */ s16                 field_314;
    /* 0x316 */ s16                 field_316;
    /* 0x318 */ s16                 field_318;
    /* 0x31A */ byte                pad_31A[2];
    /* 0x31C */ s16                 field_31C;
    /* 0x31E */ s16                 field_31E;
    /* 0x320 */ byte                pad_320[2];
    /* 0x322 */ s16                 field_322;
    /* 0x324 */ s16                 field_324;
    /* 0x326 */ byte                pad_326[2];
    /* 0x328 */ s16                 field_328;
    /* 0x32A */ u16                 field_32A;
    /* 0x32C */ s16                 field_32C;
    /* 0x32E */ s16                 field_32E;
    /* 0x330 */ byte                pad_330[2];
    /* 0x332 */ s16                 field_332;
    /* 0x334 */ byte                pad_334[4];
    /* 0x338 */ s16                 field_338;
    /* 0x33A */ byte                pad_33A[2];
    /* 0x33C */ s16                 field_33C;
    /* 0x33E */ byte                pad_33E[0xA];
} Actor02500Work;
STATIC_ASSERT_SIZEOF(Actor02500Work, 0x348);

typedef struct Actor02500 {
    /* 0x00 */ byte                  pad_0[0x1C];
    /* 0x1C */ Actor02500Work*       field_1C;
    /* 0x20 */ struct Actor02500Ctx* field_20;
    /* 0x24 */ byte                  pad_24[8];
    /* 0x2C */ Actor02500Obj2C*      field_2C;
    /* 0x30 */ s32                   field_30;
} Actor02500;

/// `Gp_UnlinkNode` list entry at +0x10 of `Actor02500Ctx`. `field_4` is the
/// flag byte `Actor02500_Fn01E60` writes (`sb` at 0x14).
typedef struct Actor02500Node {
    /* 0x0 */ struct Actor02500Node* next;
    /* 0x4 */ u8                     field_4;
    /* 0x5 */ byte                   pad_5[3];
} Actor02500Node;
STATIC_ASSERT_SIZEOF(Actor02500Node, 0x8);

/// Descriptor `Actor02500_Fn00078` parks at `Actor02500Ctx.field_50`, taking
/// `field_4` from it as the context's `field_40`.
typedef struct Actor02500Desc {
    /* 0x0 */ byte pad_0[4];
    /* 0x4 */ u16  field_4;
} Actor02500Desc;

/// Spawn record at `Actor02500Ctx.field_3C`; `field_2` selects the variant
/// this enemy starts in (0 idle, 1 and 2 already awake).
typedef struct Actor02500Kind {
    /* 0x0 */ byte pad_0[2];
    /* 0x2 */ u16  field_2;
} Actor02500Kind;

typedef struct Actor02500Ctx {
    /* 0x00 */ byte             pad_0[4];
    /* 0x04 */ MATRIX*          field_4;
    /* 0x08 */ byte             pad_8[8];
    /* 0x10 */ Actor02500Node   node;
    /* 0x18 */ GsCOORDINATE2*   field_18;
    /* 0x1C */ s32              field_1C;
    /* 0x20 */ s32              field_20;
    /* 0x24 */ s32              field_24;
    /* 0x28 */ byte             pad_28[0x14];
    /* 0x3C */ Actor02500Kind*  field_3C;
    /* 0x40 */ s16              field_40;
    /* 0x42 */ byte             pad_42[6];
    /* 0x48 */ u8               field_48;
    /* 0x49 */ byte             pad_49[3];
    /* 0x4C */ u8               field_4C;
    /* 0x4D */ byte             pad_4D[3];
    /* 0x50 */ Actor02500Desc*  field_50;
    /* 0x54 */ Actor02500Rec18* field_54;
} Actor02500Ctx;
STATIC_ASSERT_SIZEOF(Actor02500Ctx, 0x58);

/// 0x18-byte frame this overlay allocates on the scratchpad stack; only the
/// `SVECTOR` at +0x10 is used by `Actor02500_Fn016FC`.
typedef struct Actor02500RotScratch {
    /* 0x00 */ byte    pad_0[0x10];
    /* 0x10 */ SVECTOR rot;
} Actor02500RotScratch;
STATIC_ASSERT_SIZEOF(Actor02500RotScratch, 0x18);

extern s16            Actor02500_D05B88[];
extern Actor02500Desc Actor02500_D05B38[];
extern void*          Actor02500_D05BA0;
extern void*          Actor02500_D05B30;

void Actor02500_Fn00078(Actor02500Ctx* arg0, Actor02500* arg1);
void Actor02500_Fn01AC8(Actor02500Ctx* arg0, Actor02500* arg1);
void Actor02500_Fn01E60(Actor02500Ctx* arg0, Actor02500* arg1);

#endif
