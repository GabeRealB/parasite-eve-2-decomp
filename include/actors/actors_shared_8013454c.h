#ifndef ACTORS_SHARED_8013454C_H
#define ACTORS_SHARED_8013454C_H

#include "common.h"

#include "main/task.h"

/// The part of the carrier's work block shared by the root-motion variants.
/// Each carrier's block is its own, larger type; the shared unit only names
/// the fields it touches.
typedef struct ActorsShared8013454cWork {
    /* 0x000 */ byte pad_0[0x274];
    /* 0x274 */ s32  field_274; // previous root translation X
    /* 0x278 */ s32  field_278; // previous root translation Y
    /* 0x27C */ s32  field_27C; // previous root translation Z
    /* 0x280 */ byte pad_280[0x3E];
    /* 0x2BE */ s16  field_2BE; // step length, 4096 = one unit
    /* 0x2C0 */ byte pad_2C0[0x1E];
    /* 0x2DE */ s16  field_2DE;
} ActorsShared8013454cWork;

void ActorsShared8013454c(Task* task);

#endif
