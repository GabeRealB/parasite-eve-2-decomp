#ifndef ACTORS_SHARED_801326F0_H
#define ACTORS_SHARED_801326F0_H

#include "common.h"

typedef struct {
    /* 0x0 */ u8  pad0[8];
    /* 0x8 */ u16 field_8;
} ActorShared801326f0Obj20;

typedef struct {
    /* 0x0 */ u8  pad0[8];
    /* 0x8 */ s32 field_8;
} ActorShared801326f0Obj2C;

typedef struct {
    /* 0x000 */ u8  pad0[0x1C4];
    /* 0x1C4 */ u8  field_1C4;
    /* 0x1C5 */ u8  pad1C5[0x24E - 0x1C5];
    /* 0x24E */ s16 field_24E;
    /* 0x250 */ s16 field_250;
    /* 0x252 */ u8  pad252[0x260 - 0x252];
    /* 0x260 */ u16 field_260;
    /* 0x262 */ u8  pad262[0x266 - 0x262];
    /* 0x266 */ s16 field_266;
} ActorShared801326f0Work;

typedef struct {
    /* 0x00 */ u8                        pad0[0x1C];
    /* 0x1C */ ActorShared801326f0Work*  field_1C;
    /* 0x20 */ ActorShared801326f0Obj20* field_20;
    /* 0x24 */ u8                        pad24[8];
    /* 0x2C */ ActorShared801326f0Obj2C* field_2C;
} ActorShared801326f0;

s32 ActorsShared801326f0(ActorShared801326f0* arg0);

#endif
