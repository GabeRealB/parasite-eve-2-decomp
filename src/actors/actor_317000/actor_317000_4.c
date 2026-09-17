#include "common.h"

#include "actors/actor_317000.h"

#include "gameplay/1BC.h"
#include "main/task.h"
#include "main/tmd.h"

/// Display handler for the `GpMsgEntry` table `func_actor_317000_8016267C`
/// installs, message id 0x7D5 -- the same four-mode protocol on
/// `TmdObject::field_C` that `func_actor_335800_80163FB8` and
/// `func_actor_503500_801466E0` run for their own actors. Bit 0x80 marks the
/// actor hidden (the sibling tick bodies stop drawing the ground shadow while
/// it is set) and bit 0x4 the display buffers being live:
///
///   mode 0  hide, drop 0x4
///   mode 1  show, `Tmd_AllocBuffers`, drop 0x4
///   mode 2  hide, latch the frame countdown `Actor317000Work::field_4C8` that
///           ends in `Tmd_FreeBuffers`, raise 0x4
///   mode 3  show, raise 0x4
///
/// The spawn state opens with mode 0 and seeds `field_4C8` to -1. Any other
/// mode returns 1; the four known ones return 0.
s32 func_actor_317000_80162BC4(Task* task, s32 arg1, s32 mode, s32 arg3)
{
    TmdObject*       obj;
    Actor317000Work* work;
    s32              ret;

    obj  = task->extra;
    work = (Actor317000Work*)task->idMap;
    ret  = 0;
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
            obj->field_C   |= 0x80;
            work->field_4C8 = mode;
            obj->field_C   |= 4;
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

    work  = (Actor317000Work*)task->idMap;
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
