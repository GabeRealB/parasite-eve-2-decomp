#include "common.h"

#include "actors/actor_135600.h"

#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

/// The actor's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern TaskFuncTable3 D_actor_135600_80131E30;
extern TaskFuncTable3 D_actor_135600_80131E3C;

void func_actor_135600_80132AB4(void)
{
}

void func_actor_135600_80132ABC(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_135600_80131E30;
    sp.funcs[task->state](task);
}

/// Chains the actor's model root under the parent task's part `spawnArg1`:
/// places the part's coordinate at (-150, 80, 0), turns its rotation by 90
/// degrees about Y, inherits the parent's light and colour matrices, and
/// reparents the task so it is updated with the parent.
void func_actor_135600_80132B14(Task* task)
{
    Actor135600Matrix m;
    MATRIX*           mtx;
    Task*             parent;
    s32               part;
    TmdObject*        extra;
    TmdObject*        parentExtra;
    GsCOORDINATE2*    coord;
    GsCOORDINATE2*    dest;

    parent      = (Task*)task->spawnArg2;
    extra       = (TmdObject*)task->extra;
    part        = task->spawnArg1;
    parentExtra = (TmdObject*)parent->extra;
    coord       = extra->field_8;
    dest        = &parentExtra->field_8[part];

    coord->coord.t[0] = -0x96;
    coord->coord.t[1] = 0x50;
    coord->coord.t[2] = 0;

    mtx                  = &m.mat;
    m.ident.m00_m01      = 0x1000;
    *(s32*)&mtx->m[0][2] = 0;
    *(s32*)&mtx->m[1][1] = 0x1000;
    *(s32*)&mtx->m[2][0] = 0;
    mtx->m[2][2]         = 0x1000;

    func_8004BFF8(0x400, mtx);
    MulMatrix0(&coord->coord, mtx, &coord->coord);

    coord->sub      = dest;
    coord->flg      = 0;
    extra->field_1C = parentExtra->field_1C;
    extra->field_20 = parentExtra->field_20;
    extra->field_E  = 0;
    Task_Reparent(parent, task);
    task->killCountdown = 0x1000;
    task->state        += 1;
}

void func_actor_135600_80132C18(Task* task)
{
    s16 countdown;

    if (Game_Session->field_1 != 0) {
        countdown = task->killCountdown;
        if (countdown > 0 && func_actor_135600_80131E68(((TmdObject*)task->extra)->field_8, countdown) >= 0x1F5) {
            task->killCountdown = 0x800;
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_135600/actor_135600_2", func_actor_135600_80132C80);

void func_actor_135600_80132D64(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_135600_80131E3C;
    sp.funcs[task->state](task);
}

INCLUDE_RODATA("actors/nonmatchings/actor_135600/actor_135600_2", ActorsShared801327f8Table);

INCLUDE_RODATA("actors/nonmatchings/actor_135600/actor_135600_2", ActorsShared80132920Offset);
