#include "common.h"

#include "gameplay/268.h"
#include "gameplay/4CC.h"
#include "gameplay/D4.h"
#include "main/task.h"
#include "main/tmd.h"

/// Per-frame visibility hook for an item object: hides the model (`flags`
/// 0x80) once the item's 2-bit pickup flag has reached 2, otherwise shows it
/// with the default flags. The current view is queried but not used.
void func_acropolis_sanctuary_801802E0(Task* task)
{
    GpItemObj8* obj;
    TmdObject*  tmd;
    s32         flag;

    obj  = (GpItemObj8*)task->spawnArg2;
    tmd  = (TmdObject*)task->extra;
    flag = Gp_GetCurBit2Flag(obj->field_8);
    Gp_GetViewIndex();
    if (flag == 2) {
        tmd->flags = 0x80;
    } else {
        tmd->flags    = 8;
        tmd->otOffset = 0;
    }
}
