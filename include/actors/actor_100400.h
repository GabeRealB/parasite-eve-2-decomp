#ifndef ACTOR_100400_H
#define ACTOR_100400_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/3FB8.h"

/// Object the actor pushes damage and hit flags into (`Actor00400_Fn01B90`).
typedef struct Actor100400Obj {
    /* 0x00 */ byte pad_0[8];
    /* 0x08 */ u16  field_8;
    /* 0x0A */ byte pad_A[6];
    /* 0x10 */ byte field_10[0x30];
    /* 0x40 */ u16  field_40;
    /* 0x42 */ byte pad_42[0xA];
    /* 0x4C */ u8   field_4C;
    /* 0x4D */ byte pad_4D[0x13];
} Actor100400Obj;

typedef struct Actor100400QuadWork {
    /* 0x00 */ Actor100400Obj* field_0;
    /* 0x04 */ SVECTOR         vertices[4];
    /* 0x24 */ u8              intensity;
} Actor100400QuadWork;

void Actor00400_Fn03318(SVECTOR*, SVECTOR*, SVECTOR*, SVECTOR*, s32);

typedef struct Actor100400Record {
    /* 0x00 */ byte pad_0[6];
    /* 0x06 */ s16  field_6;
} Actor100400Record;

typedef union Actor100400Flags {
    u32 word;
    u16 half;
} Actor100400Flags;

typedef struct Actor100400Work {
    /* 0x000 */ byte               pad_0[8];
    /* 0x008 */ byte               field_8[0x20];
    /* 0x028 */ byte               pad_28[0x374];
    /* 0x39C */ GpRec18            field_39C[6];
    /* 0x42C */ byte               pad_42C[0x20];
    /* 0x44C */ GpRec18            field_44C[6];
    /* 0x4DC */ byte               pad_4DC[0x70];
    /* 0x54C */ s16                field_54C;
    /* 0x54E */ byte               pad_54E[2];
    /* 0x550 */ s16                field_550;
    /* 0x552 */ byte               pad_552[0x12];
    /* 0x564 */ s16                field_564;
    /* 0x566 */ byte               pad_566[2];
    /* 0x568 */ s16                field_568;
    /* 0x56A */ byte               pad_56A[0x72];
    /* 0x5DC */ GpEffArg           field_5DC;
    /* 0x5E4 */ byte               pad_5E4[0x24];
    /* 0x608 */ Actor100400Record* field_608;
    /* 0x60C */ byte               pad_60C[4];
    /* 0x610 */ s32                field_610;
    /* 0x614 */ s16                field_614[3];
    /* 0x61A */ byte               pad_61A[2];
    /* 0x61C */ s16                field_61C;
    /* 0x61E */ byte               pad_61E[6];
    /* 0x624 */ s16                field_624;
    /* 0x626 */ byte               pad_626[2];
    /* 0x628 */ s16                field_628;
    /* 0x62A */ byte               pad_62A[2];
    /* 0x62C */ Actor100400Flags   flags_62C;
    /* 0x630 */ byte               pad_630[2];
    /* 0x632 */ s16                field_632;
    /* 0x634 */ byte               pad_634[2];
    /* 0x636 */ s16                field_636;
    /* 0x638 */ s16                field_638;
    /* 0x63A */ u16                field_63A;
    /* 0x63C */ s16                field_63C;
    /* 0x63E */ s16                field_63E;
    /* 0x640 */ s16                field_640;
    /* 0x642 */ s16                field_642;
    /* 0x644 */ s16                field_644;
    /* 0x646 */ s16                field_646;
    /* 0x648 */ byte               pad_648[2];
    /* 0x64A */ s16                field_64A;
    /* 0x64C */ byte               pad_64C[2];
    /* 0x64E */ s16                field_64E;
    /* 0x650 */ byte               pad_650[0xA];
    /* 0x65A */ u8                 field_65A;
    /* 0x65B */ u8                 field_65B;
    /* 0x65C */ byte               pad_65C[1];
    /* 0x65D */ u8                 field_65D;
    /* 0x65E */ byte               pad_65E[6];
    /* 0x664 */ u8                 field_664;
    /* 0x665 */ s8                 field_665;
} Actor100400Work;

typedef struct Actor100400Ctx {
    /* 0x00 */ byte           pad_0[8];
    /* 0x08 */ GsCOORDINATE2* field_8;
    /* 0x0C */ s16            field_C;
} Actor100400Ctx;

typedef struct Actor100400 {
    /* 0x00 */ byte             pad_0[0x1C];
    /* 0x1C */ Actor100400Work* field_1C;
    /* 0x20 */ Actor100400Obj*  field_20;
    /* 0x24 */ byte             pad_24[8];
    /* 0x2C */ Actor100400Ctx*  field_2C;
    /* 0x30 */ s32              field_30;
} Actor100400;

void Actor00400_Fn00C84(Actor100400*);
void Actor00400_Fn07400(Actor100400*);
void Actor00400_Fn07518(Actor100400*);
void Actor00400_Fn0A510(Actor100400*);
void Actor00400_Fn0A57C(Actor100400*);

/// Steps the actor's root coordinate `arg0->field_2C->field_8` along the
/// heading `arg2` in the XZ plane and marks it dirty, by `arg1` units.
///
/// `coord.t[0]` gains `rsin(arg2) * arg1` and `coord.t[2]` `rcos(arg2) * arg1`;
/// the `<< 4` on the trig result and the `>> 16` after the multiply are one
/// `>> 12` split in two, the unit circle the family's rotation code uses.
/// Clearing `flg` is what makes `GsGetLw` rebuild the matrix from `coord`.
///
/// Call sites pass `arg1` as a step distance (a constant such as 0x60) and
/// `arg2` from the actor's heading (`Actor100400Work::field_556`).
void Actor00400_Fn0762C(Actor100400* arg0, s16 arg1, s16 arg2);

#endif
