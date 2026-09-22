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

void ActorsShared80169e24(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* slow;
    Actor341700Work* fast;

    work = (Actor341700Work*)arg0->work;
    if ((ActorsShared8016974c() << 0x10) != 0) {
        if (work->field_44F == 1) {
            fast            = (Actor341700Work*)arg0->work;
            fast->field_426 = 0x32;
            fast->field_41C = 0x10;
            fast->field_418 = 7;
            fast->field_414 = 1;
        } else {
            slow            = (Actor341700Work*)arg0->work;
            slow->field_426 = 0x1E;
            slow->field_41C = 0x10;
            slow->field_418 = 1;
            slow->field_414 = 1;
        }
        work->field_422 = work->field_422 + 1;
    }
}
