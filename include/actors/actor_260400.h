#ifndef ACTOR_260400_H
#define ACTOR_260400_H

#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

/// Work block this overlay hangs off the task's `Task::work` slot. The
/// actor's spawn routine, `func_actor_260400_80149FE0`, allocates it with
/// `memCalloc(0x4F8, 0)` and stores it both there and in
/// `D_actor_260400_80154C70`, so the size below is the allocation.
///
/// It opens with the light and colour matrices the actor's model is drawn
/// under, then the animation context with twenty slots and one 0x10-byte pose
/// record per slot.
///
/// The task at `field_4F0` is the helper task the spawn routine starts; the
/// exit callback `func_actor_260400_8014A630` kills it on teardown.
typedef struct Actor260400Work {
    /* 0x000 */ MATRIX     light;
    /* 0x020 */ MATRIX     color;
    /* 0x040 */ GpAnimCtx  anim;
    /* 0x054 */ GpAnimSlot slots[0x14];
    /* 0x374 */ byte       poses[0x14][0x10];
    /* 0x4B4 */ s16        field_4B4; // animation reset mode `func_actor_260400_8014A908` latches; `func_actor_260400_8014A200` dispatches on it (1 reseeds via `func_actor_260400_8014A888`, 2 via `func_actor_260400_8014A7F8`)
    /* 0x4B6 */ s16        field_4B6; // copy of `field_4B8` the reseeds record as the animation now playing
    /* 0x4B8 */ s16        field_4B8; // animation id the reset is seeded with, latched from the preset's `field_4`
    /* 0x4BA */ s16        field_4BA; // cleared before the reset is handed to `func_actor_260400_8014A200`
    /* 0x4BC */ byte       pad_4BC[0x2A];
    /* 0x4E6 */ u16        yaw;       // last yaw the placement handler `func_actor_260400_8014AA28` handed to `Gfx_RotMatrixY`
    /* 0x4E8 */ byte       pad_4E8[0x2];
    /* 0x4EA */ s16        field_4EA; // Role unproven: the state-0 handler clears it
    /* 0x4EC */ s16        field_4EC; // animation reset argument, latched to 0x14 by the case-0 branch of `func_actor_260400_8014AAA4`
    /* 0x4EE */ byte       pad_4EE[0x2];
    /* 0x4F0 */ Task*      field_4F0;
    /* 0x4F4 */ s8         field_4F4; // case selector `func_actor_260400_8014AAA4` mirrors out of the message
    /* 0x4F5 */ byte       pad_4F5[0x3];
} Actor260400Work;
STATIC_ASSERT_SIZEOF(Actor260400Work, 0x4F8);

/// The overlay's message payload, the `arg2` of `func_actor_260400_8014AAA4`.
/// Same 4-byte id/count record as `Actor143900Msg`: the halfword at 0x2 picks
/// the branch, the halfword at 0 holds the other id.
typedef struct Actor260400Msg {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
} Actor260400Msg;
STATIC_ASSERT_SIZEOF(Actor260400Msg, 0x4);

/// Animation preset the overlay's play-animation message handler applies to the
/// work block, the same record `Actor143900AnimPreset` is: `field_4` is the
/// animation id, `field_8` picks the reset path -- non-zero for the blended
/// `func_actor_260400_8014A888` reseed, zero for a plain one -- and `field_C`
/// becomes the reset argument the reseed forwards. The id range is the
/// handler's own: `func_actor_260400_8014A908` takes the first 0x10.
typedef struct Actor260400AnimPreset {
    /* 0x00 */ s32 field_0;
    /* 0x04 */ s32 field_4;
    /* 0x08 */ s32 field_8;
    /* 0x0C */ s32 field_C;
} Actor260400AnimPreset;
STATIC_ASSERT_SIZEOF(Actor260400AnimPreset, 0x10);

/// The block above, published by the spawn routine and by the dispatcher
/// `func_actor_260400_8014A550` on every frame, so the message handlers and the
/// animation loops reach it without the task.
extern Actor260400Work* D_actor_260400_80154C70;

/// Reset argument the reseed forwards, read back signed by
/// `func_actor_260400_8014A888`: the play-animation handler latches the
/// preset's `field_C` here, and `func_actor_260400_8014A200` restarts it at
/// 0xA when the mode-1 reset runs.
extern s16 D_actor_260400_80154BE4;

/// The helper task the state-0 handler spawns and parks here (0x8014A0B4),
/// the same task it hangs the exit callback on. `func_actor_260400_8014A200`
/// steps it and `func_actor_260400_8014A908` hands it the reseed.
extern Task* D_actor_260400_80154C74;

void func_actor_260400_80149FE0(GpEnemy* enemy, Task* task);
void func_actor_260400_8014A200(Task* task);
void func_actor_260400_8014A550(Task* task);
void func_actor_260400_8014A5AC(GpEnemy* enemy, Task* task);
void func_actor_260400_8014A630(Task* task);
void func_actor_260400_8014A66C(Task* task);
void func_actor_260400_8014A7AC(void);
void func_actor_260400_8014A7F8(void);
s32  func_actor_260400_8014A908(Task* task, s32 arg1, Actor260400AnimPreset* preset);
s32  func_actor_260400_8014AAA4(Task* task, s32 arg1, Actor260400Msg* msg);

#endif
