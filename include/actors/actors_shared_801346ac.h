#ifndef ACTORS_SHARED_801346AC_H
#define ACTORS_SHARED_801346AC_H

#include "common.h"

typedef struct ActorShared801346acWork {
    /* 0x000 */ byte pad_0[0x256];
    /* 0x256 */ s16  field_256;
    /* 0x258 */ s16  field_258;
    /* 0x25A */ byte pad_25A[0x106];
    /* 0x360 */ s16  field_360;
    /* 0x362 */ byte pad_362[4];
    /* 0x366 */ s16  field_366;
} ActorShared801346acWork;

typedef struct ActorShared801346ac {
    /* 0x00 */ byte                     pad_0[0x1C];
    /* 0x1C */ ActorShared801346acWork* field_1C;
} ActorShared801346ac;

void ActorsShared801346ac(ActorShared801346ac* arg0);

#endif
