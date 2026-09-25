#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/gameflag.h"
#include "main/gameflow.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/dryfield_night_factory.h"

#include <psyq/inline_c.h>
#include "gte.h"
#include <psyq/libgte.h>

extern u8 D_8007216D;

void func_8004BFF8(s32 angle, MATRIX* matrix);

/// The factory model task's spawn tables for the two stage variants.
extern TaskDesc D_dryfield_night_factory_80186DE0[];
extern TaskDesc D_dryfield_night_factory_80186E28[];

/// The collision-grid templates the factory model's faces are rebuilt from,
/// and the one two of the grid's faces are restored from.
extern GpGridParams D_dryfield_night_factory_80186C20;
extern GpGridParams D_dryfield_night_factory_80186CF0;
extern GpGridParams D_dryfield_night_factory_80186DBC;

/// The two argument blocks one of the turn handlers hands `Gp_SpawnScript18`,
/// one pair per stage variant.
extern u8 D_dryfield_night_factory_8018A39C[];
extern u8 D_dryfield_night_factory_8018A3A8[];
extern u8 D_dryfield_night_factory_8018A7BC[];
extern u8 D_dryfield_night_factory_8018A7C8[];

void func_dryfield_night_factory_8017D6F8(Task* task);
s32  func_dryfield_night_factory_8017F00C(Task* task);
s32  func_dryfield_night_factory_8017F1DC(Task* task);
void func_dryfield_night_factory_8017FBF4(Task* task);
void func_dryfield_night_factory_8017FD5C(Task* task);
s32  func_dryfield_night_factory_8017FDC8(Task* task);

const TaskFuncTable3 D_dryfield_night_factory_8017D5C4 = {
    { func_dryfield_night_factory_8017D6F8, func_dryfield_night_factory_8017FA08, taskKill },
};

const TaskFuncTable3 D_dryfield_night_factory_8017D5D0 = {
    { func_dryfield_night_factory_8017FBF4, func_dryfield_night_factory_8017FD5C, taskKill },
};

const NightFactoryCutsceneTable3 D_dryfield_night_factory_8017D5DC = {
    { func_dryfield_night_factory_8017FDC8, func_dryfield_night_factory_8017F00C, func_dryfield_night_factory_8017F1DC },
};

/// State 0 of the room's factory model: allocate the work block, seed it from
/// the progress nibble, point the model's coordinate at the seeded position
/// and the light/color matrices at the block's own, then pick the spawn table
/// for this session variant and hand the model to its own state machine.
///
/// The two spawn tables are passed straight to `Task_SpawnFromTable` from each
/// arm rather than through a variable: the argument is then a bare symbol, so
/// the `lui`/`addiu` pair is built in `$a0` itself and `jump2` merges the two
/// arms' identical tails back into one call.
void func_dryfield_night_factory_8017D6F8(Task* task)
{
    NightFactoryWork* work;
    GsCOORDINATE2*    coord;
    TmdObject*        obj;

    obj   = (TmdObject*)task->extra;
    coord = obj->coords;
    work  = memCalloc(0x58, 0);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->work     = (TaskIdMap*)work;
    work->field_0  = GameFlag_GetNibble(0x49);
    work->field_16 = -1;
    work->field_17 = -1;
    obj->flags    &= 0xFF7F;
    if (work->field_0 & 1) {
        work->field_10.value = 0x4000000;
        func_8004BFF8(0x4000000, &coord->coord);
    }
    if (work->field_0 & 2) {
        work->field_C.value = 0xFDC60000;
    } else {
        work->field_C.value = 0;
    }
    coord->coord.t[0] = 0xE4C;
    coord->coord.t[1] = work->field_C.part.whole;
    coord->coord.t[2] = 0x1AAE;
    func_dryfield_night_factory_8017FB68(task);
    func_dryfield_night_factory_8017D858(task, 1, 0);
    if (gGameSession->at4.loc.stage == 2) {
        Task_SpawnFromTable(D_dryfield_night_factory_80186E28, 7, 0, (s32)task);
    } else {
        Task_SpawnFromTable(D_dryfield_night_factory_80186DE0, 7, 0, (s32)task);
    }
    task->exitCallback  = func_dryfield_night_factory_8017FB48;
    task->killCountdown = 0;
    task->state++;
}

