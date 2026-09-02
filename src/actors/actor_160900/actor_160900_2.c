#include "common.h"

#include "main/fs.h"
#include "main/task.h"

#include <psyq/libgpu.h>

#include "actors/actor_160900.h"

INCLUDE_ASM("actors/nonmatchings/actor_160900/actor_160900_2", func_actor_160900_80134710);

INCLUDE_ASM("actors/nonmatchings/actor_160900/actor_160900_2", func_actor_160900_80134790);

INCLUDE_ASM("actors/nonmatchings/actor_160900/actor_160900_2", func_actor_160900_801347B0);

INCLUDE_ASM("actors/nonmatchings/actor_160900/actor_160900_2", func_actor_160900_801347D0);

void func_actor_160900_801347F0(void)
{
    Actor160900Work* work;

    work           = (Actor160900Work*)D_actor_160900_8013FBB4->idMap;
    work->field_4C = 0;
    work->field_54 = 0;
    work->field_5C = 0;
    CdCmd_CancelReplaceAndActivate();
    SetDispMask(1);
}
