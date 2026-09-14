#ifndef ACTOR_110700_H
#define ACTOR_110700_H

#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

typedef struct Actor110700Work {
    /* 0x000 */ GpAnimCtx  anim;
    /* 0x014 */ GpAnimSlot slots[0x13];
    /* 0x30C */ byte       field_30C[0x130];
    /* 0x43C */ byte       field_43C[0x20];
    /* 0x45C */ byte       field_45C[0x20];
    /* 0x47C */ s32        field_47C;
} Actor110700Work;
STATIC_ASSERT_SIZEOF(Actor110700Work, 0x480);

/// Animation preset `func_actor_110700_8013201C` takes as `arg2`. `field_4`
/// is the animation id copied into `Actor110700Work::field_47C`.
typedef struct Actor110700AnimPreset {
    /* 0x0 */ s32 field_0;
    /* 0x4 */ s32 field_4;
} Actor110700AnimPreset;

extern u8 D_actor_110700_8013BFA0[];
extern u8 D_actor_110700_8013BFC0[];

void ActorsShared80131e24Sub0(GpEnemy* enemy, Task* task);
s32  func_actor_110700_8013201C(Task* task, s32 arg1, Actor110700AnimPreset* preset);

#endif
