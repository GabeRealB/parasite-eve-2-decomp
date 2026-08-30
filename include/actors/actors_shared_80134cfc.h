#ifndef ACTORS_SHARED_80134CFC_H
#define ACTORS_SHARED_80134CFC_H

#include "common.h"

typedef struct ActorShared80134cfcWork {
    /* 0x000 */ byte pad_0[0x13C];
    /* 0x13C */ s16  field_13C;
} ActorShared80134cfcWork;

typedef struct ActorShared80134cfc {
    /* 0x00 */ byte                     pad_0[0x1C];
    /* 0x1C */ ActorShared80134cfcWork* field_1C;
} ActorShared80134cfc;

void ActorsShared80134cfc(ActorShared80134cfc* arg0);

#endif
