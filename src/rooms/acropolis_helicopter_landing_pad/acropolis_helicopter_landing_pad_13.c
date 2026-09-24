#include "common.h"

#include "gameplay/268.h"
#include "gameplay/4CC.h"
#include "gameplay/D4.h"
#include "main/task.h"
#include "main/tmd.h"

/// Task step of an item-pickup model: hides the mesh with flag 4 when the
/// item's 2-bit flag reads 2, otherwise resets its flags and draw offset and
/// allocates its TMD buffers. The view index is fetched and ignored.
void func_acropolis_helicopter_landing_pad_801822B0(Task* task)
{
    GpItemObj8* obj;
    TmdObject*  tmd;
    s32         flag;

    obj  = (GpItemObj8*)task->spawnArg2;
    tmd  = (TmdObject*)task->extra;
    flag = Gp_GetCurBit2Flag(obj->field_8);
    Gp_GetViewIndex();
    if (flag == 2) {
        tmd->flags |= 4;
    } else {
        tmd->flags    = 8;
        tmd->otOffset = 0;
        Tmd_AllocBuffers(tmd);
    }
}
