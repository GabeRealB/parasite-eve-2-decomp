#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/dryfield_night_factory.h"

s32 RoomsShared8017f8f4(Task* task)
{
    NightFactoryCutsceneWork* work  = (NightFactoryCutsceneWork*)task->work;
    GsCOORDINATE2*            coord = ((TmdObject*)task->extra)->coords;
    MATRIX*                   m;
    s32                       ret = 0;

    switch (work->step) {
        case 0:
            work->field_0 = 0;
            if (gGameSession->at4.loc.stage == 2) {
                Gp_EnqueueStageSnd6(0x5217000D, (s8)Gp_GetObjPan(coord),
                                    (s8)gpGetObjDepth(coord));
            } else {
                Gp_EnqueueStageSnd6(0x5317000D, (s8)Gp_GetObjPan(coord),
                                    (s8)gpGetObjDepth(coord));
            }
            work->step++;
            break;
        case 1:
            work->field_0 += -0x28000;
            if (work->field_0 < -0x300000) {
                work->field_0 = -0x300000;
            }
            work->field_4.value += work->field_0;
            if (work->field_4.value < -0x3000000) {
                work->step++;
            }
            break;
        case 2:
            work->field_0 += 0x40000;
            if (work->field_0 > 0x100000) {
                work->field_0 = 0x100000;
            }
            work->field_4.value += work->field_0;
            if (work->field_4.value >= -0x3000000) {
                work->field_4.value = -0x3000000;
                work->step++;
            }
            break;
        default:
            ret = 1;
            break;
    }

    m                            = &coord->coord;
    *(s32*)&coord->coord.m[0][0] = 0x1000;
    *(s32*)&m->m[0][2]           = 0;
    *(s32*)&m->m[1][1]           = 0x1000;
    *(s32*)&m->m[2][0]           = 0;
    m->m[2][2]                   = 0x1000;
    RotMatrixX(work->field_4.part.whole, m);
    coord->flg = 0;
    return ret;
}
