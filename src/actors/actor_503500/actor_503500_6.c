#include "common.h"

#include "actors/actor_503500.h"

#include "main/task.h"
#include "main/tmd.h"

void func_actor_503500_80146508(Task* arg0)
{
    TmdObject*            ext;
    Actor503500Effect4CC* work;

    work          = (Actor503500Effect4CC*)arg0->idMap;
    ext           = arg0->extra;
    ext->field_1C = &work->light;
    ext->field_20 = &work->color;
}

void func_actor_503500_80146524(void)
{
}

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_8014652C);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_80146664);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_801466E0);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_801467C0);
