#ifndef ACTORS_SHARED_80133CD0_H
#define ACTORS_SHARED_80133CD0_H

#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/3FB8.h"
#include "main/task.h"

/// The part of the carriers' work block this body touches. Each carrier's block
/// is its own type (`Actor107000Work`, ...); the shared unit only names node
/// 2's collision table, the previous root translation `ActorsShared8013454c`
/// saves, and the three scalars the slide step re-arms.
typedef struct ActorsShared80133cd0Work {
    /* 0x000 */ byte    pad_0[0x154];
    /* 0x154 */ GpRec18 field_154[4]; // node 2's collision table, `func_800E0C10` steps it with count 4
    /* 0x1B4 */ byte    pad_1B4[0xC0];
    /* 0x274 */ s32     field_274;    // previous root translation X
    /* 0x278 */ s32     field_278;    // previous root translation Y
    /* 0x27C */ s32     field_27C;    // previous root translation Z
    /* 0x280 */ byte    pad_280[0x3E];
    /* 0x2BE */ s16     field_2BE;    // step length, 4096 = one unit
    /* 0x2C0 */ byte    pad_2C0[0x1E];
    /* 0x2DE */ s16     field_2DE;    // re-armed to -0x64 by the slide step
    /* 0x2E0 */ s16     field_2E0;    // non-zero once the slide step has been taken
} ActorsShared80133cd0Work;

/// Steps the carrier's coordinate along the collision response; carried by
/// `actor_104600`, `actor_107000`, `actor_204600` and `actor_207000`.
void ActorsShared80133cd0(Task* arg0);

#endif
