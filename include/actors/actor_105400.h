#ifndef ACTOR_105400_H
#define ACTOR_105400_H

#include "common.h"

typedef struct Actor05400Work {
    /* 0x000 */ byte pad_0[0x338];
    /* 0x338 */ s16  field_338;
} Actor05400Work;

typedef struct Actor05400 {
    /* 0x00 */ byte            pad_0[0x1C];
    /* 0x1C */ Actor05400Work* field_1C;
} Actor05400;

s16 Actor05400_Fn01B70(Actor05400* arg0);

#endif
