#include "common.h"

#include "main/gameflag.h"
#include "main/task.h"

/// Cutscene work block the linking rooms' factory task allocates as 0xC zeroed
/// bytes in its state 0 and parks at `Task::idMap` (0x1C) -- that slot is *not*
/// a `TaskIdMap` here.
///
/// `state` selects the handler out of the room's own handler table, `step` is
/// the counter the handlers built on this block advance, and `prevFlag` is the
/// nibble of game flag 0x4E the last call saw.
typedef struct {
    /* 0x0 */ byte pad_0[0x8];
    /* 0x8 */ u8   state;
    /* 0x9 */ u8   step;
    /* 0xA */ u8   prevFlag;
    /* 0xB */ byte pad_B[0x1];
} RoomsShared8017fdc8Work;
STATIC_ASSERT_SIZEOF(RoomsShared8017fdc8Work, 0xC);

/// State 0 of the factory rooms' cutscene sequence: edge-detects game flag 0x4E
/// and re-arms the sequence when it changes. A nibble of 1 with no 1 seen the
/// call before moves the handler to state 1, and a nibble of 0 after a 1 moves
/// it to state 2; either transition restarts `step`. Every call records the
/// nibble in `prevFlag`. Shared by the night factory and the factory.
s32 RoomsShared8017fdc8(Task* task)
{
    RoomsShared8017fdc8Work* work  = (RoomsShared8017fdc8Work*)task->idMap;
    s32                      flag  = GameFlag_GetNibble(0x4E);
    s32                      state = flag & 0xFF;

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
