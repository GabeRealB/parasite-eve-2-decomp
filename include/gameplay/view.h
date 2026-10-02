#ifndef GAMEPLAY_VIEW_H
#define GAMEPLAY_VIEW_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "gameplay/message.h"

/// Number of unsigned byte entries in `gViewIdentityMap`.
///
/// Its capacity covers 1-based logical views 1 through this value; each room
/// supplies its own view count independently of the shared mapping's extent.
enum { VIEW_IDENTITY_MAP_LENGTH = 50 };

/// Shared mapping for rooms whose logical views use the same camera/image indices.
///
/// Indexed by the 1-based logical view minus one: entry `view - 1` contains
/// `view` for views 1 through `VIEW_IDENTITY_MAP_LENGTH`. There is no terminator.
/// Room tables borrow this gameplay-owned array without modifying it; it remains
/// valid while gameplay is loaded. Each room's view count limits reverse searches,
/// independently of this array's capacity and its camera/image table extents.
extern u8 gViewIdentityMap[VIEW_IDENTITY_MAP_LENGTH];

/// 2-byte record in tables pointed to by `Gp_ViewCountTables`. Indexed by
/// `GameSession.location.loc.room - 1`. Gp_GetViewCountLo reads prefix.bytes.field_0;
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
/// `GameSession.location.loc.area - 1`.
typedef struct _GpViewCountTbl {
    /* 0x0 */ GpViewCountRec** field_0;
} GpViewCountTbl;

/// A stage's directory mapping room-local logical views to camera/image indices.
///
/// `viewMaps[area - 1][room - 1][view - 1]` is an unsigned, 1-based index
/// shared by the area's camera, image and sprite resources. Areas without
/// resources may have NULL entries; lookups require a populated area and valid
/// 1-based area, room and view IDs within their respective table extents.
/// Maps have no terminator. Their capacities, room reverse-search limits and
/// indexed resource extents are separate; multiple logical views may map to
/// the same index.
///
/// The stage map overlay owns the area directory and borrows room-overlay
/// room directories and maps, or gameplay's `gViewIdentityMap`. Gameplay scripts
/// may change map entries. Borrowed room pointers require that overlay to stay
/// loaded; this record does not allocate or release any of the tables.
typedef struct {
    u8*** viewMaps; // Area directories of per-room logical-view byte maps.
} ViewIndexTable;
STATIC_ASSERT_SIZEOF(ViewIndexTable, 4);

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

/// Camera transform and projection distance for a view.
///
/// View indices are 1-based. The projection distance is supplied as a word to
/// GTE H; `DisplayState::screenDistance` retains its low 16 bits.
typedef struct {
    MATRIX mtx;            // Camera transform copied into the view coordinate nodes
    u32    screenDistance; // Projection distance; GTE H uses the low 16 bits
} GpViewRec;
STATIC_ASSERT_SIZEOF(GpViewRec, 0x24);

/// Per-stage wrapper. `field_0` is an array of `GpViewRec*`, indexed by
/// `GameSession.location.loc.area - 1` / `GameLocationKey.area - 1`.
typedef struct _GpViewTbl {
    /* 0x0 */ GpViewRec** field_0;
} GpViewTbl;

#endif // GAMEPLAY_VIEW_H
