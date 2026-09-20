#include "common.h"

#include "actors/actor_511000.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"

#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);

/// The actor's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern TaskFuncTable3 D_actor_511000_80131E30;
extern TaskFuncTable3 D_actor_511000_80131E3C;

/// Spawn table `func_actor_511000_80132480` starts its two child tasks from,
/// and the per-frame handler it parks in `Task::msgTable`.
extern TaskDesc D_actor_511000_801472E8;
extern u8       D_actor_511000_8014730C[];

void func_actor_511000_80132428(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_511000_80131E3C;
    sp.funcs[task->state](task);
}

/// Spawn handler: allocates the work block, seeds its head, mirrors the
/// deferred-kill bit into the model, draws the ground shadow under the model's
/// second part, starts the actor's two child tasks and hands the model's
/// matrices to the light/color rebuilder, then advances to the tick handler.
void func_actor_511000_80132480(Task* task)
{
    Actor511000Work2* work;
    TmdObject*        extra;
    VECTOR3           pos;
    u16               flags;

    extra = (TmdObject*)task->extra;
    work  = (Actor511000Work2*)memCalloc(0x4D4, 0);
    if (work == NULL) {
        Gp_EnemyTaskExit(task);
        return;
    }
    task->work           = (TaskIdMap*)work;
    work->field_478      = -1;
    work->field_47C      = -1;
    work->field_4D2      = 0;
    work->field_480.word = -1;
    flags                = extra->flags | 0x80;
    extra->flags         = flags;
    if (!(flags & 0x80)) {
        if (func_800EA1A8((VECTOR3*)((TmdObject*)task->extra)->coords[1].workm.t, &pos) != 0) {
            Gp_DrawEffGroundQuad(&pos, 0x200, Gp_State1C->groundShade);
        }
    }
    work->field_4C4 = Task_SpawnFromTable(&D_actor_511000_801472E8, 1, 8, (s32)task);
    work->field_4C8 = Task_SpawnFromTable(&D_actor_511000_801472E8, 2, 0xC, (s32)task);
    func_actor_511000_801325A4(task);
    task->msgTable     = D_actor_511000_8014730C;
    task->exitCallback = Gp_EnemyTaskExit;
    task->state++;
}

/// Republishes the work block's light/color matrices onto the TMD object and
/// rebuilds model part 1's world matrix from it, then hands that part's
/// translation to the ground-shadow helper.
void func_actor_511000_801325A4(Task* task)
{
    Actor511000Work2* work;
    GsCOORDINATE2*    coords;
    TmdObject*        extra;

    work            = (Actor511000Work2*)task->work;
    extra           = (TmdObject*)task->extra;
    coords          = extra->coords;
    extra->lightMtx = &work->light;
    extra->colorMtx = &work->color;
    coords[1].flg   = 0;
    Gp_UpdateCoord(&coords[1]);
    func_800D7A9C(extra, (VECTOR*)coords[1].workm.t, 0, 3);
}
