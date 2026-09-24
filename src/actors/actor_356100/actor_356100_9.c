#include "common.h"

#include "actors/actor_356100.h"
#include "main/task.h"
#include "main/wipsys.h"

/// Moves the actor out of state 0xD: to 0xE while the player has HP left,
/// to 0x16 once it has run out.
s32 func_actor_356100_8016A074(Task* task)
{
    Actor356100Work* work = (Actor356100Work*)task->work;
    PlayerStatus*    cfg  = &Player_Status;

    if (work->field_0 == 0xD) {
        if (cfg->hp > 0) {
            work->field_0 = 0xE;
        } else {
            work->field_0 = 0x16;
        }
    }
    return 1;
}
