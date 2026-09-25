#ifndef ROOMS_ACROPOLIS_BRIDGE_H
#define ROOMS_ACROPOLIS_BRIDGE_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/task.h"
#include "rooms/room_common.h"

/// Work block this room's script tasks keep at `Task::work`
/// (`memCalloc(0x10, 0)` in `func_acropolis_bridge_8017E04C`). `field_4` is
/// the script step handed to `func_acropolis_bridge_8017E60C` and
/// `promptKind` the display mode forwarded to `func_800D4E78`.
typedef struct AcropolisBridgePromptWork {
    /* 0x00 */ s32 field_0;
    /* 0x04 */ s16 field_4;
    /* 0x06 */ u8  field_6;
    /* 0x07 */ u8  retryCount;
    /* 0x08 */ s16 field_8;
    /* 0x0A */ s16 field_A;
    /* 0x0C */ s16 field_C;
    /* 0x0E */ s8  promptKind;
    /* 0x0F */ s8  promptBusy;
} AcropolisBridgePromptWork;

extern OverlayHotspot D_acropolis_bridge_8018983C[];
extern s32            D_acropolis_bridge_801917A8;

/// Cursor into the packet buffer the bridge's screen-smear effects draw from.
/// Every `DR_MOVE` task of the room takes the packet it points at
/// and bumps it by one, the same way `gGpuPrimCursor` works for the main
/// primitive heap.
extern DR_MOVE* D_acropolis_bridge_801917AC;

void func_acropolis_bridge_8017E60C(s32 digits, s32 hidePrompt);

/// One corner of the unit quad the bridge's dust-cloud task builds its
/// billboard from (`D_acropolis_bridge_8018990C`): the signed XZ pair
/// `(-1, 1)`, `(1, 1)`, `(-1, -1)`, `(1, -1)`, scaled by 0x300 before being
/// rotated into world space. Same shape as the gameplay overlay's
/// `GpQuadCorner`, but signed - the overlay loads the components with `lh`.
typedef struct AcropolisBridgeQuadCorner {
    /* 0x0 */ s16 x;
    /* 0x2 */ s16 y;
} AcropolisBridgeQuadCorner;
STATIC_ASSERT_SIZEOF(AcropolisBridgeQuadCorner, 0x4);

extern AcropolisBridgeQuadCorner D_acropolis_bridge_8018990C[4];

/// 0x2C-byte scratch block the bridge's dust-cloud task takes from
/// `G_SCRATCH_HEAD`. `vec` holds the four billboard corners, projected with
/// one `RTPS` plus one `RTPT` straight into the `POLY_FT4`; `otz` is the
/// `gte_stszotz` depth the primitive is linked into the OT at. `flag` and
/// `sxy` are the `gte_stflg` / `gte_stsxy` slots of the same layout the
/// gameplay overlay's `GpQuadScratch` uses, and this task leaves them unused.
typedef struct AcropolisBridgeQuadScratch {
    /* 0x00 */ s32     otz;
    /* 0x04 */ s32     flag;
    /* 0x08 */ DVECTOR sxy;
    /* 0x0C */ SVECTOR vec[4];
} AcropolisBridgeQuadScratch;
STATIC_ASSERT_SIZEOF(AcropolisBridgeQuadScratch, 0x2C);

/// 0x1C-byte scratch block the bridge's debris billboard
/// (`func_acropolis_bridge_80182F8C`) takes from `G_SCRATCH_HEAD`. `vec` is the
/// piece's world position copied out of its `GsCOORDINATE2` (`workm.t`) and
/// projected with a single `RTPS` through `GsWSMATRIX`: `sx` / `sy` are the
/// projected centre, `flag` the `gte_stflg` result the draw is gated on and
/// `otz` the `gte_stszotz` depth, biased by 1 so it can be divided by. `dx` /
/// `dy` are that depth's half-diagonal, `size * 31 / otz` turned by the
/// billboard's angle, and are rewritten for each of the quad's two diagonals.
typedef struct AcropolisBridgeSpriteScratch {
    /* 0x00 */ s32     otz;
    /* 0x04 */ s32     dx;
    /* 0x08 */ s32     dy;
    /* 0x0C */ s32     flag;
    /* 0x10 */ SVECTOR vec;
    /* 0x18 */ u16     sx;
    /* 0x1A */ u16     sy;
} AcropolisBridgeSpriteScratch;
STATIC_ASSERT_SIZEOF(AcropolisBridgeSpriteScratch, 0x1C);

/// 0x18-byte scratch block the bridge's axis-aligned debris billboard
/// (`func_acropolis_bridge_801833A0`) takes from `G_SCRATCH_HEAD`. Same
/// projection as `AcropolisBridgeSpriteScratch` - `vec` is the piece's world
/// position out of `workm.t`, `sx` / `sy` the projected centre, `flag` the
/// `gte_stflg` gate and `otz` the `gte_stszotz` depth biased by 1 - but the
/// quad is never turned, so one `d` (`size * 55 / otz`) sizes it instead of a
/// pair of rotated half-diagonals.
typedef struct AcropolisBridgeDebrisScratch {
    /* 0x00 */ s32     otz;
    /* 0x04 */ s32     d;
    /* 0x08 */ s32     flag;
    /* 0x0C */ SVECTOR vec;
    /* 0x14 */ u16     sx;
    /* 0x16 */ u16     sy;
} AcropolisBridgeDebrisScratch;
STATIC_ASSERT_SIZEOF(AcropolisBridgeDebrisScratch, 0x18);

#endif
