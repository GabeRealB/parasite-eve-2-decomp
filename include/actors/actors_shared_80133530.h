#ifndef ACTORS_SHARED_80133530_H
#define ACTORS_SHARED_80133530_H

#include "common.h"

#include "main/session.h"

/// Work block reached as `task->field_1C`, shared by the two enemy overlays
/// that carry this body. `field_336` gates the whole tick, `field_33C` is the
/// ceiling the context's `field_40` has to stay below, and `field_33E` is the
/// five-frame countdown that spaces two hits apart.
typedef struct ActorShared80133530Work {
    /* 0x000 */ byte pad_0[0x336];
    /* 0x336 */ s16  field_336;
    /* 0x338 */ s16  field_338;
    /* 0x33A */ byte pad_33A[2];
    /* 0x33C */ s16  field_33C;
    /* 0x33E */ s16  field_33E;
} ActorShared80133530Work;

/// Second work block, reached as `task->field_20`: the hit counter `field_40`
/// that `ActorsShared80133530` steps, and the node that hit is bound through.
typedef struct ActorShared80133530Ctx {
    /* 0x00 */ byte       pad_0[0x10];
    /* 0x10 */ GpLinkNode node;
    /* 0x18 */ byte       pad_18[0x28];
    /* 0x40 */ s16        field_40;
} ActorShared80133530Ctx;

/// The task this body is handed.
typedef struct ActorShared80133530 {
    /* 0x00 */ byte                     pad_0[0x1C];
    /* 0x1C */ ActorShared80133530Work* field_1C;
    /* 0x20 */ ActorShared80133530Ctx*  field_20;
} ActorShared80133530;

void ActorsShared80133530(ActorShared80133530* arg0);

#endif // ACTORS_SHARED_80133530_H
