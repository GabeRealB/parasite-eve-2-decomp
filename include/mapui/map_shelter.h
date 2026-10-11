#ifndef INCLUDE_MAPUI_MAP_SHELTER_H
#define INCLUDE_MAPUI_MAP_SHELTER_H

#include "types.h"

#include "gameplay/area_flags.h"
#include "gameplay/battle_reward.h"
#include "gameplay/direction.h"
#include "gameplay/map.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/areas.h"
#include "main/gfx_types.h"
#include "main/stage_types.h"
#include "main/task_types.h"

/// Mine/Shelter's image-memory slots for captured frames and auxiliary heaps.
///
/// Stage 4 selects this table; area IDs 1..49 index it directly, with every
/// area populated. Extents are byte counts below the resident image workspace,
/// not captured-frame lengths. The map overlay must remain loaded for lookup.
/// Entry 0 retains the shared 0x3A8E0-byte slot also used by M.I.S.T. parking;
/// its purpose here is unproven. No bounds or empty-slot checks are performed.
extern GfxImageSlot gMapShelterImageSlotsByArea[50];

/// Map page and live marker counts; the marker array adds a page-zero terminator.
enum {
    MAP_SHELTER_PAGE_COUNT   = 6,
    MAP_SHELTER_MARKER_COUNT = 30,
};

/// Number of scene columns in each Mine/Shelter area-music row.
enum { MAP_SHELTER_MUSIC_COLUMN_COUNT = 12 };

/// Area music for Mine/Shelter, in flat rows of twelve scene columns.
///
/// Index is area ID * `MAP_SHELTER_MUSIC_COLUMN_COUNT` + column: areas 0..49,
/// columns 0..11. Row zero retains its twelve no-sequence entries. No row enables
/// the stage ambient loop. There is no terminator or bounds check. Keep this map
/// overlay loaded throughout selection and any music task borrowing the table.
extern StageMusicEntry gMapShelterAreaMusicEntries[(GAME_AREA_SHELTER_R49 + 1) * MAP_SHELTER_MUSIC_COLUMN_COUNT];

/// Scene-indexed countdown music for Mine/Shelter.
///
/// `gStageCountdownMusicEntry` selects 0..19 directly, without a bounds check
/// or terminator. Entries 0..3 and 8..11 start immediately; 4..6 use the
/// view-ready policy, entry 7 selects no sequence, and 12..19 retain sequence
/// zero with the view-ready policy. Keep this overlay loaded until the music
/// task releases its borrowed selection.
extern StageMusicEntry gMapShelterCountdownMusicEntries[20];

/// Availability and area-shading flags for Mine/Shelter's six map pages.
///
/// Indexed by page 1..6: pages 1..3 (mine and B1) use nibble flag 0x17;
/// pages 4..6 (B2..B4) use 0x18. A nonzero nibble unlocks a page and reveals its
/// unvisited areas; otherwise visited areas determine availability. Slot 0
/// is the navigation boundary; there is no terminator. Keep this map loaded.
extern u8 gMapShelterPageFlagIds[MAP_SHELTER_PAGE_COUNT + 1];

/// Area-to-map coordinate mappings for Mine/Shelter.
///
/// Area IDs 0..49 index directly; slot 0 and areas 23, 26, 36..38 and 49 use
/// `MENU_MAP_AREA_PAGE_NONE`. Record 50 closes page-availability walks with
/// `MENU_MAP_AREA_PAGE_END`. Origins use world X/Z units, map points use
/// pixels, and scales use world units per pixel. Keep this overlay loaded.
extern MenuMapArea gMapShelterAreas[GAME_AREA_SHELTER_R49 + 2];

/// Area shapes and icon-only page selections for Mine/Shelter.
///
/// Indexed directly by area ID 0..49; slot zero is hidden. There is no
/// terminator: `gMenuMapAreaShapeCounts` includes slot zero and covers this whole array.
/// Keep this map loaded during drawing, and load the selected page's picture
/// package before dereferencing a shape model from that page.
/// Areas 22 and 48 share the pod-bottom shape and name each other in
/// pairedArea. The renderer retains each area's own 32-bit visit bank even
/// for this cross-bank pair.
/// Area 26 retains page B1 without a model, despite its coordinate mapping
/// being hidden; no icon belongs to it. The intended mapping is unproven.
extern MenuMapAreaShape gMapShelterAreaShapes[GAME_AREA_SHELTER_R49 + 1];

