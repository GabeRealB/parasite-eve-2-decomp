#ifndef ACTOR_101600_H
#define ACTOR_101600_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

/// Singly-linked node embedded in `Actor01600Ctx` at 0x10 and unlinked by
/// `Gp_UnlinkNode`. Same layout as the gameplay `GpLinkNode`.
typedef struct Actor01600Node {
    /* 0x0 */ struct Actor01600Node* next;
    /* 0x4 */ u8                     field_4;
    /* 0x5 */ byte                   pad_5[3];
} Actor01600Node;
STATIC_ASSERT_SIZEOF(Actor01600Node, 0x8);

typedef struct Actor01600Ctx {
    /* 0x00 */ byte           pad_0[0x10];
    /* 0x10 */ Actor01600Node node;
    /* 0x18 */ byte           pad_18[0x3C];
    /* 0x54 */ s32            field_54;
} Actor01600Ctx;
STATIC_ASSERT_SIZEOF(Actor01600Ctx, 0x58);

/// Overlay-local view of the object at `Actor01600.field_20`. `field_8` is
/// the u16 id nibble `Actor01600_Fn01420` shifts into a `SndEvt_EnqueueType6`
/// request; the same pointer is passed to `Gp_TickObjFlag2`.
typedef struct Actor01600Obj20 {
    /* 0x0 */ byte pad_0[8];
    /* 0x8 */ u16  field_8;
} Actor01600Obj20;

/// `field_8` is the actor's `GsCOORDINATE2` array: entry 0 is the actor's own
/// coordinate, entry 1 the attachment point whose `workm.t` feeds
/// `Gp_UpdateActorColor`.
typedef struct Actor01600Obj2C {
    /* 0x0 */ byte           pad_0[8];
    /* 0x8 */ GsCOORDINATE2* field_8;
    /* 0xC */ u16            field_C;
} Actor01600Obj2C;

typedef struct Actor01600Work {
    /* 0x000 */ byte   pad_0[0x29C];
    /* 0x29C */ byte   field_29C[0x1E];
    /* 0x2BA */ u16    field_2BA;
    /* 0x2BC */ byte   pad_2BC[0x30];
    /* 0x2EC */ byte   field_2EC[0xE0];
    /* 0x3CC */ byte   field_3CC[0x40];
    /* 0x40C */ byte   field_40C[0x90];
    /* 0x49C */ MATRIX field_49C;
    /* 0x4BC */ s32    field_4BC;
    /* 0x4C0 */ s32    field_4C0;
    /* 0x4C4 */ s32    field_4C4;
    /* 0x4C8 */ byte   pad_4C8[4];
    /* 0x4CC */ s16    field_4CC;
    /* 0x4CE */ byte   pad_4CE[0x2C];
    /* 0x4FA */ s16    field_4FA;
    /* 0x4FC */ byte   pad_4FC[2];
    /* 0x4FE */ s16    field_4FE;
    /* 0x500 */ byte   pad_500[2];
    /* 0x502 */ s16    field_502;
    /* 0x504 */ s16    field_504;
    /* 0x506 */ s16    field_506;
    /* 0x508 */ byte   pad_508[2];
    /* 0x50A */ s16    field_50A;
    /* 0x50C */ byte   pad_50C[2];
    /* 0x50E */ s16    field_50E;
    /* 0x510 */ byte   pad_510[8];
    /* 0x518 */ s16    field_518;
    /* 0x51A */ byte   pad_51A[4];
    /* 0x51E */ s16    field_51E;
    /* 0x520 */ byte   pad_520[2];
    /* 0x522 */ s16    field_522;
    /* 0x524 */ byte   pad_524[4];
    /* 0x528 */ s16    field_528;
    /* 0x52A */ byte   pad_52A[6];
    /* 0x530 */ s16    field_530;
    /* 0x532 */ byte   pad_532[6];
    /* 0x538 */ s16    field_538;
    /* 0x53A */ byte   pad_53A[6];
    /* 0x540 */ s16    field_540;
    /* 0x542 */ u16    field_542;
    /* 0x544 */ byte   pad_544[0xA];
    /* 0x54E */ s16    field_54E;
    /* 0x550 */ s16    field_550;
    /* 0x552 */ byte   pad_552[2];
} Actor01600Work;
STATIC_ASSERT_SIZEOF(Actor01600Work, 0x554);

typedef struct Actor01600 {
    /* 0x00 */ byte             pad_0[0x1C];
    /* 0x1C */ Actor01600Work*  field_1C;
    /* 0x20 */ Actor01600Obj20* field_20;
    /* 0x24 */ byte             pad_24[8];
    /* 0x2C */ Actor01600Obj2C* field_2C;
} Actor01600;
STATIC_ASSERT_SIZEOF(Actor01600, 0x30);

/// Overlay-local view of the gameplay `Gp_StateF0` block (`GpStateF0` in
/// `include/gameplay/3A34.h`). Only the two bytes this overlay touches are
/// named; `field_1C` is read with `lb`, so it is signed here.
typedef struct Actor01600StateF0 {
    /* 0x00 */ byte pad_0[4];
    /* 0x04 */ u8   field_4;
    /* 0x05 */ byte pad_5[0x17];
    /* 0x1C */ s8   field_1C;
    /* 0x1D */ byte pad_1D[0xF];
} Actor01600StateF0;
STATIC_ASSERT_SIZEOF(Actor01600StateF0, 0x2C);

/// Overlay-local view of the gameplay `Gp_StateC08` block (`GpStateC08` in
/// `include/gameplay/gameplay.h`).
typedef struct Actor01600StateC08 {
    /* 0x00 */ byte pad_0[6];
    /* 0x06 */ u8   field_6;
    /* 0x07 */ byte pad_7[3];
    /* 0x0A */ s8   field_A;
    /* 0x0B */ byte pad_B[0xD];
} Actor01600StateC08;
STATIC_ASSERT_SIZEOF(Actor01600StateC08, 0x18);

extern Actor01600StateF0  Gp_StateF0;
extern Actor01600StateC08 Gp_StateC08;

extern s32 Actor01600_D12874;

void Actor01600_Fn01420(Actor01600* arg0);
void Actor01600_Fn04054(Actor01600Ctx* arg0, Actor01600* arg1);

#endif // ACTOR_101600_H
