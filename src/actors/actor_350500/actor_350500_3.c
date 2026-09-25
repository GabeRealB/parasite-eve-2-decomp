#include "common.h"

#include <psyq/libgte.h>
#include <psyq/abs.h>

#include "actors/actor_350500.h"

#include "gameplay/1BC.h"
#include "main/task.h"
#include "main/tmd.h"

/// A `MATRIX`'s word-wise view, for the identity splat
/// `func_actor_350500_8016272C` writes over the root coordinate before
/// `RotMatrix` overwrites the 3x3: five aligned stores rather than nine
/// halfword ones.
typedef struct Actor350500MatWords {
    /* 0x00 */ s32 m00_m01;
    /* 0x04 */ s32 m02_m10;
    /* 0x08 */ s32 m11_m12;
    /* 0x0C */ s32 m20_m21;
    /* 0x10 */ s16 m22;
} Actor350500MatWords;
STATIC_ASSERT_SIZEOF(Actor350500MatWords, 0x14);

/// Local-space offset the start-moving step rotates: straight ahead along
/// the part's own +Z.
const VECTOR D_actor_350500_80161E40 = { 0, 0, 0x200000, 0 };

/// Walk step 1: rotates `D_actor_350500_80161E40` -- shrunk to -0.4 of its
/// length, a slower backward walk, while `field_4C4` is clear -- through the
/// root part's matrix into `work->step`, opens the per-axis stop threshold to
/// 0x7FFF, which disables it for the approach test, and advances `field_4C2`.
void func_actor_350500_801625E4(Task* arg0)
{
    Actor350500Work* work;
    GsCOORDINATE2*   coord;
    VECTOR           vec;

    coord = ((TmdObject*)arg0->extra)->coords;
    work  = (Actor350500Work*)arg0->work;

    vec = D_actor_350500_80161E40;
    if (work->field_4C4 == 0) {
        vec.vx = vec.vx * -0.4;
        vec.vy = vec.vy * -0.4;
        vec.vz = vec.vz * -0.4;
    }
    ApplyMatrixLV(&coord->coord, &vec, (VECTOR*)&work->step);
    work->limit.vx = 0x7FFF;
    work->limit.vy = 0x7FFF;
    work->limit.vz = 0x7FFF;
    work->field_4C2++;
}

/// Walk step 3, the final turn. Euler-extracts the root coordinate into
/// `vec`, and while the yaw gap to the placement yaw `field_4BA` is at least
/// 0x61 steps `vec.vy` toward it by 0x60; otherwise snaps the yaw to it,
/// plays anim 1 and clears `field_4C0` and `field_4C2`, which returns the
/// tick to idle. Either way the root coordinate is rebuilt as the identity
/// rotated by `vec` and its `flg` cleared.
void func_actor_350500_8016272C(Task* arg0)
{
    Actor350500Work*      work;
    Actor350500MatWords*  words;
    GsCOORDINATE2*        coord;
    SVECTOR               vec;
    Actor350500AnimPreset preset;
    s32                   vy;
    s16                   diff;

    coord = ((TmdObject*)arg0->extra)->coords;
    work  = (Actor350500Work*)arg0->work;

    Gp_ExtractEuler(&vec, &coord->coord);
    diff = (u16)work->field_4BA - (u16)vec.vy;
    if (ABS(diff) >= 0x61) {
        vy = vec.vy;
        if (diff < 0) {
            vec.vy = vy - 0x60;
        } else {
            vec.vy = vy + 0x60;
        }
    } else {
        vec.vy          = work->field_4BA;
        preset.field_0  = 0;
        preset.field_4  = 1;
        preset.field_8  = 1;
        preset.field_C  = 4;
        preset.field_10 = 0;
        func_actor_350500_80162828(arg0, 0x7D3, &preset, 0);
        work->field_4C0 = 0;
        work->field_4C2 = 0;
    }

    words          = (Actor350500MatWords*)&coord->coord;
    words->m00_m01 = ONE;
    words->m02_m10 = 0;
    words->m11_m12 = ONE;
    words->m20_m21 = 0;
    words->m22     = ONE;
    RotMatrix(&vec, &coord->coord);
    coord->flg = 0;
}

/// Message-0x7D3 handler, also called directly by the approach and final-turn
/// steps with a preset of their own. A changed bank index re-seeds the
/// animation slot array through `func_800B3F84` from the bank table and
/// forgets the current animation id. A changed animation id is then stored and
/// installed on every slot - through `func_800B4114` when the preset's
/// `field_8` is set and the slots have already been started, through
/// `Gp_AnimResetSlot` otherwise - after which every slot is ticked once and
/// `field_43C` latches. An unchanged id skips all of that. Returns 0.
s32 func_actor_350500_80162828(Task* task, s32 arg1, Actor350500AnimPreset* msg, s32 arg3)
{
    Actor350500Work* work;
    TmdObject*       ext;
    s32              i;

    work = (Actor350500Work*)task->work;
    ext  = task->extra;
    if (msg->field_0 != work->field_43E) {
        work->field_43E = msg->field_0;
        work->field_43D = -1;
        func_800B3F84(&work->anim, D_actor_350500_80168EA0[work->field_43E], ext, work->poses, work->slots);
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
