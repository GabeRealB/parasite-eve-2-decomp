#include "common.h"

#include "main/task.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"

#include "actors/actor_341700.h"

void func_actor_341700_801681C4(Task* arg0, s32 arg1)
{
    if ((arg1 << 0x10) != 0) {
        if (!((s8)Gp_StateF0.field_1F & 0x80)) {
            Gp_StateF0.field_1F = (((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) | 0x80;
        }
    } else if ((Gp_StateF0.field_1F & 0xF) == (((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC)) {
        Gp_StateF0.field_1F = 0;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_7", func_actor_341700_80168234);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_7", func_actor_341700_801682DC);
