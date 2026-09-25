#include "common.h"

#include "actors/actor_461800_move.h"
#include "actors/actor_535700.h"
#include "main/task.h"

/// The second enemy's animation state machine, run by its spawn and per-frame
/// handlers. States 1 and 2 start the clip in `animId` through
/// `func_actor_535700_80133180` or `func_actor_535700_80133108` and advance to
/// state 3. State 3 walks the model 12 units a frame while the walk clip (4)
/// has `travel` left, dropping back to clip 1 with reset argument 0xA when it
/// runs out, then ticks the slots.
void func_actor_535700_80132D68(Task* task)
{
    Actor535700SpawnWork* work;
    s16                   animId;

    work = (Actor535700SpawnWork*)task->work;
    if (work->state == 1) {
        func_actor_535700_80133180(task);
        work->state = 3;
        return;
    }
    if (work->state == 2) {
        func_actor_535700_80133108(task);
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
        func_actor_535700_801330BC(task);
        return;
    }
}
