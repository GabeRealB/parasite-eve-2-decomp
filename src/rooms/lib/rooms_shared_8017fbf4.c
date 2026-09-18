#include "common.h"

#include "main/gameflag.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

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
    RoomsShared8017fbf4Work* work     = Mem_Calloc(0xC, 0);
    u16                      flags;

    if (work == NULL) {
        Task_Kill(task);
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
