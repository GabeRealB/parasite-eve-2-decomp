#ifndef ACTOR_503500_H
#define ACTOR_503500_H

#include "common.h"

typedef struct Actor503500Work {
    /* 0x000 */ byte pad_0[0xF0];
    /* 0x0F0 */ s8   field_F0;
} Actor503500Work;

typedef struct Actor503500 {
    /* 0x00 */ byte             pad_0[0x1C];
    /* 0x1C */ Actor503500Work* field_1C;
} Actor503500;

void func_actor_503500_8013F8AC(Actor503500* arg0);

#endif
