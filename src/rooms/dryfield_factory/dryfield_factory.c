#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/268.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/gameflag.h"
#include "main/gameflow.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/dryfield_factory.h"
#include "rooms/dryfield_night_factory.h"
#include "rooms/room_common.h"

#include <psyq/inline_c.h>
#include "gte.h"
#include <psyq/libgs.h>

/// The factory model's work block as the lighting helper sees it: `light` and
/// `color` are the model's own matrices, republished onto
/// `TmdObject::lightMtx` / `colorMtx`. The leading bytes are not read here.
typedef struct RoomUtil20Work {
    /* 0x00 */ byte   pad_0[0x18];
    /* 0x18 */ MATRIX light;
    /* 0x38 */ MATRIX color;
} RoomUtil20Work;

extern s16 D_80071076;
extern u8  D_8007216D;
extern u8  D_801153F4;
extern u8  D_8018A7BC[];
extern u8  D_8018A7C8[];

void func_8004BFF8(s32 angle, MATRIX* matrix);

/// The pending event message and request the gate latched, the flag saying
/// one was latched, and the descriptor of the task the gate spawns to play it.
extern RoomEventMsg D_dryfield_factory_8018A3B4;
extern u8           D_dryfield_factory_8018A3BC;
extern RoomEventReq D_dryfield_factory_8018A3CC;
extern TaskDesc     D_dryfield_factory_801826B0;

/// The spawn tables the room entry task selected for the session variant, and
/// the slot it parked for the poller.
extern TaskDesc* D_dryfield_factory_8018A3C0;
extern TaskDesc* D_dryfield_factory_8018A3C4;
extern Task**    D_dryfield_factory_8018A3C8;

extern u8         D_dryfield_factory_8018A39C[];
extern u8         D_dryfield_factory_8018A3A8[];
extern TaskDesc   D_dryfield_factory_801826BC[];
extern GpMsgEntry D_dryfield_factory_801826D4[];
extern TaskDesc   D_dryfield_factory_80186DE0[];
extern TaskDesc   D_dryfield_factory_80186E28[];
extern TaskDesc   D_dryfield_factory_80186E94[];
extern GpMsgEntry D_dryfield_factory_80186EA0[];

extern GpGridParams D_dryfield_factory_80186C68;
extern GpGridParams D_dryfield_factory_80186D38;
extern GpGridParams D_dryfield_factory_80186E04;
extern GpGridParams D_dryfield_factory_80187BF0;
extern GpGridParams D_dryfield_factory_80187BF8;

void func_dryfield_factory_8017DF80(Task* task);
void func_dryfield_factory_8017DFE0(Task* task);
void func_dryfield_factory_8017E140(Task* task, s32 remapFaces, s32 useAltTemplate);
s32  func_dryfield_factory_8017F8F4(Task* task);
s32  func_dryfield_factory_8017FAC4(Task* task);
void func_dryfield_factory_801802F0(Task* task);
void func_dryfield_factory_80180430(Task* task);
void func_dryfield_factory_80180450(Task* task);
void func_dryfield_factory_801804B0(Task* task);
void func_dryfield_factory_801804DC(Task* task);
void func_dryfield_factory_80180644(Task* task);
s32  func_dryfield_factory_801806B0(Task* task);

s32 func_dryfield_factory_8017D6F8(RoomEventReq* req, RoomEventMsg* msg)
{
    s32 flag;
    s32 id;
    s32 mode;
    s32 got;
    s32 ret;
    s32 neg;

    flag                        = req->flagId;
    D_dryfield_factory_8018A3BC = 0;
    neg                         = flag < 0;
    got                         = (s16)flag;
    if (neg) {
        flag = -flag;
        got  = GameFlag_GetNibble(flag) == 0;
    } else {
        got = GameFlag_GetNibble(got);
    }
    ret = 1;
    if (got == 0) {
        if (Gp_HasCollectedBit(req->itemId) != 0 || req->itemId == 0) {
            ret = 2;
            if (msg->field_5 == 0) {
                D_dryfield_factory_8018A3B4 = *msg;
                D_dryfield_factory_8018A3CC = *req;
                id                          = req->flagId;
                mode                        = 1;
                if (id < 0) {
                    id   = -id;
                    mode = 0;
                }
                GameFlag_SetNibble(id, mode);
                Task_SpawnFromTable(&D_dryfield_factory_801826B0, 0, 0, 0);
                D_dryfield_factory_8018A3BC = 1;
                return 2;
            }
            return ret;
        }
        ret = 0;
        if (msg->field_5 == 0) {
            Gp_RunCapCmd1(req->field_4);
            Gp_SetNibbleIf(msg->field_6, 2);
            ret = 0;
        }
        return ret;
    }
    return ret;
}

