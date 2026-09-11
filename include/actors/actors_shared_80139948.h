#ifndef ACTORS_SHARED_80139948_H
#define ACTORS_SHARED_80139948_H

#include "common.h"

#include "main/task.h"

/// The three rotation angles `ActorsShared80139948` reads from an actor's work
/// block, at the same offset in `Actor400600Work` and `Actor405800Work`.
typedef struct ActorsShared80139948Work {
    /* 0x00 */ byte pad_0[0x80];
    /* 0x80 */ u16  pitch; // fed to RotMatrixX
    /* 0x82 */ u16  yaw;   // fed to func_8004BFF8
    /* 0x84 */ u16  roll;  // fed to RotMatrixZ
} ActorsShared80139948Work;

/// Wraps the three angles at 0x80..0x84 of the actor's work block to 12 bits
/// and rebuilds the model root's rotation from them in a scratch matrix: roll,
/// pitch, then heading. Shared by `actor_400600` and `actor_405800`.
void ActorsShared80139948(Task* arg0);

#endif
