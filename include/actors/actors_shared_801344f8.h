#ifndef ACTORS_SHARED_801344F8_H
#define ACTORS_SHARED_801344F8_H

#include "common.h"

typedef struct ActorShared801344f8Work {
    /* 0x000 */ byte pad_0[0x35A];
    /* 0x35A */ s16  field_35A;
} ActorShared801344f8Work;

typedef struct ActorShared801344f8 {
    /* 0x00 */ byte                     pad_0[0x1C];
    /* 0x1C */ ActorShared801344f8Work* field_1C;
} ActorShared801344f8;

void ActorsShared801344f8(ActorShared801344f8* arg0);

#endif
