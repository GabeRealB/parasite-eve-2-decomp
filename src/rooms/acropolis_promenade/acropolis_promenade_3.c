#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/D4.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

void func_acropolis_promenade_8017DB48(Task* task)
{
    TmdObject*     obj;
    GsCOORDINATE2* coord;

    obj   = (TmdObject*)task->extra;
    coord = obj->field_8;
    if (Gp_GetViewIndex() == 5) {
        obj->field_C = 0x80;
    } else {
        obj->field_C = 0;
    }
    coord->flg = 0;
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_promenade/acropolis_promenade_3", func_acropolis_promenade_8017DB9C);
