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

void func_actor_113100_80132E98(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_113100_80131E3C;
    sp.funcs[task->state](task);
}

void func_actor_113100_80132EF0(Task* arg0)
{
    Gp_UnlinkObj(&((Actor113100Work*)arg0->idMap)->obj);
    Gp_EnemyTaskExit(arg0);
}