void func_dryfield_factory_8017D85C(Task* task)
{
    switch (task->state) {
        case 0:
            D_801153F4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd1(D_dryfield_factory_8018A3CC.field_0);
            if (D_dryfield_factory_8018A3CC.field_8 != 0) {
                SndEvt_EnqueueType6(D_dryfield_factory_8018A3CC.field_8, 0, 0);
                task->state++;
            } else {
                task->state = 2;
            }
            break;
        case 1:
            if (SndVoice_HasActiveId(D_dryfield_factory_8018A3CC.field_8) == 0) {
                task->state++;
            }
            break;
        case 2:
            task->state++;
            break;
        case 3:
            if (D_dryfield_factory_8018A3CC.field_C != 0) {
                SndEvt_EnqueueType6(D_dryfield_factory_8018A3CC.field_C, 0, 0);
                task->state++;
            } else {
                task->state = 5;
            }
            break;
        case 4:
            if (SndVoice_HasActiveId(D_dryfield_factory_8018A3CC.field_C) == 0) {
                task->state++;
            }
            break;
        case 5:
            SndEvt_EnqueueType7(0x80000000, 0);
            D_80071076               = 1;
            Mc_SaveData.at4.loc.area = D_dryfield_factory_8018A3B4.msgId;
            Mc_SaveData.at4.loc.warp = D_dryfield_factory_8018A3B4.field_2;
            Mc_SaveData.at4.loc.room = (u8)D_dryfield_factory_8018A3B4.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(task);
            break;
    }
}

/// Room entry task: publishes the room's message table, claims game pointer
/// slot 7 and parks a fresh one-word slot at `Task::work` (also kept in
/// `D_..._8018A3C8`) for the poller to fill. It then picks the spawn tables for
/// the session variant (`stage == 2` or not), spawns entries 4 and 5 of the
/// first, and passes progress nibble 0x48 on.
///
/// Outside stage 2 the second pick is the address of the room's script message
/// table, stored where a spawn table is expected; the cast records that.
///
/// `slot` and the store to `D_..._8018A3C8` are one chained assignment on
/// purpose: GCC then materialises the global's address ahead of `memCalloc`,
/// which is the register allocation the target has.
void func_dryfield_factory_8017D9CC(Task* arg0)
{
    Task** slot;

    arg0->msgTable = D_dryfield_factory_801826D4;
    Game_SetPtrSlot(arg0, 7);
    slot       = (D_dryfield_factory_8018A3C8 = memCalloc(4, 0));
    arg0->work = (TaskIdMap*)slot;
    if (gGameSession->at4.loc.stage == 2) {
        D_dryfield_factory_8018A3C4 = D_dryfield_factory_80186E28;
    } else {
        D_dryfield_factory_8018A3C4 = D_dryfield_factory_80186DE0;
    }
    if (gGameSession->at4.loc.stage == 2) {
        D_dryfield_factory_8018A3C0 = D_dryfield_factory_80186E94;
    } else {
        D_dryfield_factory_8018A3C0 = (TaskDesc*)D_dryfield_factory_80186EA0;
    }
    Task_SpawnFromTable(D_dryfield_factory_8018A3C4, 4, 0, (s32)D_dryfield_factory_8018A3C8);
    Task_SpawnFromTable(D_dryfield_factory_8018A3C4, 5, 0, 0);
    if (gGameSession->at4.loc.stage == 2) {
        func_dryfield_factory_80181620(GameFlag_GetNibble(0x48) & 0xFF);
        SOFT_BARRIER();
    } else {
        func_dryfield_factory_80181620(GameFlag_GetNibble(0x48) & 0xFF);
    }
    arg0->state++;
}

