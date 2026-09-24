#include "common.h"

#include "main/gameflag.h"
#include "main/task.h"
#include "rooms/dryfield_night_factory.h"

/// State 0 of the cutscene sequence: edge-detects game flag 0x4E and re-arms
/// the sequence when it changes. A nibble of 1 after a 0 moves the sequence to
/// state 1, and a nibble of 0 after a 1 moves it to state 2; either transition
/// restarts `step`. Every call records the nibble in `prevFlag`.
s32 func_dryfield_night_factory_8017FDC8(Task* task)
{
    NightFactoryCutsceneWork* work  = (NightFactoryCutsceneWork*)task->work;
    s32                       flag  = GameFlag_GetNibble(0x4E);
    s32                       state = flag & 0xFF;

    if (state == 1) {
        if (work->prevFlag == 0) {
            work->state = state;
            goto reset;
        }
    }
    if (((flag & 0xFF) == 0) && (work->prevFlag == 1)) {
        work->state = 2;
    reset:
        work->step = 0;
    }
    work->prevFlag = flag;
    return 0;
}
