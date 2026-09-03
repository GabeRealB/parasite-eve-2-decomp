#ifndef ROOMS_DRYFIELD_MOTEL_BALCONY_H
#define ROOMS_DRYFIELD_MOTEL_BALCONY_H

#include "common.h"

#include "rooms/room_common.h"

#include <psyq/libgte.h>

/// 0x1C-byte scratch block taken from `G_SCRATCH_HEAD` by the room's billboard
/// draw helper (`func_dryfield_motel_balcony_8017F7E8` and its twin
/// `func_dryfield_motel_balcony_801818B0`).
///
/// `vec` is the world position copied out of the caller's `GsCOORDINATE2`
/// (`workm.t`), projected with a single `RTPS`: `sx`/`sy` come from
/// `gte_stsxy`, `flag` from `gte_stflg` and `otz` from `gte_stszotz`. `otz` is
/// then incremented and used as the divisor that turns the caller's half-size
/// into the two screen-space radii the fan is drawn with - `rOuter` for the
/// eight outer `POLY_G4` wedges, `rInner` for the two inner ones.
typedef struct _RoomBillboardScratch {
    /* 0x00 */ SVECTOR vec;
    /* 0x08 */ s32     otz;
    /* 0x0C */ s32     rOuter;
    /* 0x10 */ s32     rInner;
    /* 0x14 */ s32     flag;
    /* 0x18 */ s16     sx;
    /* 0x1A */ s16     sy;
} RoomBillboardScratch;
STATIC_ASSERT_SIZEOF(RoomBillboardScratch, 0x1C);

/// 0x18-byte scratch block taken from `G_SCRATCH_HEAD` by the room's radial
/// glow helper (`func_dryfield_motel_balcony_8017E66C` and its twin
/// `func_dryfield_motel_balcony_801809AC`).
///
/// Same projection sequence as `RoomBillboardScratch` - one `RTPS` of `vec`
/// fills `sx`/`sy`, `flag` and `otz` - but only one radius is needed, because
/// the fan is eight `POLY_G4` wedges of a single ring.
typedef struct _RoomFanScratch {
    /* 0x00 */ SVECTOR vec;
    /* 0x08 */ s32     otz;
    /* 0x0C */ s32     radius;
    /* 0x10 */ s32     flag;
    /* 0x14 */ s16     sx;
    /* 0x16 */ s16     sy;
} RoomFanScratch;
STATIC_ASSERT_SIZEOF(RoomFanScratch, 0x18);

/// 0x14-byte request record the room's event scripts build on the stack and
/// hand to the gate at `func_dryfield_motel_balcony_8017D5E8`. `flagId` is a
/// game-flag nibble index - negative means "fire when the nibble is *clear*" -
/// and `itemId` is an optional collected-bit prerequisite (0 = none).
/// `field_4` is the CAP command run when the prerequisite is missing.
typedef struct _RoomEventReq {
    /* 0x00 */ s32 field_0;
    /* 0x04 */ s32 field_4;
    /* 0x08 */ s32 field_8;
    /* 0x0C */ s32 field_C;
    /* 0x10 */ s16 flagId;
    /* 0x12 */ s16 itemId;
} RoomEventReq;
STATIC_ASSERT_SIZEOF(RoomEventReq, 0x14);

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
