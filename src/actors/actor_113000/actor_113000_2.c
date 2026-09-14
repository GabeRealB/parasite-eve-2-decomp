#include "common.h"

#include "actors/actor_113000.h"

#include "gameplay/3A34.h"

#include "main/task.h"
#include "main/tmd.h"

INCLUDE_ASM("actors/nonmatchings/actor_113000/actor_113000_2", func_actor_113000_80131F90);

INCLUDE_ASM("actors/nonmatchings/actor_113000/actor_113000_2", func_actor_113000_80132070);

/// Republishes the work block's light/color matrices onto the TMD object and
/// rebuilds model part 1's world matrix from it, then hands that part's
/// translation to the ground-shadow helper.
void func_actor_113000_801321A8(Task* task)
{
    Actor113000Work* work;
    GsCOORDINATE2*   coords;
    TmdObject*       extra;

    work            = (Actor113000Work*)task->idMap;
    extra           = (TmdObject*)task->extra;
    coords          = extra->field_8;
    extra->field_1C = &work->light;
    extra->field_20 = &work->color;
    coords[1].flg   = 0;
    Gp_UpdateCoord(&coords[1]);
    func_800D7A9C(extra, (VECTOR*)coords[1].workm.t, 0, 3);
}

INCLUDE_ASM("actors/nonmatchings/actor_113000/actor_113000_2", func_actor_113000_80132208);
