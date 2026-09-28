#ifndef GAMEPLAY_VIEW_H
#define GAMEPLAY_VIEW_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "gameplay/message.h"

/// 2-byte record in tables pointed to by `Gp_ViewCountTables`. Indexed by
/// `GameSession.at4.loc.room - 1`. Gp_GetViewCountLo reads prefix.bytes.field_0;
/// Gp_FindViewIndex reads prefix.packed as its search limit. This limit is not
/// the camera-array extent: mappings also select images, and the shared identity
/// map can be indexed beyond a room's search limit.
typedef struct _GpViewCountRec {
    union {
        struct {
            /* 0x0 */ u8 field_0;
            /* 0x1 */ u8 field_1;
        } bytes;
        s16 packed;
    } prefix;
} GpViewCountRec;
STATIC_ASSERT_SIZEOF(GpViewCountRec, 2);

/// Per-stage wrapper. `field_0` is an array of `GpViewCountRec*`, indexed by
/// `GameSession.at4.loc.area - 1`.
typedef struct _GpViewCountTbl {
    /* 0x0 */ GpViewCountRec** field_0;
} GpViewCountTbl;

/// Per-stage wrapper. `field_0` is a 3-level table of bytes, indexed
/// 1-based by `GameSession.at4.loc.area` / `at4.loc.room` / `at4.loc.view`.
/// `Gp_GetViewIndex` returns the innermost byte (camera / view index).
typedef struct _GpViewIndexTbl {
    /* 0x0 */ u8*** field_0;
} GpViewIndexTbl;

/// Spawn coordinates written with a full-word yaw, consumed with a halfword yaw.
typedef union _GpSpawnTransform {
    struct {
        s32 field_0;
        s32 field_4;
        s32 field_8;
        s32 field_C;
    } words;
    GpActorArg actor;
} GpSpawnTransform;
STATIC_ASSERT_SIZEOF(GpSpawnTransform, 0x10);

/// 0x24-byte camera/view record in tables pointed to by `Gp_ViewTables`.
/// Indexed 1-based by `Gp_GetViewIndex()`. `mtx` rotation is copied to
/// `gGfxViewRotCoord.coord` and translation to `gGfxViewCoord.coord.t` by `Gp_LoadStageView` /
/// `Gp_ApplyView` / `Gp_ApplyViewTask`; `field_20` is `lhu` into
/// `gDisplayState.screenDistance` and `lw` into GTE H (`gte_SetGeomScreen`).
typedef struct _GpViewRec {
    /* 0x00 */ MATRIX mtx;
    /* 0x20 */ u32    field_20;
} GpViewRec;
STATIC_ASSERT_SIZEOF(GpViewRec, 0x24);

/// Per-stage wrapper. `field_0` is an array of `GpViewRec*`, indexed by
/// `GameSession.at4.loc.area - 1` / `GpAreaKey.area - 1`.
typedef struct _GpViewTbl {
    /* 0x0 */ GpViewRec** field_0;
} GpViewTbl;

#endif // GAMEPLAY_VIEW_H
