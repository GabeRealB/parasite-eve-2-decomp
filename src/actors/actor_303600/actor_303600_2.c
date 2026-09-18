#include "common.h"

#include "actors/actor_303600.h"
#include "actors/actors_shared_80162850.h"
#include "gameplay/D4.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

extern TaskDesc   D_actor_303600_8016E468[];
extern GpMsgEntry D_actor_303600_8016E480[];

/// Spawn state of the overlay's rig controller: allocates the work block the
/// later states read through `Task::work` (`Mem_Calloc(0x3C, 0)`, the struct's
/// own size), clears the task's own root coordinate, then spawns the five child
/// models -- one `Task_SpawnFromTable` of `D_actor_303600_8016E468` entry 1
/// each, parked in `children` and spread 8000 apart in Y.  The spread reaches
/// the coordinate through the strength-reduced `i * 8000 - 16000` loop.c folds
/// into an accumulator, so its initialiser is scheduled at the loop head beside
/// the hoisted `%hi` of the spawn table.  A failed spawn stops the loop early, a
/// failed allocation kills the task instead of leaving a half-built controller,
/// and the last three statements install the 0x7DB handler table at
/// `Task::msgTable`, the shared kill callback and the next state.
void func_actor_303600_801626C0(Task* task)
{
    Actor303600RigWork* work;
    GsCOORDINATE2*      coord;
    GsCOORDINATE2*      childCoord;
    Task*               child;
    s32                 i;

    work = Mem_Calloc(0x3C, 0);
    if (work == NULL) {
        Task_Kill(task);
        return;
    }
    task->work        = (TaskIdMap*)work;
    coord             = ((TmdObject*)task->extra)->field_8;
    coord->coord.t[0] = 0;
    coord->coord.t[1] = 0;
    coord->coord.t[2] = 0;
    for (i = 0; i < 5; i++) {
        child = Task_SpawnFromTable(D_actor_303600_8016E468, 1, 0, (s32)task);
        if (child == NULL) {
            break;
        }
        work->children[i]      = child;
        childCoord             = ((TmdObject*)child->extra)->field_8;
        childCoord->coord.t[1] = i * 0x1F40 - 0x3E80;
        childCoord->coord.t[0] = 0;
        childCoord->coord.t[2] = 0;
    }
    task->msgTable     = D_actor_303600_8016E480;
    task->exitCallback = ActorsShared80162850;
    task->state       += 1;
}

/// Per-frame rig motion, run on the work block `func_actor_303600_801626C0`
/// fills in: ramp the 16.16 speed `field_28` toward the limit `field_38` at
/// `field_34` a frame, drop the ramp once the speed passes the limit in the
/// ramp's own direction, integrate the speed into the angle accumulator
/// `field_18`, fold that back into +/-4000, and publish its integer half as the
/// model coordinate's Y.  The accel is read once for the sum and once for the
/// limit test -- the second read is the branch's own copy of it in the target.
void func_actor_303600_801627B8(Task* task)
{
    Actor303600RigWork* work  = (Actor303600RigWork*)task->work;
    GsCOORDINATE2*      coord = ((TmdObject*)task->extra)->field_8;
    s32                 speed;
    s32                 angle;
    s32                 var;

    speed          = work->field_28 + work->field_34;
    work->field_28 = speed;
    if (work->field_34 > 0) {
        var = speed > work->field_38;
    } else {
        var = speed < work->field_38;
    }
    if (var != 0) {
        work->field_34 = 0;
    }
    angle            = work->field_18.w + work->field_28;
    work->field_18.w = angle;
    if (angle > 0x0FA00000) {
        work->field_18.w = angle - 0x1F400000;
    } else if (angle < -0x0FA00000) {
        work->field_18.w = angle + 0x1F400000;
    }
    coord->coord.t[1] = work->field_18.half.hi;
    coord->flg        = 0;
}
