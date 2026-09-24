#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/dryfield_factory.h"
#include "rooms/dryfield_night_factory.h"

s32 func_dryfield_factory_8017ED68(Task* task)
{
    NightFactoryWork* work  = (NightFactoryWork*)task->work;
    GsCOORDINATE2*    coord = ((TmdObject*)task->extra)->coords;
    s32               done  = 0;

    switch (work->field_17) {
        case 0:
            work->field_4 = 0;
            work->field_17++;
            break;
        case 1:
            if (gGameSession->at4.loc.stage == 2) {
                Gp_EnqueueStageSnd6(0x52170008, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
            } else {
                Gp_EnqueueStageSnd6(0x53170008, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
            }
            work->field_17++;
            break;
        case 2:
            work->field_4 += 0xC000;
            if (work->field_4 > 0x30000) {
                work->field_4 = 0x30000;
            }
            work->field_C.value += work->field_4;
            if (work->field_C.value > 0) {
                work->field_17++;
            }
            break;
        case 3:
            work->field_4 += -0xC000;
            if (work->field_4 < -0xC000) {
                work->field_4 = -0xC000;
            }
            work->field_C.value += work->field_4;
            if (work->field_C.value <= 0) {
                func_dryfield_factory_801804B0(*(Task**)task->spawnArg2);
                if (gGameSession->at4.loc.stage == 2) {
                    Gp_EnqueueStageSnd7(0x52170008, 1);
                    Gp_EnqueueStageSnd6(0x52170010, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                } else {
                    Gp_EnqueueStageSnd7(0x53170008, 1);
                    Gp_EnqueueStageSnd6(0x53170010, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                }
                work->field_17++;
            }
            break;
        default:
            done = 1;
            break;
    }

    if ((u8)(work->field_17 - 1) < 3 && Pad_CheckButtons(0, 1, 0x800) != 0 && (s16)work->field_14 >= 0xB) {
        func_dryfield_factory_801804B0(*(Task**)task->spawnArg2);
        if (gGameSession->at4.loc.stage == 2) {
            Gp_EnqueueStageSnd7(0x52170008, 1);
            Gp_EnqueueStageSnd6(0x52170010, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
        } else {
            Gp_EnqueueStageSnd7(0x53170008, 1);
            Gp_EnqueueStageSnd6(0x53170010, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
        }
        done                = 1;
        work->field_C.value = 0;
        work->field_17      = 4;
    }
    coord->coord.t[1] = work->field_C.part.whole;
    coord->flg        = 0;
    return done;
}
