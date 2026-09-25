#include "common.h"

#include <psyq/abs.h>

#include "actors/actor_350700.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

/// The four `TaskDesc`s `func_actor_350700_80162B30` spawns its child tasks
/// from, and the message table it points the parent's `Task::msgTable` at:
/// ids 0x7D3/0x7D4/0x7D5/0x7DD/0x7DB against the handlers starting
/// `func_actor_350700_801636A8`, terminated by 0x7FFFFFFF.
extern TaskDesc   D_actor_350700_801708DC;
extern GpMsgEntry D_actor_350700_8017090C[];

/// Animation bank table of the enemy actor.
extern void* D_actor_350700_80169D0C[];

void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// State handler at index 1 of `D_actor_350700_80161E30`, the move body that
/// mirrors the parent's `func_actor_350700_80163528`: rotates the constant local-space offset
/// `D_actor_350700_80161E40` through the root part's matrix into `work->step`,
/// opens the per-axis stop threshold to 0x7FFF, which disables it for the
/// update loop, and advances `field_4C2` so the dispatcher runs the next
/// handler. Where the parent's step rotates its offset unchanged, this one
/// shrinks it to -0.4 of its length whenever `field_4C4` is clear.
void func_actor_350700_8016261C(Task* arg0)
{
    Actor350700Work* work;
    GsCOORDINATE2*   coord;
    VECTOR           vec;

    coord = ((TmdObject*)arg0->extra)->coords;
    work  = (Actor350700Work*)arg0->work;

    vec = D_actor_350700_80161E40;
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

/// State handler at index 3 of `D_actor_350700_80161E30`, the turn-to-face body
/// that follows `func_actor_350700_80162540`. Euler-extracts the root coordinate into
/// `vec`, and while the yaw gap to the target `work->field_4BA` is at least
/// 0x61 it steps `vec.vy` toward it by 0x60 -- the step is taken on an `s32`
/// widening of the extracted yaw -- and otherwise snaps the yaw to the target
/// and plays anim 0x7D3, clearing the two body counters. Either way the root
/// coordinate is rebuilt as the identity matrix rotated by `vec`, which
/// `_gpUpdateCoordTree` picks up once `flg` is cleared.
void func_actor_350700_80162764(Task* arg0)
{
    Actor350700Work*      work;
    Actor350700MatWords*  words;
    GsCOORDINATE2*        coord;
    SVECTOR               vec;
    Actor350700AnimPreset preset;
    s32                   vy;
    s16                   diff;

    coord = ((TmdObject*)arg0->extra)->coords;
    work  = (Actor350700Work*)arg0->work;

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
        func_actor_350700_80162860(arg0, 0x7D3, &preset, 0);
        work->field_4C0 = 0;
        work->field_4C2 = 0;
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

/// Message-0x7D3 handler of the enemy actor, also called directly by the
/// approach and turn-to-face handlers with a preset of their own. A changed
/// bank index re-seeds the whole animation slot array through `func_800B3F84`
/// from the bank table and forgets the current animation id. A changed
/// animation id is then stored and installed on every slot - through
/// `func_800B4114` when the preset's `field_8` is set and the slots have
/// already been started, through `Gp_AnimResetSlot` otherwise - after which
/// every slot is ticked once and `field_43C` latches. An unchanged id skips all
/// of that. Returns 0.
s32 func_actor_350700_80162860(Task* task, s32 arg1, Actor350700AnimPreset* msg, s32 arg3)
{
    Actor350700Work* work;
    TmdObject*       ext;
    s32              i;

    work = (Actor350700Work*)task->work;
    ext  = task->extra;
    if (msg->field_0 != work->field_43E) {
        work->field_43E = msg->field_0;
        work->field_43D = -1;
        func_800B3F84(&work->anim, D_actor_350700_80169D0C[work->field_43E], ext, work->poses, work->slots);
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

/// Message-0x7D4 handler of the enemy actor: places the root part at
/// `args`. The translation goes straight into the local matrix, the Euler
/// angles into the coordinate's `rot` slot, from which `RotMatrix` rebuilds the
/// rotation; clearing `flg` makes the world matrix be recomputed. Returns 0.
s32 func_actor_350700_80162998(Task* task, s32 msgId, Actor350700Placement* args)
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
