#ifndef ACTORS_SHARED_80135210_H
#define ACTORS_SHARED_80135210_H

#include "common.h"

/// Work block as this body sees it: the animation id the actor is playing, the
/// id it was playing last frame, and how many frames that id has been on.
typedef struct ActorShared80135210Work {
    /* 0x000 */ byte pad_0[0x248];
    /* 0x248 */ s16  field_248;
    /* 0x24A */ s16  field_24A;
    /* 0x24C */ u16  field_24C;
} ActorShared80135210Work;

typedef struct ActorShared80135210 {
    /* 0x00 */ byte                     pad_0[0x1C];
    /* 0x1C */ ActorShared80135210Work* field_1C;
} ActorShared80135210;

void ActorsShared80135210(ActorShared80135210* arg0);

#endif
