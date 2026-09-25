#include "common.h"

#include <psyq/abs.h>

#include "actors/actor_317000.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"

#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

s32  func_actor_317000_80162A10(Task* task, s32 arg1, Actor317000AnimPreset* msg, s32 arg3);
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// The four step handlers `func_actor_317000_80162768` runs by
/// `Actor317000Work::field_4C2`.
extern TaskFuncTable4 D_actor_317000_80161E30;

void func_actor_317000_80162744(Task* arg0)
{
    TmdObject*       ext;
    Actor317000Work* work;

    ext           = arg0->extra;
    work          = (Actor317000Work*)arg0->work;
    ext->lightMtx = &work->light;
    ext->colorMtx = &work->color;
}

void func_actor_317000_80162760(void)
{
}

/// Index 1 of the two-entry table `func_actor_317000_80161E68` dispatches on
/// `Actor317000Work::field_4C0`: copies the four step handlers
/// `D_actor_317000_80161E30` onto the stack and runs the one
/// `Actor317000Work::field_4C2` selects, read sign-extended.
void func_actor_317000_80162768(Task* arg0)
{
    TaskFuncTable4   sp;
    Actor317000Work* work;

    work = (Actor317000Work*)arg0->work;
    sp   = D_actor_317000_80161E30;
    sp.funcs[(s16)work->field_4C2](arg0);
}

/// State handler at index 1 of `D_actor_317000_80161E30`: Euler-extracts the
/// root coordinate into `vec`, and while the yaw gap to the target
/// `work->field_4BA` is at least 0x41 it steps `vec.vy` toward it by 0x40 --
/// the step is taken on an `s32` widening of the extracted yaw -- and
/// otherwise snaps the yaw to the target and plays anim 0x7D3 with a preset
/// whose `field_4` is the literal 2, clearing the `field_4C4` flag and
/// advancing `field_4C2`. Either way the root coordinate is rebuilt as the
/// identity matrix rotated by `vec`. The threshold variant of
/// `func_actor_350700_80162764` / `ActorsShared80132920`, which step by 0x60.
void func_actor_317000_801627D0(Task* arg0)
{
    Actor317000Work*      work;
    Actor317000MatWords*  words;
    GsCOORDINATE2*        coord;
    SVECTOR               vec;
    Actor317000AnimPreset preset;
    s32                   vy;
    s16                   diff;

    coord = ((TmdObject*)arg0->extra)->coords;
    work  = (Actor317000Work*)arg0->work;

    Gp_ExtractEuler(&vec, &coord->coord);
    diff = (u16)work->field_4BA - (u16)vec.vy;
    if (ABS(diff) >= 0x41) {
        vy = vec.vy;
        if (diff < 0) {
            vec.vy = vy - 0x40;
        } else {
            vec.vy = vy + 0x40;
        }
    } else {
        vec.vy          = work->field_4BA;
        preset.field_0  = 0;
        preset.field_4  = 2;
        preset.field_8  = 1;
        preset.field_C  = 5;
        preset.field_10 = 0;
        func_actor_317000_80162A10(arg0, 0x7D3, &preset, 0);
        work->field_4C4 = 0;
        work->field_4C2++;
    }

    words          = (Actor317000MatWords*)&coord->coord;
    words->m00_m01 = ONE;
    words->m02_m10 = 0;
    words->m11_m12 = ONE;
    words->m20_m21 = 0;
    words->m22     = ONE;
    RotMatrix(&vec, &coord->coord);
    coord->flg = 0;
}

/// State handler at index 1 of `D_actor_317000_80161E30`, reached by the
/// `field_4C2` advance this body ends with: rotates the constant local-space
/// offset `D_actor_317000_80161E40` through the root part's matrix into
/// `work->step`, then raises the flag at 0x4C4 and moves the dispatcher on.
/// The same body as `func_actor_335800_80163CA0` / `ActorsShared80132920`,
/// minus their per-axis stop threshold.
void func_actor_317000_801628D8(Task* task)
{
    Actor317000Work* work;
    GsCOORDINATE2*   coord;
    VECTOR           vec;

    coord = ((TmdObject*)task->extra)->coords;
    work  = (Actor317000Work*)task->work;

    vec = D_actor_317000_80161E40;
    ApplyMatrixLV(&coord->coord, &vec, (VECTOR*)&work->step);
    work->field_4C4 = 1;
    work->field_4C2++;
}

