#ifndef ACTOR_201100_H
#define ACTOR_201100_H

#include "common.h"

#include "gameplay/3A34.h"
#include "main/task.h"

/// 0x58-byte work block allocated by the spawn state. The display node uses
/// the one-entry collision table at 0x40; the fade state clears its top flags.
typedef struct Actor201100Work {
    /* 0x00 */ byte    pad_0[0x8];
    /* 0x08 */ GpObj   model;
    /* 0x28 */ byte    pad_28[0x18];
    /* 0x40 */ GpRec18 rec;
} Actor201100Work;
STATIC_ASSERT_SIZEOF(Actor201100Work, 0x58);

/// `Task::extra` of the actor: `coord` is where the spark effects spawn.
typedef struct Actor201100Extra {
    /* 0x00 */ byte           pad_0[0x8];
    /* 0x08 */ GsCOORDINATE2* coord;
} Actor201100Extra;

extern s32       D_8007216C;
extern GpU16Pair D_actor_201100_80151318;
extern u8        D_actor_201100_8015F490;

void ActorsShared8013845cSub0(Task* task);
void ActorsShared8013845cSub1(Task* task);

#endif // ACTOR_201100_H
