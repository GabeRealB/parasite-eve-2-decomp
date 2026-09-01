#ifndef ACTOR_342100_H
#define ACTOR_342100_H

#include "common.h"

typedef struct Actor342100 {
    /* 0x00 */ byte pad_0[0x30];
    /* 0x30 */ s32  field_30;
} Actor342100;

void func_actor_342100_80163344(Actor342100* arg0, s32 arg1, s32 arg2);
void func_actor_342100_80163518(void);

#endif
