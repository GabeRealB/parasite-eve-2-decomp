#include "common.h"

#include "actors/actor_135400.h"

#include "main/gfx.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"

/// The three flat lights `func_actor_135400_80132CB0` loads into the model's
/// light / colour matrices: an axis-aligned light on X, Y and Z (`vy` / `vx` /
/// `vz`), each the same mid grey.
extern GsF_LIGHT D_actor_135400_8013F904[3];

INCLUDE_ASM("actors/nonmatchings/actor_135400/actor_135400_2", func_actor_135400_80132650);

INCLUDE_ASM("actors/nonmatchings/actor_135400/actor_135400_2", func_actor_135400_8013276C);

INCLUDE_ASM("actors/nonmatchings/actor_135400/actor_135400_2", func_actor_135400_801327E8);

INCLUDE_ASM("actors/nonmatchings/actor_135400/actor_135400_2", func_actor_135400_801328DC);

INCLUDE_ASM("actors/nonmatchings/actor_135400/actor_135400_2", func_actor_135400_801329B0);

INCLUDE_ASM("actors/nonmatchings/actor_135400/actor_135400_2", func_actor_135400_80132AF4);

INCLUDE_ASM("actors/nonmatchings/actor_135400/actor_135400_2", func_actor_135400_80132B60);

void func_actor_135400_80132C90(Task* arg0)
{
    Gp_EnemyTaskExit(arg0);
}

void func_actor_135400_80132CB0(Task* task)
{
    Actor135400Work* work = (Actor135400Work*)task->idMap;
    TmdObject*       obj  = (TmdObject*)task->extra;
    GsF_LIGHT*       light;
    s32              i;

    obj->field_1C = &work->lightMtx;
    obj->field_20 = &work->colorMtx;
    for (i = 0, light = D_actor_135400_8013F904; i < 3; i++, light++) {
        Gfx_SetFlatLight(i, light, &work->lightMtx, &work->colorMtx);
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_135400/actor_135400_2", func_actor_135400_80132D24);

INCLUDE_ASM("actors/nonmatchings/actor_135400/actor_135400_2", func_actor_135400_80132E40);

INCLUDE_ASM("actors/nonmatchings/actor_135400/actor_135400_2", func_actor_135400_80132EBC);
