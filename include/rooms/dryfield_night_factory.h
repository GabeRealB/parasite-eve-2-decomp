#ifndef ROOMS_DRYFIELD_NIGHT_FACTORY_H
#define ROOMS_DRYFIELD_NIGHT_FACTORY_H

#include "common.h"

#include "main/task.h"

/// Cutscene work block the room's factory task allocates as 0xC zeroed bytes in
/// its state 0 and parks at `Task::idMap` (0x1C) -- that slot is *not* a
/// `TaskIdMap` here.
///
/// `state` selects the handler out of `D_dryfield_night_factory_8017D5DC`,
/// `step` is the counter the room's own handlers advance, and `prevFlag` is the
/// nibble of game flag 0x4E the shared state-0 handler `RoomsShared8017fdc8`
/// last saw. That handler reads the same three bytes.
typedef struct NightFactoryCutsceneWork {
    /* 0x0 */ byte pad_0[0x8];
    /* 0x8 */ u8   state;
    /* 0x9 */ u8   step;
    /* 0xA */ u8   prevFlag;
    /* 0xB */ byte pad_B[0x1];
} NightFactoryCutsceneWork;
STATIC_ASSERT_SIZEOF(NightFactoryCutsceneWork, 0xC);

/// A handler of the cutscene sequence. Unlike `TaskFunc` these report back: a
/// non-zero return means the handler has finished its part of the scene, and
/// the sequence drops back to the shared state 0.
typedef s32 (*NightFactoryCutsceneFunc)(Task*);

/// The room's three handler slots, as the dispatcher's local copy sees them.
/// The `.rodata` table they are copied out of ends with a NULL slot.
typedef struct NightFactoryCutsceneTable3 {
    /* 0x0 */ NightFactoryCutsceneFunc funcs[3];
} NightFactoryCutsceneTable3;
STATIC_ASSERT_SIZEOF(NightFactoryCutsceneTable3, 0xC);

/// The night factory cutscene sequence's handler table: the shared state-0
/// handler, the room's own two states, and the NULL the original `.rodata`
/// table ends with. Only the three handlers are copied by the dispatcher
/// `func_dryfield_night_factory_8017FD5C`, which is why the terminator is not
/// part of `NightFactoryCutsceneTable3`.
extern const NightFactoryCutsceneTable3 D_dryfield_night_factory_8017D5DC;

#endif // ROOMS_DRYFIELD_NIGHT_FACTORY_H
