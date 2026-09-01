#ifndef ACTOR_141000_H
#define ACTOR_141000_H

#include "common.h"

typedef struct Actor141000 Actor141000;

typedef struct Actor141000Point {
    /* 0x0 */ s16  field_0;
    /* 0x2 */ s16  field_2;
    /* 0x4 */ byte pad_4[4];
} Actor141000Point;

void func_actor_141000_80131E94(Actor141000* arg0, Actor141000Point* arg1, s32 arg2);
void func_actor_141000_801323F0(Actor141000* arg0, Actor141000Point* arg1, s32* arg2, s32* arg3);
void func_actor_141000_80133260(Actor141000* arg0);

#endif
