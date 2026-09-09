#ifndef ACTOR_101200_H
#define ACTOR_101200_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

/// The actor's per-instance work block (`field_1C` of `Actor101200`). Same
/// shape as `Actor104000Work`: `field_170` is the motion state and `field_174`
/// the requested animation id, and the halfwords at 0x2E6 / 0x31E / 0x356 are
/// the flag words of three records in a 0x38-byte-stride table, with 0x24E the
/// flag word of an earlier record in the same table. The high bit gates one
/// behaviour and bit 0x4000 another.
typedef struct Actor101200Work {
    /* 0x000 */ s16  field_0;
    /* 0x002 */ byte pad_2[2];
    /* 0x004 */ s16  field_4;
    /* 0x006 */ byte pad_6[0x52];
    /* 0x058 */ u16  field_58;
    /* 0x05A */ byte pad_5A[0x116];
    /* 0x170 */ s16  field_170;
    /* 0x172 */ byte pad_172[2];
    /* 0x174 */ s16  field_174;
    /* 0x176 */ byte pad_176[2];
    /* 0x178 */ s16  field_178;
    /* 0x17A */ byte pad_17A[0xD4];
    /* 0x24E */ u16  field_24E;
    /* 0x250 */ byte pad_250[0x96];
    /* 0x2E6 */ u16  field_2E6;
    /* 0x2E8 */ byte pad_2E8[0x36];
    /* 0x31E */ u16  field_31E;
    /* 0x320 */ byte pad_320[0x36];
    /* 0x356 */ u16  field_356;
} Actor101200Work;

/// Display object hung off `field_2C`; `field_C` is the visibility/alpha value
/// the state handlers clear when the actor restarts.
typedef struct Actor101200Obj2C {
    /* 0x0 */ byte           pad_0[8];
    /* 0x8 */ GsCOORDINATE2* field_8;
    /* 0xC */ s16            field_C;
} Actor101200Obj2C;

typedef struct Actor101200 {
    /* 0x00 */ byte              pad_0[0x1C];
    /* 0x1C */ Actor101200Work*  field_1C;
    /* 0x20 */ byte              pad_20[0xC];
    /* 0x2C */ Actor101200Obj2C* field_2C;
} Actor101200;

/// Caller-owned context passed alongside the actor; `field_14` is the flag the
/// state handlers clear.
typedef struct Actor101200Ctx {
    /* 0x00 */ byte pad_0[0x14];
    /* 0x14 */ s8   field_14;
    /* 0x15 */ byte pad_15[3];
} Actor101200Ctx;

void func_actor_101200_80132640(Actor101200* arg0);
void func_actor_101200_80135BE0(Actor101200Ctx* arg0, Actor101200* arg1);
void func_actor_101200_80135C98(Actor101200Ctx* arg0, Actor101200* arg1);
void func_actor_101200_80135D50(Actor101200Ctx* arg0, Actor101200* arg1);

#endif
