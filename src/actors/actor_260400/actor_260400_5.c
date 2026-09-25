#include "common.h"

#include <psyq/libgte.h>

#include "actors/actor_260400.h"
#include "gameplay/268.h"
#include "main/gfx.h"
#include "main/tmd.h"

/// Message handler: the payload's halfword at 0x2 selects one of three
/// latches. Case 0 latches the work block's animation reset argument; case 1
/// mirrors the step back out and clears the helper task's model field, but
/// only while `func_800B7420(0x88)` still reports the item unheld; case 2
/// clears the mirror and sets the model field to 0x84.
s32 func_actor_260400_8014AAA4(Task* task, s32 arg1, Actor260400Msg* msg)
{
    TmdObject* obj;
    s32        mode;

    obj  = (TmdObject*)D_actor_260400_80154C70->field_4F0->extra;
    mode = msg->field_2;

    switch (mode) {
        case 0:
            D_actor_260400_80154C70->field_4EC = 0x14;
            break;
        case 1:
            if (func_800B7420(0x88) == 0) {
                D_actor_260400_80154C70->field_4F4 = mode;
                obj->flags                         = 0;
            }
            break;
        case 2:
            D_actor_260400_80154C70->field_4F4 = 0;
            obj->flags                         = 0x84;
            break;
    }
    return 0;
}

/// Approach message handler: turns the model to face `target` -- away from it
/// in mode 1 -- keeps the approach mode in `D_actor_260400_80154C78`, and
/// stores the per-step distance of the walk the update then performs: the
/// planar distance over 60 steps in mode 0, 15 in mode 1 and 25 otherwise.
s32 func_actor_260400_8014AB50(Task* task, s32 arg1, VECTOR* target, s32 mode)
{
    GsCOORDINATE2*   coord;
    Actor260400Work* work;
    s32              dx;
    s32              dz;
    s32              steps;
    s32              dist;
    s32              angle;

    coord                   = ((TmdObject*)task->extra)->coords;
    work                    = (Actor260400Work*)task->work;
    D_actor_260400_80154C78 = mode;
    dx                      = target->vx - coord->coord.t[0];
    dz                      = target->vz - coord->coord.t[2];
    angle                   = ratan2(dx, dz);
    work->yaw               = angle;
    if (D_actor_260400_80154C78 == 1) {
        work->yaw = angle + 0x800;
    }
    Gfx_RotMatrixY(&coord->coord, (s16)work->yaw, 1);
    dist  = SquareRoot0(dx * dx + dz * dz);
    steps = 0x19;
    switch (D_actor_260400_80154C78) {
        case 0:
            steps = 0x3C;
            break;
        case 1:
            steps = 0xF;
            break;
        case 2:
            break;
    }
    work->field_4EA = dist / steps;
    return 0;
}
