#ifndef ACTOR_143900_H
#define ACTOR_143900_H

#include "common.h"
#include "actors/actors_shared_8013411c.h"
#include "gameplay/1BC.h"
#include "gameplay/D4.h"
#include "main/task.h"

/// Work block of the overlay's first actor variant.
///
/// That variant's spawn routine, `func_actor_143900_80131E70`, allocates it
/// with `memCalloc(0x4F0, 0)` and stores the pointer both in
/// `D_actor_143900_801496B8` and in the task's `Task::work` slot, so the size
/// below is the allocation and not a guess. Every other function of the
/// variant reaches the block through the global.
///
/// `light` and `color` are the two matrices the block supplies to the model:
/// the spawn routine points the object's `lightMtx` / `colorMtx` at them.
/// `anim` is the animation context the tick and reseed loops walk, and `slots`
/// and `pad_374` are what `func_800B3F84` fills in beside it.
typedef struct Actor143900Work {
    /* 0x000 */ MATRIX     light;
    /* 0x020 */ MATRIX     color;
    /* 0x040 */ GpAnimCtx  anim;
    /* 0x054 */ GpAnimSlot slots[0x14];
    /* 0x374 */ byte       pad_374[0x140];
    /* 0x4B4 */ s16        field_4B4; // reset mode `func_actor_143900_80132624` selects (1 or 2)
    /* 0x4B6 */ s16        field_4B6; // copy of `field_4B8`, kept for change detection
    /* 0x4B8 */ s16        field_4B8; // animation id the slots are seeded with
    /* 0x4BA */ s16        field_4BA; // cleared by `func_actor_143900_80132624` before the reseed
    /* 0x4BC */ byte       pad_4BC[0x2A];
    /* 0x4E6 */ u16        yaw;       // last yaw handed to `Gfx_RotMatrixY`
    /* 0x4E8 */ byte       pad_4E8[2];
    /* 0x4EA */ s16        field_4EA; // steps left in the walk the update performs
    /* 0x4EC */ s16        field_4EC; // turn steps left, latched by the 0x7DB handler
    /* 0x4EE */ byte       pad_4EE[2];
} Actor143900Work;
STATIC_ASSERT_SIZEOF(Actor143900Work, 0x4F0);

/// The first variant's work block, published by its dispatcher
/// `func_actor_143900_80132324` and its spawn routine.
extern Actor143900Work* D_actor_143900_801496B8;

/// The same block under the name the library copies of the tick and placement
/// bodies (`src/lib/actors_shared_80132840.c`, `src/lib/actors_shared_80132a98.c`)
/// still reach it by in the actors that link them. This overlay no longer links
/// either file; the declaration stays until they are deleted.
extern Actor143900Work* ActorsShared80131f9cWork;

/// The first variant's task, published by its spawn routine so the
/// visibility and play-animation handlers can reach it.
extern Task* D_actor_143900_801496BC;

/// Work block of the overlay's second actor variant. Its spawn routine,
/// `func_actor_143900_801328D4`, allocates it with `memCalloc(0x4F8, 0)` and
/// stores the pointer in `D_actor_143900_801496C4` and in the task's
/// `Task::work` slot, so the size below is the allocation. The head is laid out
/// as `Actor143900Work`'s; the tail holds the two helper tasks that routine
/// starts and the exit callback kills.
typedef struct Actor143900Work2 {
    /* 0x000 */ MATRIX     light;
    /* 0x020 */ MATRIX     color;
    /* 0x040 */ GpAnimCtx  anim;
    /* 0x054 */ GpAnimSlot slots[0x14];
    /* 0x374 */ byte       pose[0x140];
    /* 0x4B4 */ s16        field_4B4; // reset mode the play-animation handler selects (1 or 2)
    /* 0x4B6 */ s16        field_4B6; // copy of `field_4B8`, kept for change detection
    /* 0x4B8 */ s16        field_4B8; // animation id the slots are seeded with
    /* 0x4BA */ s16        field_4BA; // cleared by the handler before the reseed
    /* 0x4BC */ byte       pad_4BC[0x2E];
    /* 0x4EA */ s16        field_4EA; // cleared next to `field_4EC` by the spawn routine
    /* 0x4EC */ s16        field_4EC;
    /* 0x4EE */ byte       pad_4EE[0x2];
    /* 0x4F0 */ Task*      field_4F0; // first helper task
    /* 0x4F4 */ Task*      field_4F4; // second helper task
} Actor143900Work2;
STATIC_ASSERT_SIZEOF(Actor143900Work2, 0x4F8);

