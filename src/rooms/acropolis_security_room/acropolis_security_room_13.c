#include "common.h"

#include "gameplay/268.h"
#include "gameplay/4CC.h"
#include "gameplay/D4.h"
#include "main/task.h"
#include "main/tmd.h"

/// Per-frame visibility hook for a pick-up prop: the model is drawn with flags
/// 8 at OT offset 0 until the item's 2-bit flag reaches 2, after which it is
/// hidden (flags 0x80).
void func_acropolis_security_room_80182574(Task* task)
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
