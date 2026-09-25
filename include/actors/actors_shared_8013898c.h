#ifndef ACTORS_SHARED_8013898C_H
#define ACTORS_SHARED_8013898C_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgs.h>

/// Movement freeze flag: the body skips its whole step while this is set.
extern u8 D_80072729;

/// The caller's vector slot: the column copy fills the `SVECTOR` at 0x10, GPF
/// scales it in place and the translation update reads `vx` / `vz` back out of
/// it. Nothing ahead of 0x10 is read, and no carrier references this body at
/// all, so what the caller keeps there is not visible from here.
typedef struct ActorsShared8013898cVec {
    /* 0x00 */ byte    pad_0[0x10];
    /* 0x10 */ SVECTOR vec;
} ActorsShared8013898cVec;

void ActorsShared8013898c(GsCOORDINATE2* arg0, ActorsShared8013898cVec* arg1, s32 arg2);

#endif
