#include "common.h"

#include "gameplay/3CD8.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

#include <psyq/libgs.h>
#include <psyq/libgte.h>

/// Work block of the bridge model task, stored at `Task::work`; it is exactly
/// the `memCalloc(4, 0)` the setup state allocates.
typedef struct _AcropolisBridgeModelWork {
    s32 field_0;
} _AcropolisBridgeModelWork;
STATIC_ASSERT_SIZEOF(_AcropolisBridgeModelWork, 0x4);

/// First state of the bridge model task: allocates its work block, parks the
/// model at (-0x23F0, 0x12C, -0xAF0) and parents it to the room's view
/// coordinate system. The task is killed if the allocation fails.
void func_acropolis_bridge_8017DA64(Task* task)
{
    TmdObject*                 extra;
    GsCOORDINATE2*             coord;
    _AcropolisBridgeModelWork* work;

    extra = (TmdObject*)task->extra;
    coord = extra->coords;
    work  = (_AcropolisBridgeModelWork*)memCalloc(sizeof(_AcropolisBridgeModelWork), 0);
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
