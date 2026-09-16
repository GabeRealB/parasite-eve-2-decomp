#include "common.h"

#include "actors/actor_213000.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/gameplay.h"

#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

/// The actor's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern TaskFuncTable3 D_actor_213000_80149E30;
extern TaskFuncTable3 D_actor_213000_80149E3C;
extern TaskFuncTable3 D_actor_213000_80149E48;

void func_actor_213000_8014A158(void)
{
}

void func_actor_213000_8014A160(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_213000_80149E30;
    sp.funcs[task->state](task);
}

INCLUDE_ASM("actors/nonmatchings/actor_213000/actor_213000_2", func_actor_213000_8014A1B8);

INCLUDE_ASM("actors/nonmatchings/actor_213000/actor_213000_2", func_actor_213000_8014A2C4);

INCLUDE_ASM("actors/nonmatchings/actor_213000/actor_213000_2", func_actor_213000_8014A35C);

INCLUDE_ASM("actors/nonmatchings/actor_213000/actor_213000_2", func_actor_213000_8014A488);

void func_actor_213000_8014A520(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_213000_80149E3C;
    sp.funcs[task->state](task);
}

void func_actor_213000_8014A578(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_213000_80149E48;
    sp.funcs[task->state](task);
}

/// Per-frame tick: advances the animation context's slots while the work block
/// says it is live, and once the session has finished loading a stage rebuilds
/// model part 1's world matrix and hands its translation to the ground-shadow
/// helper. The work block's countdown then frees the model buffers as it
/// reaches zero.
void func_actor_213000_8014A5D0(Task* task)
{
    Actor213000Work* work;
    TmdObject*       extra;
    GsCOORDINATE2*   coords;
    s32              i;

    extra  = (TmdObject*)task->extra;
    work   = (Actor213000Work*)task->idMap;
    coords = &extra->field_8[1];
    if (work->field_474 != 0) {
        for (i = 1; i < 0x14; i++) {
            Gp_AnimTickIndex((GpAnimCtx*)work, i);
        }
    }
    if (Game_Session->field_4D != 0) {
        coords->flg = 0;
        Gp_UpdateCoord(coords);
        func_800D7A9C(extra, (VECTOR*)coords->workm.t, 0, 3);
    }
    if (work->field_477 >= 0) {
        if (work->field_477 == 0) {
            Tmd_FreeBuffers(extra);
        }
        work->field_477--;
    }
}

/// Republishes the work block's light/color matrices onto the TMD object and
/// rebuilds model part 1's world matrix from it, then hands that part's
/// translation to the ground-shadow helper.
void func_actor_213000_8014A6AC(Task* task)
{
    Actor213000Work* work;
    GsCOORDINATE2*   coords;
    TmdObject*       extra;

    work            = (Actor213000Work*)task->idMap;
    extra           = (TmdObject*)task->extra;
    coords          = extra->field_8;
    extra->field_1C = &work->light;
    extra->field_20 = &work->color;
    coords[1].flg   = 0;
    Gp_UpdateCoord(&coords[1]);
    func_800D7A9C(extra, (VECTOR*)coords[1].workm.t, 0, 3);
}

INCLUDE_ASM("actors/nonmatchings/actor_213000/actor_213000_2", func_actor_213000_8014A70C);
