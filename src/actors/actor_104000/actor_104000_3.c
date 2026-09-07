#include "common.h"
#include "main/task.h"

extern TaskFunc D_actor_104000_8013E50C[];

INCLUDE_ASM("actors/nonmatchings/actor_104000/actor_104000_3", func_actor_104000_80138698);

INCLUDE_ASM("actors/nonmatchings/actor_104000/actor_104000_3", func_actor_104000_801387B4);

INCLUDE_ASM("actors/nonmatchings/actor_104000/actor_104000_3", func_actor_104000_8013887C);

INCLUDE_ASM("actors/nonmatchings/actor_104000/actor_104000_3", func_actor_104000_801388E4);

INCLUDE_ASM("actors/nonmatchings/actor_104000/actor_104000_3", func_actor_104000_801389E8);

INCLUDE_ASM("actors/nonmatchings/actor_104000/actor_104000_3", func_actor_104000_80138AA0);

INCLUDE_ASM("actors/nonmatchings/actor_104000/actor_104000_3", func_actor_104000_80138B58);

INCLUDE_ASM("actors/nonmatchings/actor_104000/actor_104000_3", func_actor_104000_80138C6C);

INCLUDE_ASM("actors/nonmatchings/actor_104000/actor_104000_3", func_actor_104000_80138CC8);

INCLUDE_ASM("actors/nonmatchings/actor_104000/actor_104000_3", func_actor_104000_80138D74);

/// Per-frame entry point: runs the actor task's current state from the
/// dispatch table in the overlay's trailing data.
void func_actor_104000_80138E5C(Task* arg0)
{
    D_actor_104000_8013E50C[arg0->state](arg0);
}
