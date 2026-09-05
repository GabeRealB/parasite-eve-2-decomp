#include "common.h"

#include "gameplay/268.h"
#include "gameplay/4CC.h"
#include "gameplay/D4.h"

#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

/// Item-pickup model task step: on the first run resets the mesh flags and
/// arms the task, then hides the mesh with flag 0x80 unless the room is being
/// drawn from view 5, and always hides it once the item's 2-bit flag reads 2
/// (already taken).
void func_acropolis_hallway_8017E120(Task* task)
{
    GpItemObj8* obj;
    TmdObject*  tmd;
    s32         flag;

    obj  = (GpItemObj8*)task->spawnArg2;
    tmd  = (TmdObject*)task->extra;
    flag = Gp_GetCurBit2Flag(obj->field_8);
    if (task->state == 0) {
        tmd->field_C = 8;
        tmd->field_E = 0;
        task->state++;
    }
    if (Gp_GetViewIndex() == 5) {
        tmd->field_C = 8;
    } else {
        tmd->field_C = 0x80;
    }
    if (flag == 2) {
        tmd->field_C = 0x80;
    }
}
