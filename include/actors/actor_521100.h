#ifndef ACTOR_521100_H
#define ACTOR_521100_H

#include "common.h"

typedef struct Actor521100Work {
    /* 0x000 */ byte pad_0[0x69E];
    /* 0x69E */ s16  field_69E;
} Actor521100Work;

typedef struct Actor521100 {
    /* 0x00 */ byte             pad_0[0x1C];
    /* 0x1C */ Actor521100Work* field_1C;
} Actor521100;

void func_actor_521100_801355C8(Actor521100* arg0);

#endif
