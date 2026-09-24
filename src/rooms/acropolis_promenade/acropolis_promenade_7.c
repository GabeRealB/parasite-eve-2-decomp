#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/3CD8.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/gfx.h"

/// Work block of the prop task, at `Task::work`. The task allocates it with
/// `memCalloc(4, 0)` and clears its one word; nothing else in the room reads it.
typedef struct {
    s32 field_0;
} ApmPropWork;
STATIC_ASSERT_SIZEOF(ApmPropWork, 0x4);

/// First state of the prop task whose model sits under the room's view
/// coordinate system (`D_acropolis_promenade_8017D5D0`): allocates its work
/// block, shows the model, places it at (-0x23F0, 0x12C, -0xAF0) under
/// `gGfxViewCoord` and moves to the next state.
void func_acropolis_promenade_8017DAA4(Task* task)
{
    TmdObject*     extra;
    GsCOORDINATE2* coord;
    ApmPropWork*   work;

    extra = (TmdObject*)task->extra;
    coord = extra->coords;
    work  = (ApmPropWork*)memCalloc(sizeof(ApmPropWork), 0);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->work        = (TaskIdMap*)work;
    work->field_0     = 0;
    extra->flags      = 0;
    coord->sub        = &gGfxViewCoord;
    coord->coord.t[0] = -0x23F0;
    coord->coord.t[1] = 0x12C;
    coord->coord.t[2] = -0xAF0;
    coord->flg        = 0;
    task->state++;
}