void func_dryfield_night_factory_8017D858(Task* task, s32 remapFaces, s32 useAltTemplate)
{
    long           flag;
    GsCOORDINATE2* coord;
    MATRIX*        m;
    GpGridParams*  geom;
    GpGridParams*  src;
    SVECTOR*       s;
    SVECTOR*       d;
    GpGridFace*    sf;
    GpGridFace*    df;
    u16*           sv;
    u16*           dv;
    s32            i;
    s32            j;

    coord = ((TmdObject*)task->extra)->coords;
    if (gGameSession->at4.loc.stage == 2) {
        geom = &D_dryfield_night_factory_80187BF8;
    } else {
        geom = &D_dryfield_night_factory_80187BF0;
    }
    if (useAltTemplate != 0) {
        src = &D_dryfield_night_factory_80186DBC;
    } else {
        src = &D_dryfield_night_factory_80186CF0;
    }

    m = &coord->coord;
    s = src->field_4;
    d = geom->field_4 + 2;
    for (i = 0; i < 4; i++) {
        gte_SetRotMatrix(m);
        gte_ldv0(s);
        s++;
        gte_rtv0();
        gte_stsv(d);
        d++;
    }

    gte_SetRotMatrix(m);
    gte_SetTransMatrix(m);
    s = src->field_8;
    d = geom->field_8 + 8;
    for (i = 0; i < 8; i++) {
        RotTransSV(s++, d++, &flag);
    }

    if (remapFaces != 0) {
        sf = src->field_C;
        df = geom->field_C + 2;
        for (i = 0; i < 4; i++) {
            j  = 0;
            dv = df->verts;
            sv = sf->verts;
            do {
                *dv++ = *sv++ + 8;
            } while (++j < 4);
            df->field_8 = sf->field_8 + 2;
            df->field_A = sf->field_A;
            df++;
            sf++;
        }
    }
}

