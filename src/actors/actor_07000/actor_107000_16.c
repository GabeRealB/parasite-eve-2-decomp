#include "common.h"

#include "actors/actor_107000.h"
#include "gameplay/1BC.h"
#include "main/task.h"
#include "main/tmd.h"

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// Steps the specimen's root part one frame along its own facing: saves the
/// current translation in `field_274` as the previous position, then advances
/// X and Z along the rotation's Z column scaled by the step length
/// `field_2BE`, and Y by a fixed 0x80.
void Actor07000_Fn027D0(Task* task)
{
    GsCOORDINATE2*   coord;
    Actor107000Work* work;

    coord              = &((TmdObject*)task->extra)->coords[0];
    work               = (Actor107000Work*)task->work;
    work->field_274.vx = coord->coord.t[0];
    work->field_274.vy = coord->coord.t[1];
    work->field_274.vz = coord->coord.t[2];
    coord->coord.t[0] += (coord->coord.m[0][2] * work->field_2BE) >> 12;
    coord->coord.t[2] += (coord->coord.m[2][2] * work->field_2BE) >> 12;
    coord->coord.t[1] += 0x80;
}

/// Rebinds the animation id `field_2B8` to the specimen's two helper slots
/// unless `field_2D2` suppresses the rebind. When the id has changed since the
/// last frame `field_2BA` follows it, the frame count `field_2BC` restarts and
/// both slots are pointed at the new id; otherwise the count ticks and the
/// slots advance by one frame.
void Actor07000_Fn02860(Task* arg0)
{
    Actor107000Work* work;
    s32              i;

    work = (Actor107000Work*)arg0->work;
    if (work->field_2D2 == 0) {
        i = 1;
        if (work->field_2B8 != work->field_2BA) {
            work->field_2BA = work->field_2B8;
            work->field_2BC = 0;
            do {
                func_800B4114((GpAnimCtx*)work, i, work->field_2B8, 0, 0);
                i++;
            } while (i < 3);
            return;
        }
        TOUCH_REG(i);
        work->field_2BC = (u16)(work->field_2BC + i);
        do {
            Gp_AnimTickIndex((GpAnimCtx*)work, i);
            i++;
        } while (i < 3);
    }
}
