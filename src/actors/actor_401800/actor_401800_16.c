#include "common.h"

#include "actors/actor_401800.h"
#include "main/wipsys.h"

/// Leaves state 0xD: to 0xE while the player has HP left, to 0x16 once it is
/// gone. Any other state is left alone.
s32 func_actor_401800_8013DF3C(Task* task)
{
    Actor401800Work* work = (Actor401800Work*)task->work;
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
