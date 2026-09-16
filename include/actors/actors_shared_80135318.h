#ifndef ACTORS_SHARED_80135318_H
#define ACTORS_SHARED_80135318_H

#include "common.h"

typedef struct ActorShared80135318Work {
    /* 0x000 */ byte pad_0[0x248];
    /* 0x248 */ s16  field_248;
    /* 0x24A */ byte pad_24A[0x4];
    /* 0x24E */ s16  field_24E;
    /* 0x250 */ s16  field_250;
    /* 0x252 */ byte pad_252[0x10];
    /* 0x262 */ s16  field_262;
} ActorShared80135318Work;

typedef struct ActorShared80135318 {
    /* 0x00 */ byte                     pad_0[0x1C];
    /* 0x1C */ ActorShared80135318Work* field_1C;
    /* 0x20 */ byte                     pad_20[0x10];
    /* 0x30 */ s32                      field_30;
} ActorShared80135318;

s32 ActorsShared80135318(ActorShared80135318* arg0);

#endif
