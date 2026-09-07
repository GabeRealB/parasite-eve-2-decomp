#ifndef ACTOR_202600_H
#define ACTOR_202600_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

typedef struct Actor202600Obj2C {
    /* 0x00 */ byte           pad_0[8];
    /* 0x08 */ GsCOORDINATE2* field_8;
} Actor202600Obj2C;

typedef struct Actor202600Ctx {
    /* 0x00 */ byte pad_0[8];
    /* 0x08 */ u16  field_8;
} Actor202600Ctx;

typedef struct Actor202600 {
    /* 0x00 */ byte              pad_0[0x20];
    /* 0x20 */ Actor202600Ctx*   field_20;
    /* 0x24 */ byte              pad_24[8];
    /* 0x2C */ Actor202600Obj2C* field_2C;
} Actor202600;

#endif
