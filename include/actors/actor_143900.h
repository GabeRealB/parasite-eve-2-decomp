#ifndef ACTOR_143900_H
#define ACTOR_143900_H

#include "common.h"
#include "actors/actors_shared_80132ecc.h"
#include "actors/actors_shared_8013411c.h"
#include "gameplay/1BC.h"
#include "main/task.h"

/// Per-actor work block for the `actor_143900` overlay.
///
/// The overlay's state-0 handler (`ActorsShared80131f9cSub0`, here at
/// 0x80131E70) allocates it with `Mem_Calloc(0x4F0, 0)` and
/// stores the pointer both in `ActorsShared80131f9cWork` and in the task's
/// 0x1C slot, so the size below is the allocation and not a guess. Every
/// other function in the overlay reaches the block through the global.
///
/// `anim` is the animation context `Gp_AnimTickIndex` and friends walk.
typedef struct Actor143900Work {
    /* 0x000 */ byte       pad_0[0x40];
    /* 0x040 */ GpAnimCtx  anim;
    /* 0x054 */ GpAnimSlot slots[0x14];
    /* 0x374 */ byte       pad_374[0x142];
    /* 0x4B6 */ s16        field_4B6; // copy of `field_4B8`, kept for change detection
    /* 0x4B8 */ s16        field_4B8; // animation id the slots are seeded with
    /* 0x4BA */ byte       pad_4BA[0x2C];
    /* 0x4E6 */ u16        yaw;       // last yaw handed to `Gfx_RotMatrixY`
    /* 0x4E8 */ byte       pad_4E8[4];
    /* 0x4EC */ s16        field_4EC; // animation reset argument, latched by the 0x7DB handler
    /* 0x4EE */ byte       pad_4EE[2];
} Actor143900Work;
STATIC_ASSERT_SIZEOF(Actor143900Work, 0x4F0);

extern Actor143900Work* ActorsShared80131f9cWork;

/// The overlay's own variant publishes the task's work block here, the same
/// pair of places `func_actor_143900_801328D4` fills in. It is not the block
/// above: that one is the shared body's `Mem_Calloc(0x4F0, 0)`, while this
/// variant allocates `Mem_Calloc(0x4F8, 0)` and keeps two helper tasks at
/// 0x4F0 / 0x4F4 - the `ActorsShared80132eccWork` shape, which is what the
/// dispatcher here parks in the task's 0x1C slot.
extern ActorsShared80132eccWork* D_actor_143900_801496C4;

/// Payload the sender of message 0x7DB passes as `Gp_DispatchMsg`'s `arg2`;
/// the same 4-byte record as `Actor335800Msg` and `Actor342400Msg`. This
/// overlay's 0x7DB handler, `func_actor_143900_80132778`, reads the halfword
/// at 0x2.
typedef struct Actor143900Msg {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
} Actor143900Msg;
STATIC_ASSERT_SIZEOF(Actor143900Msg, 0x4);

void func_actor_143900_801324C8(void);
s32  func_actor_143900_801326FC(Task* task, s32 arg1, ActorShared8013411cPlacement* placement);
s32  func_actor_143900_80132778(Task* task, s32 arg1, Actor143900Msg* msg);
void func_actor_143900_801328D4(GpEnemy* enemy, Task* task);
void func_actor_143900_80132E48(GpEnemy* enemy, Task* task);
s32  func_actor_143900_80133360(Task* task, s32 arg1, Actor143900Msg* msg);

#endif
