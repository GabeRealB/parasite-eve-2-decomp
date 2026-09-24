#include "common.h"

#include "main/gfx.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

#include "actors/actor_341700.h"

void func_actor_341700_8016966C(Task* arg0)
{
    u16              ticks;
    Actor341700Work* work;
    TmdObject*       model;

    work            = (Actor341700Work*)arg0->work;
    model           = (TmdObject*)arg0->extra;
    ticks           = work->field_412 + 1;
    work->field_412 = ticks;
    if ((s16)ticks >= 0x18) {
        model->flags    = model->flags | 2;
        work->field_412 = 0U;
        work->field_451 = 1;
        work->field_420 = work->field_420 + 1;
    }
}

void func_actor_341700_801696C8(Task* arg0)
{
    Actor341700Work* work;

    work            = (Actor341700Work*)arg0->work;
    arg0->state     = 5;
    work->field_420 = 0;
    work->field_422 = 0;
}

void func_actor_341700_801696E0(Task* arg0)
{
    u16              ticks;
    Actor341700Work* work;

    work            = (Actor341700Work*)arg0->work;
    ticks           = work->field_412 + 1;
    work->field_412 = ticks;
    if ((s16)ticks >= 2) {
        work->field_420 = work->field_420 + 1;
    }
}

void func_actor_341700_80169724(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* work2;
    TmdObject*       model;
    GpEnemy*         enemy;

    model = (TmdObject*)arg0->extra;
    enemy = (GpEnemy*)arg0->spawnArg2;
    Tmd_FreeBuffers(model);
    model->flags |= 4;
    func_actor_341700_801624F8(arg0);
    Gp_ReleaseStateF0Add((GpObj20E*)arg0, 0);
    enemy->recs = 0;
    work        = (Actor341700Work*)arg0->work;
    Gp_UnlinkObj(&work->obj_2AC);
    Gp_UnlinkObj(&work->obj_2CC);
    Gp_UnlinkObj(&work->obj_3AC);
    work2            = (Actor341700Work*)arg0->work;
    arg0->state      = 5;
    work2->field_420 = 0;
    work2->field_422 = 0;
}

void func_actor_341700_801697B8(Task* arg0)
{
    Actor341700Work* work;

    work            = (Actor341700Work*)arg0->work;
    work->field_412 = 0;
    work->field_420 = work->field_420 + 1;
}

void func_actor_341700_801697D4(Task* arg0)
{
    Actor341700Work* work;
    u16              ticks;

    work            = (Actor341700Work*)arg0->work;
    ticks           = work->field_412 + 1;
    work->field_412 = ticks;
    if ((s16)ticks >= 0x24) {
        if ((gGameSession->at4.loc.stage == 4) && ((u32)(gGameSession->at4.loc.area - 0x27) < 2U) && (gGameSession->at4.loc.place == 1)) {
            Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x13F4, 1, 0);
        }
        Gp_DestroyEnemy(arg0->spawnArg2, arg0);
    }
}

void func_actor_341700_80169888(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* work2;
    s32              soundId;
    s32              pan;

    work            = (Actor341700Work*)arg0->work;
    work->field_44F = D_actor_341700_80174D88[work->field_418 - 1];
    if (work->field_44F == 1) {
        work2            = (Actor341700Work*)arg0->work;
        work2->field_426 = 8;
        work2->field_41C = 0x10;
        work2->field_418 = 0xB;
        work2->field_414 = 1;
        SndEvt_EnqueueType7(0x402C0002, 1);
    } else {
        work2            = (Actor341700Work*)arg0->work;
        work2->field_426 = 8;
        work2->field_41C = 0x10;
        work2->field_418 = 0x11;
        work2->field_414 = 1;
    }
    soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x402C0003;
    pan     = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
    SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
    work->field_422++;
}

void func_actor_341700_8016999C(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* work2;
    s32              soundId;
    s32              pan;

    work            = (Actor341700Work*)arg0->work;
    work->field_44F = D_actor_341700_80174D88[work->field_418 - 1];
    if (work->field_44F == 1) {
        work2            = (Actor341700Work*)arg0->work;
        work2->field_426 = 2;
        work2->field_41C = 0x10;
        work2->field_418 = 0xC;
        work2->field_414 = 1;
        SndEvt_EnqueueType7(0x402C0002, 1);
    } else {
        work2            = (Actor341700Work*)arg0->work;
        work2->field_426 = 8;
        work2->field_41C = 0x10;
        work2->field_418 = 0x11;
        work2->field_414 = 1;
    }
    soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x402C0003;
    pan     = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
    SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
    work->field_422++;
}

void func_actor_341700_80169AB0(Task* arg0)
{
    Actor341700Work* work;
    s32              cond;

    work = (Actor341700Work*)arg0->work;
    if ((work->flags_EC.half & 1) || (work->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        if (work->field_44F == 1) {
            work            = (Actor341700Work*)arg0->work;
            work->field_420 = 3;
            work->field_422 = 0;
        } else {
            func_actor_341700_801681C4(arg0, 1);
            work            = (Actor341700Work*)arg0->work;
            work->field_420 = 5;
            work->field_422 = 0;
        }
    }
}

void func_actor_341700_80169B40(Task* arg0)
{
    Actor341700Work* work;
    TaskFuncTable3   sp;

    work = (Actor341700Work*)arg0->work;
    sp   = D_actor_341700_80161F70;
    if ((func_actor_341700_80168178(arg0) << 0x10) == 0) {
        sp.funcs[(s16)work->field_422](arg0);
    }
}

void func_actor_341700_80169BC8(Task* arg0)
{
    Actor341700Work* work;
    TaskFuncTable3   sp;

    work = (Actor341700Work*)arg0->work;
    sp   = D_actor_341700_80161F7C;
    if ((func_actor_341700_80168178(arg0) << 0x10) == 0) {
        sp.funcs[(s16)work->field_422](arg0);
    }
}

void func_actor_341700_80169C50(Task* arg0)
{
    Actor341700Work* work                = (Actor341700Work*)arg0->work;
    void             (*states[2])(Task*) = {
        func_actor_341700_8016A058,
        func_actor_341700_8016A08C,
    };

    if (func_actor_341700_80168178(arg0) == 0) {
        states[(s16)work->field_422](arg0);
    }
}

void func_actor_341700_80169CC4(Task* arg0)
{
    Actor341700Work* work;
    TaskFuncTable3   sp;

    work = (Actor341700Work*)arg0->work;
    sp   = D_actor_341700_80161F88;
    if ((func_actor_341700_80168178(arg0) << 0x10) != 0) {
        work->field_438 = 0;
        return;
    }
    sp.funcs[(s16)work->field_422](arg0);
}

/// The four handlers of the actor's final state, dispatched on the same
/// `field_422` sub-state index the earlier tables use.
extern TaskFuncTable4 D_actor_341700_80161F94;

void func_actor_341700_80169D54(Task* arg0)
{
    Actor341700Work* work;
    TaskFuncTable4   sp;

    work = (Actor341700Work*)arg0->work;
    sp   = D_actor_341700_80161F94;
    sp.funcs[(s16)work->field_422](arg0);
}
