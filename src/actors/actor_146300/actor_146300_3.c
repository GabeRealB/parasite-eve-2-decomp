#include "common.h"

#include "actors/actor_146300.h"
#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"

/// Per-frame update: reset mode 1 runs the reseed with the latched reset
/// argument and mode 2 the plain reseed, each then switching to mode 3; mode 3
/// ticks the animation. Steps 1 and 2 each return through their own copy of the
/// switch to mode 3; the two are identical, so jump.c cross-jumps them and only
/// the second survives.
void func_actor_146300_801327CC(Task* task)
{
    if (D_actor_146300_80142828->field_4B4 == 1) {
        func_actor_146300_8013291C();
        D_actor_146300_80142828->field_4B4 = 3;
        return;
    }
    if (D_actor_146300_80142828->field_4B4 == 2) {
        func_actor_146300_8013288C();
        D_actor_146300_80142828->field_4B4 = 3;
        return;
    }
    if (D_actor_146300_80142828->field_4B4 == 3) {
        func_actor_146300_80132840();
    }
}