/// State handler at index 2 of `D_actor_317000_80161E30`, the step after
/// `func_actor_317000_801628D8` and reached by the `field_4C2` advance that
/// body ends with. While the root coordinate's Y is below -0x30 it does
/// nothing; above it the rise is over: the local-space `work->step` the
/// previous body wrote is cleared, the animation is re-applied through message
/// 0x7D3 with the latched `field_43F` state, and sound 0x400A000B is queued
/// panned and attenuated from the root coordinate's matrix. The `field_4C4`
/// flag the previous body raised is cleared and the dispatcher advances again.
void func_actor_317000_80162950(Task* arg0)
{
    GsCOORDINATE2*        coord;
    Actor317000Work*      work;
    Actor317000AnimPreset preset;
    s32                   pan;

    coord = ((TmdObject*)arg0->extra)->coords;
    work  = (Actor317000Work*)arg0->work;
    if (coord->coord.t[1] < -0x30) {
        return;
    }
    preset.field_0  = 0;
    preset.field_4  = work->field_43F;
    preset.field_8  = 1;
    preset.field_C  = 5;
    preset.field_10 = 0;
    func_actor_317000_80162A10(arg0, 0x7D3, &preset, 0);
    pan = (s8)Gp_GetObjPan(coord);
    SndEvt_EnqueueType6(0x400A000B, pan, (s8)gpGetObjDepth(coord));

    work->step.vx   = 0;
    work->step.vy   = 0;
    work->step.vz   = 0;
    work->field_4C4 = 0;
    work->field_4C2++;
}

/// Message 0x7D3 handler of `D_actor_317000_8016CF50`, also called directly by
/// the step handlers `func_actor_317000_801627D0` and
/// `func_actor_317000_80162950` with presets of their own. A preset `field_0`
/// that differs from `Actor317000Work::field_43E` latches it, forgets the
/// current animation (`field_43D` = -1) and re-seeds the slots through
/// `func_800B3F84` from `D_actor_317000_8016CF40[field_43E]`. A `field_4` that
/// differs from `field_43D` latches it and installs it on slots 1..0x12 --
/// through `func_800B4114` with `field_C` when the preset's `field_8` is set and
/// `field_43C` says the slots are already ticking, through `Gp_AnimResetSlot`
/// otherwise -- then ticks each slot once and raises `field_43C`. Returns 0.
s32 func_actor_317000_80162A10(Task* task, s32 arg1, Actor317000AnimPreset* msg, s32 arg3)
{
    Actor317000Work* work;
    TmdObject*       ext;
    s32              i;

    work = (Actor317000Work*)task->work;
    ext  = task->extra;
    if (msg->field_0 != work->field_43E) {
        work->field_43E = msg->field_0;
        work->field_43D = -1;
        func_800B3F84(&work->ctx, D_actor_317000_8016CF40[work->field_43E], ext, work->poses, work->slots);
    }
    if (msg->field_4 != work->field_43D) {
        work->field_43D = msg->field_4;
        if (msg->field_8 != 0 && work->field_43C != 0) {
            for (i = 1; i < 0x13; i++) {
                func_800B4114(&work->ctx, i, work->field_43D, 0, msg->field_C);
            }
        } else {
            for (i = 1; i < 0x13; i++) {
                Gp_AnimResetSlot(&work->ctx, i, work->field_43D);
            }
        }
        for (i = 1; i < 0x13; i++) {
            Gp_AnimTickIndex(&work->ctx, i);
        }
        work->field_43C = 1;
    }
    return 0;
}

INCLUDE_RODATA("actors/nonmatchings/actor_317000/actor_317000_3", D_actor_317000_80161E50);

INCLUDE_RODATA("actors/nonmatchings/actor_317000/actor_317000_3", D_actor_317000_80161E60);

INCLUDE_RODATA("actors/nonmatchings/actor_317000/actor_317000_3", D_actor_317000_80161E64);
