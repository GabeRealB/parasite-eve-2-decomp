#ifndef ACTORS_SHARED_80137B78_H
#define ACTORS_SHARED_80137B78_H

#include "common.h"

typedef struct ActorShared80137b78Work {
    /* 0x000 */ byte pad_0[0x6CC];
    /* 0x6CC */ s16  field_6CC;
} ActorShared80137b78Work;

typedef struct ActorShared80137b78 {
    /* 0x00 */ byte                     pad_0[0x1C];
    /* 0x1C */ ActorShared80137b78Work* field_1C;
} ActorShared80137b78;

void ActorsShared80137b78(ActorShared80137b78* arg0);

#endif
