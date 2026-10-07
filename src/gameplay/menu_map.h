#ifndef GAMEPLAY_PRIVATE_MENU_MAP_H
#define GAMEPLAY_PRIVATE_MENU_MAP_H

#include "types.h"

/// Packed marker-table entry and marker-state values used by the map screen.
enum {
    MENU_MAP_MARKER_FLAG_ID_MASK      = 0x7FF,
    MENU_MAP_MARKER_ALTERNATE_PICTURE = 0x800,
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
