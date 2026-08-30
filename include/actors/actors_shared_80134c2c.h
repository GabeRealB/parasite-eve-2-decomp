#ifndef ACTORS_SHARED_80134C2C_H
#define ACTORS_SHARED_80134C2C_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

typedef struct ActorShared80134c2cObj2C {
    /* 0x00 */ byte           pad_0[8];
    /* 0x08 */ GsCOORDINATE2* field_8;
    /* 0x0C */ s16            field_C;
} ActorShared80134c2cObj2C;

typedef struct ActorShared80134c2c {
    /* 0x00 */ byte                      pad_0[0x2C];
    /* 0x2C */ ActorShared80134c2cObj2C* field_2C;
} ActorShared80134c2c;

typedef struct ActorShared80134c2cCtx {
    /* 0x00 */ byte pad_0[0x14];
    /* 0x14 */ u8   field_14;
} ActorShared80134c2cCtx;

void ActorsShared80134c2c(ActorShared80134c2cCtx* arg0, ActorShared80134c2c* arg1);

#endif
