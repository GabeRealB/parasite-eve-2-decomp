#ifndef ACTOR_101200_H
#define ACTOR_101200_H

#include "common.h"
#include "main/session.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

/// The actor's per-instance work block (`field_1C` of `Actor101200`). Same
/// shape as `Actor201200Work`: `field_170` is the motion state and `field_174`
/// the requested animation id, and the halfwords at 0x2E6 / 0x31E / 0x356 are
/// the flag words of three records in a 0x38-byte-stride table, with 0x24E the
/// flag word of an earlier record in the same table. The high bit gates one
/// behaviour and bit 0x4000 another. `origin` is the model position at spawn.
typedef struct Actor101200Work {
    /* 0x000 */ s16     field_0;
    /* 0x002 */ byte    pad_2[2];
    /* 0x004 */ s16     field_4;
    /* 0x006 */ s16     field_6;
    /* 0x008 */ byte    pad_8[0x50];
    /* 0x058 */ u16     field_58;
    /* 0x05A */ byte    pad_5A[0x116];
    /* 0x170 */ s16     field_170;
    /* 0x172 */ byte    pad_172[2];
    /* 0x174 */ s16     field_174;
    /* 0x176 */ byte    pad_176[2];
    /* 0x178 */ s16     field_178;
    /* 0x17A */ byte    pad_17A[0x3E];
    /* 0x1B8 */ GpRec18 rec1B8;
    /* 0x1D0 */ byte    pad_1D0[0x7E];
    /* 0x24E */ u16     field_24E;
    /* 0x250 */ GpRec18 rec250;
    /* 0x268 */ byte    pad_268[0x7E];
    /* 0x2E6 */ u16     field_2E6;
    /* 0x2E8 */ byte    pad_2E8[0x36];
    /* 0x31E */ u16     field_31E;
    /* 0x320 */ byte    pad_320[0x36];
    /* 0x356 */ u16     field_356;
    /* 0x358 */ SVECTOR origin;
    /* 0x360 */ byte    pad_360[0x7C];
    /* 0x3DC */ s16     field_3DC;
    /* 0x3DE */ byte    pad_3DE[2];
} Actor101200Work;
STATIC_ASSERT_SIZEOF(Actor101200Work, 0x3E0);

/// Display object hung off `field_2C`; `field_C` is the visibility flag word
/// the state handlers clear when the actor restarts, and whose bit 0x80 the
/// teardown state raises.
typedef struct Actor101200Obj2C {
    /* 0x0 */ byte           pad_0[8];
    /* 0x8 */ GsCOORDINATE2* field_8;
    /* 0xC */ u16            field_C;
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

/// 0xC-byte scratch taken from `0x1F8003FC` for the player-in-radius test:
/// the X/Z offset to the camera target and the radius, squared in place.
typedef struct Actor101200RangeScratch {
    /* 0x0 */ s32 dx;
    /* 0x4 */ s32 dz;
    /* 0x8 */ s32 r;
} Actor101200RangeScratch;
STATIC_ASSERT_SIZEOF(Actor101200RangeScratch, 0xC);

/// Nonzero when the XZ offset `d` lies outside radius `r`; squares in a scratch block.
static __inline__ s32 Actor101200_OutOfRange(SVECTOR* d, s16 r)
{
    u8*                      head;
    Actor101200RangeScratch* blk;
    s32                      ret;

    head                                          = *(u8**)0x1F8003FC;
    ((Actor101200RangeScratch*)(head - 0xC))->dx  = d->vx;
    blk                                           = (Actor101200RangeScratch*)(head - 0xC);
    blk->dz                                       = d->vz;
    blk->r                                        = r;
    ((Actor101200RangeScratch*)(head - 0xC))->dx *= ((Actor101200RangeScratch*)(head - 0xC))->dx;
    *(Actor101200RangeScratch**)0x1F8003FC        = blk;
    blk->dz                                      *= blk->dz;
    blk->r                                       *= blk->r;
    *(u8**)0x1F8003FC                             = head;
    ret                                           = ((Actor101200RangeScratch*)(head - 0xC))->dx + blk->dz >= blk->r;
    return ret;
}

/// 0xC-byte scratch taken from `0x1F8003FC` by the return-to-spawn walk: the
/// offset to the spawn point and the clamped new yaw.
typedef struct Actor101200TurnScratch {
    /* 0x0 */ SVECTOR d;
    /* 0x8 */ s16     angle;
    /* 0xA */ s16     pad;
} Actor101200TurnScratch;
STATIC_ASSERT_SIZEOF(Actor101200TurnScratch, 0xC);

s16  func_actor_101200_80131F50(GsCOORDINATE2* coord, GpRec18* rec, s32 n, SVECTOR* d);
s32  func_actor_101200_8013249C(GsCOORDINATE2* coord, GpRec18* rec, s32 n);
void func_actor_101200_80132640(Actor101200* arg0);
void func_actor_101200_801350B4(Actor101200Ctx* arg0, Actor101200* arg1);
void func_actor_101200_80135BE0(Actor101200Ctx* arg0, Actor101200* arg1);
void func_actor_101200_80135C98(Actor101200Ctx* arg0, Actor101200* arg1);
void func_actor_101200_80135D50(Actor101200Ctx* arg0, Actor101200* arg1);

#endif
