#ifndef ACTOR_311500_H
#define ACTOR_311500_H

#include "common.h"

typedef struct Actor311500Work {
    /* 0x000 */ byte pad_0[0x4D4];
    /* 0x4D4 */ u16  field_4D4;
} Actor311500Work;

typedef struct Actor311500 {
    /* 0x00 */ byte             pad_0[0x1C];
    /* 0x1C */ Actor311500Work* field_1C;
} Actor311500;

void func_actor_311500_801636A0(Actor311500* arg0, s32 arg1, s32 arg2, u32* arg3);

#endif
