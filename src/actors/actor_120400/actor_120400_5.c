#include "common.h"

#include <psyq/libgte.h>
#include <psyq/abs.h>

#include "actors/actor_120400.h"

#include "gameplay/1BC.h"
#include "main/task.h"
#include "main/tmd.h"

/// A `MATRIX`'s word-wise view, for the identity splat
/// `func_actor_120400_801329A0` writes over the root coordinate before
/// `RotMatrix` overwrites the 3x3: five aligned stores rather than nine
/// halfword ones.
typedef struct Actor120400MatWords {
    /* 0x00 */ s32 m00_m01;
    /* 0x04 */ s32 m02_m10;
    /* 0x08 */ s32 m11_m12;
    /* 0x0C */ s32 m20_m21;
    /* 0x10 */ s16 m22;
} Actor120400MatWords;
STATIC_ASSERT_SIZEOF(Actor120400MatWords, 0x14);

/// Walk step 3, the turn to the placement yaw: Euler-extracts the root
/// coordinate into `vec`, and while the yaw gap to `field_4F2` is at least
/// 0x41 steps `vec.vy` toward it by 0x40, taking the step on an `s32` widening
/// of the extracted yaw. Otherwise it snaps the yaw to the target, plays the
/// preset carrying the `field_477` byte through the 0x7D3 handler and clears
/// `field_4F8` / `field_4FA`, which returns the parent to idle. Either way the
/// root coordinate is rebuilt as the identity matrix rotated by `vec`.
void func_actor_120400_801329A0(Task* arg0)
{
    Actor120400MainWork*  work;
    Actor120400MatWords*  words;
    GsCOORDINATE2*        coord;
    SVECTOR               vec;
    Actor120400AnimPreset preset;
    s32                   vy;
    s16                   diff;

    coord = ((TmdObject*)arg0->extra)->coords;
    work  = (Actor120400MainWork*)arg0->work;

    Gp_ExtractEuler(&vec, &coord->coord);
    diff = (u16)work->field_4F2 - (u16)vec.vy;
    if (ABS(diff) >= 0x41) {
        vy = vec.vy;
        if (diff < 0) {
            vec.vy = vy - 0x40;
        } else {
            vec.vy = vy + 0x40;
        }
    } else {
        vec.vy          = work->field_4F2;
        preset.field_0  = 0;
        preset.field_4  = work->field_477;
        preset.field_8  = 1;
        preset.field_C  = 5;
        preset.field_10 = 0;
        func_actor_120400_80132AA0(arg0, 0x7D3, &preset, 0);
        work->field_4F8 = 0;
        work->field_4FA = 0;
    }

    words          = (Actor120400MatWords*)&coord->coord;
    words->m00_m01 = ONE;
    words->m02_m10 = 0;
    words->m11_m12 = ONE;
    words->m20_m21 = 0;
    words->m22     = ONE;
    RotMatrix(&vec, &coord->coord);
    coord->flg = 0;
}

/// Message 0x7D3 handler of the parent, the animation preset: when the preset
/// names a different bank than the one latched in `field_476`, re-seeds the
/// slots from `D_actor_120400_8013E744`; then, with `field_8` set and the slots
/// already live, restarts animation `field_4` on every slot through
/// `func_800B4114` (passing `field_C`), otherwise resets every slot to it, and
/// ticks them all once. Returns 0.
s32 func_actor_120400_80132AA0(Task* task, s32 arg1, Actor120400AnimPreset* msg, s32 arg3)
{
    Actor120400MainWork* work;
    TmdObject*           ext;
    s32                  i;

    work = (Actor120400MainWork*)task->work;
    ext  = task->extra;
    if (msg->field_0 != work->field_476) {
        work->field_476 = msg->field_0;
        func_800B3F84(&work->anim, D_actor_120400_8013E744[work->field_476], ext, work->poses,
                      work->slots);
    }
    work->field_475 = msg->field_4;
    if (msg->field_8 != 0 && work->field_474 != 0) {
        for (i = 1; i < 0x14; i++) {
            func_800B4114(&work->anim, i, work->field_475, 0, msg->field_C);
        }
    } else {
        for (i = 1; i < 0x14; i++) {
            Gp_AnimResetSlot(&work->anim, i, work->field_475);
        }
    }
    for (i = 1; i < 0x14; i++) {
        Gp_AnimTickIndex(&work->anim, i);
    }
    work->field_474 = 1;
    return 0;
}
