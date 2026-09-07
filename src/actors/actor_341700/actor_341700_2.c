#include "common.h"

#include "main/gfx.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

#include "actors/actor_341700.h"

void func_actor_341700_8016A568(Task* arg0);
void func_actor_341700_8016A630(Task* arg0);

/* Both are called with no argument: the caller's own `Task*` is already in
 * `$a0` at the `jal` and the callee reads it as its own `arg0`, so the target
 * has no register copy.  A real prototype would make GCC emit one, so these
 * stay unprototyped. */
s32 func_actor_341700_80168178();
s32 func_actor_341700_80168468();

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_80164CDC);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_80164E9C);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_80165008);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_801651E0);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_80165388);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_8016583C);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_80165984);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_80165AF0);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_80165C70);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_80165DDC);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_80166114);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_801663F0);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_80166568);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_801666F0);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_8016688C);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_801669F8);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_80166B94);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_80166D2C);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_80166E90);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_801670B0);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_8016724C);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_80167744);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_80167890);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_80167C30);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_80167E18);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_80168004);

void func_actor_341700_80168124(Task* arg0)
{
    Actor341700Work* work                = (Actor341700Work*)arg0->idMap;
    void             (*states[2])(Task*) = {
        func_actor_341700_8016A568,
        func_actor_341700_8016A630,
    };

    states[(s16)work->field_420](arg0);
}

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_80168178);

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

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_80168234);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_801682DC);
