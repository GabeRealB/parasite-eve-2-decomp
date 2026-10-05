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

/// Number of logical view entries searched for a room's reverse lookup.
///
/// Counts are signed 16-bit element counts; a nonpositive value searches no
/// entries. A positive count requires that many readable bytes in the room's
/// logical-view map. It does not give the map's capacity or the extent of the
/// camera, image or sprite arrays selected by that map.
///
/// Stage directories borrow room-overlay count arrays, indexed by the 1-based
/// room ID minus one. Arrays have no terminator; zero is an empty room count.
/// Lookups require valid 1-based stage, area and room IDs within the directories
/// and count array, with a populated area and the owning room overlay still
/// loaded. Byte-sized consumers retain only the low byte.
typedef s16 ViewCount;
STATIC_ASSERT_SIZEOF(ViewCount, 2);

/// A stage's directory of per-room view counts.
///
/// `viewCounts[area - 1][room - 1]` is the number of logical views searched in
/// that room's `ViewIndexTable` map (see `ViewCount`). Areas without views have
/// NULL entries; there is no count or terminator at either level, so lookups
/// require a populated area and valid 1-based area and room IDs.
///
/// The stage map overlay owns the area directory and borrows each room
/// overlay's count array, which stays valid only while that room is loaded.
/// Consumers only read the tables; this record allocates and releases nothing.
typedef struct {
    ViewCount** viewCounts; // Borrowed per-area room view-count arrays, indexed by area - 1; NULL when that area has none
} ViewCountTable;
STATIC_ASSERT_SIZEOF(ViewCountTable, 4);

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

/// Returns the current logical view's 1-based camera, image and sprite index.
///
/// Uses the live session's stage, area, room and view to select a byte from its
/// `ViewIndexTable` map, promoted to `s32` (0..255). Valid views map to nonzero
/// indices, and logical views may share an index. The active stage must
/// be 1..5 and each remaining ID must be within its loaded directory or map;
/// all selected pointers must be populated and their owning overlays loaded.
/// This lookup does not validate IDs or return a missing-view sentinel. Its
/// result must be within the selected area's resource arrays before indexing
/// those arrays at result minus one. Mappings may change during play, so the
/// result describes the current mapping rather than a permanent view ID.
s32 viewGetMappedIndex(void);

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

/// A stage's directory of per-area camera arrays.
///
/// `cameras[area - 1][camera - 1]` is the `ViewCamera` for a 1-based camera
/// index, obtained by mapping the session's view through the stage's
/// `ViewIndexTable`. Areas without a room folder have NULL entries; there is no
/// count or terminator at either level, so lookups require a populated area and
/// valid 1-based area and camera indices.
///
/// The stage map overlay owns the area directory and borrows each room
/// overlay's camera array, which stays valid only while that room is loaded.
/// Consumers only read through this record; it allocates and releases nothing.
typedef struct {
    ViewCamera** cameras; // Borrowed per-area camera arrays, indexed by area - 1; NULL when that area has none
} ViewCameraTable;
STATIC_ASSERT_SIZEOF(ViewCameraTable, 4);

#endif // GAMEPLAY_VIEW_H
