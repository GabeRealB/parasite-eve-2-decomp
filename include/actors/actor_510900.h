#ifndef ACTOR_510900_H
#define ACTOR_510900_H

#include "common.h"

typedef struct Actor510900Work {
    /* 0x000 */ byte pad_0[0x58E];
    /* 0x58E */ s16  field_58E;
} Actor510900Work;

typedef struct Actor510900 {
    /* 0x00 */ byte             pad_0[0x1C];
    /* 0x1C */ Actor510900Work* field_1C;
} Actor510900;

void func_actor_510900_8013B870(Actor510900* arg0);

#endif
