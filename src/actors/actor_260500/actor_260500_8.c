#include "common.h"

#include "actors/actor_260500.h"
#include "main/task.h"
#include "main/tmd.h"

/// Approach mode the last `func_actor_260500_8014A83C` call selected.
extern s16 D_actor_260500_80159E54;

/// Turns the model to face `target` -- away from it in mode 1 -- and stores the
/// per-step distance in the work block: the planar distance over 60 steps in
/// mode 0, 15 in mode 1 and 25 in mode 2.
s32 func_actor_260500_8014A83C(Task* task, s32 arg1, VECTOR* target, s32 mode)
{
    GsCOORDINATE2*   coord;
    Actor260500Work* work;
    s32              steps;
    s32              dx;
    s32              dz;
    s32              dist;
    s32              angle;

    steps                   = 0;
    coord                   = ((TmdObject*)task->extra)->field_8;
    work                    = (Actor260500Work*)task->idMap;
    D_actor_260500_80159E54 = mode;
    dx                      = target->vx - coord->coord.t[0];
    dz                      = target->vz - coord->coord.t[2];
    angle                   = ratan2(dx, dz);
    work->field_4AE         = angle;
    if (D_actor_260500_80159E54 == 1) {
        work->field_4AE = angle + 0x800;
    }
    Gfx_RotMatrixY(&coord->coord, work->field_4AE, 1);
    dist = SquareRoot0(dx * dx + dz * dz);
    switch (D_actor_260500_80159E54) {
        case 0:
            steps = 0x3C;
            break;
        case 1:
            steps = 0xF;
            break;
        case 2:
            steps = 0x19;
            break;
    }
    work->field_4B2 = dist / steps;
    return 0;
}
