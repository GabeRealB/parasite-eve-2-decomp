#include "common.h"
#include "actors/actor_420700.h"
#include "main/task.h"

extern Task* D_actor_420700_8013EFE8;

void func_actor_420700_8013239C(Task* arg0)
{
    Task_Kill(D_actor_420700_8013EFE8);
    Gp_DestroyEnemy(arg0->spawnArg2, arg0);
}

INCLUDE_ASM("actors/nonmatchings/actor_420700/actor_420700_2", func_actor_420700_801323D8);

/// Runs the body the actor's step selects and then leaves it in step 3, the
/// running state. Steps 1 and 2 each return through their own copy of the
/// advance; the two are identical, so jump.c cross-jumps them and only the
/// second survives.
void func_actor_420700_80132478(void)
{
    if (ActorsShared80131f9cWork->field_4B4 == 1) {
        func_actor_420700_801325C8();
        ActorsShared80131f9cWork->field_4B4 = 3;
        return;
    }
    if (ActorsShared80131f9cWork->field_4B4 == 2) {
        ActorsShared80132538();
        ActorsShared80131f9cWork->field_4B4 = 3;
        return;
    }
    if (ActorsShared80131f9cWork->field_4B4 == 3) {
        func_actor_420700_801324EC();
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_420700/actor_420700_2", func_actor_420700_801324EC);
