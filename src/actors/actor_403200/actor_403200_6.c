#include "common.h"
#include "actors/actor_403200.h"
#include "gameplay/1BC.h"
#include "main/task.h"
#include "main/tmd.h"

/// The enemy's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_403200_80131E84;

/// State tables of the enemies this actor spawns, one per enemy kind.
extern GpEnemyTaskFuncTable3 D_actor_403200_80131E90;
extern GpEnemyTaskFuncTable5 D_actor_403200_80131E9C;
extern GpEnemyTaskFuncTable4 D_actor_403200_80131F04;
extern GpEnemyTaskFuncTable5 D_actor_403200_80131F14;
extern GpEnemyTaskFuncTable4 D_actor_403200_80131F28;

/// Global game mode: 0 while play runs; the dispatchers park their models on
/// 1 and hide them on 2.
extern u8 D_801153F4;

/// Non-zero while the overlay is shutting down.
extern s16 D_actor_403200_80141C50;
/// Set to 1 when the spinner enemies are released from their waiting state.
extern s16 D_actor_403200_80141C5A;

void func_actor_403200_80141430(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_403200_80131E84;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

void func_actor_403200_8014148C(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_403200_80131E84;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

/// Dispatcher of the enemy `D_actor_403200_80131E90` drives: run the handler for
/// the task's state, skipped while the global game mode is 2.
void func_actor_403200_801414E8(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_403200_80131E90;
    switch (D_801153F4) {
        default:
        case 0:
        case 1:
            sp.funcs[arg0->state](arg0->spawnArg2, arg0);
            break;
        case 2:
            break;
    }
}

/// Dispatcher of the grab enemy (`D_actor_403200_80131E9C`): park the model
/// object while the global game mode is 1 or 2, otherwise note in the work
/// block whether the state changed since the last step and run the handler for
/// it. Unlike `func_actor_403200_80141670` it guards the bookkeeping on the
/// state being non-zero rather than on the work block existing, and clears the
/// model object's flag word rather than leaving it 2.
void func_actor_403200_80141564(Task* arg0)
{
    GpEnemyTaskFuncTable5 sp;
    Actor403200GrabWork*  work;

    sp   = D_actor_403200_80131E9C;
    work = (Actor403200GrabWork*)arg0->work;

    switch (D_801153F4) {
        case 0:
            ((TmdObject*)arg0->extra)->flags = 0;
            break;
        case 1:
            ((TmdObject*)arg0->extra)->flags = 0;
            return;
        case 2:
            ((TmdObject*)arg0->extra)->flags = 0x80;
            return;
    }

    if (arg0->state != 0) {
        if (work->field_1B4 != arg0->state) {
            work->field_1A8 = 1;
        } else {
            work->field_1A8 = 0;
        }
        work->field_1B4 = arg0->state;
    }
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

/// Dispatcher of the `D_actor_403200_80131F04` enemy: park the model object
/// while the global game mode is 1 or 2, otherwise note in the work block
/// whether the state changed since the last step and run the handler for it.
/// The model object's flag word is left at 2 while the enemy runs.
void func_actor_403200_80141670(Task* arg0)
{
    GpEnemyTaskFuncTable4 sp;
    Actor403200GrabWork*  work;

    sp = D_actor_403200_80131F04;

    switch (D_801153F4) {
        case 0:
            ((TmdObject*)arg0->extra)->flags = 2;
            break;
        case 1:
            ((TmdObject*)arg0->extra)->flags = 2;
            return;
        case 2:
            ((TmdObject*)arg0->extra)->flags = 0x80;
            return;
    }

    if (arg0->work != NULL) {
        work = (Actor403200GrabWork*)arg0->work;
        if (work->field_1B4 != arg0->state) {
            work->field_1A8 = 1;
        } else {
            work->field_1A8 = 0;
        }
        work->field_1B4 = arg0->state;
    }
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

/// Dispatcher of the dropped enemy (`D_actor_403200_80131F14`): run the handler
/// for the task's state, skipped while the global game mode is 1 or 2.
void func_actor_403200_80141778(Task* arg0)
{
    GpEnemyTaskFuncTable5 sp;

    sp = D_actor_403200_80131F14;
    switch (D_801153F4) {
        case 0:
        default:
            sp.funcs[arg0->state](arg0->spawnArg2, arg0);
            break;
        case 1:
        case 2:
            break;
    }
}

/// Waiting state of the spinner enemy (`D_actor_403200_80131F28`): hand the
/// enemy back to `Gp_DestroyEnemy` once `D_actor_403200_80141C50` is set;
/// otherwise keep the model's flag word cleared, so it is not drawn, and step
/// the task on once `D_actor_403200_80141C5A` is 1.
void func_actor_403200_80141800(GpEnemy* arg0, Task* arg1)
{
    if (D_actor_403200_80141C50 == 1) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }

    if (D_actor_403200_80141C5A == 1) {
        arg1->state++;
    }

    ((TmdObject*)arg1->extra)->flags = 0;
}

/// Dispatcher of the spinner enemy (`D_actor_403200_80131F28`): park the model
/// object while the global game mode is 1 or 2, otherwise note in the work
/// block whether the state changed since the last step and run the handler for
/// it.
void func_actor_403200_80141868(Task* arg0)
{
    GpEnemyTaskFuncTable4   sp;
    Actor403200SpinnerWork* work;

    sp = D_actor_403200_80131F28;

    switch (D_801153F4) {
        case 0:
            ((TmdObject*)arg0->extra)->flags = 0;
            break;
        case 1:
            ((TmdObject*)arg0->extra)->flags = 0;
            return;
        case 2:
            ((TmdObject*)arg0->extra)->flags = 0x80;
            return;
    }

    if (arg0->work != NULL) {
        work = (Actor403200SpinnerWork*)arg0->work;
        if (work->field_94 != arg0->state) {
            work->field_90 = 1;
        } else {
            work->field_90 = 0;
        }
        work->field_94 = arg0->state;
    }
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

void func_actor_403200_8014196C(void)
{
}
