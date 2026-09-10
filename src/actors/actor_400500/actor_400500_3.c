#include "common.h"
#include "main/gfx.h"
#include "main/task.h"
#include "main/tmd.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "actors/actor_400500.h"

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500_3", func_actor_400500_8013DEFC);

void func_actor_400500_8013DF50(Task* arg0)
{
    Actor400500Work* work = (Actor400500Work*)arg0->idMap;

    work->field_A3C = 0;
    work->field_A3E = 0;
}

void func_actor_400500_8013DF64(void)
{
}

void func_actor_400500_8013DF6C(void)
{
}

void func_actor_400500_8013DF74(Task* arg0)
{
    Actor400500Work* work;
    Task*            child;
    s32              i;

    work = (Actor400500Work*)arg0->idMap;
    for (i = 0; i < 2; i++) {
        child = work->field_9F0[i];
        if (child != NULL) {
            Task_Kill(child);
        }
    }
    work->field_A04 = 0;
    work->field_A06 = work->field_A06 + 1;
}

void func_actor_400500_8013DFE4(Task* arg0)
{
    Actor400500Work* work;
    u16              frame;

    work            = (Actor400500Work*)arg0->idMap;
    frame           = work->field_A04 + 1;
    work->field_A04 = frame;
    if ((s16)frame >= 0x12D) {
        Gp_DestroyEnemy(arg0->spawnArg2, arg0);
    }
}
