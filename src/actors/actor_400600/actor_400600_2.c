#include "common.h"

#include "main/task.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"

#include "actors/actor_400600.h"

void func_actor_400600_80139A78(Task* arg0)
{
    Actor400600Work* work;
    s32              i;

    work = (Actor400600Work*)arg0->idMap;
    i    = 1;
    do {
        work->slots[i].field_9 = work->field_726;
        Gp_AnimResetSlot(&work->anim, i, work->field_746);
        i++;
    } while (i < 0x12);
    work->field_744 = work->field_746;
}

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600_2", func_actor_400600_80139AE8);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600_2", func_actor_400600_80139BA0);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600_2", func_actor_400600_80139C00);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600_2", func_actor_400600_80139CAC);

void func_actor_400600_80139D98(Task* arg0, s16 arg1, s16 arg2)
{
    Actor400600Work* work = (Actor400600Work*)arg0->idMap;

    work->field_726 = arg2;
    work->field_746 = arg1;
    work->field_742 = 2;
}

void func_actor_400600_80139DB0(Task* arg0, s16 arg1, s16 arg2, s16 arg3)
{
    Actor400600Work* work = (Actor400600Work*)arg0->idMap;

    work->field_720 = arg3;
    work->field_726 = arg2;
    work->field_746 = arg1;
    work->field_742 = 1;
}
