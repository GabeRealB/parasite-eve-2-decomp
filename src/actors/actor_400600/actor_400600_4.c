#include "common.h"

#include "main/task.h"

#include "actors/actor_400600.h"

/* Defined in a later unit of this overlay, or still `INCLUDE_ASM`. */
void func_actor_400600_8013AAD8(Task* arg0);
void func_actor_400600_8013AB44(Task* arg0);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600_4", func_actor_400600_8013A0F0);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600_4", func_actor_400600_8013A170);

void func_actor_400600_8013A26C(Task* arg0)
{
    Actor400600Work* work                = (Actor400600Work*)arg0->idMap;
    void             (*states[2])(Task*) = {
        func_actor_400600_8013AAD8,
        func_actor_400600_8013AB44,
    };

    states[(s16)work->field_71C](arg0);
}