/// The second variant's work block, published by its dispatcher
/// `func_actor_143900_80132DEC` and its spawn routine.
extern Actor143900Work2* D_actor_143900_801496C4;

/// The second variant's task, published by its spawn routine so the placement,
/// visibility and play-animation handlers can reach it.
extern Task* D_actor_143900_801496C8;

/// Reset argument the second variant forwards to the reseed: its
/// play-animation handler latches the preset's `field_C` here.
extern s16 D_actor_143900_80149630;

/// The second variant's message table; its spawn routine publishes it as
/// `Task::msgTable`.
extern u8 D_actor_143900_80149634[];

/// Spawn table the second variant's spawn routine starts its two helper tasks
/// from, indices 1 and 2; the tasks are parked in `field_4F0` / `field_4F4`.
extern TaskDesc D_actor_143900_80149664[];

/// Animation stream the second variant's spawn routine binds into its work
/// block's animation context with `func_800B3F84`.
extern u8 D_actor_143900_80149688[];

/// Animation stream the first variant's spawn routine binds into its work
/// block's animation context with `func_800B3F84`.
extern u8 D_actor_143900_801413F8[];

/// The first variant's message table; its spawn routine publishes it as
/// `Task::msgTable`.
extern GpMsgEntry D_actor_143900_801413BC[];

/// Payload the overlay's 0x7DB message handlers take as `Gp_DispatchMsg`'s
/// `arg2`; both read only the halfword at 0x2.
typedef struct Actor143900Msg {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
} Actor143900Msg;
STATIC_ASSERT_SIZEOF(Actor143900Msg, 0x4);

/// Animation preset the overlay's "play animation" message handler applies to
/// the work block: `field_4` is the animation id, `field_8` picks the reset
/// path -- 1 for the blended `func_800B4114` reseed, 2 for a plain one -- and
/// `field_C` becomes the reset argument the reseed forwards. The id range is
/// the handler's own: `func_actor_143900_80132624` takes the first 0x14.
typedef struct Actor143900AnimPreset {
    /* 0x00 */ s32 field_0;
    /* 0x04 */ s32 field_4;
    /* 0x08 */ s32 field_8;
    /* 0x0C */ s32 field_C;
} Actor143900AnimPreset;
STATIC_ASSERT_SIZEOF(Actor143900AnimPreset, 0x10);

void func_actor_143900_80131E70(GpEnemy* enemy, Task* task);
void func_actor_143900_80131FD4(Task* task);
void func_actor_143900_80132324(Task* task);
void func_actor_143900_80132380(GpEnemy* enemy, Task* task);
void func_actor_143900_80132404(Task* task);
void func_actor_143900_8013242C(Task* task);
void func_actor_143900_801324C8(void);
void func_actor_143900_80132514(void);
void func_actor_143900_801325A4(void);
s32  func_actor_143900_80132624(Task* task, s32 arg1, Actor143900AnimPreset* preset);
s32  func_actor_143900_801326B4(Task* task, s32 arg1, s32 arg2);
s32  func_actor_143900_801326FC(Task* task, s32 arg1, ActorShared8013411cPlacement* placement);
s32  func_actor_143900_80132778(Task* task, s32 arg1, Actor143900Msg* msg);
void func_actor_143900_801328D4(GpEnemy* enemy, Task* task);
void func_actor_143900_80132A9C(Task* task);
void func_actor_143900_80132E48(GpEnemy* enemy, Task* task);
void func_actor_143900_80132ECC(Task* task);
s32  func_actor_143900_80133360(Task* task, s32 arg1, Actor143900Msg* msg);

#endif
