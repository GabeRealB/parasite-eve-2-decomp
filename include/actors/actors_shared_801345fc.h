#ifndef ACTORS_SHARED_801345FC_H
#define ACTORS_SHARED_801345FC_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

typedef struct ActorShared801345fcObj2C {
    /* 0x00 */ byte           pad_0[8];
    /* 0x08 */ GsCOORDINATE2* field_8;
} ActorShared801345fcObj2C;

/// The work block's target position (`field_23C`..`field_240`), the mode
/// `field_24E`, the phase `field_250` and its countdown `field_256`.
typedef struct ActorShared801345fcWork {
    /* 0x000 */ byte pad_0[0x23C];
    /* 0x23C */ s16  field_23C;
    /* 0x23E */ s16  field_23E;
    /* 0x240 */ s16  field_240;
    /* 0x242 */ byte pad_242[0xC];
    /* 0x24E */ s16  field_24E;
    /* 0x250 */ s16  field_250;
    /* 0x252 */ byte pad_252[0x4];
    /* 0x256 */ s16  field_256;
} ActorShared801345fcWork;

typedef struct ActorShared801345fc {
    /* 0x00 */ byte                      pad_0[0x1C];
    /* 0x1C */ ActorShared801345fcWork*  field_1C;
    /* 0x20 */ byte                      pad_20[0xC];
    /* 0x2C */ ActorShared801345fcObj2C* field_2C;
} ActorShared801345fc;

/// Scratchpad frame: the offset to the target and its normalised direction.
typedef struct ActorShared801345fcScratch {
    /* 0x00 */ VECTOR  dir;
    /* 0x10 */ SVECTOR norm;
} ActorShared801345fcScratch;
STATIC_ASSERT_SIZEOF(ActorShared801345fcScratch, 0x18);

void Gp_ArmStateF0(s32 active);

void ActorsShared801345fc(ActorShared801345fc* arg0);

#endif
