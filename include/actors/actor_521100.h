#ifndef ACTOR_521100_H
#define ACTOR_521100_H

#include "common.h"
#include "gameplay/1BC.h"

typedef struct Actor521100Coord {
    /* 0x00 */ s32  field_0;
    /* 0x04 */ byte pad_4[0x4C];
    /* 0x50 */ s32  field_50;
} Actor521100Coord;

typedef struct Actor521100Obj2C {
    /* 0x00 */ byte              pad_0[8];
    /* 0x08 */ Actor521100Coord* field_8;
    /* 0x0C */ s16               field_C;
} Actor521100Obj2C;

typedef struct Actor521100 {
    /* 0x00 */ byte                    pad_0[0x1C];
    /* 0x1C */ struct Actor521100Work* field_1C;
    /* 0x20 */ byte                    pad_20[0xC];
    /* 0x2C */ Actor521100Obj2C*       field_2C;
} Actor521100;

/// Per-actor work block. `Mem_Calloc(0x4B4, 0)` in `func_actor_521100_80135DDC`
/// stores the pointer both in `D_actor_521100_8016A3D8` and in the task's
/// 0x1C slot. `anim` sits at 0x40 and the slot array at 0x54, the same layout
/// as `Actor202900Work`; the nineteen slots are the ones
/// `func_actor_521100_80136724` ticks.
typedef struct Actor521100Work {
    /* 0x000 */ byte         pad_0[0x40];
    /* 0x040 */ GpAnimCtx    anim;
    /* 0x054 */ GpAnimSlot   slots[0x13];
    /* 0x34C */ byte         pad_34C[0x308];
    /* 0x654 */ Actor521100* field_654;
    /* 0x658 */ byte         pad_658[0x28];
    /* 0x680 */ s16          field_680;
    /* 0x682 */ byte         pad_682[0xA];
    /* 0x68C */ s16          field_68C;
    /* 0x68E */ s16          field_68E;
    /* 0x690 */ s16          field_690;
    /* 0x692 */ s16          field_692;
    /* 0x694 */ s16          field_694;
    /* 0x696 */ byte         pad_696[8];
    /* 0x69E */ s16          field_69E;
    /* 0x6A0 */ byte         pad_6A0[8];
    /* 0x6A8 */ s16          field_6A8;
    /* 0x6AA */ byte         pad_6AA[6];
    /* 0x6B0 */ s16          field_6B0;
    /* 0x6B2 */ s16          field_6B2;
} Actor521100Work;

/// 4-byte message record; the handler switches on the halfword at 0x2,
/// the same shape as `Actor361100Msg`.
typedef struct Actor521100Msg {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
} Actor521100Msg;
STATIC_ASSERT_SIZEOF(Actor521100Msg, 0x4);

typedef struct Actor521100Ctx {
    /* 0x00 */ byte pad_0[0x14];
    /* 0x14 */ u8   field_14;
} Actor521100Ctx;

extern Actor521100Work* D_actor_521100_8016A3D8;

void func_actor_521100_80135414(Actor521100Ctx* arg0, Actor521100* arg1);
void func_actor_521100_80135478(Actor521100Ctx* arg0, Actor521100* arg1);
void func_actor_521100_801355C8(Actor521100* arg0);
void func_actor_521100_80136724(void);

#endif
