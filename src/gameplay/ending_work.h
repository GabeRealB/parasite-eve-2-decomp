#ifndef GAMEPLAY_PRIVATE_ENDING_WORK_H
#define GAMEPLAY_PRIVATE_ENDING_WORK_H

#include "types.h"

/// Overlay of `Task::spawnArg2` for `Gp_EndingTask` / `Gp_AreaEnterTask`.
/// `Gp_EndingTask` sets `field_4` to 1 on the first run (state 0).
/// `Gp_AreaEnterTask` zeros both words when `spawnArg1` is 0.
typedef struct _GpEndWork {
    /* 0x00 */ s32 field_0;
    /* 0x04 */ s32 field_4;
} GpEndWork;

#endif // GAMEPLAY_PRIVATE_ENDING_WORK_H
