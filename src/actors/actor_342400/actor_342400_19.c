#include "common.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "actors/actor_342400.h"

/// Death cry: plays sound 2, releases this enemy's `Gp_StateF0` hold and
/// unlinks the enemy node. A pending request 4 hides the model and jumps to
/// state 7; otherwise the state advances.
void func_actor_342400_8016A664(Task* arg0)
{
    GpEnemy*         enemy;
    Actor342400Work* work;
    TmdObject*       model;
    Actor342400Work* work2;

    enemy = (GpEnemy*)arg0->spawnArg2;
    model = (TmdObject*)arg0->extra;
    work  = (Actor342400Work*)arg0->work;
    SndEvt_EnqueueType7(((enemy->placeKey >> 0xC) << 8) | 0x402C0002, 0xF);
    func_actor_342400_801694A8(arg0, 0);
    Gp_UnlinkNode(&enemy->node);
    if (work->field_448 == 4) {
        work->field_412  = 0;
        model->flags     = model->flags | 0x80;
        work2            = (Actor342400Work*)arg0->work;
        work2->field_420 = 7;
        work2->field_422 = 0;
        return;
    }
    work->field_420 = work->field_420 + 1;
}

void func_actor_342400_8016A724(Task* arg0)
{
    Actor342400Work* work;
    Actor342400Work* work2;
    Actor342400Work* work3;
    Actor342400Work* work4;
    s16              anim;
    s16              next;

    work = (Actor342400Work*)arg0->work;
    Gp_ReleaseStateF0Add((GpObj20E*)arg0, 0);
    anim = work->field_418;
    if (anim == 8) {
        if (work->field_440 == 0) {
            work2            = (Actor342400Work*)arg0->work;
            work2->field_426 = 4;
            work2->field_41C = 0x10;
            work2->field_418 = 5;
            work2->field_414 = 1;
        } else {
            work3            = (Actor342400Work*)arg0->work;
            work3->field_426 = 4;
            work3->field_41C = 0x10;
            work3->field_418 = 6;
            work3->field_414 = 1;
        }
    } else {
        next             = D_actor_342400_80173A98[anim - 1];
        work4            = (Actor342400Work*)arg0->work;
        work4->field_426 = 4;
        work4->field_41C = 0x10;
        work4->field_418 = next;
        work4->field_414 = 1;
    }
    func_actor_342400_80165CC0(arg0);
    work->field_420++;
}

/// Ticks the animation and, once the hit flags are set, advances the state.
void func_actor_342400_8016A804(Task* arg0)
{
    Actor342400Work* work;
    Actor342400Work* work2;
    s32              cond;

    work = (Actor342400Work*)arg0->work;
    func_actor_342400_80165CC0(arg0);
    work2 = (Actor342400Work*)arg0->work;
    if ((work2->flags_EC.half & 1) || (work2->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->field_420 = work->field_420 + 1;
    }
}
