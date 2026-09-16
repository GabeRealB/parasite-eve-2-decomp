#ifndef ACTOR_100400_H
#define ACTOR_100400_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/3FB8.h"

/// Stat record `Actor00400_Fn00B48` hangs off `Actor100400Obj.field_50`;
/// `field_4` seeds both HP fields.
typedef struct Actor100400Stats {
    /* 0x0 */ byte pad_0[4];
    /* 0x4 */ u16  field_4;
} Actor100400Stats;

/// Overlay-local view of the spawn parameter block reached through
/// `Actor100400Obj.field_3C`. Byte 0xF selects, in its low 3 bits, the marker
/// height `Actor00400_Fn064B0` reads out of `Actor00400_D1609C`.
typedef struct Actor100400Params {
    /* 0x0 */ byte pad_0[0xF];
    /* 0xF */ u8   field_F;
} Actor100400Params;

/// Object the actor pushes damage and hit flags into (`Actor00400_Fn01B90`).
typedef struct Actor100400Obj {
    /* 0x00 */ byte               pad_0[4];
    /* 0x04 */ MATRIX*            field_4;
    /* 0x08 */ u16                field_8;
    /* 0x0A */ byte               pad_A[6];
    /* 0x10 */ byte               field_10[4];
    /* 0x14 */ u8                 field_14;
    /* 0x15 */ byte               pad_15[3];
    /* 0x18 */ GsCOORDINATE2*     field_18;
    /* 0x1C */ s32                field_1C;
    /* 0x20 */ s32                field_20;
    /* 0x24 */ s32                field_24;
    /* 0x28 */ byte               pad_28[0x14];
    /* 0x3C */ Actor100400Params* field_3C;
    /* 0x40 */ u16                field_40;
    /* 0x42 */ u16                field_42;
    /* 0x44 */ byte               pad_44[4];
    /* 0x48 */ u8                 field_48;
    /* 0x49 */ byte               pad_49[3];
    /* 0x4C */ u8                 field_4C;
    /* 0x4D */ byte               pad_4D[3];
    /* 0x50 */ Actor100400Stats*  field_50;
    /* 0x54 */ GpRec18*           field_54;
    /* 0x58 */ byte               pad_58[8];
} Actor100400Obj;

typedef struct Actor100400QuadWork {
    /* 0x00 */ Actor100400Obj* field_0;
    /* 0x04 */ SVECTOR         vertices[4];
    /* 0x24 */ u8              intensity;
} Actor100400QuadWork;

/// 0x64-byte work block of the marker task `Actor00400_Fn064B0` spawns from
/// `Actor00400_D16028[1]`: a display object and its two `GpRec18` slots, then
/// the view-space span between the marker's base and tip.
typedef struct Actor100400MarkerWork {
    /* 0x00 */ byte    pad_0[8];
    /* 0x08 */ GpObj   obj;
    /* 0x28 */ GpRec18 recs[2];
    /* 0x58 */ s16     field_58;
    /* 0x5A */ s16     field_5A;
    /* 0x5C */ s16     field_5C;
    /* 0x5E */ byte    pad_5E[2];
    /* 0x60 */ s32     field_60;
} Actor100400MarkerWork;
STATIC_ASSERT_SIZEOF(Actor100400MarkerWork, 0x64);

/// 0x1C-byte scratch `Actor00400_Fn03318` carves off `G_SCRATCH_HEAD` to hold
/// `RotTransPers4`'s outputs for the quad it projects: the four screen-space
/// corners, the perspective term, the clip flags and the average depth used as
/// the OT key.
typedef struct Actor100400TextQuadScratch {
    /* 0x00 */ s32 screen0;
    /* 0x04 */ s32 screen1;
    /* 0x08 */ s32 screen2;
    /* 0x0C */ s32 screen3;
    /* 0x10 */ s32 perspective;
    /* 0x14 */ s32 flags;
    /* 0x18 */ s32 depth;
} Actor100400TextQuadScratch;
STATIC_ASSERT_SIZEOF(Actor100400TextQuadScratch, 0x1C);

/// Projects the four `corner` vertices through the view matrix and queues one
/// semi-transparent textured quad shaded grey `shade` (half intensity on red).
void Actor00400_Fn03318(SVECTOR* corner0, SVECTOR* corner1, SVECTOR* corner2, SVECTOR* corner3, u8 shade);

