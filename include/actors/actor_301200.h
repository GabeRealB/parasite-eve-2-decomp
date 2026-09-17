#ifndef ACTOR_301200_H
#define ACTOR_301200_H

#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/3FB8.h"
#include "main/tmd.h"

/// Work block this overlay parks in `Actor301200::field_1C`. `field_0` is the
/// substate the handlers below switch on.
typedef struct Actor301200Work {
    /* 0x000 */ s16   field_0;
    /* 0x002 */ byte  pad_2[0x2];
    /* 0x004 */ s16   field_4;
    /* 0x006 */ byte  pad_6[0x52];
    /* 0x058 */ u16   field_58;
    /* 0x05A */ byte  pad_5A[0x116];
    /* 0x170 */ s16   field_170;
    /* 0x172 */ byte  pad_172[0x2];
    /* 0x174 */ s16   field_174;
    /* 0x176 */ byte  pad_176[0x2];
    /* 0x178 */ s16   field_178;
    /* 0x17A */ byte  pad_17A[0x2];
    /* 0x17C */ s16   field_17C;
    /* 0x17E */ byte  pad_17E[0xB2];
    /* 0x230 */ GpObj obj230;
    /* 0x250 */ byte  pad_250[0x78];
    /* 0x2C8 */ GpObj obj2C8;
    /* 0x2E8 */ byte  pad_2E8[0x18];
    /* 0x300 */ GpObj obj300;
    /* 0x320 */ byte  pad_320[0x18];
    /* 0x338 */ GpObj obj338;
} Actor301200Work;

/// Context block the state handlers take as their first argument; `field_14` is
/// cleared by the restart arm of `func_actor_301200_80162E60`.
typedef struct Actor301200Ctx {
    /* 0x00 */ byte pad_0[0x14];
    /* 0x14 */ s8   field_14;
} Actor301200Ctx;

typedef struct Actor301200 {
    /* 0x00 */ byte             pad_0[0x1C];
    /* 0x1C */ Actor301200Work* field_1C;
    /* 0x20 */ Actor301200Ctx*  field_20;
    /* 0x24 */ byte             pad_24[0x8];
    /* 0x2C */ TmdObject*       field_2C;
} Actor301200;

/// 0xC-byte scratch taken from `0x1F8003FC` for the player-in-radius test:
/// the X/Z offset to the camera target and the radius, squared in place.
typedef struct Actor301200RangeScratch {
    /* 0x0 */ s32 dx;
    /* 0x4 */ s32 dz;
    /* 0x8 */ s32 r;
} Actor301200RangeScratch;

/// Nonzero when the XZ offset `d` lies outside radius `r`; squares in a scratch block.
static __inline__ s32 Actor301200_OutOfRange(SVECTOR* d, s16 r)
{
    u8*                      head;
    Actor301200RangeScratch* blk;
    s32                      ret;

    head                                          = *(u8**)0x1F8003FC;
    ((Actor301200RangeScratch*)(head - 0xC))->dx  = d->vx;
    blk                                           = (Actor301200RangeScratch*)(head - 0xC);
    blk->dz                                       = d->vz;
    blk->r                                        = r;
    ((Actor301200RangeScratch*)(head - 0xC))->dx *= ((Actor301200RangeScratch*)(head - 0xC))->dx;
    *(Actor301200RangeScratch**)0x1F8003FC        = blk;
    blk->dz                                      *= blk->dz;
    blk->r                                       *= blk->r;
    *(u8**)0x1F8003FC                             = head;
    ret                                           = ((Actor301200RangeScratch*)(head - 0xC))->dx + blk->dz >= blk->r;
    return ret;
}

extern MATRIX* D_80073B8C;
extern u32     Gp_LcgState;

void Gp_ArmStateF0(s32 active);
void func_actor_301200_80162640(Actor301200* arg0);

#endif
