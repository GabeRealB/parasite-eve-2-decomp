#include "common.h"

#include "gameplay/268.h"
#include "gameplay/4CC.h"
#include "gameplay/D4.h"
#include "main/task.h"
#include "main/tmd.h"

/// Model task step for a pickup's mesh: when the pickup's 2-bit flag reads 2
/// it sets mesh flag 4, otherwise it resets the mesh flags and draw offset and
/// allocates the mesh's TMD buffers. The view index is fetched but unused.
void func_acropolis_hallway_8017E1C0(Task* task)
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
