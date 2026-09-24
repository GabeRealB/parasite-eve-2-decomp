#include "common.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "actors/actor_342400.h"

void func_actor_342400_8016B33C(Task* arg0);

/// After 0x18 frames sets model flag 2, clears the frame counter, sets
/// `field_451` and advances the state.
void func_actor_342400_8016A950(Task* arg0)
{
    u16              ticks;
    Actor342400Work* work;
    TmdObject*       model;

    work            = (Actor342400Work*)arg0->work;
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

void func_actor_342400_8016A9AC(Task* arg0)
{
    Actor342400Work* work;

    work            = (Actor342400Work*)arg0->work;
    arg0->state     = 5;
    work->field_420 = 0;
    work->field_422 = 0;
}

/// Advances the state after two frames.
void func_actor_342400_8016A9C4(Task* arg0)
{
    u16              ticks;
    Actor342400Work* work;

    work            = (Actor342400Work*)arg0->work;
    ticks           = work->field_412 + 1;
    work->field_412 = ticks;
    if ((s16)ticks >= 2) {
        work->field_420 = work->field_420 + 1;
    }
}

void func_actor_342400_8016AA08(Task* arg0)
{
    Actor342400Work* work;
    Actor342400Work* work2;
    TmdObject*       model;
    GpEnemy*         enemy;

    model = (TmdObject*)arg0->extra;
    enemy = (GpEnemy*)arg0->spawnArg2;
    Tmd_FreeBuffers(model);
    model->flags |= 4;
    func_actor_342400_801637DC(arg0);
    Gp_ReleaseStateF0Add((GpObj20E*)arg0, 0);
    enemy->recs = 0;
    work        = (Actor342400Work*)arg0->work;
    Gp_UnlinkObj(&work->obj_2AC);
    Gp_UnlinkObj(&work->obj_2CC);
    Gp_UnlinkObj(&work->obj_3AC);
    work2            = (Actor342400Work*)arg0->work;
    arg0->state      = 5;
    work2->field_420 = 0;
    work2->field_422 = 0;
}

void func_actor_342400_8016AA9C(Task* arg0)
{
    Actor342400Work* work;

    work            = (Actor342400Work*)arg0->work;
    work->field_412 = 0;
    work->field_420 = work->field_420 + 1;
}

/// After 0x24 frames destroys the enemy, first telling slot-4 task 0 with
/// message 0x13F4 when in place 1 of stage 4 areas 0x27/0x28.
void func_actor_342400_8016AAB8(Task* arg0)
{
    Actor342400Work* work;
    u16              ticks;

    work            = (Actor342400Work*)arg0->work;
    ticks           = work->field_412 + 1;
    work->field_412 = ticks;
    if ((s16)ticks >= 0x24) {
        if ((gGameSession->at4.loc.stage == 4) && ((u32)(gGameSession->at4.loc.area - 0x27) < 2U) && (gGameSession->at4.loc.place == 1)) {
            Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x13F4, 1, 0);
        }
        Gp_DestroyEnemy(arg0->spawnArg2, arg0);
    }
}

void func_actor_342400_8016AB6C(Task* arg0)
{
    Actor342400Work* work;
    Actor342400Work* work2;
    s32              soundId;
    s32              pan;

    work            = (Actor342400Work*)arg0->work;
    work->field_44F = D_actor_342400_80173A84[work->field_418 - 1];
    if (work->field_44F == 1) {
        work2            = (Actor342400Work*)arg0->work;
        work2->field_426 = 8;
        work2->field_41C = 0x10;
        work2->field_418 = 0xB;
        work2->field_414 = 1;
        SndEvt_EnqueueType7(0x402C0002, 1);
    } else {
        work2            = (Actor342400Work*)arg0->work;
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

void func_actor_342400_8016AC80(Task* arg0)
{
    Actor342400Work* work;
    Actor342400Work* work2;
    s32              soundId;
    s32              pan;

    work            = (Actor342400Work*)arg0->work;
    work->field_44F = D_actor_342400_80173A84[work->field_418 - 1];
    if (work->field_44F == 1) {
        work2            = (Actor342400Work*)arg0->work;
        work2->field_426 = 2;
        work2->field_41C = 0x10;
        work2->field_418 = 0xC;
        work2->field_414 = 1;
        SndEvt_EnqueueType7(0x402C0002, 1);
    } else {
        work2            = (Actor342400Work*)arg0->work;
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

void func_actor_342400_8016AD94(Task* arg0)
{
    Actor342400Work* work;
    s32              cond;

    work = (Actor342400Work*)arg0->work;
    if ((work->flags_EC.half & 1) || (work->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        if (work->field_44F == 1) {
            work            = (Actor342400Work*)arg0->work;
            work->field_420 = 3;
            work->field_422 = 0;
        } else {
            func_actor_342400_801694A8(arg0, 1);
            work            = (Actor342400Work*)arg0->work;
            work->field_420 = 5;
            work->field_422 = 0;
        }
    }
}

extern TaskFuncTable3 D_actor_342400_80161FB4;

void func_actor_342400_8016AE24(Task* arg0)
{
    Actor342400Work* work;
    TaskFuncTable3   sp;

    work = (Actor342400Work*)arg0->work;
    sp   = D_actor_342400_80161FB4;
    if ((func_actor_342400_8016945C(arg0) << 0x10) == 0) {
        sp.funcs[(s16)work->field_422](arg0);
    }
}

extern TaskFuncTable3 D_actor_342400_80161FC0;

void func_actor_342400_8016AEAC(Task* arg0)
{
    Actor342400Work* work;
    TaskFuncTable3   sp;

    work = (Actor342400Work*)arg0->work;
    sp   = D_actor_342400_80161FC0;
    if ((func_actor_342400_8016945C(arg0) << 0x10) == 0) {
        sp.funcs[(s16)work->field_422](arg0);
    }
}

void func_actor_342400_8016AF34(Task* arg0)
{
    Actor342400Work* work                = (Actor342400Work*)arg0->work;
    void             (*states[2])(Task*) = {
        func_actor_342400_8016B33C,
        func_actor_342400_8016B370,
    };

    if (func_actor_342400_8016945C(arg0) == 0) {
        states[(s16)work->field_422](arg0);
    }
}

extern TaskFuncTable3 D_actor_342400_80161FCC;

void func_actor_342400_8016AFA8(Task* arg0)
{
    Actor342400Work* work;
    TaskFuncTable3   sp;

    work = (Actor342400Work*)arg0->work;
    sp   = D_actor_342400_80161FCC;
    if ((func_actor_342400_8016945C(arg0) << 0x10) != 0) {
        work->field_438 = 0;
        return;
    }
    sp.funcs[(s16)work->field_422](arg0);
}

extern TaskFuncTable4 D_actor_342400_80161FD8;

/// Runs the sub-state handler for `field_422` from a four-entry table.
void func_actor_342400_8016B038(Task* arg0)
{
    Actor342400Work* work;
    TaskFuncTable4   sp;

    work = (Actor342400Work*)arg0->work;
    sp   = D_actor_342400_80161FD8;
    sp.funcs[(s16)work->field_422](arg0);
}
