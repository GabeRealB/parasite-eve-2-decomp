#include "common.h"

#include <psyq/abs.h>

#include "actors/actor_141000.h"

#include "main/task.h"
#include "main/tmd.h"

/// State-1 handler of the actor's main-body table `D_actor_141000_80131E58`,
/// the step after the shared turn-to-face body. Rotates the constant
/// local-space offset `D_actor_141000_80131E68` through the root part's matrix
/// into `work->step`, halving it first while `field_4C8` is clear -- the
/// variant `func_actor_141000_80133F6C` latches through message 0x7DB -- then
/// opens the per-axis stop threshold to 0x7FFF, which disables it for the
/// update loop, and advances the state.
void func_actor_141000_80133B28(Task* arg0)
{
    Actor141000Work* work;
    GsCOORDINATE2*   coord;
    VECTOR           vec;

    coord = ((TmdObject*)arg0->extra)->coords;
    work  = (Actor141000Work*)arg0->work;

    vec = D_actor_141000_80131E68;
    if (work->field_4C8 == 0) {
        vec.vx >>= 1;
        vec.vy >>= 1;
        vec.vz >>= 1;
    }
    ApplyMatrixLV(&coord->coord, &vec, (VECTOR*)&work->step);
    work->limit.vx  = 0x7FFF;
    work->limit.vy  = 0x7FFF;
    work->limit.vz  = 0x7FFF;
    work->field_4C2 = work->field_4C2 + 1;
}

/// The main body's turn-to-face handler, third in `D_actor_141000_80131E58`
/// after the shared body. Euler-extracts the root coordinate into `vec`, and
/// while the yaw gap to the target `work->field_4BA` is at least 0x41 it steps
/// `vec.vy` toward it by 0x40 -- the step is taken on an `s32` widening of the
/// extracted yaw, which the shared tail adds to -- and otherwise snaps the yaw
/// to the target and plays anim 0x7D3 with a preset carrying the `field_43F`
/// byte, clearing the two body counters. Either way the root coordinate is
/// rebuilt as the identity matrix rotated by `vec`.
void func_actor_141000_80133BD8(Task* arg0)
{
    Actor141000Work*      work;
    Actor141000MatWords*  words;
    GsCOORDINATE2*        coord;
    SVECTOR               vec;
    Actor141000AnimPreset preset;
    s32                   vy;
    s16                   diff;

    coord = ((TmdObject*)arg0->extra)->coords;
    work  = (Actor141000Work*)arg0->work;

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
        preset.field_4  = work->field_43F;
        preset.field_8  = 1;
        preset.field_C  = 5;
        preset.field_10 = 0;
        func_actor_141000_80133CD8(arg0, 0x7D3, &preset, 0);
        work->field_4C0 = 0;
        work->field_4C2 = 0;
    }

    words          = (Actor141000MatWords*)&coord->coord;
    words->m00_m01 = ONE;
    words->m02_m10 = 0;
    words->m11_m12 = ONE;
    words->m20_m21 = 0;
    words->m22     = ONE;
    RotMatrix(&vec, &coord->coord);
    coord->flg = 0;
}

void         func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
extern void* D_actor_141000_8013D778[];

/// Message-0x7D3 handler, also called directly by the turn-to-face and
/// approach states with a preset of their own. A changed bank index re-seeds
/// the whole animation slot array through `func_800B3F84` from the bank table
/// and forgets the current animation id. A changed animation id is then stored
/// and installed on every slot - through `func_800B4114` when the preset's
/// `field_8` is set and the slots have already been started, through
/// `Gp_AnimResetSlot` otherwise - after which every slot is ticked once and
/// `field_43C` latches. An unchanged id skips all of that. Returns 0.
s32 func_actor_141000_80133CD8(Task* task, s32 arg1, Actor141000AnimPreset* msg, s32 arg3)
{
    Actor141000Work* work;
    TmdObject*       ext;
    s32              i;

    work = (Actor141000Work*)task->work;
    ext  = task->extra;
    if (msg->field_0 != work->field_43E) {
        work->field_43E = msg->field_0;
        work->field_43D = -1;
        func_800B3F84(&work->anim, D_actor_141000_8013D778[work->field_43E], ext, work->poses, work->slots);
    }
    if (msg->field_4 != work->field_43D) {
        work->field_43D = msg->field_4;
        if (msg->field_8 != 0 && work->field_43C != 0) {
            for (i = 1; i < 0x13; i++) {
                func_800B4114(&work->anim, i, work->field_43D, 0, msg->field_C);
            }
        } else {
            for (i = 1; i < 0x13; i++) {
                Gp_AnimResetSlot(&work->anim, i, work->field_43D);
            }
        }
        for (i = 1; i < 0x13; i++) {
            Gp_AnimTickIndex(&work->anim, i);
        }
        work->field_43C = 1;
    }
    return 0;
}
