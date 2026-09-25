#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"

#include "actors/actor_104600.h"

/// Steps the first enemy's root one frame along its own facing: saves the
/// current translation in `field_274`, advances X and Z along the rotation's Z
/// column scaled by the step length `field_2BE`, and Y by a fixed 0x80.
void Actor04600_Fn0272C(Task* task)
{
    GsCOORDINATE2*   coord;
    Actor104600Work* work;

    coord              = &((TmdObject*)task->extra)->coords[0];
    work               = (Actor104600Work*)task->work;
    work->field_274.vx = coord->coord.t[0];
    work->field_274.vy = coord->coord.t[1];
    work->field_274.vz = coord->coord.t[2];
    coord->coord.t[0] += (coord->coord.m[0][2] * work->field_2BE) >> 12;
    coord->coord.t[2] += (coord->coord.m[2][2] * work->field_2BE) >> 12;
    coord->coord.t[1] += 0x80;
}

/// Rebinds the first enemy's animation id `field_2B8` to its two helper slots
/// unless `field_2D2` suppresses the rebind. A changed id is remembered in
/// `field_2BA`, the frame count `field_2BC` restarts and both slots switch to
/// it; otherwise the count ticks and the slots advance by one frame.
void Actor04600_Fn027BC(Task* arg0)
{
    Actor104600Work* work;
    s32              i;

    work = (Actor104600Work*)arg0->work;
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
