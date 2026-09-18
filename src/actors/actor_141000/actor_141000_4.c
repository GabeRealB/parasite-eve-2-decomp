#include "common.h"

#include <psyq/abs.h>

#include "actors/actor_141000.h"

#include "main/task.h"
#include "main/tmd.h"

void func_actor_141000_80133CD8(Task* arg0, s32 arg1, Actor141000AnimPreset* arg2, s32 arg3);

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

INCLUDE_ASM("actors/nonmatchings/actor_141000/actor_141000_4", func_actor_141000_80133CD8);
