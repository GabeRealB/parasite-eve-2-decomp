#ifndef ACTOR_104000_H
#define ACTOR_104000_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

/// Payload `func_actor_104000_80138CC8` passes as `Gp_DispatchMsg`'s `arg2`
/// for message 0x7DA, which the slot-4 task forwards to the 0x7DB handlers.
/// The same four bytes as `Actor444000Msg7DA`: two id bytes followed by a
/// halfword the receiver switches on.
typedef struct Actor104000Msg7DA {
    /* 0x0 */ u8  field_0;
    /* 0x1 */ u8  field_1;
    /* 0x2 */ s16 field_2;
} Actor104000Msg7DA;
STATIC_ASSERT_SIZEOF(Actor104000Msg7DA, 0x4);

/// Two counters the overlay clears together with `D_actor_104000_8013E538`
/// when it restarts its run; still assembly everywhere they are written.
extern s32 D_actor_104000_8013E530[2];

/// Six-entry counter table the overlay resets before dispatching its 0x7DA
/// message.
extern s32 D_actor_104000_8013E538[8];

/// The actor's per-instance work block (`field_1C` of `Actor104000`). The
/// halfwords at 0x28E / 0x36E / 0x3A6 / 0x3DE are the flag words of four
/// records in a 0x38-byte-stride table; the high bit gates one behaviour and
/// bit 0x4000 another.
typedef struct Actor104000Work {
    /* 0x000 */ s16  field_0;
    /* 0x002 */ byte pad_2[2];
    /* 0x004 */ s16  field_4;
    /* 0x006 */ u16  field_6;
    /* 0x008 */ byte pad_8[0x42];
    /* 0x04A */ u16  field_4A; // low ten bits: animation id
    /* 0x04C */ byte pad_4C[0xC];
    /* 0x058 */ u16  field_58;
    /* 0x05A */ byte pad_5A[0x116];
    /* 0x170 */ s16  field_170;
    /* 0x172 */ byte pad_172[2];
    /* 0x174 */ s16  field_174;
    /* 0x176 */ s16  field_176;
    /* 0x178 */ s16  field_178;
    /* 0x17A */ byte pad_17A[2];
    /* 0x17C */ s16  field_17C;
    /* 0x17E */ byte pad_17E[0x110];
    /* 0x28E */ u16  field_28E;
    /* 0x290 */ byte pad_290[0xDE];
    /* 0x36E */ u16  field_36E;
    /* 0x370 */ byte pad_370[0x36];
    /* 0x3A6 */ u16  field_3A6;
    /* 0x3A8 */ byte pad_3A8[0x36];
    /* 0x3DE */ u16  field_3DE;
    /* 0x3E0 */ byte pad_3E0[0x94];
    /* 0x474 */ u16  field_474; // animation id that last raised the reaction
    /* 0x476 */ byte pad_476[4];
    /* 0x47A */ u8   field_47A;
    /* 0x47B */ byte pad_47B[1];
    /* 0x47C */ byte field_47C[0x14];
    /* 0x490 */ s32  field_490;
} Actor104000Work;
STATIC_ASSERT_SIZEOF(Actor104000Work, 0x494);

/// Display object hung off `field_2C`; `field_C` is the visibility/alpha value
/// the state handlers clear when the actor restarts.
typedef struct Actor104000Obj2C {
    /* 0x0 */ byte           pad_0[8];
    /* 0x8 */ GsCOORDINATE2* field_8;
    /* 0xC */ s16            field_C;
} Actor104000Obj2C;

/// Caller-owned context the actor also keeps a pointer to at `field_20`;
/// `field_14` is the flag the state handlers clear; the top nibble of `field_8`
/// selects whether it is raised again.
typedef struct Actor104000Ctx {
    /* 0x00 */ byte pad_0[8];
    /* 0x08 */ u16  field_8;
    /* 0x0A */ byte pad_A[0xA];
    /* 0x14 */ s8   field_14;
    /* 0x15 */ byte pad_15[3];
} Actor104000Ctx;

typedef struct Actor104000 {
    /* 0x00 */ byte              pad_0[0x1C];
    /* 0x1C */ Actor104000Work*  field_1C;
    /* 0x20 */ Actor104000Ctx*   field_20;
    /* 0x24 */ byte              pad_24[8];
    /* 0x2C */ Actor104000Obj2C* field_2C;
} Actor104000;
STATIC_ASSERT_SIZEOF(Actor104000, 0x30);

/// 0xC-byte scratch taken from `0x1F8003FC` for the player-in-radius test:
/// the X/Z offset to the camera target and the radius, squared in place.
typedef struct Actor104000RangeScratch {
    /* 0x0 */ s32 dx;
    /* 0x4 */ s32 dz;
    /* 0x8 */ s32 r;
} Actor104000RangeScratch;
STATIC_ASSERT_SIZEOF(Actor104000RangeScratch, 0xC);

void func_actor_104000_80132C8C(Actor104000* arg0);
void func_actor_104000_80138698(Actor104000Ctx* arg0, Actor104000* arg1);
void func_actor_104000_80138AA0(Actor104000Ctx* arg0, Actor104000* arg1);

#endif
