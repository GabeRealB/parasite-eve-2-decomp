#include "common.h"

#include <psyq/abs.h>

#include "actors/actor_350700.h"
#include "actors/actors_shared_801327f8.h"
#include "actors/actors_shared_80132f24.h"
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

/// State handler at index 1 of `D_actor_350700_80161E30`, the move body that
/// mirrors `ActorsShared80132920`: rotates the constant local-space offset
/// `D_actor_350700_80161E40` through the root part's matrix into `work->step`,
/// opens the per-axis stop threshold to 0x7FFF, which disables it for the
/// update loop, and advances `field_4C2` so the dispatcher runs the next
/// handler. Where the shared body rotates the offset unchanged, this one
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
/// that mirrors `ActorsShared80162540`. Euler-extracts the root coordinate into
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

INCLUDE_ASM("actors/nonmatchings/actor_350700/actor_350700_2", func_actor_350700_80162860);

INCLUDE_ASM("actors/nonmatchings/actor_350700/actor_350700_2", func_actor_350700_80162998);
