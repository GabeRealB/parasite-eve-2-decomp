#include "common.h"

#include "main/gameflag.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/dryfield_night_factory.h"

/// State 0 of the room's factory task: allocates the 0xC cutscene work block
/// into `Task::idMap` and re-dresses the task's model off the cap task in
/// `Task::spawnArg2`. Both bits of the model's `field_C` follow the cap model's,
/// the root coordinate is seeded with the factory's position and takes the cap
/// model's coordinate as its `sub`, and the cap model's light and colour
/// matrices are copied across. Game flag 0x4E's nibble of 1 means the scene is
/// already on, which parks the cutscene state at 0xFF and turns the root
/// rotation -0x300 about X. The cap then adopts the task, which steps on.
void func_dryfield_night_factory_8017FBF4(Task* task)
{
    Task*                     cap      = task->spawnArg2;
    TmdObject*                model    = task->extra;
    TmdObject*                capModel = cap->extra;
    GsCOORDINATE2*            coord    = model->field_8;
    GsCOORDINATE2*            capCoord = capModel->field_8;
    NightFactoryCutsceneWork* work     = Mem_Calloc(0xC, 0);
    u16                       flags;

    if (work == NULL) {
        Task_Kill(task);
        return;
    }
    task->idMap    = (TaskIdMap*)work;
    flags          = model->field_C | 0x80;
    model->field_C = flags;
    if (!(capModel->field_C & 0x80)) {
        model->field_C = flags & 0xFF7F;
    }
    if (!(capModel->field_C & 4)) {
        model->field_C &= 0xFFFB;
        Tmd_AllocBuffers(model);
    } else {
        model->field_C |= 4;
    }
    model->field_E    = -1;
    coord->coord.t[1] = -0x316;
    coord->sub        = capCoord;
    coord->coord.t[0] = 0;
    coord->coord.t[2] = -0x5FA;
    if (GameFlag_GetNibble(0x4E) == 1) {
        work->state = 0xFF;
        RotMatrixX(-0x300, &coord->coord);
    }
    coord->flg      = 0;
    model->field_1C = capModel->field_1C;
    model->field_20 = capModel->field_20;
    Task_Reparent(cap, task);
    task->state += 1;
}

/// Runs the current state of the room's cutscene sequence, copying the room's
/// three handlers onto the stack first so the call goes through a local table
/// rather than through `.rodata`. A handler returning non-zero has finished its
/// part of the scene, which drops the sequence back to the shared state 0.
void func_dryfield_night_factory_8017FD5C(Task* task)
{
    NightFactoryCutsceneWork*  work = (NightFactoryCutsceneWork*)task->idMap;
    NightFactoryCutsceneTable3 sp;

    sp = D_dryfield_night_factory_8017D5DC;
    if (sp.funcs[work->state](task) != 0) {
        work->state = 0;
    }
}
