#include "common.h"

#include "actors/actor_421600.h"
#include "gameplay/3CD8.h"
#include "main/task.h"

s32 func_actor_421600_8013E654(Task* task)
{
    Actor421600Work* work = (Actor421600Work*)task->work;

    work->field_EAC = 0x1E;
    return 1;
}

void func_actor_421600_8013E668(Task* task)
{
    Actor421600Work* work;
    GpEnemy*         enemy;

    work  = (Actor421600Work*)task->work;
    enemy = (GpEnemy*)task->spawnArg2;
    if (work != NULL) {
        if (work->field_E94 != NULL) {
            taskKill(work->field_E94);
        }
        if (work->field_E98 != NULL) {
            taskKill(work->field_E98);
        }
        Gp_UnlinkObj(&work->field_8EC);
        Gp_UnlinkObj(&work->field_A2C);
        Gp_UnlinkObj(&work->field_B6C);
        enemy->recs = 0;
    }
    Gp_DestroyEnemy(enemy, task);
}
