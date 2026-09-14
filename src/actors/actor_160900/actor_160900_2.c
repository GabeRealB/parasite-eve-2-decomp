#include "common.h"

#include "main/fs.h"
#include "main/task.h"

#include <psyq/libgpu.h>

#include "actors/actor_160900.h"

void func_actor_160900_80134710(void)
{
    Actor160900Work* work;
    Task*            task;
    s16              i;

    work = (Actor160900Work*)D_actor_160900_8013FBB4->idMap;
    for (i = 0; i < 10; i++) {
        task = work->field_C[i];
        if (task != NULL) {
            Task_Kill(task);
            work->field_C[i] = NULL;
        }
    }
}
void func_actor_160900_80134790(s16 arg0)
{
    Actor160900Work* work;

    work           = (Actor160900Work*)D_actor_160900_8013FBB4->idMap;
    work->field_4C = arg0;
    work->field_4E = 0;
}
void func_actor_160900_801347B0(s16 arg0)
{
    Actor160900Work* work;

    work           = (Actor160900Work*)D_actor_160900_8013FBB4->idMap;
    work->field_54 = arg0;
    work->field_56 = 0;
}

void func_actor_160900_801347D0(s16 arg0)
{
    Actor160900Work* work;

    work           = (Actor160900Work*)D_actor_160900_8013FBB4->idMap;
    work->field_5C = arg0;
    work->field_5E = 0;
}
void func_actor_160900_801347F0(void)
{
    Actor160900Work* work;

    work           = (Actor160900Work*)D_actor_160900_8013FBB4->idMap;
    work->field_4C = 0;
    work->field_54 = 0;
    work->field_5C = 0;
    CdCmd_CancelReplaceAndActivate();
    SetDispMask(1);
}