/// Flag-controlled Mine/Shelter map markers in signed picture-centred pixels.
///
/// Entries 0..29 correspond to `gMapShelterMarkerFlags`; a flag nibble of two
/// shows its marker. Entries 17, 18, 20, 28 and 29 also require their area's
/// visit bit. The final page-zero record ends the list and has no flag entry.
/// Keep this map overlay loaded during lookup and drawing.
extern MenuMapMarker gMapShelterMarkers[MAP_SHELTER_MARKER_COUNT + 1];

/// Fixed and objective icons on the Mine/Shelter map.
///
/// Fourteen records and a page-zero terminator; areas and pages are one-based.
/// Fixed icons require a visited area and optionally a nonzero flag nibble;
/// objectives compare their condition with the current objective byte and
/// can appear before visiting. Coordinates are signed picture-centred pixels.
/// The kind-zero picture's subject is unproven. Keep this map loaded to draw.
extern MenuMapIcon gMapShelterIcons[15];

/// Area names displayed by the Mine/Shelter map.
///
/// Indexed by area ID minus one, covering 1..49 without a terminator. Each
/// record holds a NUL-terminated UI-text line in 32 bytes; areas 37 and 38
/// are unnamed. Oval Office suffixes retain 0x81 bytes followed by literal
/// i/j; the large font's 0x81 metrics are zero and the intended punctuation
/// is unproven. Keep this map loaded while measuring or drawing the text.
extern MenuMapAreaName gMapShelterAreaNames[GAME_AREA_SHELTER_R49];

/// Mine/Shelter's object placements and spawn recipes, indexed directly by area.
///
/// Entries 1..49 select areas; entry 0 is empty and entry 50 ends flag seeding
/// with `AREA_OBJECT_ROOM_END`. Place and spawn lists use their own end markers.
/// Empty spawn lists retain pickup metadata without creating models. Areas 41
/// and 45 have place lists but NULL spawn pointers. Keep this map overlay loaded
/// for lookup and borrowed room/actor callbacks and models loaded while in use.
extern AreaObjectRoom gMapShelterObjectRooms[GAME_AREA_SHELTER_R49 + 2];

/// Default room-controller recipes for all 49 Mine/Shelter areas.
///
/// Location keys select the first matching `GP_TASK_LOC_KEY(stage, area, 0)`
/// recipe, rather than indexing by area. Each bodyless task has priority 32.
/// The fiftieth record closes lookup with `TASK_DESC_END`. Keep the map loaded
/// through spawning and the selected room callback loaded for task execution.
extern TaskDesc gMapShelterRoomTaskDescriptors[GAME_AREA_SHELTER_R49 + 1];

/// Packed flag words parallel to Mine/Shelter's thirty live map markers.
///
/// Indices 0..29 match `gMapShelterMarkers`, excluding its terminator. Low
/// eleven bits select a game-flag nibble; `MENU_MAP_MARKER_ALTERNATE_PICTURE`
/// selects the second picture. Marker zero also uses that picture in chapter
/// six. Repeated flag IDs intentionally control several marker locations.
/// Keep this overlay loaded for lookup; this array has no terminator.
extern u16 gMapShelterMarkerFlags[MAP_SHELTER_MARKER_COUNT];

/// Mine/Shelter model-lighting arrays indexed by `GameLocationKey.area - 1`.
///
/// Stage 4 has 49 area entries. Each leads to room descriptors indexed by
/// `GameLocationKey.room - 1`; area 39 has two dumping-hole rooms and area 40
/// seven incinerator rooms. Room extents are determined by each area's array.
/// The map overlay must be loaded for the table and its local descriptors;
/// pointers into a room overlay require that specific overlay to remain loaded.
extern WorldCoordRoomLighting* gMapShelterRoomLightingTables[49];

/// Borrowed warp endpoint arrays for Mine/Shelter.
///
/// Select area - 1 for IDs 1..49, then warp - 1 within that area's own
/// endpoint extent; neither level has a terminator. Every area entry is populated.
/// The map owns this directory. Keep it and the selected room overlay loaded
/// during lookup; endpoints are copied by value and are independent of room ID.
extern DirectionWarpEntry* gMapShelterAreaWarpEntries[GAME_AREA_SHELTER_R49];

