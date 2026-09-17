#include "common.h"

#include "actors/actor_135400.h"

#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"

/// The actor's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern TaskFuncTable3 D_actor_135400_80131E24;
extern TaskFuncTable3 D_actor_135400_80131E30;

INCLUDE_ASM("actors/nonmatchings/actor_135400/actor_135400", func_actor_135400_80131EB4);

INCLUDE_RODATA("actors/nonmatchings/actor_135400/actor_135400", D_actor_135400_80131E20);

INCLUDE_RODATA("actors/nonmatchings/actor_135400/actor_135400", D_actor_135400_80131E24);

INCLUDE_RODATA("actors/nonmatchings/actor_135400/actor_135400", D_actor_135400_80131E30);

INCLUDE_RODATA("actors/nonmatchings/actor_135400/actor_135400", D_actor_135400_80131E3C);

INCLUDE_ASM("actors/nonmatchings/actor_135400/actor_135400", func_actor_135400_80132064);

/// Per-frame tick of the actor's main task: ticks the twenty animation slots
/// once `field_474` has latched, and while the model is not flagged killed
/// (bit 0x80 of `TmdObject::field_C`) draws its ground shadow from the second
/// part's translation, recomputes that part's world matrix, re-ranks it
/// through `func_800D7A9C`, ramps the head-tracking rate `headRate` and finally
/// turns the head toward the slot-3 skeleton with `func_800B0928`.
void func_actor_135400_801322A8(Task* task)
{
    Actor135400MainWork* work;
    TmdObject*           ext;
    VECTOR3              pos;
    s32                  i;
    s32                  rate;

    work = (Actor135400MainWork*)task->idMap;
    ext  = task->extra;
    if (work->field_474 != 0) {
        for (i = 1; i < 0x14; i++) {
            Gp_AnimTickIndex(&work->anim, i);
        }
    }
    if (!(ext->field_C & 0x80)) {
        if (func_800EA1A8((VECTOR3*)((TmdObject*)task->extra)->field_8[1].workm.t, &pos) != 0) {
            Gp_DrawEffGroundQuad(&pos, 0x300, Gp_State1C->field_8);
        }
        Gp_UpdateCoord(&((TmdObject*)task->extra)->field_8[1]);
        func_800D7A9C(ext, (VECTOR*)((TmdObject*)task->extra)->field_8[1].workm.t, 0, 3);
        if (work->headAim != 0) {
            rate           = work->headRate + 0x100;
            work->headRate = rate;
            if (rate >= 0x1000) {
                work->headRate = 0xFFF;
            }
        } else {
            rate           = work->headRate - 0x80;
            work->headRate = rate;
            if (rate < 0) {
                work->headRate = 0;
            }
        }
        func_800B0928(task, Game_GetPtrSlot(3), 0x200, 0x100, work->headRate);
    }
}

void func_actor_135400_801323F8(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_135400_80131E24;
    sp.funcs[task->state](task);
}

void func_actor_135400_80132450(Task* task)
{
    Task*          parent;
    s32            part;
    TmdObject*     extra;
    TmdObject*     parentExtra;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* dest;

    parent          = (Task*)task->spawnArg2;
    part            = task->spawnArg1;
    extra           = (TmdObject*)task->extra;
    parentExtra     = (TmdObject*)parent->extra;
    coord           = extra->field_8;
    dest            = &parentExtra->field_8[part];
    coord->flg      = 0;
    coord->sub      = dest;
    extra->field_1C = parentExtra->field_1C;
    extra->field_20 = parentExtra->field_20;
    Task_Reparent(parent, task);
    task->state += 1;
}

void func_actor_135400_801324CC(void)
{
}

void func_actor_135400_801324D4(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_135400_80131E30;
    sp.funcs[task->state](task);
}

INCLUDE_ASM("actors/nonmatchings/actor_135400/actor_135400", func_actor_135400_8013252C);

INCLUDE_ASM("actors/nonmatchings/actor_135400/actor_135400", func_actor_135400_801325A8);

void func_actor_135400_80132614(Task* arg0)
{
    Gp_EnemyTaskExit(arg0);
}
