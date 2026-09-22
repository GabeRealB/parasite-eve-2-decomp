#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

void Actor02300_Fn028AC(Task* arg0);
void Actor02300_Fn02EA0(Task* arg0);
void Actor02300_Fn01A20(Task* arg0);

/// The enemy's three state handlers - spawn/setup, per-frame tick
/// and teardown - dispatched through by state.
const GpEnemyTaskFuncTable3 Actor02300_D00078 = {
    Actor02300_Fn028AC,
    Actor02300_Fn02EA0,
    Actor02300_Fn01A20,
};

INCLUDE_ASM("actors/nonmatchings/actor_02300/actor_102300_5", Actor02300_Fn03D88);

void Actor02300_Fn03EE8(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor02300_D00078;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}
