#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/gameplay.h"
#include "main/gameflag.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/dryfield_night_factory.h"

void func_dryfield_night_factory_8017FB68(Task* task)
{
    GsCOORDINATE2*    coord;
    NightFactoryWork* work;
    TmdObject*        extra;

    work            = (NightFactoryWork*)task->work;
    extra           = (TmdObject*)task->extra;
    coord           = extra->coords;
    extra->lightMtx = &work->light;
    extra->colorMtx = &work->color;
    coord->flg      = 0;
    Gp_UpdateCoord(coord);
    func_800D7A9C(extra, (VECTOR*)coord->workm.t, 0, 3);
}

void func_dryfield_night_factory_8017FBC8(Task* arg0)
{
    if (arg0 != NULL) {
        Gp_DispatchMsg(arg0, 0x13F3, 0, 0);
    }
}

/// State 0 of the room's cutscene task: allocates the cutscene work block into
/// `Task::work` and dresses the task's model after the factory model in
/// `Task::spawnArg2`. Both bits of the model's flags follow the factory
/// model's, the root coordinate is seeded with a fixed offset under the
/// factory model's coordinate, and the factory model's light and colour
/// matrices are shared. A nibble of 1 in game flag 0x4E means the scene is
/// already on, which parks the cutscene state at 0xFF and turns the root
/// rotation -0x300 about X. The factory task then adopts this one, which steps
/// on.
void func_dryfield_night_factory_8017FBF4(Task* task)
{
    Task*                     cap      = task->spawnArg2;
    TmdObject*                model    = task->extra;
    TmdObject*                capModel = cap->extra;
    GsCOORDINATE2*            coord    = model->coords;
    GsCOORDINATE2*            capCoord = capModel->coords;
    NightFactoryCutsceneWork* work     = memCalloc(0xC, 0);
    u16                       flags;

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