s32 func_dryfield_factory_8017DB08(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomEventReq req;
    u8           variant;

    *out = *in;
    if (in->msgId == 0x19) {
        variant = gGameSession->at4.loc.stage;
        if (variant == 2) {
            if (in->field_5 == 0) {
                if (GameFlag_GetNibble(0x3A) >= 2) {
                    out->field_3 = variant;
                } else {
                    out->field_3 = 1;
                }
            }
        } else if (in->field_5 == 0) {
            out->field_3 = GameFlag_GetNibble(0x61) + 1;
        }
    }
    if (in->msgId == 0x18) {
        if (in->field_5 == 0) {
            if (GameFlag_GetNibble(0x7A) < 4) {
                out->field_3 = 1;
            } else {
                out->field_3 = 2;
            }
        }
        if (in->msgId == 0x18) {
            if (GameFlag_GetNibble(0x4A) != 2) {
                if (in->field_5 != 0) {
                    return 0;
                }
                Gp_StartCapSlot(4, 1, 0);
                Gp_SetNibbleIf(in->field_6, 2);
                return 0;
            }
        }
    }
    if (in->msgId == 0x16) {
        if (GameFlag_GetNibble(0x37) == 0) {
            if (in->field_5 != 0) {
                return 0;
            }
            Gp_SetNibbleIf(in->field_6, 2);
            Gp_RunCapCmd1(0xD);
            return 0;
        }
    }
    if (in->msgId == 0x19) {
        req.field_0 = 0xE;
        req.field_4 = 0xE;
        req.field_8 = 0x52170013;
        req.field_C = 0x52170003;
        req.flagId  = -0x30;
        req.itemId  = 0;
        return func_dryfield_factory_8017D6F8(&req, in);
    }
    return 1;
}

/// Spawns entry 0 of the table the entry task selected into the slot it
/// parked, then kills itself once that task has been killed.
void func_dryfield_factory_8017DD00(Task* task)
{
    s32 poll;

    switch (task->state) {
        case 0:
            *D_dryfield_factory_8018A3C8 = Task_SpawnFromTable(D_dryfield_factory_8018A3C0, 0, 0, 0);
            task->state++;
            return;
        case 1:
            if (Task_PollKill(*D_dryfield_factory_8018A3C8, &poll) != 0) {
                taskKill(task);
            }
            return;
    }
}

/// State handlers of the room entry task, indexed by `Task::state`: the set-up
/// tick, the idle tick and `taskKill`.
const TaskFuncTable3 D_dryfield_factory_8017D5DC = {
    { func_dryfield_factory_8017D9CC, func_dryfield_factory_8017DF80, taskKill },
};

s32 func_dryfield_factory_8017DDA0(void)
{
    return 0;
}

/// Command handler of the room's message table. Commands 1, 2, 3, 5 and 12
/// spawn entries 2, 3, 1, 0 and 6 of the spawn table the entry task selected,
/// passing the command on; 12 only while progress nibble 0x49 is 1, after
/// silencing both weapons. Command 6 silences both characters and spawns the
/// room's own table instead. Every command answers 0.
///
/// The `goto`s give every path the single `return 0` at `end`, the target's
/// shape.
s32 func_dryfield_factory_8017DDA8(s32 arg0, s32 arg1, s32 cmd)
{
    TaskDesc* table;
    s32       idx;

    switch (cmd) {
        case 1:
            table = D_dryfield_factory_8018A3C4;
            idx   = 2;
            break;
        case 2:
            table = D_dryfield_factory_8018A3C4;
            idx   = 3;
            break;
        case 3:
            table = D_dryfield_factory_8018A3C4;
            idx   = 1;
            break;
        case 5:
            table = D_dryfield_factory_8018A3C4;
            idx   = 0;
            break;
        case 6:
            Gp_MsgPlayerWeapon(0);
            Gp_MsgPlayer3F3(0);
            Gp_MsgAllyWeapon(0);
            Gp_MsgAlly3F3(0);
            Task_SpawnFromTable(D_dryfield_factory_801826BC, 0, 0, 0);
            goto end;
        case 12:
            if (GameFlag_GetNibble(0x49) == 1) {
                Gp_MsgPlayerWeapon(0);
                Gp_MsgAllyWeapon(0);
                table = D_dryfield_factory_8018A3C4;
                idx   = 6;
                break;
            }
            goto end;
        default:
            goto end;
    }
    Task_SpawnFromTable(table, idx, cmd, 0);
end:
    return 0;
}

