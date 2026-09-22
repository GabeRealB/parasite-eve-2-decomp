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

/* Both are called with no argument: the caller's own `Task*` is already in
 * `$a0` at the `jal` and the callee reads it as its own `arg0`, so the target
 * has no register copy.  A real prototype would make GCC emit one, so these
 * stay unprototyped. */
s32 ActorsShared8016974c();

/* This one, by contrast, is a normal prototyped call: the redundant
 * `move $a0, $s0` is deleted again after reload because `$a0` still holds the
 * caller's own `arg0`. */

void ActorsShared80169d2c(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* work2;

    if ((ActorsShared8016974c() << 0x10) != 0) {
        work            = (Actor341700Work*)arg0->work;
        work->field_426 = 4;
        work->field_41C = 0x10;
        work->field_418 = 0xB;
        work->field_414 = 1;
    }
    if (Gp_TickObjFlag2((GpObj5D*)arg0->spawnArg2) != 0) {
        work2            = (Actor341700Work*)arg0->work;
        work2->field_420 = 3;
        work2->field_422 = 0;
    }
}
