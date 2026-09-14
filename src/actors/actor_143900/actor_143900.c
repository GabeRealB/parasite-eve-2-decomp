#include "common.h"

#include "main/mc.h"
#include "main/task.h"

/// Spawn table this overlay hands to `Task_SpawnFromTable`. It sits at an
/// absolute address outside the actor slot - offset 0x440 into the loaded room
/// overlay, whose base is 0x8017D5C0 - so splat cannot name it and it keeps its
/// raw `D_` form, as `D_80147E48` does in `actor_206100`. `shelter_r49` spawns
/// from a table at this same address (`D_shelter_r49_8017DA00`).
extern TaskDesc D_8017DA00;

/// Arms the `field_5C5` scene event and starts the table's task, unless
/// `field_23` is 9 - the `Task_Spawn` bank the attract-demo prompts
/// (`Gp_StrDemoWait` / `Gp_StrDemoPause`) key off, so this story trigger is
/// skipped while the demo plays. Same shape as `func_actor_450800_80132080`.
void func_actor_143900_80131E24(void)
{
    if (Mc_SaveData.field_23 != 9) {
        Mc_SaveData.field_5C5 = 0x14;
        Task_SpawnFromTable(&D_8017DA00, 0, 0, 0);
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_143900/actor_143900", ActorsShared80131f9cSub0);

INCLUDE_ASM("actors/nonmatchings/actor_143900/actor_143900", func_actor_143900_80131FD4);

INCLUDE_RODATA("actors/nonmatchings/actor_143900/actor_143900", D_actor_143900_80131E20);
