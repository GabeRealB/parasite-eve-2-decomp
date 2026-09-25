#ifndef ACTOR_260500_H
#define ACTOR_260500_H

#include "common.h"

#include "gameplay/1BC.h"
#include "main/session.h"
#include "main/task.h"

/// Per-actor work block for the `actor_260500` overlay.
///
/// The spawn routine, `func_actor_260500_80149FB0`, allocates it with
/// `memCalloc(0x4B8, 0)` and stores the pointer both in
/// `D_actor_260500_80159E4C` and in the task's `Task::work` slot, so the size
/// below is the allocation and not a guess. Every other function in the
/// overlay reaches the block through the global.
///
/// It opens with the light and colour matrices the actor's model is drawn
/// under, then the animation context with nineteen slots and one 0x10-byte
/// pose record per slot.
typedef struct Actor260500Work {
    /* 0x000 */ MATRIX     light;
    /* 0x020 */ MATRIX     color;
    /* 0x040 */ GpAnimCtx  anim;
    /* 0x054 */ GpAnimSlot slots[0x13];
    /* 0x34C */ byte       poses[0x13][0x10];
    /* 0x47C */ s16        field_47C; // animation reset mode `func_actor_260500_8014A110` dispatches on (1 reseeds via `func_actor_260500_8014A644`, 2 via `func_actor_260500_8014A5B4`, 3 after)
    /* 0x47E */ s16        field_47E; // copy of `field_480` the plain reseed records as the animation now playing
    /* 0x480 */ s16        field_480; // animation id the reset is seeded with, latched from the preset's `field_4`
    /* 0x482 */ s16        field_482; // cleared before the reset is handed to `func_actor_260500_8014A110`
    /* 0x484 */ byte       pad_484[0x2A];
    /* 0x4AE */ s16        yaw;       // last yaw handed to `Gfx_RotMatrixY`
    /* 0x4B0 */ byte       pad_4B0[0x2];
    /* 0x4B2 */ s16        field_4B2; // steps left in the walk the approach handler sets up
    /* 0x4B4 */ s16        field_4B4; // turn steps left while animation 3 plays; message 0x7DB arms it at 0x14
    /* 0x4B6 */ byte       pad_4B6[0x2];
} Actor260500Work;
STATIC_ASSERT_SIZEOF(Actor260500Work, 0x4B8);

/// The work block, published by the spawn routine and by the task handler
/// `func_actor_260500_8014A460` on every frame, so the message handlers and
/// the animation loops reach it without the task.
extern Actor260500Work* D_actor_260500_80159E4C;

/// The actor's own task, published by the spawn routine: the play-animation
/// handler runs the update on it and the visibility handler reaches its model.
extern Task* D_actor_260500_80159E50;

/// Reset argument the reseed forwards. The play-animation handler latches the
/// preset's `field_C` here and `func_actor_260500_8014A110`'s reseed path reads
/// it back.
extern s16 D_actor_260500_80159D7C;

/// Approach mode the last `func_actor_260500_8014A83C` call selected; the
/// update picks its step length from it.
extern s16 D_actor_260500_80159E54;

/// Animation preset the overlay's "play animation" message handler applies to
/// the work block: `field_4` is the animation id, `field_8` picks the reset
/// path -- non-zero seeds `field_47C` with 1 and takes `field_C` as the reset
/// argument, zero seeds it with 2 -- and the id range is the handler's own:
/// `func_actor_260500_8014A6C4` accepts the first 0x24.
typedef struct Actor260500AnimPreset {
    /* 0x00 */ s32 field_0;
    /* 0x04 */ s32 field_4;
    /* 0x08 */ s32 field_8;
    /* 0x0C */ s32 field_C;
} Actor260500AnimPreset;
STATIC_ASSERT_SIZEOF(Actor260500AnimPreset, 0x10);

/// Payload of the actor's 0x7DB message; the handler reads only the halfword
/// at 0x2.
typedef struct Actor260500Msg {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
} Actor260500Msg;
STATIC_ASSERT_SIZEOF(Actor260500Msg, 0x4);

void func_actor_260500_80149FB0(GpEnemy* enemy, Task* task);
void func_actor_260500_8014A110(Task* task);
void func_actor_260500_8014A4BC(GpEnemy* enemy, Task* task);
void func_actor_260500_8014A540(Task* task);
void func_actor_260500_8014A568(void);
void func_actor_260500_8014A5B4(void);
void func_actor_260500_8014A644(void);
void func_actor_260500_8014A99C(Task* task);
s32  func_actor_260500_8014A6C4(Task* task, s32 arg1, Actor260500AnimPreset* preset);

#endif
