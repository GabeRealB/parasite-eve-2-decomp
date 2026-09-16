#include "common.h"

#include "actors/actor_560800.h"

#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_4", func_actor_560800_80136AA8);

/// Sets up the animated model part the spawn argument names: allocates its
/// `Actor560800ModelWork`, hangs it off `Task::idMap`, points the object's light
/// and colour matrices into it, makes the named task this one's parent and hands
/// the part to `func_800B3F84` with the overlay's animation bank. The three
/// `Gp_LcgState` draws taken along the way seed the handlers' random headings,
/// and the slot count comes from the spawner's `spawnArg1`.
void func_actor_560800_801376E0(Task* arg0)
{
    Actor560800ModelWork* mem;
    Actor560800ModelWork* work;
    TmdObject*            obj;
    GsCOORDINATE2*        coord;
    Task*                 child;

    obj         = (TmdObject*)arg0->extra;
    coord       = obj->field_8;
    mem         = (Actor560800ModelWork*)Mem_Malloc(0x28C, 0);
    arg0->idMap = (TaskIdMap*)mem;
    if (mem == NULL) {
        Task_Kill(arg0);
        return;
    }
    Mem_Set(mem, 0, 0x28C);
    work            = (Actor560800ModelWork*)arg0->idMap;
    child           = (Task*)arg0->spawnArg2;
    work->field_26C = child;
    coord->sub      = ((TmdObject*)child->extra)->field_8;
    obj->field_1C   = &work->light;
    obj->field_20   = &work->color;
    Task_Reparent(work->field_26C, arg0);
    Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
    work->field_270 = Gp_LcgState >> 16;
    Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
    work->field_274 = Gp_LcgState >> 16;
    Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
    work->field_278 = (Gp_LcgState >> 16) & 0x3FF;
    func_800B3F84(&work->anim, D_actor_560800_801752F0, (GpAnimObj*)obj, work->poseBuf,
                  work->slots);
    work->field_280 = arg0->spawnArg1;
}

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_4", func_actor_560800_80137820);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_4", func_actor_560800_80137BEC);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_4", func_actor_560800_80137F58);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_4", func_actor_560800_801384EC);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_4", func_actor_560800_801386D4);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_4", func_actor_560800_80138A4C);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_4", func_actor_560800_80138BCC);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_4", func_actor_560800_80138D04);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_4", func_actor_560800_80138FC8);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_4", func_actor_560800_80139360);

void func_actor_560800_801393EC(Task* task, s32 arg1, s32 arg2)
{
    TmdObject* extra;

    extra = (TmdObject*)task->extra;
    switch (arg2) {
        case 0:
            break;
        case 1:
            extra->field_C = extra->field_C & 0xFF7B;
            return;
        case 2:
            extra->field_C = extra->field_C | 0x84;
            return;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_4", func_actor_560800_80139440);