s32 func_dryfield_night_factory_8017DA54(Task* task)
{
    NightFactoryWork* work  = (NightFactoryWork*)task->work;
    GsCOORDINATE2*    coord = ((TmdObject*)task->extra)->coords;
    s32               done  = 0;
    OverlayMat*       mat;

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
            if (work->field_10.value > 0x4000000) {
                work->field_16++;
            }
            break;
        case 3:
            work->field_8 += -0x8000;
            if (work->field_8 < -0x20000) {
                work->field_8 = -0x20000;
            }
            work->field_10.value += work->field_8;
            if (work->field_10.value <= 0x4000000) {
                func_dryfield_night_factory_8017FBC8(*(Task**)task->spawnArg2);
                if (gGameSession->at4.loc.stage == 2) {
                    Gp_EnqueueStageSnd7(0x5217000F, 1);
                    Gp_EnqueueStageSnd6(0x52170011, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                } else {
                    Gp_EnqueueStageSnd7(0x5317000F, 1);
                    Gp_EnqueueStageSnd6(0x53170011, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                }
                work->field_10.value = 0x4000000;
                work->field_16++;
            }
            break;
        default:
            done = 1;
            break;
    }

    if ((u8)(work->field_16 - 1) < 3 && Pad_CheckButtons(0, 1, 0x800) != 0 && (s16)work->field_14 >= 0xB) {
        func_dryfield_night_factory_8017FBC8(*(Task**)task->spawnArg2);
        if (gGameSession->at4.loc.stage == 2) {
            Gp_EnqueueStageSnd7(0x5217000F, 1);
            Gp_EnqueueStageSnd6(0x52170011, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
        } else {
            Gp_EnqueueStageSnd7(0x5317000F, 1);
            Gp_EnqueueStageSnd6(0x53170011, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
        }
        done                 = 1;
        work->field_10.value = 0x4000000;
        work->field_16       = 4;
    }
    mat                = (OverlayMat*)&coord->coord;
    mat->ident.m00_m01 = 0x1000;
    mat->ident.m02_m10 = 0;
    mat->ident.m11_m12 = 0x1000;
    mat->ident.m20_m21 = 0;
    mat->ident.m22     = 0x1000;
    func_8004BFF8(work->field_10.part.whole, &mat->mat);
    coord->flg = 0;
    return done;
}

s32 func_dryfield_night_factory_8017DDD4(Task* task)
{
    NightFactoryWork* work  = (NightFactoryWork*)task->work;
    GsCOORDINATE2*    coord = ((TmdObject*)task->extra)->coords;
    s32               done  = 0;
    OverlayMat*       mat;

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
            if (work->field_10.value < 0) {
                work->field_16++;
            }
            break;
        case 3:
            work->field_8 += 0x8000;
            if (work->field_8 > 0x20000) {
                work->field_8 = 0x20000;
            }
            work->field_10.value += work->field_8;
            if (work->field_10.value >= 0) {
                func_dryfield_night_factory_8017FBC8(*(Task**)task->spawnArg2);
                if (gGameSession->at4.loc.stage == 2) {
                    Gp_EnqueueStageSnd7(0x5217000F, 1);
                    Gp_EnqueueStageSnd6(0x52170011, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                } else {
                    Gp_EnqueueStageSnd7(0x5317000F, 1);
                    Gp_EnqueueStageSnd6(0x53170011, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                }
                work->field_10.value = 0;
                work->field_16++;
            }
            break;
        default:
            done = 1;
            break;
    }

    if ((u8)(work->field_16 - 1) < 3 && Pad_CheckButtons(0, 1, 0x800) != 0 && (s16)work->field_14 >= 0xB) {
        func_dryfield_night_factory_8017FBC8(*(Task**)task->spawnArg2);
        if (gGameSession->at4.loc.stage == 2) {
            Gp_EnqueueStageSnd7(0x5217000F, 1);
            Gp_EnqueueStageSnd6(0x52170011, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
        } else {
            Gp_EnqueueStageSnd7(0x5317000F, 1);
            Gp_EnqueueStageSnd6(0x53170011, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
        }
        done                 = 1;
        work->field_10.value = 0;
        work->field_16       = 4;
    }
    mat                = (OverlayMat*)&coord->coord;
    mat->ident.m00_m01 = 0x1000;
    mat->ident.m02_m10 = 0;
    mat->ident.m11_m12 = 0x1000;
    mat->ident.m20_m21 = 0;
    mat->ident.m22     = 0x1000;
    func_8004BFF8(work->field_10.part.whole, &mat->mat);
    coord->flg = 0;
    return done;
}

s32 func_dryfield_night_factory_8017E13C(Task* task)
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
            work->field_4 += -0xC000;
            if (work->field_4 < -0x30000) {
                work->field_4 = -0x30000;
            }
            work->field_C.value += work->field_4;
            if (work->field_C.value < -0x23A0000) {
                work->field_17++;
            }
            break;
        case 3:
            work->field_4 += 0xC000;
            if (work->field_4 > 0xC000) {
                work->field_4 = 0xC000;
            }
            work->field_C.value += work->field_4;
            if (work->field_C.value >= -0x23A0000) {
                func_dryfield_night_factory_8017FBC8(*(Task**)task->spawnArg2);
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
        func_dryfield_night_factory_8017FBC8(*(Task**)task->spawnArg2);
        if (gGameSession->at4.loc.stage == 2) {
            Gp_EnqueueStageSnd7(0x52170008, 1);
            Gp_EnqueueStageSnd6(0x52170010, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
        } else {
            Gp_EnqueueStageSnd7(0x53170008, 1);
            Gp_EnqueueStageSnd6(0x53170010, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
        }
        done                = 1;
        work->field_C.value = -0x23A0000;
        work->field_17      = 4;
    }
    coord->coord.t[1] = work->field_C.part.whole;
    coord->flg        = 0;
    return done;
}

s32 func_dryfield_night_factory_8017E480(Task* task)
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
                func_dryfield_night_factory_8017FBC8(*(Task**)task->spawnArg2);
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
        func_dryfield_night_factory_8017FBC8(*(Task**)task->spawnArg2);
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

s32 func_dryfield_night_factory_8017E7A4(Task* task)
{
    NightFactoryWork* work  = (NightFactoryWork*)task->work;
    GsCOORDINATE2*    coord = ((TmdObject*)task->extra)->coords;
    s32               done  = 0;
    OverlayMat*       mat;

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
                    Gp_SpawnScript18((s32)D_dryfield_night_factory_8018A39C, (s32)D_dryfield_night_factory_8018A3A8);
                } else {
                    Gp_EnqueueStageSnd6(0x53170012, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                    Gp_SpawnScript18((s32)D_dryfield_night_factory_8018A7BC, (s32)D_dryfield_night_factory_8018A7C8);
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
                func_dryfield_night_factory_8017FBC8(*(Task**)task->spawnArg2);
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
        func_dryfield_night_factory_8017FBC8(*(Task**)task->spawnArg2);
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
    mat                = (OverlayMat*)&coord->coord;
    mat->ident.m00_m01 = 0x1000;
    mat->ident.m02_m10 = 0;
    mat->ident.m11_m12 = 0x1000;
    mat->ident.m20_m21 = 0;
    mat->ident.m22     = 0x1000;
    func_8004BFF8(work->field_10.part.whole, &mat->mat);
    coord->flg = 0;
    return done;
}

s32 func_dryfield_night_factory_8017EBD4(Task* task)
{
    NightFactoryWork* work  = (NightFactoryWork*)task->work;
    GsCOORDINATE2*    coord = ((TmdObject*)task->extra)->coords;
    s32               done  = 0;
    OverlayMat*       mat;

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
                    Gp_SpawnScript18((s32)D_dryfield_night_factory_8018A39C, (s32)D_dryfield_night_factory_8018A3A8);
                } else {
                    Gp_EnqueueStageSnd6(0x53170012, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                    Gp_SpawnScript18((s32)D_dryfield_night_factory_8018A7BC, (s32)D_dryfield_night_factory_8018A7C8);
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
                func_dryfield_night_factory_8017FBC8(*(Task**)task->spawnArg2);
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
        func_dryfield_night_factory_8017FBC8(*(Task**)task->spawnArg2);
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
    mat                = (OverlayMat*)&coord->coord;
    mat->ident.m00_m01 = 0x1000;
    mat->ident.m02_m10 = 0;
    mat->ident.m11_m12 = 0x1000;
    mat->ident.m20_m21 = 0;
    mat->ident.m22     = 0x1000;
    func_8004BFF8(work->field_10.part.whole, &mat->mat);
    coord->flg = 0;
    return done;
}

/// Cutscene state 1: plays the movement's sound, swings the model about X
/// towards -0x300, overshooting and settling back on it, and answers non-zero
/// once it has settled.
s32 func_dryfield_night_factory_8017F00C(Task* task)
{
    NightFactoryCutsceneWork* work  = (NightFactoryCutsceneWork*)task->work;
    GsCOORDINATE2*            coord = ((TmdObject*)task->extra)->coords;
    OverlayMat*               mat;
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

    mat                = (OverlayMat*)&coord->coord;
    mat->ident.m00_m01 = 0x1000;
    mat->ident.m02_m10 = 0;
    mat->ident.m11_m12 = 0x1000;
    mat->ident.m20_m21 = 0;
    mat->ident.m22     = 0x1000;
    RotMatrixX(work->field_4.part.whole, &mat->mat);
    coord->flg = 0;
    return ret;
}

/// Cutscene state 2: swings the model about X back up past 0, playing the
/// movement's sound as it gets there, and answers non-zero afterwards.
s32 func_dryfield_night_factory_8017F1DC(Task* task)
{
    NightFactoryCutsceneWork* work  = (NightFactoryCutsceneWork*)task->work;
    GsCOORDINATE2*            coord = ((TmdObject*)task->extra)->coords;
    OverlayMat*               mat;
    s32                       ret = 0;

    switch (work->step) {
        case 0:
            work->field_0 = 0;
            work->step++;
            break;
        case 1:
            work->field_0 += 0x20000;
            if (work->field_0 > 0x700000) {
                work->field_0 = 0x700000;
            }
            work->field_4.value += work->field_0;
            if (work->field_4.value > 0) {
                if (gGameSession->at4.loc.stage == 2) {
                    Gp_EnqueueStageSnd6(0x5217000E, (s8)Gp_GetObjPan(coord),
                                        (s8)gpGetObjDepth(coord));
                } else {
                    Gp_EnqueueStageSnd6(0x5317000E, (s8)Gp_GetObjPan(coord),
                                        (s8)gpGetObjDepth(coord));
                }
                work->step++;
            }
            break;
        default:
            ret = 1;
            break;
    }

    mat                = (OverlayMat*)&coord->coord;
    mat->ident.m00_m01 = 0x1000;
    mat->ident.m02_m10 = 0;
    mat->ident.m11_m12 = 0x1000;
    mat->ident.m20_m21 = 0;
    mat->ident.m22     = 0x1000;
    RotMatrixX(work->field_4.part.whole, &mat->mat);
    coord->flg = 0;
    return ret;
}

/// Cutscene driver for the night factory room: silences both weapons, runs the
/// cap (cutscene) command in `Task::spawnArg1`, then waits for the cap to
/// report event key 3 before setting the two progress flags and starting the
/// follow-up cap slot. Any state past 4 restores the weapons and kills the task.
void func_dryfield_night_factory_8017F330(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_MsgPlayerWeapon(0);
            Gp_MsgAllyWeapon(0);
            Gp_RunCapCmd(task->spawnArg1, 0);
            goto advance;
        case 1:
            if (GameFlag_GetNibble(0x48) <= 0) {
                if (gGameSession->at4.loc.stage == 2) {
                    func_dryfield_night_factory_80181B38(0);
                    SOFT_BARRIER();
                } else {
                    func_dryfield_night_factory_80181B38(0);
                }
                Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F3, 0, 0);
            }
            task->state++;
            /* fallthrough */
        case 2:
            if (Gp_CapBusy() != 0) {
                return;
            }
            /* fallthrough */
        case 3:
            if (Gp_GetCapEventKey() == 3) {
                GameFlag_SetNibble(0x48, 1);
                GameFlag_SetNibble(0x4A, 1);
                if (gGameSession->at4.loc.stage == 2) {
                    func_dryfield_night_factory_80181B38(1);
                    func_dryfield_night_factory_80181620(1);
                    SOFT_BARRIER();
                } else {
                    func_dryfield_night_factory_80181B38(1);
                    func_dryfield_night_factory_80181620(1);
                }
                Gp_StartCapSlot(task->spawnArg1, 1, 2);
            }
        advance:
            task->state++;
            return;
        case 4:
            if (Gp_CapBusy() != 0) {
                return;
            }
            /* fallthrough */
        default:
            Gp_MsgPlayerWeapon(1);
            Gp_MsgAllyWeapon(1);
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F3, 1, 0);
            taskKill(task);
            break;
    }
}

/// The room's second cutscene: states 0..2 silence both weapons, run the cap in
/// `Task::spawnArg1` and wait for it to report event key 1; states 3 and 6 count
/// `Task::killCountdown` up to and back down from 0x1E and tint the screen with
/// the count scaled to 0xFF over 30 steps; states 4 and 5 publish the progress
/// flags and tint it white, and anything past 6 restores the weapons and kills
/// the task.
///
/// `fade` does two jobs on purpose: state 4 reads the session variant through
/// it before testing it. That cross-block use is what makes the state-3/6 tint
/// value a *global* pseudo, and `local-alloc` only folds the `(u8)fade`
/// conversion into the division's quantity when that pseudo is local to one
/// block -- global, the conversion keeps its own quantity and takes `$a0` from
/// the argument move, while the division chain keeps `$v1`.
void func_dryfield_night_factory_8017F4F4(Task* task)
{
    u8 fade;

    switch (task->state) {
        case 0:
            if (GameFlag_GetNibble(0x47) != 0) {
                goto kill;
            }
            Gp_MsgPlayerWeapon(0);
            Gp_MsgAllyWeapon(0);
            Gp_RunCapCmd1(task->spawnArg1);
            goto advance;
        case 2:
            if (Gp_GetCapEventKey() == 1) {
                task->killCountdown = 0;
                if (gGameSession->at4.loc.stage == 2) {
                    Gp_EnqueueStageSnd6(0x5217000C, 0, 0);
                }
                goto advance;
            }
            task->state = -1;
            return;
        case 3:
            task->killCountdown = task->killCountdown + 1;
            if (task->killCountdown < 0x1E) {
                goto draw;
            }
            goto bump;
        case 4:
            gGameSession->viewDirty = 1;
            GameFlag_SetNibble(0x47, 1);
            fade = gGameSession->at4.loc.stage;
            if (fade == 2) {
                Gp_EnqueueStageSnd6(0x5217000B, 0, 0);
            }
            Fade_DrawOverlay(0xFF, 0xFF, 0xFF, 2);
            goto advance;
        case 5:
            D_8007216D                  = 2;
            gGameSession->at4.loc.room  = 2;
            gGameSession->roomObjsDirty = 1;
            Fade_DrawOverlay(0xFF, 0xFF, 0xFF, 2);
            goto advance;
        case 1:
        advance:
            task->state = task->state + 1;
            return;
        case 6:
            task->killCountdown = task->killCountdown - 1;
            if (task->killCountdown > 0) {
                goto draw;
            }
        bump:
            task->state = task->state + 1;
        draw:
            fade = (task->killCountdown * 255) / 30;
            Fade_DrawOverlay(fade, fade, fade, 2);
            return;
        default:
            Gp_MsgPlayerWeapon(1);
            Gp_MsgAllyWeapon(1);
        kill:
            taskKill(task);
            return;
    }
}

/// Restores two faces of the stage variant's collision grid -- normals,
/// corners and face records -- from a template, then slides their eight
/// corners 2000 units along x once game flag 0x47 is set: at once in state 0,
/// which then kills the task, or from state 1 when the flag turns positive
/// later.
void func_dryfield_night_factory_8017F734(Task* task)
{
    GpGridParams* src = &D_dryfield_night_factory_80186C20;
    GpGridParams* geom;
    s32           i;

    if (gGameSession->at4.loc.stage == 2) {
        geom = &D_dryfield_night_factory_80187BF8;
    } else {
        geom = &D_dryfield_night_factory_80187BF0;
    }
    switch (task->state) {
        case 0:
            for (i = 0; i < 2; i++) {
                geom->field_4[i].vx         = src->field_4[i].vx;
                geom->field_4[i].vy         = src->field_4[i].vy;
                geom->field_4[i].vz         = src->field_4[i].vz;
                geom->field_8[i * 4 + 0].vx = src->field_8[i * 4 + 0].vx;
                geom->field_8[i * 4 + 0].vy = src->field_8[i * 4 + 0].vy;
                geom->field_8[i * 4 + 0].vz = src->field_8[i * 4 + 0].vz;
                geom->field_8[i * 4 + 1].vx = src->field_8[i * 4 + 1].vx;
                geom->field_8[i * 4 + 1].vy = src->field_8[i * 4 + 1].vy;
                geom->field_8[i * 4 + 1].vz = src->field_8[i * 4 + 1].vz;
                geom->field_8[i * 4 + 2].vx = src->field_8[i * 4 + 2].vx;
                geom->field_8[i * 4 + 2].vy = src->field_8[i * 4 + 2].vy;
                geom->field_8[i * 4 + 2].vz = src->field_8[i * 4 + 2].vz;
                geom->field_8[i * 4 + 3].vx = src->field_8[i * 4 + 3].vx;
                geom->field_8[i * 4 + 3].vy = src->field_8[i * 4 + 3].vy;
                geom->field_8[i * 4 + 3].vz = src->field_8[i * 4 + 3].vz;
                geom->field_C[i]            = src->field_C[i];
            }
            if (GameFlag_GetNibble(0x47) != 0) {
                for (i = 0; i < 8; i++) {
                    geom->field_8[i].vx += 2000;
                }
                taskKill(task);
                return;
            }
            task->state++;
            break;
        case 1:
            if (GameFlag_GetNibble(0x47) > 0) {
                for (i = 0; i < 8; i++) {
                    geom->field_8[i].vx += 2000;
                }
                task->state++;
            }
            break;
        default:
            taskKill(task);
            break;
    }
}

void func_dryfield_night_factory_8017FA08(Task* task)
{
    GsCOORDINATE2*    coord;
    NightFactoryWork* work;
    TmdObject*        obj;
    s32               flag;
    s32               prev;

    /* The model pointer is read twice on purpose: the second read is what
       leaves the target's `move s4, v0` copy. */
    coord = ((TmdObject*)task->extra)->coords;
    work  = (NightFactoryWork*)task->work;
    obj   = (TmdObject*)task->extra;
    flag  = GameFlag_GetNibble(0x49);
    prev  = work->field_0;
    if (flag != prev) {
        if ((flag ^ prev) & 1) {
            work->field_16 = 0;
        }
        if ((flag ^ work->field_0) & 2) {
            work->field_17 = 0;
        }
        work->field_0  = flag;
        work->field_14 = 0;
    }
    if (flag & 2) {
        func_dryfield_night_factory_8017E13C(task);
        if (flag & 1) {
            func_dryfield_night_factory_8017DA54(task);
        } else {
            func_dryfield_night_factory_8017DDD4(task);
        }
    } else {
        func_dryfield_night_factory_8017E480(task);
        if (flag & 1) {
            func_dryfield_night_factory_8017E7A4(task);
        } else {
            func_dryfield_night_factory_8017EBD4(task);
        }
    }
    work->field_14++;
    func_dryfield_night_factory_8017D858(task, 0, flag & 1);
    Gp_UpdateCoord(coord);
    func_800D7A9C(obj, (VECTOR*)coord->workm.t, 0, 3);
}

void func_dryfield_night_factory_8017FB48(Task* task)
{
    taskKill(task);
}

void func_dryfield_night_factory_8017FB68(Task* task)
{
    GsCOORDINATE2*    coord;
    NightFactoryWork* work;
    TmdObject*        extra;

    work            = (NightFactoryWork*)task->work;
    extra           = (TmdObject*)task->extra;
    coord           = extra->coords;
    extra->lightMtx = &work->light;
    extra->colorMtx = &work->color;
    coord->flg      = 0;
    Gp_UpdateCoord(coord);
    func_800D7A9C(extra, (VECTOR*)coord->workm.t, 0, 3);
}

void func_dryfield_night_factory_8017FBC8(Task* arg0)
{
    if (arg0 != NULL) {
        Gp_DispatchMsg(arg0, 0x13F3, 0, 0);
    }
}

/// State 0 of the room's cutscene task: allocates the cutscene work block into
/// `Task::work` and dresses the task's model after the factory model in
/// `Task::spawnArg2`. Both bits of the model's flags follow the factory
/// model's, the root coordinate is seeded with a fixed offset under the
/// factory model's coordinate, and the factory model's light and colour
/// matrices are shared. A nibble of 1 in game flag 0x4E means the scene is
/// already on, which parks the cutscene state at 0xFF and turns the root
/// rotation -0x300 about X. The factory task then adopts this one, which steps
/// on.
void func_dryfield_night_factory_8017FBF4(Task* task)
{
    Task*                     cap      = task->spawnArg2;
    TmdObject*                model    = task->extra;
    TmdObject*                capModel = cap->extra;
    GsCOORDINATE2*            coord    = model->coords;
    GsCOORDINATE2*            capCoord = capModel->coords;
    NightFactoryCutsceneWork* work     = memCalloc(0xC, 0);
    u16                       flags;

    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->work   = (TaskIdMap*)work;
    flags        = model->flags | 0x80;
    model->flags = flags;
    if (!(capModel->flags & 0x80)) {
        model->flags = flags & 0xFF7F;
    }
    if (!(capModel->flags & 4)) {
        model->flags &= 0xFFFB;
        Tmd_AllocBuffers(model);
    } else {
        model->flags |= 4;
    }
    model->otOffset   = -1;
    coord->coord.t[1] = -0x316;
    coord->sub        = capCoord;
    coord->coord.t[0] = 0;
    coord->coord.t[2] = -0x5FA;
    if (GameFlag_GetNibble(0x4E) == 1) {
        work->state = 0xFF;
        RotMatrixX(-0x300, &coord->coord);
    }
    coord->flg      = 0;
    model->lightMtx = capModel->lightMtx;
    model->colorMtx = capModel->colorMtx;
    Task_Reparent(cap, task);
    task->state += 1;
}

/// Runs the current state of the room's cutscene sequence, copying the room's
/// three handlers onto the stack first so the call goes through a local table
/// rather than through `.rodata`. A handler returning non-zero has finished its
/// part of the scene, which drops the sequence back to the shared state 0.
void func_dryfield_night_factory_8017FD5C(Task* task)
{
    NightFactoryCutsceneWork*  work = (NightFactoryCutsceneWork*)task->work;
    NightFactoryCutsceneTable3 sp;

    sp = D_dryfield_night_factory_8017D5DC;
    if (sp.funcs[work->state](task) != 0) {
        work->state = 0;
    }
}

/// State 0 of the cutscene sequence: edge-detects game flag 0x4E and re-arms
/// the sequence when it changes. A nibble of 1 after a 0 moves the sequence to
/// state 1, and a nibble of 0 after a 1 moves it to state 2; either transition
/// restarts `step`. Every call records the nibble in `prevFlag`.
s32 func_dryfield_night_factory_8017FDC8(Task* task)
{
    NightFactoryCutsceneWork* work  = (NightFactoryCutsceneWork*)task->work;
    s32                       flag  = GameFlag_GetNibble(0x4E);
    s32                       state = flag & 0xFF;

    if (state == 1) {
        if (work->prevFlag == 0) {
            work->state = state;
            goto reset;
        }
    }
    if (((flag & 0xFF) == 0) && (work->prevFlag == 1)) {
        work->state = 2;
    reset:
        work->step = 0;
    }
    work->prevFlag = flag;
    return 0;
}

/// Runs the factory model task's current state, through a copy of its handler
/// table on the stack.
void func_dryfield_night_factory_8017FE44(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_factory_8017D5C4;
    sp.funcs[task->state](task);
}

/// Runs the cutscene task's current state, through a copy of its handler table
/// on the stack.
void func_dryfield_night_factory_8017FE9C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_factory_8017D5D0;
    sp.funcs[task->state](task);
}

/// Second cutscene driver for the night factory: silences both weapons, runs
/// the cap command in `Task::spawnArg1`, and once the cap reports event key 3
/// records progress flag 0x4A, restores the weapons and kills the task.
void func_dryfield_night_factory_8017FEF4(Task* task)
{
    s32 state = task->state;

    switch (state) {
        case 0:
            Gp_MsgPlayerWeapon(0);
            Gp_MsgAllyWeapon(0);
            Gp_RunCapCmd(task->spawnArg1, 0);
            goto advance;
        case 1:
            if (GameFlag_GetNibble(0x4A) < 2) {
                Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F3, 0, 0);
            }
            task->state++;
            /* fallthrough */
        case 2:
            if (Gp_CapBusy() != 0) {
                return;
            }
        advance:
            task->state++;
            return;
        case 3:
            if (Gp_GetCapEventKey() == state) {
                GameFlag_SetNibble(0x4A, 2);
            }
            Gp_MsgPlayerWeapon(1);
            Gp_MsgAllyWeapon(1);
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F3, 1, 0);
            taskKill(task);
            break;
    }
}

/// Runs the cap command in `Task::spawnArg1` unless game flag 0x47 is set, then
/// kills the task.
void func_dryfield_night_factory_80180038(Task* arg0)
{
    if (GameFlag_GetNibble(0x47) == 0) {
        Gp_RunCapCmd1(arg0->spawnArg1);
    }
    taskKill(arg0);
}

/// Plays the cutscene sequence out: sets game flag 0x4E, waits 0x3C frames,
/// runs the cap command in `Task::spawnArg1` and clears the flag again, waits
/// 0x1E frames, then hands the player and the ally their weapons back and kills
/// the task.
void func_dryfield_night_factory_8018007C(Task* task)
{
    switch (task->state) {
        case 0:
            GameFlag_SetNibble(0x4E, 1);
            task->killCountdown = 0x3C;
            task->state         = task->state + 1;
            return;
        case 2:
            Gp_RunCapCmd1(task->spawnArg1);
            GameFlag_SetNibble(0x4E, 0);
            task->killCountdown = 0x1E;
            task->state         = task->state + 1;
            return;
        case 1:
        case 3:
            if (--task->killCountdown < 0) {
                task->state = task->state + 1;
            }
            return;
        default:
            Gp_MsgPlayerWeapon(1);
            Gp_MsgAllyWeapon(1);
            taskKill(task);
            return;
    }
}
