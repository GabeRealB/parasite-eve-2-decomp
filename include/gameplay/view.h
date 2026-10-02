#ifndef GAMEPLAY_VIEW_H
#define GAMEPLAY_VIEW_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

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

/// Camera orientation, origin and perspective distance for a gameplay view.
///
/// `transform.m` rotates world axes into camera axes, with `ONE` (4096) for
/// 1.0. `transform.t` is the negated camera origin in signed world-coordinate
/// units. The view applies translation before rotation: a world point `p`
/// becomes `transform.m * (p + transform.t)`. Only the nine rotation
/// coefficients and three translation words are used; the matrix's alignment
/// bytes are not copied into the active view.
///
/// Area camera arrays use a mapped 1-based camera index, stored at index minus
/// one; animated camera paths use their own zero-based frame indices. The
/// record stores no array count or terminator. Room overlays own their camera
/// arrays and may edit them; actors also supply persistent camera records.
/// A queued view task borrows the record until it applies it, so its storage
/// must stay loaded and readable until then. Application copies the camera
/// components into the active view and retains no pointer to the record.
typedef struct {
    MATRIX transform;      // World-to-camera rotation and negated world-space camera origin
    u32    screenDistance; // Projection-plane distance in pixels; GTE H and DisplayState retain the low 16 bits
} ViewCamera;
STATIC_ASSERT_SIZEOF(ViewCamera, 0x24);

/// Per-stage wrapper. `field_0` is an array of `ViewCamera*`, indexed by
/// `GameSession.location.loc.area - 1` / `GameLocationKey.area - 1`.
typedef struct _GpViewTbl {
    /* 0x0 */ ViewCamera** field_0;
} GpViewTbl;

#endif // GAMEPLAY_VIEW_H
