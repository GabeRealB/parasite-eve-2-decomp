#include "common.h"

#include "main/task.h"

#include "actors/actor_342400.h"

/// Returns 1 when the hit flags are set - bit 0 of the flag halfword or bits
/// 0x102 of the word - and 0 otherwise.
s16 func_actor_342400_8016974C(Task* arg0)
{
    Actor342400Work* work = (Actor342400Work*)arg0->work;

    if ((work->flags_EC.half & 1) || (work->flags_EC.word & 0x102)) {
        return 1;
    }
    return 0;
}
