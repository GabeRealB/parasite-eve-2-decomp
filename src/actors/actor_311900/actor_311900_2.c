#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/D4.h"
#include "main/task.h"
#include "main/tmd.h"

void func_actor_311900_80161E3C(Task* task, s32 arg1, s32 arg2);

void func_actor_311900_80162100(Task* task);

INCLUDE_ASM("actors/nonmatchings/actor_311900/actor_311900_2", func_actor_311900_801624F8);

void func_actor_311900_801625F0(GpEnemy* enemy, Task* task)
{
    TmdObject* obj;

    obj = (TmdObject*)task->extra;
    func_actor_311900_80161E3C(task, 4, 2);
    if ((Gp_GetViewIndex() & 0xFF) == 0xB) {
        obj->field_C = 0;
    } else {
        obj->field_C = 0x80;
    }
    func_actor_311900_80162100(task);
}

INCLUDE_ASM("actors/nonmatchings/actor_311900/actor_311900_2", func_actor_311900_80162658);

INCLUDE_ASM("actors/nonmatchings/actor_311900/actor_311900_2", func_actor_311900_8016278C);

INCLUDE_ASM("actors/nonmatchings/actor_311900/actor_311900_2", func_actor_311900_8016281C);
