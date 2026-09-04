#include "common.h"

#include "gameplay/3CD8.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

#include <psyq/libgs.h>
#include <psyq/libgte.h>

/// Scratch state of the task, stored at `Task::idMap`. The allocation below is
/// `Mem_Calloc(4, 0)`, so the size is the allocation and not a guess.
typedef struct {
    /* 0x0 */ s32 field_0;
} RoomsShared8017daa4Work;
STATIC_ASSERT_SIZEOF(RoomsShared8017daa4Work, 0x4);

/// Second state of an Acropolis prop task: allocates its scratch block, parks
/// the model at its starting position and parents it to the room's view
/// coordinate system. Shared by the Promenade and the Bridge.
void RoomsShared8017daa4(Task* task)
{
    TmdObject*               extra;
    GsCOORDINATE2*           coord;
    RoomsShared8017daa4Work* work;

    extra = (TmdObject*)task->extra;
    coord = extra->field_8;
    work  = (RoomsShared8017daa4Work*)Mem_Calloc(sizeof(RoomsShared8017daa4Work), 0);
    if (work == NULL) {
        Task_Kill(task);
        return;
    }
    task->idMap       = (TaskIdMap*)work;
    work->field_0     = 0;
    extra->field_C    = 0;
    coord->sub        = &Gfx_ViewCoord;
    coord->coord.t[0] = -0x23F0;
    coord->coord.t[1] = 0x12C;
    coord->coord.t[2] = -0xAF0;
    coord->flg        = 0;
    task->state++;
}
