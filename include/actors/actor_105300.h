#ifndef ACTOR_105300_H
#define ACTOR_105300_H

#include "common.h"

typedef struct Actor05300Work {
    /* 0x000 */ byte pad_0[0x338];
    /* 0x338 */ s16  field_338;
} Actor05300Work;

typedef struct Actor05300 {
    /* 0x00 */ byte            pad_0[0x1C];
    /* 0x1C */ Actor05300Work* field_1C;
} Actor05300;

s16 Actor05300_Fn01B70(Actor05300* arg0);

#endif
