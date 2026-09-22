#include "common.h"

#include "actors/actors_shared_80135318.h"

/// Gate for one actor's mode step: does nothing until the work block's mode
/// halfword has run below 7 while the actor sits in state 1, and then either
/// flags `field_250` for the caller or steps the mode to 5, copies the state
/// into `field_248` and clears `field_250`. Reports 0 either way.
s32 ActorsShared80135318(ActorShared80135318* arg0)
{
    ActorShared80135318Work* work;
    s32                      state;

    work = arg0->field_1C;
    if (work->field_24E >= 7) {
        return 0;
    }
    state = arg0->field_30;
    if (state != 1) {
        return 0;
    }
    if (work->field_262 != 0) {
        work->field_250 = 3;
    } else {
        work->field_24E = 5;
        work->field_248 = state;
        work->field_250 = 0;
    }
    return 0;
}
