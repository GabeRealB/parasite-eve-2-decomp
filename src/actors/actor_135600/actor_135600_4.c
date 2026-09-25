#include "common.h"

#include <psyq/abs.h>

#include "actors/actor_135600.h"

#include "gameplay/1BC.h"
#include "main/task.h"
#include "main/tmd.h"

/// Step 3 of the motion sequence, the turn to the placement yaw.
/// Euler-extracts the root coordinate into `vec`, and while the yaw gap to
/// `work->field_4F2` is at least 0x41 steps `vec.vy` toward it by 0x40, taking
/// the step on an `s32` widening of the extracted yaw; otherwise snaps the yaw
/// to the target, plays the animation with a preset carrying the `field_477`
/// byte and clears `field_4F8` / `field_4FA`, ending the sequence. Either way
/// the root coordinate is rebuilt as the identity matrix rotated by `vec`.
void func_actor_135600_80132FA8(Task* arg0)
{
    Actor135600Work*        work;
    Actor135600MatrixWords* words;
    GsCOORDINATE2*          coord;
    SVECTOR                 vec;
    Actor135600AnimPreset   preset;
    s32                     vy;
    s16                     diff;

    coord = ((TmdObject*)arg0->extra)->coords;
    work  = (Actor135600Work*)arg0->work;

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
        func_actor_135600_801330A8(arg0, 0x7D3, &preset, 0);
        work->field_4F8 = 0;
        work->field_4FA = 0;
    }

    words          = (Actor135600MatrixWords*)&coord->coord;
    words->m00_m01 = ONE;
    words->m02_m10 = 0;
    words->m11_m12 = ONE;
    words->m20_m21 = 0;
    words->m22     = ONE;
    RotMatrix(&vec, &coord->coord);
    coord->flg = 0;
}

/// Re-seeds the 20-slot array off bank table `D_actor_135600_8013B0C0` when the
/// preset's bank index changes, then stores the animation id and restarts or
/// resets every slot with it and ticks them, latching `field_474`. The
/// animation is reinstalled even when the id is unchanged.
s32 func_actor_135600_801330A8(Task* task, s32 msgId, Actor135600AnimPreset* msg, s32 arg3)
{
    Actor135600Work* work;
    TmdObject*       ext;
    s32              i;

    work = (Actor135600Work*)task->work;
    ext  = task->extra;
    if (msg->field_0 != work->field_476) {
        work->field_476 = msg->field_0;
        func_800B3F84(&work->anim, D_actor_135600_8013B0C0[work->field_476], ext, work->poses, work->slots);
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
