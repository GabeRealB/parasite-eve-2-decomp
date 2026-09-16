#include "common.h"

#include "actors/actor_303600.h"
#include "main/tmd.h"

INCLUDE_ASM("actors/nonmatchings/actor_303600/actor_303600_3", func_actor_303600_80162870);

INCLUDE_RODATA("actors/nonmatchings/actor_303600/actor_303600_3", D_actor_303600_80161E48);

INCLUDE_ASM("actors/nonmatchings/actor_303600/actor_303600_3", func_actor_303600_801628E4);

INCLUDE_ASM("actors/nonmatchings/actor_303600/actor_303600_3", func_actor_303600_80162950);

void func_actor_303600_80162A04(void)
{
}

/// Points the task's model at the light / colour matrix pair in its own work
/// block and loads the overlay's three flat lights into them.
void func_actor_303600_80162A0C(Task* task)
{
    Actor303600LightMats* mats = (Actor303600LightMats*)task->idMap;
    TmdObject*            obj  = task->extra;
    GsF_LIGHT*            light;
    s32                   i;

    obj->field_1C = &mats->lightMtx;
    obj->field_20 = &mats->colorMtx;
    for (i = 0, light = D_actor_303600_8016E490; i < 3; i++, light++) {
        Gfx_SetFlatLight(i, light, &mats->lightMtx, &mats->colorMtx);
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_303600/actor_303600_3", func_actor_303600_80162A7C);
