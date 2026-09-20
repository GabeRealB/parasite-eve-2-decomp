#include "common.h"

#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"

#include "actors/actor_104600.h"

/// The enemy's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_104600_80131E5C;

INCLUDE_ASM("actors/nonmatchings/actor_104600/actor_104600_7", func_actor_104600_8013528C);

INCLUDE_ASM("actors/nonmatchings/actor_104600/actor_104600_7", func_actor_104600_80135778);

void func_actor_104600_801359A0(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_104600_80131E5C;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

INCLUDE_ASM("actors/nonmatchings/actor_104600/actor_104600_7", func_actor_104600_801359FC);
