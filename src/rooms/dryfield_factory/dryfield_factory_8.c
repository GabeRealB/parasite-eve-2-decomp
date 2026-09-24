#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/dryfield_factory.h"
#include "rooms/dryfield_night_factory.h"

void func_8004BFF8(s16 angle, MATRIX* matrix);

extern u8 D_dryfield_factory_8018A39C[];
extern u8 D_dryfield_factory_8018A3A8[];
extern u8 D_8018A7BC[];
extern u8 D_8018A7C8[];

s32 func_dryfield_factory_8017F08C(Task* task)
{
    NightFactoryWork*     work  = (NightFactoryWork*)task->work;
    GsCOORDINATE2*        coord = ((TmdObject*)task->extra)->coords;
    s32                   done  = 0;
    NightFactoryMatWords* mat;

    switch (work->field_16) {
        case 0:
            work->field_8 = 0;
            work->field_16++;
            break;
        case 1:
            if (gGameSession->at4.loc.stage == 2) {
                Gp_EnqueueStageSnd6(0x5217000F, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
            } else {
                Gp_EnqueueStageSnd6(0x5317000F, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
            }
            work->field_16++;
            break;
        case 2:
            work->field_8 += 0x18000;
            if (work->field_8 > 0x40000) {
                work->field_8 = 0x40000;
            }
            work->field_10.value += work->field_8;
            if (work->field_10.value > 0x800000) {
                if (gGameSession->at4.loc.stage == 2) {
                    Gp_EnqueueStageSnd6(0x52170012, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                    Gp_SpawnScript18((s32)D_dryfield_factory_8018A39C, (s32)D_dryfield_factory_8018A3A8);
                } else {
                    Gp_EnqueueStageSnd6(0x53170012, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                    Gp_SpawnScript18((s32)D_8018A7BC, (s32)D_8018A7C8);
                }
                work->field_16++;
            }
            break;
        case 3:
            work->field_8 += -0xC000;
            if (work->field_8 < -0x20000) {
                work->field_8 = -0x20000;
            }
            work->field_10.value += work->field_8;
            if (work->field_10.value <= 0) {
                work->field_0 = GameFlag_GetNibble(0x49) & 0xFE;
                GameFlag_SetNibble(0x49, work->field_0);
                work->field_10.value = 0;
                func_dryfield_factory_801804B0(*(Task**)task->spawnArg2);
                if (gGameSession->at4.loc.stage == 2) {
                    Gp_EnqueueStageSnd7(0x5217000F, 1);
                    Gp_EnqueueStageSnd6(0x52170011, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                } else {
                    Gp_EnqueueStageSnd7(0x5317000F, 1);
                    Gp_EnqueueStageSnd6(0x53170011, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                }
                work->field_16++;
            }
            break;
        default:
            done = 1;
            break;
    }

    if ((u8)(work->field_16 - 1) < 3 && Pad_CheckButtons(0, 1, 0x800) != 0 && (s16)work->field_14 >= 0xB) {
        work->field_0 = GameFlag_GetNibble(0x49) & 0xFE;
        GameFlag_SetNibble(0x49, work->field_0);
        work->field_10.value = 0;
        func_dryfield_factory_801804B0(*(Task**)task->spawnArg2);
        if (gGameSession->at4.loc.stage == 2) {
            Gp_EnqueueStageSnd7(0x5217000F, 1);
            Gp_EnqueueStageSnd6(0x52170011, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
        } else {
            Gp_EnqueueStageSnd7(0x5317000F, 1);
            Gp_EnqueueStageSnd6(0x53170011, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
        }
        work->field_16 = 4;
        done           = 1;
    }
    mat                = (NightFactoryMatWords*)&coord->coord;
    mat->ident.m00_m01 = 0x1000;
    mat->ident.m02_m10 = 0;
    mat->ident.m11_m12 = 0x1000;
    mat->ident.m20_m21 = 0;
    mat->ident.m22     = 0x1000;
    func_8004BFF8(work->field_10.part.whole, &mat->mat);
    coord->flg = 0;
    return done;
}

s32 func_dryfield_factory_8017F4BC(Task* task)
{
    NightFactoryWork*     work  = (NightFactoryWork*)task->work;
    GsCOORDINATE2*        coord = ((TmdObject*)task->extra)->coords;
    s32                   done  = 0;
    NightFactoryMatWords* mat;

    switch (work->field_16) {
        case 0:
            work->field_8 = 0;
            work->field_16++;
            break;
        case 1:
            if (gGameSession->at4.loc.stage == 2) {
                Gp_EnqueueStageSnd6(0x5217000F, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
            } else {
                Gp_EnqueueStageSnd6(0x5317000F, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
            }
            work->field_16++;
            break;
        case 2:
            work->field_8 += -0x18000;
            if (work->field_8 < -0x40000) {
                work->field_8 = -0x40000;
            }
            work->field_10.value += work->field_8;
            if (work->field_10.value < 0x3800000) {
                if (gGameSession->at4.loc.stage == 2) {
                    Gp_EnqueueStageSnd6(0x52170012, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                    Gp_SpawnScript18((s32)D_dryfield_factory_8018A39C, (s32)D_dryfield_factory_8018A3A8);
                } else {
                    Gp_EnqueueStageSnd6(0x53170012, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                    Gp_SpawnScript18((s32)D_8018A7BC, (s32)D_8018A7C8);
                }
                work->field_16++;
            }
            break;
        case 3:
            work->field_8 += 0xC000;
            if (work->field_8 > 0x20000) {
                work->field_8 = 0x20000;
            }
            work->field_10.value += work->field_8;
            if (work->field_10.value >= 0x4000000) {
                work->field_0 = GameFlag_GetNibble(0x49) | 1;
                GameFlag_SetNibble(0x49, work->field_0);
                work->field_10.value = 0x4000000;
                func_dryfield_factory_801804B0(*(Task**)task->spawnArg2);
                if (gGameSession->at4.loc.stage == 2) {
                    Gp_EnqueueStageSnd7(0x5217000F, 1);
                    Gp_EnqueueStageSnd6(0x52170011, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                } else {
                    Gp_EnqueueStageSnd7(0x5317000F, 1);
                    Gp_EnqueueStageSnd6(0x53170011, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                }
                work->field_16++;
            }
            break;
        default:
            done = 1;
            break;
    }

    if ((u8)(work->field_16 - 1) < 3 && Pad_CheckButtons(0, 1, 0x800) != 0 && (s16)work->field_14 >= 0xB) {
        work->field_0 = GameFlag_GetNibble(0x49) | 1;
        GameFlag_SetNibble(0x49, work->field_0);
        work->field_10.value = 0x4000000;
        func_dryfield_factory_801804B0(*(Task**)task->spawnArg2);
        if (gGameSession->at4.loc.stage == 2) {
            Gp_EnqueueStageSnd7(0x5217000F, 1);
            Gp_EnqueueStageSnd6(0x52170011, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
        } else {
            Gp_EnqueueStageSnd7(0x5317000F, 1);
            Gp_EnqueueStageSnd6(0x53170011, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
        }
        work->field_16 = 4;
        done           = 1;
    }
    mat                = (NightFactoryMatWords*)&coord->coord;
    mat->ident.m00_m01 = 0x1000;
    mat->ident.m02_m10 = 0;
    mat->ident.m11_m12 = 0x1000;
    mat->ident.m20_m21 = 0;
    mat->ident.m22     = 0x1000;
    func_8004BFF8(work->field_10.part.whole, &mat->mat);
    coord->flg = 0;
    return done;
}
