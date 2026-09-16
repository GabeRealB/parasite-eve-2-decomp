#include "common.h"

#include "actors/actor_113100.h"

#include "main/task.h"

#include "gameplay/1BC.h"

/// The actor's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern TaskFuncTable3 D_actor_113100_80131E24;
extern TaskFuncTable3 D_actor_113100_80131E30;
extern TaskFuncTable3 D_actor_113100_80131E3C;

/// Declared here rather than taken from `gameplay.h`: the overlays call this
/// with the part index and the owning task as extra arguments that the body
/// never reads, so the shared one-argument prototype does not describe this
/// call site.
extern MATRIX* Gp_GetStageView(u8*, s32, void*);

#include "common.h"

#include "actors/actor_113100.h"

#include "main/task.h"

#include "gameplay/1BC.h"

/// The actor's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern TaskFuncTable3 D_actor_113100_80131E24;
extern TaskFuncTable3 D_actor_113100_80131E30;
extern TaskFuncTable3 D_actor_113100_80131E3C;

/// Declared here rather than taken from `gameplay.h`: the overlays call this
/// with the part index and the owning task as extra arguments that the body
/// never reads, so the shared one-argument prototype does not describe this
/// call site.
extern MATRIX* Gp_GetStageView(u8*, s32, void*);

INCLUDE_ASM("actors/nonmatchings/actor_113100/actor_113100", func_actor_113100_80131E58);

INCLUDE_ASM("actors/nonmatchings/actor_113100/actor_113100", func_actor_113100_80132104);

INCLUDE_ASM("actors/nonmatchings/actor_113100/actor_113100", func_actor_113100_801324DC);

INCLUDE_ASM("actors/nonmatchings/actor_113100/actor_113100", func_actor_113100_8013264C);

INCLUDE_ASM("actors/nonmatchings/actor_113100/actor_113100", func_actor_113100_80132790);

INCLUDE_ASM("actors/nonmatchings/actor_113100/actor_113100", func_actor_113100_801328EC);

INCLUDE_RODATA("actors/nonmatchings/actor_113100/actor_113100", D_actor_113100_80131E20);

INCLUDE_RODATA("actors/nonmatchings/actor_113100/actor_113100", D_actor_113100_80131E24);

INCLUDE_RODATA("actors/nonmatchings/actor_113100/actor_113100", D_actor_113100_80131E30);

INCLUDE_RODATA("actors/nonmatchings/actor_113100/actor_113100", D_actor_113100_80131E3C);

void func_actor_113100_80132AD8(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_113100_80131E24;
    sp.funcs[task->state](task);
}

INCLUDE_ASM("actors/nonmatchings/actor_113100/actor_113100", func_actor_113100_80132B30);

/// Builds the display matrix of the modelled part this actor is posed on.
/// `spawnArg1` indexes the part in the model task's coordinate array: the part's
/// `workm` is transposed into the actor coordinate, the stage view is multiplied
/// in, and the part's X euler angle is applied, after which the coordinate's
/// update flag is cleared so the GTE sees the new matrix.
void func_actor_113100_80132BDC(Task* task)
{
    MATRIX         sp10;
    SVECTOR        sp30;
    MATRIX*        view;
    MATRIX*        coord;
    GsCOORDINATE2* part;
    GsCOORDINATE2* node;
    s32            index;

    index = task->spawnArg1;
    node  = (GsCOORDINATE2*)((TmdObject*)task->extra)->field_8;
    part  = &((TmdObject*)((Task*)task->spawnArg2)->extra)->field_8[index];
    view  = Gp_GetStageView(&Game_Session->field_4, index, task);
    coord = &node->coord;
    TransposeMatrix(&part->workm, coord);
    TransposeMatrix(view, &sp10);
    MulMatrix0(coord, &sp10, coord);
    Gp_ExtractEuler(&sp30, &part->coord);
    RotMatrixX(sp30.vy, coord);
    node->flg = 0;
}

void func_actor_113100_80132C9C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_113100_80131E30;
    sp.funcs[task->state](task);
}

INCLUDE_ASM("actors/nonmatchings/actor_113100/actor_113100", func_actor_113100_80132CF4);
