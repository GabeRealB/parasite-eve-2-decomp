#ifndef SRC_ROOMS_SHELTER_B6_GROWTH_ROOM_SHELTER_B6_GROWTH_ROOM_PRIVATE_H
#define SRC_ROOMS_SHELTER_B6_GROWTH_ROOM_SHELTER_B6_GROWTH_ROOM_PRIVATE_H

#include "gameplay/area_flags.h"

/// Complete saved-area update list, including its AREA_APPLY_END record.
enum { SHELTER_B6_GROWTH_ROOM_ENTRY_AREA_UPDATE_COUNT = 58 };

/// Saved-area progression updates requested when the growth-room controller initializes.
///
/// Fifty-seven ordered records cover Shelter/Neo Ark, Mine/Shelter and night
/// Dryfield, followed by AREA_APPLY_END. Policy nibbles encode save-mode
/// groups and map-mark requests; some areas have consecutive mode policies.
/// Layout changes also discard saved enemy poses. The list is borrowed only
/// during the synchronous application call and lives with this room overlay.
extern AreaApplyRec gShelterB6GrowthRoomEntryAreaUpdates[SHELTER_B6_GROWTH_ROOM_ENTRY_AREA_UPDATE_COUNT];

#endif // SRC_ROOMS_SHELTER_B6_GROWTH_ROOM_SHELTER_B6_GROWTH_ROOM_PRIVATE_H
