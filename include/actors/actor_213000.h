#ifndef ACTOR_213000_H
#define ACTOR_213000_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/task.h"

/// Work block this actor parks in `Task::idMap`. `field_474` / `field_477`
/// drive the animation teardown, the animation context `Gp_AnimTickIndex`
/// walks starts the block, and `light` / `color` are the matrices the TMD
/// object's `field_1C` / `field_20` are republished from. `field_4BC` /
/// `field_4C0` hold the two model tasks the visibility switch picks between;
/// the TMD object is then reached through their `Task::extra`.
typedef struct Actor213000Work {
    /* 0x000 */ byte   pad_0[0x474];
    /* 0x474 */ s8     field_474; // nonzero while the animation context is live
    /* 0x475 */ byte   pad_475[0x2];
    /* 0x477 */ s8     field_477; // counts down to the buffer teardown
    /* 0x478 */ byte   pad_478[0x4];
    /* 0x47C */ MATRIX light;
    /* 0x49C */ MATRIX color;
    /* 0x4BC */ Task*  field_4BC; // model task modes 0 / 1 act on
    /* 0x4C0 */ Task*  field_4C0; // model task modes 2 / 3 act on
} Actor213000Work;

/// The message body the visibility handler reads its mode from: the kind and
/// the mode halfword pair the other actor headers carry at 0x0 / 0x2.
typedef struct Actor213000Msg {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
} Actor213000Msg;
STATIC_ASSERT_SIZEOF(Actor213000Msg, 0x4);

s32 func_actor_213000_8014A980(Task* task, s32 arg1, Actor213000Msg* msg);

#endif // ACTOR_213000_H