/// State handlers of the factory model task: set-up, the per-frame state and
/// `taskKill`.
const TaskFuncTable3 D_dryfield_factory_8017D61C = {
    { func_dryfield_factory_8017DFE0, func_dryfield_factory_801802F0, taskKill },
};

/// State handlers of the cutscene task: set-up, the cutscene sequence and
/// `taskKill`.
const TaskFuncTable3 D_dryfield_factory_8017D628 = {
    { func_dryfield_factory_801804DC, func_dryfield_factory_80180644, taskKill },
};

/// The cutscene sequence's handlers: the flag watcher of state 0 and the two
/// movements it arms.
const NightFactoryCutsceneTable3 D_dryfield_factory_8017D634 = {
    { func_dryfield_factory_801806B0, func_dryfield_factory_8017F8F4, func_dryfield_factory_8017FAC4 },
};

s32 func_dryfield_factory_8017DEA8(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    switch (arg2) {
        case 7:
            Gp_EnqueueStageSnd6(0x52170007, 0, 0);
            break;
        case 21:
            Gp_EnqueueStageSnd6(0x52170015, 0, 0);
            GameFlag_SetNibble(0x4A, 2);
            break;
    }
    return 0;
}

s32 func_dryfield_factory_8017DF14(Task* task, s32 msgId, GpMsg13EF* arg2)
{
    if ((arg2->field_2 == 1) && (GameFlag_GetNibble(0x2C) == 0)) {
        Gp_SpawnIfCapIdle(0xB, 1);
        GameFlag_SetNibble(0x2C, 1);
        func_800E3FAC(0xA2, 0xA);
        SndEvt_EnqueueType6(0x5217000A, 0, 0);
    }
    return 0;
}

/// Idle state of the room entry task.
void func_dryfield_factory_8017DF80(Task* task)
{
}

/// Runs the task's current state out of a three-entry table copied onto the
/// stack.
void func_dryfield_factory_8017DF88(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_factory_8017D5DC;
    sp.funcs[task->state](task);
}

/// State 0 of the factory model: allocates and seeds its work block from
/// progress nibble 0x49, places the model, rebuilds the collision faces it
/// carries, spawns entry 7 of the session variant's spawn table with the task
/// as its argument and advances.
///
/// Each arm passes its table straight to `Task_SpawnFromTable` rather than
/// through a variable, which lets the two identical call tails merge as in the
/// target.
void func_dryfield_factory_8017DFE0(Task* task)
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
    func_dryfield_factory_80180450(task);
    func_dryfield_factory_8017E140(task, 1, 0);
    if (gGameSession->at4.loc.stage == 2) {
        Task_SpawnFromTable(D_dryfield_factory_80186E28, 7, 0, (s32)task);
    } else {
        Task_SpawnFromTable(D_dryfield_factory_80186DE0, 7, 0, (s32)task);
    }
    task->exitCallback  = func_dryfield_factory_80180430;
    task->killCountdown = 0;
    task->state++;
}

