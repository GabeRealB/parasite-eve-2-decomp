#include "common.h"

#include "actors/actors_shared_80133de4.h"

#include "main/gfx.h"
#include "main/tmd.h"

void ActorsShared80133de4(Task* task, s16 arg1)
{
    ActorsShared80133de4Work* work = (ActorsShared80133de4Work*)task->idMap;
    s16                       value;
    s16                       pitch4;
    s16                       pitch3;

    value = arg1;
    if (arg1 > 0x500) {
        value = 0x500;
    }
    if (arg1 < 0) {
        value = 0;
    }

    if (work->field_F00 < value) {
        if (value - work->field_F00 >= 0x11) {
            work->field_F00 = work->field_F00 + 0x10;
        } else {
            work->field_F00 = value;
        }
    } else if (value < work->field_F00) {
        if (abs(work->field_F00 - value) >= 0x11) {
            work->field_F00 = work->field_F00 - 0x10;
        } else {
            work->field_F00 = value;
        }
    }

    pitch4 = -ratan2(((TmdObject*)task->extra)->field_8[4].coord.m[1][2],
                     ((TmdObject*)task->extra)->field_8[4].coord.m[2][2]);
    pitch3 = -ratan2(((TmdObject*)task->extra)->field_8[3].coord.m[1][2],
                     ((TmdObject*)task->extra)->field_8[3].coord.m[2][2]);

    Gfx_RotMatrixX(&((TmdObject*)task->extra)->field_8[3].coord, work->field_F00 / 2 - pitch3, 0);
    ((TmdObject*)task->extra)->field_8[3].flg = 0;
    Gfx_RotMatrixX(&((TmdObject*)task->extra)->field_8[4].coord, -work->field_F00 - pitch4, 0);
    ((TmdObject*)task->extra)->field_8[4].flg = 0;
}
