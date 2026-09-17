#ifndef ACTOR_123200_H
#define ACTOR_123200_H

#include "common.h"

/// The work block of `actor_123200`'s instances as this check reads it:
/// `field_174` is the motion state, `field_4A` the current animation id (low
/// ten bits), and `field_220` latches the last trigger id reported.
typedef struct Actor123200Work {
    /* 0x000 */ byte pad_0[0x4A];
    /* 0x04A */ u16  field_4A;
    /* 0x04C */ byte pad_4C[0xC];
    /* 0x058 */ u16  field_58;
    /* 0x05A */ byte pad_5A[0x11A];
    /* 0x174 */ s16  field_174;
    /* 0x176 */ byte pad_176[0xAA];
    /* 0x220 */ u16  field_220;
} Actor123200Work;

/// In states 2 and 3, reports 0x400C0001 the first time the animation id in
/// `field_4A` reaches one of the state's trigger ids (latched in `field_220`);
/// in state 5, 0x400C0005 while bit 2 of `field_58` is set. Returns 0
/// otherwise.
s32 func_actor_123200_80133450(Actor123200Work* arg0);

#endif
