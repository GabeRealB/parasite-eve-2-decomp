#ifndef ACTOR_143000_H
#define ACTOR_143000_H

#include "common.h"

typedef struct Actor143000Work {
    /* 0x00 */ byte pad_0[0xC];
    /* 0x0C */ s32  field_C;
    /* 0x10 */ s16  field_10;
    /* 0x12 */ s8   field_12;
    /* 0x13 */ s8   field_13;
    /* 0x14 */ s16  field_14;
} Actor143000Work;

typedef struct Actor143000 {
    /* 0x00 */ byte             pad_0[0x1C];
    /* 0x1C */ Actor143000Work* field_1C;
    /* 0x20 */ byte             pad_20[0xA];
    /* 0x2A */ s16              field_2A;
    /* 0x2C */ byte             pad_2C[4];
    /* 0x30 */ s32              field_30;
} Actor143000;

typedef struct Actor143000Spawn {
    /* 0x0 */ u8  field_0;
    /* 0x1 */ u8  field_1;
    /* 0x2 */ s16 field_2;
} Actor143000Spawn;
STATIC_ASSERT_SIZEOF(Actor143000Spawn, 4);

void func_actor_143000_80132A04(Actor143000* arg0);

#endif