/// 8-byte waypoint record in the `Actor100400Work.field_608` array, walked
/// until `field_6` is -1. `field_0` / `field_4` are the X and Z the actor
/// steers toward; `field_6` selects the kind, where 1 is only eligible for
/// the record `field_64A` already points at.
typedef struct Actor100400Record {
    /* 0x00 */ s16  field_0;
    /* 0x02 */ byte pad_2[2];
    /* 0x04 */ s16  field_4;
    /* 0x06 */ s16  field_6;
} Actor100400Record;

/// 0x1C-byte scratch taken off `G_SCRATCH_HEAD` by `Actor00400_Fn031A4` while
/// it searches `Actor100400Work.field_608` for the nearest record: `delta`
/// holds the XZ difference from `field_5E4`, `best` the smallest distance seen
/// so far and `index` the record being tested.
typedef struct Actor100400NearestScratch {
    /* 0x00 */ VECTOR delta;
    /* 0x10 */ s32    best;
    /* 0x14 */ s32    dist;
    /* 0x18 */ s16    index;
    /* 0x1A */ s16    bestIndex;
} Actor100400NearestScratch;
STATIC_ASSERT_SIZEOF(Actor100400NearestScratch, 0x1C);

/// 8-byte waypoint indexed by `Actor100400Work.field_65B` (wraps at 8);
/// `field_2` offsets the Y base in `Actor00400_Fn05D00`, and
/// `Actor00400_Fn02208` measures the XZ distance from the root coordinate.
typedef struct Actor100400Entry8 {
    /* 0x0 */ u16  field_0;
    /* 0x2 */ u16  field_2;
    /* 0x4 */ u16  field_4;
    /* 0x6 */ byte pad_6[2];
} Actor100400Entry8;

/// Word-wise view of a `MATRIX` used to splat an identity rotation before
/// `RotMatrixX` / `RotMatrixY` overwrites it: five aligned stores instead of
/// nine halfword ones, each word holding two adjacent `m[][]` entries.
typedef struct Actor100400MatWords {
    /* 0x00 */ s32 m00_m01;
    /* 0x04 */ s32 m02_m10;
    /* 0x08 */ s32 m11_m12;
    /* 0x0C */ s32 m20_m21;
    /* 0x10 */ s16 m22;
} Actor100400MatWords;

typedef union Actor100400Mat {
    MATRIX              mat;
    Actor100400MatWords ident;
} Actor100400Mat;
STATIC_ASSERT_SIZEOF(Actor100400Mat, 0x20);

typedef union Actor100400Flags {
    u32 word;
    u16 half;
    struct {
        u16 lo;
        s16 field_62E;
    } hi;
} Actor100400Flags;

