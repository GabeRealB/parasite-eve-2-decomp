#ifndef ACTORS_SHARED_8014A7B0_H
#define ACTORS_SHARED_8014A7B0_H

#include "common.h"

/// The work block of the `x01200` actor slots as this check reads it:
/// `field_174` is the motion state, `field_4A` the current animation id (low
/// ten bits), and `field_3D4` latches the last trigger id reported.
typedef struct ActorsShared8014a7b0Work {
    /* 0x000 */ byte pad_0[0x4A];
    /* 0x04A */ u16  field_4A;
    /* 0x04C */ byte pad_4C[0xC];
    /* 0x058 */ u16  field_58;
    /* 0x05A */ byte pad_5A[0x11A];
    /* 0x174 */ s16  field_174;
    /* 0x176 */ byte pad_176[0x25E];
    /* 0x3D4 */ u16  field_3D4;
} ActorsShared8014a7b0Work;

/// Shared by `actor_101200`, `actor_201200` and `actor_301200`: in states 2 and
/// 3, reports 0x400C0001 the first time the animation id in `field_4A` reaches
/// one of the state's trigger ids (latched in `field_3D4`); in state 4,
/// 0x400C0005 while bit 0 of `field_58` is set. Returns 0 otherwise.
s32 ActorsShared8014a7b0(ActorsShared8014a7b0Work* arg0);

#endif
