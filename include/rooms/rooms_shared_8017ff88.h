#ifndef ROOMS_SHARED_8017FF88_H
#define ROOMS_SHARED_8017FF88_H

#include "common.h"

#include "main/task.h"

/// One row of `RoomsShared8017ff88Shades`, indexed by
/// `GpEffWork.field_20` (the palette selector packed into the spawn arg).
/// Each field is the right-shift applied to the effect's fade level to get
/// that colour channel, so a row picks the tint of the halo.
typedef struct _RoomsShared8017ff88Shade {
    /* 0x0 */ s16 r;
    /* 0x2 */ s16 g;
    /* 0x4 */ s16 b;
} RoomsShared8017ff88Shade;
STATIC_ASSERT_SIZEOF(RoomsShared8017ff88Shade, 0x6);

extern RoomsShared8017ff88Shade RoomsShared8017ff88Shades[];

/// Expanding halo using `Room_Draw04` and `Room_DrawBillboard`: state 0 parks
/// the effect frame on its anchor and works the fade step out of the spawn
/// argument, state 1 draws the ring (plus a half-bright echo on odd ticks)
/// while the level ramps up, and state 2 fades it back out through the
/// billboard helper before releasing the work block. Four rooms carry this
/// body.
void RoomsShared8017ff88(Task* task);

#endif // ROOMS_SHARED_8017FF88_H
