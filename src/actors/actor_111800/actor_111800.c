#include "common.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"

#include "actors/actor_111800.h"

INCLUDE_RODATA("actors/nonmatchings/actor_111800/actor_111800", D_actor_111800_80131E20);

INCLUDE_ASM("actors/nonmatchings/actor_111800/actor_111800", func_actor_111800_8013214C);

/// Spawn/setup handler for the actor's model: allocates the 0x498-byte work
/// block into `Task::work`, hands the model object the view coordinate and the
/// block's two matrices, builds the animation context over the nineteen slots
/// and applies the nested area record matching id 0x13 through `Gp_SetTmdBytes`.
///
/// The allocation is parked in `Task::work` and read back before it is used, so
/// the first thing the block is named by is a reload: the `Mem_Calloc` result is
/// stored straight from `$v0` and the failing branch tests that register, which
/// is what leaves the surviving copy of it to be emitted *after* the branch --
/// one declaration earlier and the copy lands before the test.
void func_actor_111800_80132390(Task* task)
{
    Actor111800Work* work;
    Actor111800Work* work2;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    GpAreaPlace*     place;
    s32              i;

    coord      = ((TmdObject*)task->extra)->field_8;
    obj        = (TmdObject*)task->extra;
    task->work = (TaskIdMap*)Mem_Calloc(0x498, false);
    if (task->work == NULL) {
        Task_Kill(task);
        return;
    }
    work = (Actor111800Work*)task->work;
    Mem_Set(work, 0U, 0x498U);
    coord->sub = &Gfx_ViewCoord;
    Tmd_AllocBuffers(obj);
    obj->field_1C = &work->field_43C;
    obj->field_20 = &work->field_45C;
    obj->field_C  = 0;
    func_800B3F84(&work->anim, D_actor_111800_8013A448, (GpAnimObj*)obj, work->field_30C,
                  &work->slots[0]);
    work->field_47C  = Game_GetPtrSlot(3);
    work->field_480  = D_80073B8C[0];
    i                = 1;
    work2            = (Actor111800Work*)task->work;
    work2->field_492 = 0;
    do {
        work2->slots[i & 0xFFFF].field_9 = 0x10;
        Gp_AnimResetSlot(&work2->anim, i & 0xFFFF, 5);
        i += 1;
    } while ((u32)(i & 0xFFFF) < 0x13U);
    work->field_494 = 0x155;
    place           = (GpAreaPlace*)Gp_GetNestedAreaRec((GpAreaKey*)&gGameSession->loc)->field_0;
    while (place->field_0 != 0xFF && place->field_0 != 0x13) {
        place++;
    }
    Gp_SetTmdBytes(obj, (s8)place->field_D, (s8)place->field_E);
}

INCLUDE_ASM("actors/nonmatchings/actor_111800/actor_111800", func_actor_111800_8013251C);
