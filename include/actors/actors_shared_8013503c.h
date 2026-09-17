#ifndef ACTORS_SHARED_8013503C_H
#define ACTORS_SHARED_8013503C_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

typedef struct ActorShared8013503cObj2C {
    /* 0x00 */ byte           pad_0[8];
    /* 0x08 */ GsCOORDINATE2* field_8;
} ActorShared8013503cObj2C;

/// The work block's saved position (`field_22C`..`field_230`), target height
/// (`field_23E`) and forward speed (`field_252`); the same offsets as
/// `Actor103700Work`.
typedef struct ActorShared8013503cWork {
    /* 0x000 */ byte pad_0[0x22C];
    /* 0x22C */ s16  field_22C;
    /* 0x22E */ s16  field_22E;
    /* 0x230 */ s16  field_230;
    /* 0x232 */ byte pad_232[0xC];
    /* 0x23E */ s16  field_23E;
    /* 0x240 */ byte pad_240[0x12];
    /* 0x252 */ s16  field_252;
} ActorShared8013503cWork;

typedef struct ActorShared8013503c {
    /* 0x00 */ byte                      pad_0[0x1C];
    /* 0x1C */ ActorShared8013503cWork*  field_1C;
    /* 0x20 */ byte                      pad_20[0xC];
    /* 0x2C */ ActorShared8013503cObj2C* field_2C;
} ActorShared8013503c;

void ActorsShared8013503c(ActorShared8013503c* arg0);

#endif
