#include "common.h"

#include "gameplay/268.h"
#include "gameplay/4CC.h"
#include "gameplay/D4.h"
#include "main/task.h"
#include "main/tmd.h"

/// Item-pickup model task step: hides the mesh with flag 4 once the item's
/// 2-bit flag reads 2 (already taken), otherwise resets it and allocates
/// its TMD buffers. The view index is fetched but unused.
void RoomsShared801822b0(Task* task)
{
    GpItemObj8* obj;
    TmdObject*  tmd;
    s32         flag;

    obj  = (GpItemObj8*)task->spawnArg2;
    tmd  = (TmdObject*)task->extra;
    flag = Gp_GetCurBit2Flag(obj->field_8);
    Gp_GetViewIndex();
    if (flag == 2) {
        tmd->field_C |= 4;
    } else {
        tmd->field_C = 8;
        tmd->field_E = 0;
        Tmd_AllocBuffers(tmd);
    }
}
