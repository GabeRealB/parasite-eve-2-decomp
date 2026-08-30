#ifndef ACTORS_SHARED_801355A4_H
#define ACTORS_SHARED_801355A4_H

#include "common.h"

typedef struct ActorShared801355a4Work {
    /* 0x000 */ byte pad_0[0x39A];
    /* 0x39A */ s16  field_39A;
} ActorShared801355a4Work;

typedef struct ActorShared801355a4 {
    /* 0x00 */ byte                     pad_0[0x1C];
    /* 0x1C */ ActorShared801355a4Work* field_1C;
} ActorShared801355a4;

void ActorsShared801355a4(ActorShared801355a4* arg0);

#endif