/// Rebuilds four faces of the room's collision grid from a template, moved
/// into the frame of the task's model: the template normals are rotated into
/// grid normals 2..5 and its corners rotated and translated into corners 8..15.
/// When `remapFaces` is set, the template's four face records are copied into
/// faces 2..5 with their corner and normal indices rebased onto those slots.
void func_dryfield_factory_8017E140(Task* task, s32 remapFaces, s32 useAltTemplate)
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
        geom = &D_dryfield_factory_80187BF8;
    } else {
        geom = &D_dryfield_factory_80187BF0;
    }
    if (useAltTemplate != 0) {
        src = &D_dryfield_factory_80186E04;
    } else {
        src = &D_dryfield_factory_80186D38;
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

s32 func_dryfield_factory_8017E33C(Task* task)
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
                func_dryfield_factory_801804B0(*(Task**)task->spawnArg2);
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
        func_dryfield_factory_801804B0(*(Task**)task->spawnArg2);
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

s32 func_dryfield_factory_8017E6BC(Task* task)
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
                func_dryfield_factory_801804B0(*(Task**)task->spawnArg2);
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
        func_dryfield_factory_801804B0(*(Task**)task->spawnArg2);
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

s32 func_dryfield_factory_8017EA24(Task* task)
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
        work->field_C.value = -0x23A0000;
        work->field_17      = 4;
    }
    coord->coord.t[1] = work->field_C.part.whole;
    coord->flg        = 0;
    return done;
}

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

s32 func_dryfield_factory_8017F8F4(Task* task)
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

