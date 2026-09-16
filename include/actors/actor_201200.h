#ifndef ACTOR_201200_H
#define ACTOR_201200_H

#include "common.h"
#include "main/tmd.h"

/// Work block this overlay parks in `Actor201200::field_1C`. `field_0` is the
/// substate the message handler below switches on; the three bytes at 0x194
/// are the message echo the dispatcher copies in for every 0xB02 message.
typedef struct Actor201200Work {
    /* 0x000 */ s16  field_0;
    /* 0x002 */ byte pad_2[0x192];
    /* 0x194 */ u8   field_194;
    /* 0x195 */ u8   field_195;
    /* 0x196 */ u8   field_196;
} Actor201200Work;

/// Context block at `Actor201200::field_20`; `field_40` is the counter the
/// message handler tests before raising the substate.
typedef struct Actor201200Ctx {
    /* 0x00 */ byte pad_0[0x40];
    /* 0x40 */ s16  field_40;
} Actor201200Ctx;

typedef struct Actor201200 {
    /* 0x00 */ byte             pad_0[0x1C];
    /* 0x1C */ Actor201200Work* field_1C;
    /* 0x20 */ Actor201200Ctx*  field_20;
    /* 0x24 */ byte             pad_24[0x8];
    /* 0x2C */ TmdObject*       field_2C;
} Actor201200;

/// Message handed to the handler: a type word that selects the actor and a
/// command word, over the same four bytes the handler also copies out one at
/// a time.
typedef union Actor201200Msg {
    struct {
        u8 b0;
        u8 b1;
        u8 b2;
        u8 b3;
    } bytes;
    struct {
        u16 type;
        u16 cmd;
    } words;
} Actor201200Msg;

s32 func_actor_201200_8014D8DC(Actor201200* arg0, s32 arg1, Actor201200Msg* arg2);

#endif
