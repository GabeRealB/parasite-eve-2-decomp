#include "common.h"

#include <psyq/abs.h>

#include "actors/actor_350700.h"
#include "main/task.h"
#include "main/tmd.h"
#include "gameplay/1BC.h"

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

    coord = ((TmdObject*)arg0->extra)->field_8;
    work  = (Actor350700Work*)arg0->idMap;

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
/// `Gp_UpdateCoordTree` picks up once `flg` is cleared.
void func_actor_350700_80162764(Task* arg0)
{
    Actor350700Work*      work;
    Actor350700MatWords*  words;
    GsCOORDINATE2*        coord;
    SVECTOR               vec;
    Actor350700AnimPreset preset;
    s32                   vy;
    s16                   diff;

    coord = ((TmdObject*)arg0->extra)->field_8;
    work  = (Actor350700Work*)arg0->idMap;

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

/// `Gp_DispatchMsg` handler, the four-way visibility/mode switch of
/// `func_actor_141000_80133E8C` run against the `TmdObject` parked in
/// `Task::extra`. Mode 0 shows the model and clears the 4 flag, 1 hides it,
/// frees the aux buffers and clears the flag, 2 does both plus latching the
/// mode into the work block's `field_4C5`, and 3 hides it while setting the
/// flag. Anything else returns 1 and leaves the object alone; the handled
/// modes return 0.
s32 func_actor_350700_80162A14(Task* task, s32 arg1, s32 mode)
{
    TmdObject* obj;
    s32        ret;

    obj = task->extra;
    ret = 0;
    switch (mode) {
        case 0:
            obj->field_C |= 0x80;
            obj->field_C &= ~4;
            break;
        case 1:
            obj->field_C &= ~0x80;
            Tmd_AllocBuffers(obj);
            obj->field_C &= ~4;
            break;
        case 2:
            obj->field_C                              |= 0x80;
            ((Actor350700Work*)task->idMap)->field_4C5 = mode;
            obj->field_C                              |= 4;
            break;
        case 3:
            obj->field_C &= ~0x80;
            obj->field_C |= 4;
            break;
        default:
            ret = 1;
            break;
    }
    return ret;
}

INCLUDE_ASM("actors/nonmatchings/actor_350700/actor_350700_2", func_actor_350700_80162AF4);

INCLUDE_ASM("actors/nonmatchings/actor_350700/actor_350700_2", func_actor_350700_80162B30);

INCLUDE_ASM("actors/nonmatchings/actor_350700/actor_350700_2", func_actor_350700_80162D5C);

INCLUDE_ASM("actors/nonmatchings/actor_350700/actor_350700_2", func_actor_350700_80162F7C);

INCLUDE_ASM("actors/nonmatchings/actor_350700/actor_350700_2", func_actor_350700_801630C0);

INCLUDE_RODATA("actors/nonmatchings/actor_350700/actor_350700_2", ActorsShared801327f8Table);

INCLUDE_RODATA("actors/nonmatchings/actor_350700/actor_350700_2", ActorsShared80132920Offset);