/// Mine/Shelter's directory of logical-view counts for all 49 areas.
///
/// Select `viewCounts[area - 1][room - 1]` with valid one-based area and room
/// IDs; every area pointer is populated, and each borrowed room array has its
/// own extent. Counts are signed halfword element counts for reverse view-map
/// searches, independent of camera/image capacities. Neither level terminates.
/// Keep the map and selected room overlays loaded during lookup.
extern ViewCountTable gMapShelterViewCountTable;

/// Stage collision-resource directory for Mine/Shelter.
///
/// Select `areaRooms[area - 1][room - 1]` with area IDs 1..49 and a room
/// within that area's array. Every area entry is populated.
/// Neither level has a terminator. This map owns the directory and some
/// descriptors; selected room overlays own the mutable grids and linked
/// trigger/occluder storage, which must remain loaded until retired.
extern WorldCollisionStageResources gMapShelterWorldCollisionResources;

/// Mine/Shelter's camera directory for 49 areas.
///
/// Area and mapped-camera indices are one-based with separate extents. All area
/// entries are populated; camera arrays have no terminator or count header.
/// The map owns the directory and borrows room-owned mutable camera arrays.
/// Keep the map and selected room overlays loaded through lookup, and the camera
/// record readable until any queued camera task applies it.
extern ViewCameraTable gMapShelterViewCameraTable;

/// Mine/Shelter's directory of logical-view maps for 49 areas.
///
/// Area, room and logical-view IDs are one-based with separate extents. The map
/// overlay owns the area directory and borrows each loaded room's maps.
extern ViewIndexTable gMapShelterViewIndexTable;

/// Mine/Shelter's directory of mapped-view sprite arrays for 49 areas.
///
/// Contains one directory record; area and mapped-view IDs are one-based with
/// separate extents. Room-owned mutable sprite resources must remain loaded.
extern SpriteAreaTable gMapShelterSpriteAreaTable;

/// Mine/Shelter's collision and footstep property directory for 49 areas.
///
/// Stage 4's `gWorldCollisionSurfacePropertiesTables` entry. Select with area - 1 (areas 1..49),
/// then surface class 0..7; room variants share their area's class table.
/// Each table supplies eight non-NULL class records, without a count header
/// or terminator in that lookup domain. Any retained storage after class 7
/// is outside the domain. The map overlay owns the directory and must stay
/// loaded; borrowed class tables and records require their owning room overlay.
extern WorldCollisionSurfaceProperties** gMapShelterSurfacePropertiesByArea[49];

/// Mine/Shelter battle rewards for normal, replay and Scavenger runs.
///
/// 46 rows keyed by stage, area and placement variant, followed by
/// `INVENTORY_BATTLE_REWARD_LIST_END`. The first matching row grants one pack
/// per nonzero slot; slot `INVENTORY_BATTLE_REWARD_BONUS_SLOT` additionally
/// requires an attached Medicine Wheel. Keep this map loaded during lookup.
extern InventoryBattleReward gMapShelterBattleRewards[47];

/// Mine/Shelter battle rewards for Bounty and Nightmare runs.
///
/// 12 rows keyed by stage, area and placement variant, followed by
/// `INVENTORY_BATTLE_REWARD_LIST_END`. The first matching row grants one pack
/// per nonzero slot; slot `INVENTORY_BATTLE_REWARD_BONUS_SLOT` additionally
/// requires an attached Medicine Wheel. Keep this map loaded during lookup.
extern InventoryBattleReward gMapShelterHardBattleRewards[13];

/// Resolves a Mine/Shelter destination's room variant from game progress.
///
/// The `map_shelter` overlay must be loaded. Reads `request->areaId` only when
/// `request->queryOnly` is `ROOM_EVENT_EXECUTE`; a query leaves `reply` untouched.
/// Only `reply->room` can change. Initialize the reply's destination selectors
/// before calling: unhandled areas and unmet progress thresholds keep its room.
/// Request and reply may be the same record; neither pointer is retained.
/// Returns 1 even when no room changes. Progress nibbles used as room indices
/// must select existing rooms; this resolver does not validate their range.
s32 mapShelterRoomVariantResolve(RoomEventMsg* request, RoomEventMsg* reply);

#endif // INCLUDE_MAPUI_MAP_SHELTER_H
