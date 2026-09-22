#include "common.h"
#include "gameplay/3A34.h"
#include "gameplay/gameplay.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/gameflag.h"
#include "main/mem.h"

/// Work block parked at `Task::work` by the factory rooms that link this
/// body. `light` / `color` are the model's own matrices, republished onto
/// `TmdObject::lightMtx` / `field_20`. Prefix fields are unreferenced here.
typedef struct RoomUtil20Work {
    /* 0x00 */ byte   pad_0[0x18];
    /* 0x18 */ MATRIX light;
    /* 0x38 */ MATRIX color;
} RoomUtil20Work;

/// Cutscene work block the linking rooms' factory task allocates as 0xC zeroed
/// bytes in its state 0 and parks at `Task::work` (0x1C) -- that slot is *not*
/// a `TaskIdMap` here. The same three bytes `RoomsShared8017fdc8` reads.
typedef struct {
    /* 0x0 */ byte pad_0[0x8];
    /* 0x8 */ u8   state;
    /* 0x9 */ u8   step;
    /* 0xA */ u8   prevFlag;
    /* 0xB */ byte pad_B[0x1];
} RoomsShared8017fbf4Work;
STATIC_ASSERT_SIZEOF(RoomsShared8017fbf4Work, 0xC);

/// Binds the task's TMD object to the work-block light/color matrices, clears
/// the root coordinate flag, and rebuilds lighting from the world translation.
void Room_Util20(Task* task)
{
    GsCOORDINATE2*  coord;
    RoomUtil20Work* work;
    TmdObject*      extra;

    work            = (RoomUtil20Work*)task->work;
    extra           = (TmdObject*)task->extra;
    coord           = extra->coords;
    extra->lightMtx = &work->light;
    extra->colorMtx = &work->color;
    coord->flg      = 0;
    Gp_UpdateCoord(coord);
    func_800D7A9C(extra, (VECTOR*)coord->workm.t, 0, 3);
}

void Room_Util29(Task* arg0)
{
    if (arg0 != NULL) {
        Gp_DispatchMsg(arg0, 0x13F3, 0, 0);
    }
}

/// State 0 of the linking rooms' factory task: allocates the 0xC cutscene work
/// block into `Task::work` and re-dresses the task's model off the cap task in
/// `Task::spawnArg2`. Both bits of the model's `field_C` follow the cap model's,
/// the root coordinate is seeded with the factory's position and takes the cap
/// model's coordinate as its `sub`, and the cap model's light and colour
/// matrices are copied across. Game flag 0x4E's nibble of 1 means the scene is
/// already on, which parks the cutscene state at 0xFF and turns the root
/// rotation -0x300 about X. The cap then adopts the task, which steps on.
/// Shared by the night factory and the factory.
void RoomsShared8017fbf4(Task* task)
{
    Task*                    cap      = task->spawnArg2;
    TmdObject*               model    = task->extra;
    TmdObject*               capModel = cap->extra;
    GsCOORDINATE2*           coord    = model->coords;
    GsCOORDINATE2*           capCoord = capModel->coords;
    RoomsShared8017fbf4Work* work     = memCalloc(0xC, 0);
    u16                      flags;

    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->work   = (TaskIdMap*)work;
    flags        = model->flags | 0x80;
    model->flags = flags;
    if (!(capModel->flags & 0x80)) {
        model->flags = flags & 0xFF7F;
    }
    if (!(capModel->flags & 4)) {
        model->flags &= 0xFFFB;
        Tmd_AllocBuffers(model);
    } else {
        model->flags |= 4;
    }
    model->otOffset   = -1;
    coord->coord.t[1] = -0x316;
    coord->sub        = capCoord;
    coord->coord.t[0] = 0;
    coord->coord.t[2] = -0x5FA;
    if (GameFlag_GetNibble(0x4E) == 1) {
        work->state = 0xFF;
        RotMatrixX(-0x300, &coord->coord);
    }
    coord->flg      = 0;
    model->lightMtx = capModel->lightMtx;
    model->colorMtx = capModel->colorMtx;
    Task_Reparent(cap, task);
    task->state += 1;
}
