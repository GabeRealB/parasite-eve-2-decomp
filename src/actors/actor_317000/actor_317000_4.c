#include "common.h"

#include "actors/actor_317000.h"

#include "gameplay/1BC.h"
#include "main/task.h"
#include "main/tmd.h"

/// Message 0x7DB handler of the table `func_actor_317000_8016267C` installs:
/// latches the payload's halfword at 0x2 into `Actor317000Work::field_4C5` --
/// 0 for mode 0, the mode itself for mode 1 -- and for any other mode dumps the
/// root coordinate's matrix translation (`"pos"`) and the euler angles
/// `Gp_ExtractEuler` derives from its rotation matrix (`"rot"`) through
/// `GPU_printf`, both under the overlay's `"%s=(%d,%d,%d)\n"` format. Returns 0
/// either way.
s32 func_actor_317000_80162CA0(Task* task, s32 arg1, Actor317000Msg* msg)
{
    Actor317000Work* work;
    GsCOORDINATE2*   coord;
    SVECTOR          rot;
    s32              mode;

    work  = (Actor317000Work*)task->work;
    coord = ((TmdObject*)task->extra)->field_8;
    mode  = msg->field_2;
    switch (mode) {
        case 0:
            work->field_4C5 = 0;
            break;
        case 1:
            work->field_4C5 = mode;
            break;
        default:
            GPU_printf(D_actor_317000_80161E50, D_actor_317000_80161E60,
                       coord->coord.t[0], coord->coord.t[1], coord->coord.t[2]);
            Gp_ExtractEuler(&rot, &coord->coord);
            GPU_printf(D_actor_317000_80161E50, D_actor_317000_80161E64,
                       rot.vx, rot.vy, rot.vz);
            break;
    }
    return 0;
}
