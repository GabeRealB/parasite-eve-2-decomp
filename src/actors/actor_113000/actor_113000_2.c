#include "common.h"

#include "actors/actor_113000.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"

#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);

/// Message dispatch table the spawn handler parks in `Task::field_24`:
/// message id / handler pairs, terminated by 0x7FFFFFFF and a null word.
extern u8 D_actor_113000_8013ABC0[];

/// Spawn handler: allocates the work block, seeds its head, mirrors the
/// deferred-kill bit into the model, draws the ground shadow under the model's
/// second part, then hands the model's matrices to the light/color rebuilder.
void func_actor_113000_80131F90(Task* task)
{
    Actor113000Work* work;
    TmdObject*       extra;
    VECTOR3          pos;
    u16              flags;

    extra = (TmdObject*)task->extra;
    work  = (Actor113000Work*)Mem_Calloc(0x4CC, 0);
    if (work == NULL) {
        Gp_EnemyTaskExit(task);
        return;
    }
    task->idMap     = (TaskIdMap*)work;
    work->field_478 = -1;
    work->field_47C = -1;
    work->field_4C6 = 0;
    work->field_4C8 = -1;
    flags           = extra->field_C | 0x80;
    extra->field_C  = flags;
    if (!(flags & 0x80)) {
        if (func_800EA1A8((VECTOR3*)((TmdObject*)task->extra)->field_8[1].workm.t, &pos) != 0) {
            Gp_DrawEffGroundQuad(&pos, 0x200, Gp_State1C->field_8);
        }
    }
    func_actor_113000_801321A8(task);
    task->field_24     = D_actor_113000_8013ABC0;
    task->exitCallback = Gp_EnemyTaskExit;
    task->state++;
}

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