typedef struct Actor100400Work {
    /* 0x000 */ byte               pad_0[8];
    /* 0x008 */ byte               field_8[0xC];
    /* 0x014 */ byte               field_14[0x14];
    /* 0x028 */ byte               pad_28[0x24];
    /* 0x04C */ u16                field_4C;
    /* 0x04E */ byte               pad_4E[0x21E];
    /* 0x26C */ byte               field_26C[0xF0];
    /* 0x35C */ GpObj              obj_35C;
    /* 0x37C */ GpObj              obj_37C;
    /* 0x39C */ GpRec18            field_39C[6];
    /* 0x42C */ GpObj              obj_42C;
    /* 0x44C */ GpRec18            field_44C[6];
    /* 0x4DC */ GpObj              obj_4DC;
    /* 0x4FC */ GpRec18            rec_4FC[3];
    /* 0x544 */ byte               pad_544[2];
    /* 0x546 */ u16                field_546;
    /* 0x548 */ byte               pad_548[4];
    /* 0x54C */ s16                field_54C;
    /* 0x54E */ s16                field_54E;
    /* 0x550 */ s16                field_550;
    /* 0x552 */ byte               pad_552[2];
    /* 0x554 */ s16                field_554;
    /* 0x556 */ s16                field_556;
    /* 0x558 */ s16                field_558;
    /* 0x55A */ byte               pad_55A[0xA];
    /* 0x564 */ s16                field_564;
    /* 0x566 */ byte               pad_566[2];
    /* 0x568 */ s16                field_568;
    /* 0x56A */ byte               pad_56A[2];
    /* 0x56C */ SVECTOR            field_56C;
    /* 0x574 */ SVECTOR            field_574;
    /* 0x57C */ MATRIX             field_57C;
    /* 0x59C */ MATRIX             field_59C;
    /* 0x5BC */ MATRIX             field_5BC;
    /* 0x5DC */ GpEffArg           field_5DC;
    /* 0x5E4 */ SVECTOR            field_5E4;
    /* 0x5EC */ SVECTOR            field_5EC;
    /* 0x5F4 */ SVECTOR            field_5F4;
    /* 0x5FC */ byte               pad_5FC[0xC];
    /* 0x608 */ Actor100400Record* field_608;
    /* 0x60C */ Actor100400Entry8* field_60C;
    /* 0x610 */ s32                field_610;
    /* 0x614 */ s16                field_614[3];
    /* 0x61A */ byte               pad_61A[2];
    /* 0x61C */ s16                field_61C;
    /* 0x61E */ s16                field_61E;
    /* 0x620 */ byte               pad_620[4];
    /* 0x624 */ s16                field_624;
    /* 0x626 */ s16                field_626;
    /* 0x628 */ s16                field_628;
    /* 0x62A */ s16                field_62A;
    /* 0x62C */ Actor100400Flags   flags_62C;
    /* 0x630 */ s16                field_630;
    /* 0x632 */ s16                field_632;
    /* 0x634 */ u16                field_634;
    /* 0x636 */ s16                field_636;
    /* 0x638 */ s16                field_638;
    /* 0x63A */ u16                field_63A;
    /* 0x63C */ s16                field_63C;
    /* 0x63E */ s16                field_63E;
    /* 0x640 */ s16                field_640;
    /* 0x642 */ s16                field_642;
    /* 0x644 */ s16                field_644;
    /* 0x646 */ s16                field_646;
    /* 0x648 */ s16                field_648;
    /* 0x64A */ s16                field_64A;
    /* 0x64C */ s16                field_64C;
    /* 0x64E */ s16                field_64E;
    /* 0x650 */ s16                field_650;
    /* 0x652 */ s16                field_652;
    /* 0x654 */ s16                field_654;
    /* 0x656 */ byte               pad_656[2];
    /* 0x658 */ u16                field_658;
    /* 0x65A */ u8                 field_65A;
    /* 0x65B */ u8                 field_65B;
    /* 0x65C */ byte               pad_65C[1];
    /* 0x65D */ u8                 field_65D;
    /* 0x65E */ u8                 field_65E;
    /* 0x65F */ u8                 field_65F;
    /* 0x660 */ u8                 field_660;
    /* 0x661 */ u8                 field_661;
    /* 0x662 */ byte               pad_662[1];
    /* 0x663 */ u8                 field_663;
    /* 0x664 */ u8                 field_664;
    /* 0x665 */ s8                 field_665;
    /* 0x666 */ u8                 field_666;
} Actor100400Work;

/// One 0x28-byte animation slot record, as walked from `Actor100400Work`
/// by `Actor00400_Fn085B8`; slot 0 overlaps the work header.
typedef struct Actor100400AnimStride {
    /* 0x00 */ byte pad[0x1D];
    /* 0x1D */ u8   field_1D;
    /* 0x1E */ byte pad_1E[0xA];
} Actor100400AnimStride;

typedef struct Actor100400Ctx {
    /* 0x00 */ byte           pad_0[8];
    /* 0x08 */ GsCOORDINATE2* field_8;
    /* 0x0C */ s16            field_C;
    /* 0x0E */ byte           pad_E[0xE];
    /* 0x1C */ MATRIX*        field_1C;
    /* 0x20 */ MATRIX*        field_20;
} Actor100400Ctx;

/// Event passed to `Actor00400_Fn0805C`; `field_2` selects the reaction (1-6).
typedef struct Actor100400Msg {
    /* 0x00 */ u16 field_0;
    /* 0x02 */ u16 field_2;
} Actor100400Msg;

typedef struct Actor100400 {
    /* 0x00 */ byte             pad_0[0x1C];
    /* 0x1C */ Actor100400Work* field_1C;
    /* 0x20 */ Actor100400Obj*  field_20;
    /* 0x24 */ byte             pad_24[8];
    /* 0x2C */ Actor100400Ctx*  field_2C;
    /* 0x30 */ s32              field_30;
    /* 0x34 */ s32              field_34;
} Actor100400;

void Actor00400_Fn00C84(Actor100400*);
void Actor00400_Fn07400(Actor100400*);
void Actor00400_Fn07518(Actor100400*);
void Actor00400_Fn0A034(Actor100400*);
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
