#ifndef ACTOR_341700_H
#define ACTOR_341700_H

#include "common.h"
#include "main/tmd.h"

typedef struct Actor341700 {
    /* 0x00 */ byte       pad_0[0x2C];
    /* 0x2C */ TmdObject* field_2C;
} Actor341700;

s32 func_actor_341700_8016CE28(Actor341700* arg0, s32 arg1, s32 arg2);

#endif
