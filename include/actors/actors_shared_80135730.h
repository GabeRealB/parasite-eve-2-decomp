#ifndef ACTORS_SHARED_80135730_H
#define ACTORS_SHARED_80135730_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

typedef struct ActorShared80135730Obj2C {
    /* 0x0 */ byte           pad_0[0x8];
    /* 0x8 */ GsCOORDINATE2* field_8;
} ActorShared80135730Obj2C;

typedef struct ActorShared80135730Work {
    /* 0x000 */ byte    pad_0[0x35C];
    /* 0x35C */ VECTOR3 field_35C; ///< previous frame's coord translation
    /* 0x368 */ byte    pad_368[0x30];
    /* 0x398 */ s16     field_398; ///< forward speed, 4096 = 1.0
    /* 0x39A */ byte    pad_39A[0xE];
    /* 0x3A8 */ s16     field_3A8; ///< vertical speed, whole units
} ActorShared80135730Work;

typedef struct ActorShared80135730 {
    /* 0x00 */ byte                      pad_0[0x1C];
    /* 0x1C */ ActorShared80135730Work*  field_1C;
    /* 0x20 */ byte                      pad_20[0xC];
    /* 0x2C */ ActorShared80135730Obj2C* field_2C;
} ActorShared80135730;

void ActorsShared80135730(ActorShared80135730* arg0);

#endif
