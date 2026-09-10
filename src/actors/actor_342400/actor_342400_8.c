#include "common.h"

#include "main/sound.h"
#include "main/task.h"

#include "gameplay/1BC.h"

#include "actors/actor_342400.h"

void func_actor_342400_8016BBD0(void)
{
}

void func_actor_342400_8016BBD8(Task* arg0)
{
    Actor342400Work* work;
    GpEnemy*         enemy;

    enemy = (GpEnemy*)arg0->spawnArg2;
    work  = (Actor342400Work*)arg0->idMap;
    SndEvt_EnqueueType7(((enemy->field_8 >> 0xC) << 8) | 0x402C0002, 0xF);
    if ((Gp_StateF0.field_1F & 0xF) == (((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC)) {
        Gp_StateF0.field_1F = 0;
    }
    Gp_UnlinkNode(&enemy->node);
    work->field_420 = work->field_420 + 1;
}
