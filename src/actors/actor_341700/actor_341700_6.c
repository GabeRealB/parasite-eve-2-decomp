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
s32 ActorsShared8016945c();
s32 ActorsShared8016974c();

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_6", func_actor_341700_80167890);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_6", func_actor_341700_80167C30);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_6", func_actor_341700_80167E18);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_6", func_actor_341700_80168004);

void func_actor_341700_80168124(Task* arg0)
{
    Actor341700Work* work                = (Actor341700Work*)arg0->idMap;
    void             (*states[2])(Task*) = {
        func_actor_341700_8016A568,
        func_actor_341700_8016A630,
    };

    states[(s16)work->field_420](arg0);
}
