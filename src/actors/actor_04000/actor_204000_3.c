#include "common.h"
#include "actors/actor_104000.h"

/// Message handler for 0x7DE: advances the work block's state from 0xB to 0xD
/// and leaves any other state alone.
s32 Actor04000_Fn06704(Actor104000* arg0, s32 arg1, void* arg2)
{
    Actor104000Work* work;

    work = arg0->field_1C;
    if (work->field_0 == 0xB) {
        work->field_0 = 0xD;
    }
    return 1;
}
