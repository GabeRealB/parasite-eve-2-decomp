#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/gameplay.h"

#include "main/task.h"
#include "main/tmd.h"

/// Work block parked at `Task::idMap` by the factory rooms that link this
/// body. `light` / `color` are the model's own matrices, republished onto
/// `TmdObject::field_1C` / `field_20`. Prefix fields are unreferenced here.
typedef struct RoomUtil20Work {
    /* 0x00 */ byte   pad_0[0x18];
    /* 0x18 */ MATRIX light;
    /* 0x38 */ MATRIX color;
} RoomUtil20Work;

/// Binds the task's TMD object to the work-block light/color matrices, clears
/// the root coordinate flag, and rebuilds lighting from the world translation.
void Room_Util20(Task* task)
{
    GsCOORDINATE2*  coord;
    RoomUtil20Work* work;
    TmdObject*      extra;

    work            = (RoomUtil20Work*)task->idMap;
    extra           = (TmdObject*)task->extra;
    coord           = extra->field_8;
    extra->field_1C = &work->light;
    extra->field_20 = &work->color;
    coord->flg      = 0;
    Gp_UpdateCoord(coord);
    func_800D7A9C(extra, (VECTOR*)coord->workm.t, 0, 3);
}
