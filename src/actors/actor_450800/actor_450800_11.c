#include "common.h"

#include "main/tmd.h"

#include "actors/actor_450800.h"
#include "actors/actor_461800_move.h"

/// The enemy's animation state machine, run by its spawn and per-frame
/// handlers. States 1 and 2 start the clip in `animId` through
/// `func_actor_450800_801334C4` or `func_actor_450800_8013344C` and advance to
/// state 3. State 3 walks the model 12 units a frame while the walk clip (4)
/// has `travel` left, dropping back to clip 1 with reset argument 0xA when it
/// runs out, then ticks the slots.
void func_actor_450800_801330AC(Task* task)
{
    Actor450800SpawnWork* work;
    s16                   animId;

    work = (Actor450800SpawnWork*)task->work;
    if (work->state == 1) {
        func_actor_450800_801334C4(task);
        work->state = 3;
        return;
    }
    if (work->state == 2) {
        func_actor_450800_8013344C(task);
        work->state = 3;
        return;
    }
    if (work->state == 3) {
        do {
        } while (0);
        animId = work->animId;
        if (animId == 4 && work->travel != 0) {
            Actor461800_MoveForward(task, 0xC);
            work->travel = (u16)work->travel - 1;
            if (work->travel == 0) {
                work->animArg = 0xA;
                work->animId  = 1;
            }
        }
        func_actor_450800_80133400(task);
        return;
    }
}
