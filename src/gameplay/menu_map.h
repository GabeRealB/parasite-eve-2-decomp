#ifndef GAMEPLAY_PRIVATE_MENU_MAP_H
#define GAMEPLAY_PRIVATE_MENU_MAP_H

#include "types.h"

#include "gameplay/map.h"

#include "main/areas.h"

/// Per-stage inclusive area-ID limits for the map's marked-area cache.
///
/// Index by stage minus `GAME_STAGE_AKROPOLIS` for stages 1..5. Refresh visits
/// IDs 1 through the selected signed-byte limit; higher cache bits are retained.
/// These limits do not size the area directory or the map's shape tables.
/// The trailing bytes retain the image span; their role and the original
/// object's end are unproven.
typedef struct {
    s8   markedAreaIdLimits[GAME_STAGE_SHELTER_NEO_ARK - GAME_STAGE_AKROPOLIS + 1]; // Last refreshed area ID per stage: 17, 38, 38, 49, 33
    byte unknown_05[3];                                                             // Retained nonzero bytes with no known consumer; role unproven
} MenuMapMarkedAreaLimitStorage;
STATIC_ASSERT_SIZEOF(MenuMapMarkedAreaLimitStorage, 8);

/// Current-stage areas drawn with the map's marked-area fill.
///
/// Bit (area-1) in word 0 covers areas 1..32; bit (area-33) in word 1 covers
/// 33..64. A set bit requires AREA_SAVED_MAP_MARK and disabled saved-pose
/// restoration. Refresh updates only the current stage's count range, retaining
/// higher bits; direction-task initialization clears both words. This cache
/// is separate from visited-area bits, which rendering tests independently.
extern u32 gMenuMapMarkedAreaBits[2];

/// Per-stage signed area limits used when refreshing the marked-area cache.
///
/// Index `markedAreaIdLimits` by stage-1 for stages 1..5. These limits describe the cache's
/// refresh range, not the complete area's directory extent. Tail bytes are opaque.
extern MenuMapMarkedAreaLimitStorage gMenuMapStageAreaCounts;

/// Packed marker-table entry and marker-state values used by the map screen.
enum {
    MENU_MAP_MARKER_FLAG_ID_MASK      = 0x7FF,
    MENU_MAP_MARKER_STATE_VISIBLE     = 2,
    MENU_MAP_MARKER_STATE_UNAVAILABLE = -1
};

/// Returns a current-stage map marker's flag nibble and picture selection.
///
/// `markerIndex` is a nonnegative index parallel to the stage's `MenuMapMarker`
/// array. The stage's marker-flag overlay must be loaded. Returns 0..15 plus
/// `MENU_MAP_MARKER_ALTERNATE_PICTURE` when the alternate picture is selected;
/// a flag nibble of `MENU_MAP_MARKER_STATE_VISIBLE` enables drawing. Dryfield night's final
/// marker and Shelter's first marker have story-dependent overrides.
/// An unsupported stage or an index past its table returns
/// `MENU_MAP_MARKER_STATE_UNAVAILABLE`; negative indices are not checked.
s16 menuMapGetMarkerState(s16 markerIndex);

#endif // GAMEPLAY_PRIVATE_MENU_MAP_H
