#include "common.h"

#include "main/sound.h"
#include "main/task.h"

#include "gameplay/1BC.h"

#include "actors/actor_342400.h"

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_14", func_actor_342400_8016B3C4);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_14", func_actor_342400_8016B414);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_14", func_actor_342400_8016B48C);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_14", func_actor_342400_8016B500);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_14", func_actor_342400_8016B5B0);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_14", func_actor_342400_8016B744);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_14", func_actor_342400_8016B84C);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_14", func_actor_342400_8016B914);

void func_actor_342400_8016B9A4(Task* arg0)
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

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_14", func_actor_342400_8016BA3C);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_14", func_actor_342400_8016BAF4);
