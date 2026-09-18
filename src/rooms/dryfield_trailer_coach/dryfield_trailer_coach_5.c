#include "common.h"

#include <psyq/libgte.h>

#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

#include "rooms/room_common.h"

extern SVECTOR D_dryfield_trailer_coach_801871C4;

void func_dryfield_trailer_coach_80182EB4(GsCOORDINATE2* arg0, SVECTOR* arg1, s32 arg2, s32 arg3);

INCLUDE_ASM("rooms/nonmatchings/dryfield_trailer_coach/dryfield_trailer_coach_5", func_dryfield_trailer_coach_80182EB4);

/// Picks the trailer's shaft drawer for the current camera view. The
/// stage-visit byte `gGameSession->loc.view` is used as a bit index: views 2 and
/// 8 (bits 2 and 8, `0x104`) take `Room_Draw37` with the tall half-extent 0xC0,
/// and view 10 (bit 10, `0x400`) takes `func_dryfield_trailer_coach_80182EB4`
/// with 0x30. `Task::extra` is the task's `TmdObject`, so `field_8` is the
/// coordinate both draws share.
void func_dryfield_trailer_coach_801838DC(Task* arg0)
{
    s32            mask;
    GsCOORDINATE2* coord;

    mask  = 1 << gGameSession->loc.view;
    coord = ((TmdObject*)arg0->extra)->field_8;
    if (mask & 0x104) {
        Room_Draw37(coord, &D_dryfield_trailer_coach_801871C4, 0x60, 0xC0);
        return;
    }
    if (mask & 0x400) {
        func_dryfield_trailer_coach_80182EB4(coord, &D_dryfield_trailer_coach_801871C4, 0x60, 0x30);
    }
}