s32 func_dryfield_factory_8017FAC4(Task* task)
{
    NightFactoryCutsceneWork* work  = (NightFactoryCutsceneWork*)task->work;
    GsCOORDINATE2*            coord = ((TmdObject*)task->extra)->coords;
    MATRIX*                   m;
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

/// Cutscene driver for the factory room: silences both weapons, runs the cap
/// (cutscene) command in `Task::spawnArg1`, then waits for the cap to report
/// event key 3 before setting the two progress flags and starting the follow-up
/// cap slot. Any state past 4 restores the weapons and kills the task.
void func_dryfield_factory_8017FC18(Task* task)
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
                    func_dryfield_factory_80181B38(0);
                    SOFT_BARRIER();
                } else {
                    func_dryfield_factory_80181B38(0);
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
                    func_dryfield_factory_80181B38(1);
                    func_dryfield_factory_80181620(1);
                    SOFT_BARRIER();
                } else {
                    func_dryfield_factory_80181B38(1);
                    func_dryfield_factory_80181620(1);
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
/// `Task::spawnArg1` and wait for it to report event key 1; states 3 and 6
/// count `Task::killCountdown` up to and back down from 0x1E, tinting the
/// screen with the count scaled to 0xFF; states 4 and 5 record the progress
/// flags, switch the room and tint it white; anything past 6 restores the
/// weapons and kills the task.
///
/// `fade` doubles as the stage read in state 4 on purpose: that cross-block use
/// makes the tint a global pseudo, which is what gives the target's register
/// allocation for the `(u8)` conversion.
void func_dryfield_factory_8017FDDC(Task* task)
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

/// Restores two faces of the room's collision grid (their normals, corners and
/// face records) from a template, then slides the eight corners 2000 units
/// along x once game flag 0x47 is set: at once in state 0, which then kills the
/// task, or from state 1 when the flag turns positive later.
void func_dryfield_factory_8018001C(Task* task)
{
    GpGridParams* src = &D_dryfield_factory_80186C68;
    GpGridParams* geom;
    s32           i;

    if (gGameSession->at4.loc.stage == 2) {
        geom = &D_dryfield_factory_80187BF8;
    } else {
        geom = &D_dryfield_factory_80187BF0;
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

/// Per-frame state of the factory model: when progress nibble 0x49 changes,
/// restarts the handler whose bit changed. Bit 1 selects the first handler pair
/// and bit 0 the member of the pair; the frame counter is bumped, the
/// collision faces rebuilt and the model's coordinate refreshed.
void func_dryfield_factory_801802F0(Task* task)
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
        func_dryfield_factory_8017EA24(task);
        if (flag & 1) {
            func_dryfield_factory_8017E33C(task);
        } else {
            func_dryfield_factory_8017E6BC(task);
        }
    } else {
        func_dryfield_factory_8017ED68(task);
        if (flag & 1) {
            func_dryfield_factory_8017F08C(task);
        } else {
            func_dryfield_factory_8017F4BC(task);
        }
    }
    work->field_14++;
    func_dryfield_factory_8017E140(task, 0, flag & 1);
    Gp_UpdateCoord(coord);
    func_800D7A9C(obj, (VECTOR*)coord->workm.t, 0, 3);
}

void func_dryfield_factory_80180430(Task* task)
{
    taskKill(task);
}

/// Binds the task's TMD object to the work-block light/color matrices, clears
/// the root coordinate flag, and rebuilds lighting from the world translation.
void func_dryfield_factory_80180450(Task* task)
{
    GsCOORDINATE2*  coord;
    RoomUtil20Work* work;
    TmdObject*      extra;

    work            = (RoomUtil20Work*)task->work;
    extra           = (TmdObject*)task->extra;
    coord           = extra->coords;
    extra->lightMtx = &work->light;
    extra->colorMtx = &work->color;
    coord->flg      = 0;
    Gp_UpdateCoord(coord);
    func_800D7A9C(extra, (VECTOR*)coord->workm.t, 0, 3);
}

void func_dryfield_factory_801804B0(Task* arg0)
{
    if (arg0 != NULL) {
        Gp_DispatchMsg(arg0, 0x13F3, 0, 0);
    }
}

/// State 0 of the room's cutscene task: allocates the 0xC cutscene work block
/// into `Task::work` and re-dresses the task's model off the cap task in
/// `Task::spawnArg2`. Both bits of the model's `field_C` follow the cap model's,
/// the root coordinate is seeded with the factory's position and takes the cap
/// model's coordinate as its `sub`, and the cap model's light and colour
/// matrices are copied across. Game flag 0x4E's nibble of 1 means the scene is
/// already on, which parks the cutscene state at 0xFF and turns the root
/// rotation -0x300 about X. The cap then adopts the task, which steps on.
void func_dryfield_factory_801804DC(Task* task)
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

/// Runs the current state of the room's cutscene sequence out of its three
/// handlers, copied onto the stack. A handler returning non-zero has finished
/// its part of the scene, which drops the sequence back to state 0.
void func_dryfield_factory_80180644(Task* task)
{
    NightFactoryCutsceneWork*  work = (NightFactoryCutsceneWork*)task->work;
    NightFactoryCutsceneTable3 sp;

    sp = D_dryfield_factory_8017D634;
    if (sp.funcs[work->state](task) != 0) {
        work->state = 0;
    }
}

/// State 0 of the room's cutscene sequence: edge-detects game flag 0x4E and
/// re-arms the sequence when it changes. A nibble of 1 with no 1 seen the call
/// before moves the handler to state 1, and a nibble of 0 after a 1 moves it
/// to state 2; either transition restarts `step`. Every call records the
/// nibble in `prevFlag`.
s32 func_dryfield_factory_801806B0(Task* task)
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

/// Runs the task's current state out of a three-entry table copied onto the
/// stack.
void func_dryfield_factory_8018072C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_factory_8017D61C;
    sp.funcs[task->state](task);
}

/// Runs the task's current state out of a three-entry table copied onto the
/// stack.
void func_dryfield_factory_80180784(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_factory_8017D628;
    sp.funcs[task->state](task);
}

/// Shorter variant of the factory cutscene driver: silences both weapons, runs
/// the cap command in `Task::spawnArg1`, waits for the cap to finish, then on
/// event key 3 records progress flag 0x4A and kills the task after restoring
/// the weapons.
void func_dryfield_factory_801807DC(Task* task)
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

void func_dryfield_factory_80180920(Task* arg0)
{
    if (GameFlag_GetNibble(0x47) == 0) {
        Gp_RunCapCmd1(arg0->spawnArg1);
    }
    taskKill(arg0);
}

void func_dryfield_factory_80180964(Task* task)
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
