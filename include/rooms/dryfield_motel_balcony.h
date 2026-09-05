#ifndef ROOMS_DRYFIELD_MOTEL_BALCONY_H
#define ROOMS_DRYFIELD_MOTEL_BALCONY_H

#include "common.h"

#include "rooms/room_common.h"

#include <psyq/libgte.h>

/// Same 0x18-byte scratch block as `RoomDraw04Scratch`. This overlay holds two
/// copies of that body (`func_dryfield_motel_balcony_8017E66C` and
/// `func_dryfield_motel_balcony_801809AC`), so they cannot share one object.
typedef RoomDraw04Scratch RoomFanScratch;

/// One row of `D_dryfield_motel_balcony_801822AC`, indexed by
/// `GpEffWork.field_20` (the palette selector packed into the spawn arg).
/// Each field is the right-shift applied to the effect's fade level to get
/// that colour channel, so a row picks the tint of the halo.
typedef struct _RoomShadeShift {
    /* 0x0 */ s16 r;
    /* 0x2 */ s16 g;
    /* 0x4 */ s16 b;
} RoomShadeShift;
STATIC_ASSERT_SIZEOF(RoomShadeShift, 0x6);

extern RoomShadeShift D_dryfield_motel_balcony_801822AC[];

#endif
