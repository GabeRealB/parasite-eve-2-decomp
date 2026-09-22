#include "common.h"

#include "actors/actor_104000.h"
#include "main/task.h"
#include "main/tmd.h"

/// Selects the display object's visibility bits and the work block's state from
/// a mode: 0 shows the object (0x80) and sets state 7, 1 hides it and sets state
/// 7, 2 raises the lost-model flag (4) and clears the state, 3 hides it, clears
/// the state and *then* raises the flag, and any other mode leaves both alone.
/// Case 3's two writes are the original's: the work store between them is what
/// keeps the second read from folding back into the first.
///
/// Shared: the three actor slots (`actor_104000` / `204000` / `304000`) carry
/// the same body byte for byte, so one object serves every overlay that lists
/// this unit in `configs/USA/overlays.toml`.
s32 ActorsShared801503b0(Task* task, s32 arg1, s32 arg2)
{
    Actor104000Work* work;
    TmdObject*       obj;

    obj  = task->extra;
    work = (Actor104000Work*)task->work;

    switch (arg2) {
        case 0:
            obj->flags    = 0x80;
            work->field_0 = 7;
            break;
        case 1:
            obj->flags    = 0;
            work->field_0 = 7;
            break;
        case 2:
            obj->flags    = (u16)(obj->flags | 4);
            work->field_0 = 0;
            break;
        case 3:
            obj->flags    = 0;
            work->field_0 = 0;
            obj->flags    = (u16)(obj->flags | 4);
            break;
        default:
            return 0;
    }
    return 0;
}
