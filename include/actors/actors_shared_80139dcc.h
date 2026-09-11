#ifndef ACTORS_SHARED_80139DCC_H
#define ACTORS_SHARED_80139DCC_H

#include "common.h"

#include "main/task.h"

/// Three packed halfwords, laid out like `Actor400600ViewPos`. This body only
/// writes `x` and `z`.
typedef struct ActorsShared80139dccPos {
    /* 0x0 */ s16 x;
    /* 0x2 */ s16 y;
    /* 0x4 */ s16 z;
} ActorsShared80139dccPos;
STATIC_ASSERT_SIZEOF(ActorsShared80139dccPos, 0x6);

void ActorsShared80139dcc(Task* task, s16 index, ActorsShared80139dccPos* out);

#endif
