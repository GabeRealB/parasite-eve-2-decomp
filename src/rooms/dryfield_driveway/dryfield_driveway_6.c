#include "common.h"

#include "gameplay/3CD8.h"

/// Script-event hook: queues stage sound 6 with a distinct cue for events 8 and
/// 10, and reports every event as unhandled.
s32 func_dryfield_driveway_8017DC70(s32 arg0, s32 arg1, s32 arg2)
{
    switch (arg2) {
        case 8:
            Gp_EnqueueStageSnd6(0x52190008, 0, 0);
            break;
        case 10:
            Gp_EnqueueStageSnd6(0x5219000A, 0, 0);
            break;
    }
    return 0;
}
