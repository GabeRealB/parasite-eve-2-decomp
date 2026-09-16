#include "common.h"

#include "actors/actor_511000.h"

#include "gameplay/3A34.h"
#include "gameplay/gameplay.h"

#include "main/task.h"
#include "main/tmd.h"

/// The actor's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern TaskFuncTable3 D_actor_511000_80131E30;
extern TaskFuncTable3 D_actor_511000_80131E3C;

void func_actor_511000_80132428(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_511000_80131E3C;
    sp.funcs[task->state](task);
}

INCLUDE_ASM("actors/nonmatchings/actor_511000/actor_511000_3", func_actor_511000_80132480);

/// Republishes the work block's light/color matrices onto the TMD object and
/// rebuilds model part 1's world matrix from it, then hands that part's
/// translation to the ground-shadow helper.
void func_actor_511000_801325A4(Task* task)
{
    Actor511000Work2* work;
    GsCOORDINATE2*    coords;
    TmdObject*        extra;

    work            = (Actor511000Work2*)task->idMap;
    extra           = (TmdObject*)task->extra;
    coords          = extra->field_8;
    extra->field_1C = &work->light;
    extra->field_20 = &work->color;
    coords[1].flg   = 0;
    Gp_UpdateCoord(&coords[1]);
    func_800D7A9C(extra, (VECTOR*)coords[1].workm.t, 0, 3);
}

INCLUDE_ASM("actors/nonmatchings/actor_511000/actor_511000_3", func_actor_511000_80132604);
