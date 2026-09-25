#include "common.h"

#include <psyq/libgte.h>
#include <psyq/abs.h>

#include "actors/actor_350700.h"
#include "main/task.h"
#include "main/tmd.h"
#include "gameplay/1BC.h"

/// Animation bank table of the parent block.
extern void* D_actor_350700_801708D8[];

void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// Step 3 of the parent: turn to the placement yaw. Euler-extracts the root
/// coordinate into `vec`, and while the yaw gap to `work->field_4F2` is at
/// least 0x41 it steps `vec.vy` toward it by 0x40, taking the step on an `s32`
/// widening of the extracted yaw; otherwise it snaps the yaw to the target and
/// plays anim 0x7D3 with a preset carrying the `field_477` byte, clearing the
/// two motion counters. Either way the root coordinate is rebuilt as the
/// identity matrix rotated by `vec`.
void func_actor_350700_801635A8(Task* arg0)
{
    Actor350700MainWork*  work;
    Actor350700MatWords*  words;
    GsCOORDINATE2*        coord;
    SVECTOR               vec;
    Actor350700AnimPreset preset;
    s32                   vy;
    s16                   diff;

    coord = ((TmdObject*)arg0->extra)->coords;
    work  = (Actor350700MainWork*)arg0->work;

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
        func_actor_350700_801636A8(arg0, 0x7D3, &preset, 0);
        work->field_4F8 = 0;
        work->field_4FA = 0;
    }

    words          = (Actor350700MatWords*)&coord->coord;
    words->m00_m01 = ONE;
    words->m02_m10 = 0;
    words->m11_m12 = ONE;
    words->m20_m21 = 0;
    words->m22     = ONE;
    RotMatrix(&vec, &coord->coord);
    coord->flg = 0;
}

/// Message-0x7D3 handler of the parent, also called directly by the arrival
/// and turn steps: re-seeds the 20-slot array off bank table
/// `D_actor_350700_801708D8` when the preset's bank index changes, then
/// stores the animation id and restarts or resets every slot with it and ticks
/// them, latching `field_474`. Unlike the enemy actor's handler it reinstalls
/// the animation even when the id is unchanged. Returns 0.
s32 func_actor_350700_801636A8(Task* task, s32 arg1, Actor350700AnimPreset* msg, s32 arg3)
{
    Actor350700MainWork* work;
    TmdObject*           ext;
    s32                  i;

    work = (Actor350700MainWork*)task->work;
    ext  = task->extra;
    if (msg->field_0 != work->field_476) {
        work->field_476 = msg->field_0;
        func_800B3F84(&work->anim, D_actor_350700_801708D8[work->field_476], ext, work->poses,
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

/// Message-0x7D4 handler of the parent: places the root part at `args`, the
/// translation straight into the local matrix and the Euler angles into the
/// coordinate's `rot` slot, from which `RotMatrix` rebuilds the rotation.
/// Returns 0.
s32 func_actor_350700_801637C4(Task* task, s32 msgId, Actor350700Placement* args)
{
    Actor350700Coord* coord;

    coord             = (Actor350700Coord*)((TmdObject*)task->extra)->coords;
    coord->coord.t[0] = args->pos.vx;
    coord->coord.t[1] = args->pos.vy;
    coord->coord.t[2] = args->pos.vz;
    coord->rot.vx     = args->rot.vx;
    coord->rot.vy     = args->rot.vy;
    coord->rot.vz     = args->rot.vz;
    RotMatrix(&coord->rot, &coord->coord);
    coord->flg = 0;
    return 0;
}

/// `Gp_DispatchMsg` handler: the four-way visibility/mode switch on the
/// message's mode word, run against the `TmdObject` parked in `Task::extra`,
/// then the resulting `field_C` flags are republished onto the objects of the
/// three child tasks the spawn handler parked at `field_4FC` / `field_500` /
/// `field_504`. Mode 0 shows the model and clears the 4 flag, 1 hides it, frees
/// the aux buffers and clears the flag, 2 does both plus latching the mode into
/// the work block's `field_508`, and 3 hides it while setting the flag.
/// Anything else returns 1 and leaves the object alone; the handled modes
/// return 0. The same body shape as `func_actor_335800_8016343C`, over one more
/// child.
s32 func_actor_350700_80163840(Task* task, s32 arg1, s32 mode)
{
    Actor350700MainWork* work;
    TmdObject*           obj;
    TmdObject*           objA;
    TmdObject*           objB;
    TmdObject*           objC;
    u16                  flags;
    s32                  ret;

    work = (Actor350700MainWork*)task->work;
    obj  = task->extra;
    objA = work->field_4FC->extra;
    objB = work->field_500->extra;
    objC = work->field_504->extra;
    ret  = 0;
    switch (mode) {
        case 0:
            obj->flags |= 0x80;
            obj->flags &= ~4;
            break;
        case 1:
            obj->flags &= ~0x80;
            Tmd_AllocBuffers(obj);
            obj->flags &= ~4;
            break;
        case 2:
            obj->flags     |= 0x80;
            work->field_508 = mode;
            obj->flags     |= 4;
            break;
        case 3:
            obj->flags &= ~0x80;
            obj->flags |= 4;
            break;
        default:
            ret = 1;
            break;
    }
    flags       = obj->flags;
    objB->flags = flags;
    objA->flags = flags;
    objC->flags = flags;
    return ret;
}

s32 func_actor_350700_8016395C(void)
{
    return 0;
}
