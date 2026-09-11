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

/* Called with no argument: the caller's own `Task*` is already in
 * `$a0` at the `jal` and the callee reads it as its own `arg0`, so the target
 * has no register copy.  A real prototype would make GCC emit one, so it
 * stays unprototyped. */
s32 ActorsShared8016974c();

void func_actor_341700_80168F5C(Task* arg0)
{
    u16              ticks;
    Actor341700Work* work;

    work            = (Actor341700Work*)arg0->idMap;
    ticks           = work->field_412;
    work->field_412 = ticks + 1;
    if ((s16)ticks >= 0x51) {
        work->field_422 = work->field_422 + 1;
    }
}

void func_actor_341700_80168F9C(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* work2;

    work = (Actor341700Work*)arg0->idMap;
    if ((ActorsShared8016974c() << 0x10) != 0) {
        func_actor_341700_801681C4(arg0, 0);
        work2            = (Actor341700Work*)arg0->idMap;
        work2->field_426 = 8;
        work2->field_41C = 0x10;
        work2->field_418 = 0xF;
        work2->field_414 = 1;
        work->field_422  = work->field_422 + 1;
    }
}

void func_actor_341700_80169018(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* work2;

    work = (Actor341700Work*)arg0->idMap;
    if ((ActorsShared8016974c() << 0x10) != 0) {
        work->field_438  = 1;
        work->field_412  = 0;
        work2            = (Actor341700Work*)arg0->idMap;
        work2->field_426 = 4;
        work2->field_41C = 0x10;
        work2->field_418 = 4;
        work2->field_414 = 1;
        work->field_422  = work->field_422 + 1;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_12", func_actor_341700_8016908C);

/// The four sub-state handlers this branch of the actor dispatches through.
extern TaskFuncTable4 D_actor_341700_80161ED0;

void func_actor_341700_801691B0(Task* arg0)
{
    Actor341700Work* work;
    TaskFuncTable4   sp;

    work = (Actor341700Work*)arg0->idMap;
    sp   = D_actor_341700_80161ED0;
    sp.funcs[(s16)work->field_422](arg0);
}

void func_actor_341700_80169218(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* work2;

    work             = (Actor341700Work*)arg0->idMap;
    work->field_432  = 1;
    work2            = (Actor341700Work*)arg0->idMap;
    work2->field_41C = 0x10;
    work2->field_418 = 7;
    work2->field_414 = 2;
    work->field_422  = work->field_422 + 1;
}
