#ifndef INCLUDE_MAPUI_MAP_DRYFIELD_H
#define INCLUDE_MAPUI_MAP_DRYFIELD_H

#include "types.h"

#include "gameplay/area_flags.h"
#include "gameplay/areaplace.h"
#include "gameplay/battle_reward.h"
#include "gameplay/direction.h"
#include "gameplay/map.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/areas.h"
#include "main/gfx_types.h"
#include "main/session_types.h"
#include "main/stage_types.h"
#include "main/task_types.h"

/// Map-page and live-marker counts; marker records add a page-zero terminator.
enum {
    MAP_DRYFIELD_PAGE_COUNT   = 3,
    MAP_DRYFIELD_MARKER_COUNT = 29,
};

/// Scene columns per area in the flat music table.
enum { MAP_DRYFIELD_MUSIC_COLUMN_COUNT = 7 };

/// Captured-frame and auxiliary-heap RAM regions for daytime Dryfield.
///
/// Indexed directly by area ID (0..38), without a terminator. Areas 10, 33
/// and 35..37 are empty and cannot be selected for frame transfer. Nonempty
/// extents end at the resident image workspace; they are not frame lengths.
/// Slot 0 retains the MIST-parking region also stored by other stage maps;
/// its purpose is unproven. Keep this map loaded while selecting a slot;
/// the selected RAM arena is separate borrowed storage.
extern GfxImageSlot gMapDryfieldImageSlots[GAME_AREA_DRYFIELD_UNDERPASS + 1];

/// Movie-workspace reservation sizes in bytes for daytime Dryfield.
///
/// Indexed directly by the live save's area ID (0..31); Gas Station requests
/// 0x2A00 bytes and every other entry requests no allocation. Areas 32..38
/// have no entries. The CD subsystem reads this row only after finding a movie
/// slot; a lookup still requires an area within this row's extent. These are
/// total workspace sizes, not frame sizes. Keep this map loaded for lookup.
extern s32 gMapDryfieldMovieWorkspaceBytesByArea[32];

/// Area music for daytime Dryfield, in flat rows of seven scene columns.
///
/// Index is area ID * `MAP_DRYFIELD_MUSIC_COLUMN_COUNT` + column: areas
/// 0..38, columns 0..6. Each row's first entry also controls the ambient loop.
/// There is no terminator. Keep this overlay loaded throughout music selection
/// and any music task borrowing the table.
extern StageMusicEntry gMapDryfieldAreaMusicEntries[(GAME_AREA_DRYFIELD_UNDERPASS + 1) * MAP_DRYFIELD_MUSIC_COLUMN_COUNT];

/// Scene-indexed countdown music for daytime Dryfield.
///
/// `gStageCountdownMusicEntry` selects an entry directly, without a bounds check
/// or terminator. Entries 0..11 hold the scene selections; entries 12..19
/// retain sequence zero and start mode zero. Keep this overlay loaded until
/// the music task finishes using its selection.
extern StageMusicEntry gMapDryfieldCountdownMusicEntries[20];

/// Availability flags for the daytime Dryfield map pages.
///
/// Indexed by page (1..3); slot 0 is the navigation boundary. All pages use
/// nibble flag 0x16: a nonzero value unlocks them, otherwise visited areas
/// determine availability. There is no terminator; navigation uses the page
/// count. Keep this overlay loaded during lookup.
extern u8 gMapDryfieldPageFlagIds[MAP_DRYFIELD_PAGE_COUNT + 1];

/// Area-to-map coordinate mappings for daytime Dryfield.
///
/// Indexed directly by area ID (0..38); unmapped slots use
/// `MENU_MAP_AREA_PAGE_NONE`. Record 39 is `MENU_MAP_AREA_PAGE_END` for
/// page-availability walks. Origins use world X/Z units, map points use
/// pixels and scales use world units per pixel. Keep this overlay loaded for use.
extern MenuMapArea gMapDryfieldAreas[GAME_AREA_DRYFIELD_UNDERPASS + 2];

/// Area shapes and icon-only page selections for daytime Dryfield.
///
/// Indexed directly by area ID 0..38; slot zero is hidden. There is no
/// terminator: `gMenuMapAreaShapeCounts` includes slot zero and covers this whole array.
/// Keep this map loaded during drawing, and load the selected page's picture
/// package before dereferencing a shape model from that page.
extern MenuMapAreaShape gMapDryfieldAreaShapes[GAME_AREA_DRYFIELD_UNDERPASS + 1];

/// Fixed map markers for daytime Dryfield, followed by a page-zero terminator.
///
/// Indices 0..28 correspond to `gMapDryfieldMarkerFlags`; every marker is
/// independent of area visit bits. A flag nibble of two shows its marker.
/// Coordinates are signed map pixels. Keep this overlay loaded while drawing.
extern MenuMapMarker gMapDryfieldMarkers[MAP_DRYFIELD_MARKER_COUNT + 1];

/// Fixed and objective icons on the daytime Dryfield map.
///
/// Ten records and a page-zero terminator; areas and pages are one-based.
/// Fixed icons require a visited area; their zero condition adds no flag gate.
/// Objective icons compare their condition with the current objective byte.
/// Coordinates are signed map pixels. Keep this overlay loaded while drawing.
extern MenuMapIcon gMapDryfieldIcons[11];

/// Area names displayed by the daytime Dryfield map.
///
/// Indexed by area ID minus one (0..37), without a terminator. Each record is
/// a NUL-terminated ASCII line in 32 bytes; unnamed areas have empty strings.
/// Keep this overlay loaded while measuring or drawing the text.
extern MenuMapAreaName gMapDryfieldAreaNames[GAME_AREA_DRYFIELD_UNDERPASS];

/// Placed-object lists and spawn recipes for daytime Dryfield.
///
/// Indexed directly by area ID (0..38); areas without objects have two NULL
/// pointers. Entry 39 stores `AREA_OBJECT_ROOM_END` for flag-seeding walks.
/// Place lists borrow gameplay storage; spawn recipes belong to this map.
/// Only Junk Yard's Wire Rope and Trailer Coach's M950 container lid have
/// model-backed recipes. Keep the map and selected room geometry loaded while
/// their spawned tasks use them.
extern AreaObjectRoom gMapDryfieldObjectRooms[GAME_AREA_DRYFIELD_UNDERPASS + 2];

/// Default room-task recipes for the 31 populated daytime Dryfield areas.
///
/// Each data value is `GP_TASK_LOC_KEY(stage, area, 0)`, so every room of that
/// area shares its callback. Tasks have no body and priority 32. The final
/// `TASK_DESC_END` entry closes the search; keep this overlay and the selected
/// room callback's overlay loaded through spawning and task execution.
extern TaskDesc gMapDryfieldRoomTaskDescriptors[32];

/// Packed flag and picture selectors for daytime Dryfield map markers.
///
/// Indexed in parallel with `gMapDryfieldMarkers` (0..28), without a
/// terminator. Low eleven bits select a game-flag nibble; bit eleven selects
/// the alternate picture and is clear in every entry. Keep this overlay
/// loaded during lookup.
extern u16 gMapDryfieldMarkerFlags[MAP_DRYFIELD_MARKER_COUNT];

/// Daytime Dryfield's 38-area directory of room-lighting descriptor arrays.
///
/// Indexed by area minus one, then room minus one within that area's own extent.
/// NULL areas have no room lighting. The map borrows room-owned records, which
/// remain valid only while the corresponding room overlay is loaded.
extern WorldCoordRoomLighting* gMapDryfieldRoomLightingTables[38];

/// Borrowed warp endpoint arrays for daytime Dryfield.
///
/// Select area - 1 for IDs 1..38, then warp - 1 within that area's own
/// endpoint extent; neither level has a terminator. Areas 10, 33, 35, 36, 37 are NULL and cannot be selected.
/// The map owns this directory. Keep it and the selected room overlay loaded
/// during lookup; endpoints are copied by value and are independent of room ID.
extern DirectionWarpEntry* gMapDryfieldAreaWarpEntries[GAME_AREA_DRYFIELD_UNDERPASS];

/// Logical-view search counts for daytime Dryfield areas 1..38.
///
/// Indexed by area - 1, then room - 1 in that area's borrowed room array.
/// Areas 10, 33 and 35..37 are NULL and cannot be selected. Counts are signed
/// halfword element counts, independent of view-map and camera/image/sprite
/// capacities. Neither level has a terminator. Require in-range populated
/// slots and keep this map and the selected room overlay loaded during lookup.
extern ViewCountTable gMapDryfieldViewCountTable;

/// Stage collision-resource directory for daytime Dryfield.
///
/// Select `areaRooms[area - 1][room - 1]` with area IDs 1..38 and a room
/// within that area's array. Areas 10, 33, 35, 36, 37 are NULL and bind no resources.
/// Neither level has a terminator. This map owns the directory and some
/// descriptors; selected room overlays own the mutable grids and linked
/// trigger/occluder storage, which must remain loaded until retired.
extern WorldCollisionStageResources gMapDryfieldWorldCollisionResources;

/// Daytime Dryfield's directory of camera arrays for areas 1..38.
///
/// Area and mapped-camera IDs are one-based; camera bounds come from each
/// room's array, independently of its logical-view count. Areas 10, 33 and
/// 35..37 have NULL entries. The map owns the directory and borrows room
/// cameras; keep this map and the selected room loaded through queued use.
extern ViewCameraTable gMapDryfieldViewCameraTable;

/// Daytime Dryfield's directory of logical-view maps for 38 areas.
///
/// Area, room and logical-view IDs are one-based with separate extents; NULL
/// areas have no maps. The map overlay owns the area directory and borrows
/// each room overlay's map directory, valid while that room is loaded.
extern ViewIndexTable gMapDryfieldViewIndexTable;

/// Daytime Dryfield's directory of mapped-view sprite arrays for 38 areas.
///
/// Contains one directory record; area and mapped-view IDs are one-based with
/// separate extents. Room-owned mutable sprite resources must remain loaded.
extern SpriteAreaTable gMapDryfieldSpriteAreaTable;

/// Per-area collision surface-class directories for daytime Dryfield.
///
/// Indexed by area ID minus one (0..37), then by surface class (0..7),
/// independently of room variant. Areas 10, 33 and 35..37 have NULL entries;
/// no level has a terminator. Lookups require a populated area slot. Each
/// nonempty entry borrows eight property pointers from its room overlay;
/// keep both this map and the selected room overlay loaded during use.
extern WorldCollisionSurfaceProperties** gMapDryfieldSurfacePropertiesByArea[GAME_AREA_DRYFIELD_UNDERPASS];

/// Empty actor-placement list for daytime Gas Station variant 1.
///
/// Contains only `AREA_PLACEMENT_END`; it spawns no actors.
/// This map-owned record must remain loaded while its room variant uses it.
extern AreaPlacement gMapDryfieldGasStationVariant1Placements[1];

/// Cutscene and regular Desert Chasers for daytime Main Street variant 1.
///
/// The regular actor starts approaching with its second tuning; the cutscene
/// actor starts at the origin.
///
/// World-coordinate XYZ and yaw (4096 units per turn), followed by
/// `AREA_PLACEMENT_END`. Entry keys select this variant's room resource recipes;
/// texture offsets count 64-word VRAM columns and CLUT rows. Base files and
/// parameter row zero are used. Actors borrow the records, so this map must
/// remain loaded for their lives; saved poses may replace initial transforms.
extern AreaPlacement gMapDryfieldMainStreetVariant1Placements[3];

/// Six buried ambush Scorpions for daytime Main Street variant 2.
///
/// The first two lead the ambush; four followers wait for its signal.
///
/// World-coordinate XYZ and yaw (4096 units per turn), followed by
/// `AREA_PLACEMENT_END`. Entry keys select this variant's room resource recipes;
/// texture offsets count 64-word VRAM columns and CLUT rows. Base files and
/// parameter row zero are used. Actors borrow the records, so this map must
/// remain loaded for their lives; saved poses may replace initial transforms.
extern AreaPlacement gMapDryfieldMainStreetVariant2Placements[7];

/// Two steering Desert Chasers for daytime Main Street variant 3.
///
/// Both use the second tuning row.
///
/// World-coordinate XYZ and yaw (4096 units per turn), followed by
/// `AREA_PLACEMENT_END`. Entry keys select this variant's room resource recipes;
/// texture offsets count 64-word VRAM columns and CLUT rows. Base files and
/// parameter row zero are used. Actors borrow the records, so this map must
/// remain loaded for their lives; saved poses may replace initial transforms.
extern AreaPlacement gMapDryfieldMainStreetVariant3Placements[3];

/// Beam Sword Pawn and Rook GOLEMs for daytime Main Street variant 7.
///
/// The Pawn patrols 6000 world units and the Rook patrols 4000.
///
/// World-coordinate XYZ and yaw (4096 units per turn), followed by
/// `AREA_PLACEMENT_END`. Entry keys select this variant's room resource recipes;
/// texture offsets count 64-word VRAM columns and CLUT rows. Base files and
/// parameter row zero are used. Actors borrow the records, so this map must
/// remain loaded for their lives; saved poses may replace initial transforms.
extern AreaPlacement gMapDryfieldMainStreetVariant7Placements[3];

/// Five Blood Sucklers for daytime General Store variant 1.
///
/// All five start patrolling.
///
/// World-coordinate XYZ and yaw (4096 units per turn), followed by
/// `AREA_PLACEMENT_END`. Entry keys select this variant's room resource recipes;
/// texture offsets count 64-word VRAM columns and CLUT rows. Base files and
/// parameter row zero are used. Actors borrow the records, so this map must
/// remain loaded for their lives; saved poses may replace initial transforms.
extern AreaPlacement gMapDryfieldGeneralStoreVariant1Placements[6];

/// Two steering Desert Chasers for daytime Back Street variant 1.
///
/// Both use the second tuning row and texture-page/CLUT-row shifts of +2/+4.
///
/// World-coordinate XYZ and yaw (4096 units per turn), followed by
/// `AREA_PLACEMENT_END`. Entry keys select this variant's room resource recipes;
/// texture offsets count 64-word VRAM columns and CLUT rows. Base files and
/// parameter row zero are used. Actors borrow the records, so this map must
/// remain loaded for their lives; saved poses may replace initial transforms.
extern AreaPlacement gMapDryfieldBackStreetDesertChaserPlacements[3];

/// Beam Sword and grenade-launcher Pawn GOLEMs for daytime Back Street variant 7.
///
/// The sword Pawn patrols 6000 world units; the launcher Pawn starts idle.
///
/// World-coordinate XYZ and yaw (4096 units per turn), followed by
/// `AREA_PLACEMENT_END`. Entry keys select this variant's room resource recipes;
/// texture offsets count 64-word VRAM columns and CLUT rows. Base files and
/// parameter row zero are used. Actors borrow the records, so this map must
/// remain loaded for their lives; saved poses may replace initial transforms.
extern AreaPlacement gMapDryfieldBackStreetPawnGolemPlacements[3];

/// One Mind Suckler and two Blood Sucklers for daytime Souvenir Shop variant 1.
///
/// The Mind Suckler starts on a wall with later wall settling disabled. Both
/// Blood Sucklers start idle.
///
/// World-coordinate XYZ and yaw (4096 units per turn), followed by
/// `AREA_PLACEMENT_END`. Entry keys select this variant's room resource recipes;
/// texture offsets count 64-word VRAM columns and CLUT rows. Base files and
/// parameter row zero are used. Actors borrow the records, so this map must
/// remain loaded for their lives; saved poses may replace initial transforms.
extern AreaPlacement gMapDryfieldSouvenirShopDefaultPlacements[4];

/// Aya double scene actor for daytime area R08 variant 1.
///
/// Entry 132 selects the room resource recipe for the Aya double scene task.
///
/// World-coordinate XYZ and yaw (4096 units per turn), followed by
/// `AREA_PLACEMENT_END`. Entry keys select this variant's room resource recipes;
/// texture offsets count 64-word VRAM columns and CLUT rows. Base files and
/// parameter row zero are used. Actors borrow the records, so this map must
/// remain loaded for their lives; saved poses may replace initial transforms.
extern AreaPlacement gMapDryfieldR08Variant1Placements[2];

/// No. 9 GOLEM and the ANMC woman for daytime Dilapidated House variant 1.
///
/// Entries 34 and 29 select the room-owned No. 9 and ANMC-woman recipes.
///
/// World-coordinate XYZ and yaw (4096 units per turn), followed by
/// `AREA_PLACEMENT_END`. Entry keys select this variant's room resource recipes;
/// texture offsets count 64-word VRAM columns and CLUT rows. Base files and
/// parameter row zero are used. Actors borrow the records, so this map must
/// remain loaded for their lives; saved poses may replace initial transforms.
extern AreaPlacement gMapDryfieldDilapidatedHouseVariant1Placements[3];

/// Empty actor-placement list for daytime Motel Room 1 variant 1.
///
/// Contains only `AREA_PLACEMENT_END`; it spawns no actors.
/// This map-owned record must remain loaded while its room variant uses it.
extern AreaPlacement gMapDryfieldMotelRoom1Variant1Placements[1];

/// Four Bone Sucklers and an Odd Stranger for daytime Motel Room 1 variant 2.
///
/// Three Bone Sucklers start idle and one patrols; the Odd Stranger selects
/// stalking with the second tuning.
///
/// World-coordinate XYZ and yaw (4096 units per turn), followed by
/// `AREA_PLACEMENT_END`. Entry keys select this variant's room resource recipes;
/// texture offsets count 64-word VRAM columns and CLUT rows. Base files and
/// parameter row zero are used. Actors borrow the records, so this map must
/// remain loaded for their lives; saved poses may replace initial transforms.
extern AreaPlacement gMapDryfieldMotelRoom1Variant2Placements[6];

/// Two cutscene and two regular Bone Sucklers for daytime Motel Room 1 variant 3.
///
/// The first two records deliberately share the same transform; all four remain
/// separate placement keys.
///
/// World-coordinate XYZ and yaw (4096 units per turn), followed by
/// `AREA_PLACEMENT_END`. Entry keys select this variant's room resource recipes;
/// texture offsets count 64-word VRAM columns and CLUT rows. Base files and
/// parameter row zero are used. Actors borrow the records, so this map must
/// remain loaded for their lives; saved poses may replace initial transforms.
extern AreaPlacement gMapDryfieldMotelRoom1Variant3Placements[5];

/// Four Blood Sucklers and an Odd Stranger for daytime Motel Room 1 variant 7.
///
/// Three Blood Sucklers start idle and one patrols; the Odd Stranger selects
/// stalking with the third tuning.
///
/// World-coordinate XYZ and yaw (4096 units per turn), followed by
/// `AREA_PLACEMENT_END`. Entry keys select this variant's room resource recipes;
/// texture offsets count 64-word VRAM columns and CLUT rows. Base files and
/// parameter row zero are used. Actors borrow the records, so this map must
/// remain loaded for their lives; saved poses may replace initial transforms.
extern AreaPlacement gMapDryfieldMotelRoom1Variant7Placements[6];

/// Two Odd Strangers for daytime Motel Room 2 variant 1.
///
/// Both select stalking with the second tuning.
///
/// World-coordinate XYZ and yaw (4096 units per turn), followed by
/// `AREA_PLACEMENT_END`. Entry keys select this variant's room resource recipes;
/// texture offsets count 64-word VRAM columns and CLUT rows. Base files and
/// parameter row zero are used. Actors borrow the records, so this map must
/// remain loaded for their lives; saved poses may replace initial transforms.
extern AreaPlacement gMapDryfieldMotelRoom2Variant1Placements[3];

/// Two Mind Sucklers and four Bone Sucklers for daytime Motel Room 2 variant 2.
///
/// The Mind Sucklers use ceiling and wall perches with later wall settling
/// disabled; all Bone Sucklers start idle.
///
/// World-coordinate XYZ and yaw (4096 units per turn), followed by
/// `AREA_PLACEMENT_END`. Entry keys select this variant's room resource recipes;
/// texture offsets count 64-word VRAM columns and CLUT rows. Base files and
/// parameter row zero are used. Actors borrow the records, so this map must
/// remain loaded for their lives; saved poses may replace initial transforms.
extern AreaPlacement gMapDryfieldMotelRoom2Variant2Placements[7];

/// Two Mind Sucklers and four Blood Sucklers for daytime Motel Room 2 variant 7.
///
/// The Mind Sucklers use ceiling and wall perches with later wall settling
/// disabled; all Blood Sucklers start idle.
///
/// World-coordinate XYZ and yaw (4096 units per turn), followed by
/// `AREA_PLACEMENT_END`. Entry keys select this variant's room resource recipes;
/// texture offsets count 64-word VRAM columns and CLUT rows. Base files and
/// parameter row zero are used. Actors borrow the records, so this map must
/// remain loaded for their lives; saved poses may replace initial transforms.
extern AreaPlacement gMapDryfieldMotelRoom2Variant7Placements[7];

/// Empty actor-placement list for daytime Parking Lot variant 1.
///
/// Contains only `AREA_PLACEMENT_END`; it spawns no actors.
/// This map-owned record must remain loaded while its room variant uses it.
extern AreaPlacement gMapDryfieldParkingLotVariant1Placements[1];

/// Two steering Desert Chasers for daytime Parking Lot variant 2.
///
/// Both use the first tuning row.
///
/// World-coordinate XYZ and yaw (4096 units per turn), followed by
/// `AREA_PLACEMENT_END`. Entry keys select this variant's room resource recipes;
/// texture offsets count 64-word VRAM columns and CLUT rows. Base files and
/// parameter row zero are used. Actors borrow the records, so this map must
/// remain loaded for their lives; saved poses may replace initial transforms.
extern AreaPlacement gMapDryfieldParkingLotVariant2Placements[3];

/// Six buried ambush Scorpions and a Desert Chaser for daytime Parking Lot variant 3.
///
/// All six Scorpions wait for the ambush signal; the Chaser starts approaching
/// with its second tuning.
///
/// World-coordinate XYZ and yaw (4096 units per turn), followed by
/// `AREA_PLACEMENT_END`. Entry keys select this variant's room resource recipes;
/// texture offsets count 64-word VRAM columns and CLUT rows. Base files and
/// parameter row zero are used. Actors borrow the records, so this map must
/// remain loaded for their lives; saved poses may replace initial transforms.
extern AreaPlacement gMapDryfieldParkingLotVariant3Placements[8];

/// One GOLEM Knight for daytime Parking Lot variant 7.
///
/// World-coordinate XYZ and yaw (4096 units per turn), followed by
/// `AREA_PLACEMENT_END`. Entry keys select this variant's room resource recipes;
/// texture offsets count 64-word VRAM columns and CLUT rows. Base files and
/// parameter row zero are used. Actors borrow the records, so this map must
/// remain loaded for their lives; saved poses may replace initial transforms.
extern AreaPlacement gMapDryfieldParkingLotVariant7Placements[2];

/// Woman Stranger and Odd Stranger encounter placements for daytime Toilet variant 1.
///
/// The Odd Stranger selects stalking with its second tuning; the Woman Stranger
/// starts at the origin.
///
/// World-coordinate XYZ and yaw (4096 units per turn), followed by
/// `AREA_PLACEMENT_END`. Entry keys select this variant's room resource recipes;
/// texture offsets count 64-word VRAM columns and CLUT rows. Base files and
/// parameter row zero are used. Actors borrow the records, so this map must
/// remain loaded for their lives; saved poses may replace initial transforms.
extern AreaPlacement gMapDryfieldToiletEncounterPlacements[3];

/// Seven idle Bone Sucklers for daytime Toilet variant 2.
///
/// World-coordinate XYZ and yaw (4096 units per turn), followed by
/// `AREA_PLACEMENT_END`. Entry keys select this variant's room resource recipes;
/// texture offsets count 64-word VRAM columns and CLUT rows. Base files and
/// parameter row zero are used. Actors borrow the records, so this map must
/// remain loaded for their lives; saved poses may replace initial transforms.
extern AreaPlacement gMapDryfieldToiletBoneSucklerPlacements[8];

/// Three Amoebae for daytime Toilet variant 7.
///
/// Entry 24 selects the stronger tuning (mode 1, 280 HP); row 0 and
/// texture offsets page 0 / CLUT +2 apply to all three.
/// World poses and texture relocations use the `AreaPlacement` contract.
/// No additional files are requested. The final record is `AREA_PLACEMENT_END`;
/// keep this map overlay loaded while actors borrow the list.
extern AreaPlacement gMapDryfieldToiletAmoebaPlacements[4];

/// Five Mind Sucklers for daytime G & R Saloon variant 1.
///
/// Entry 15 starts on ceiling perches, with later wall settling disabled
/// (mode 3); texture offsets are page 0 / CLUT +2.
/// World poses and texture relocations use the `AreaPlacement` contract.
/// No additional files are requested. The final record is `AREA_PLACEMENT_END`;
/// keep this map overlay loaded while actors borrow the list.
extern AreaPlacement gMapDryfieldSaloonGRVariant1Placements[6];

/// Two Mind Sucklers and five Bone Sucklers for daytime G & R Saloon variant 2.
///
/// Mind Sucklers start on ceiling perches without later wall settling
/// (mode 3, page 0 / CLUT +2). Bone Sucklers start idle (variant 1),
/// using page +2 / CLUT +4.
/// World poses and texture relocations use the `AreaPlacement` contract.
/// No additional files are requested. The final record is `AREA_PLACEMENT_END`;
/// keep this map overlay loaded while actors borrow the list.
extern AreaPlacement gMapDryfieldSaloonGRVariant2Placements[8];

/// Three Mind Sucklers for daytime G & R Kitchen variant 1.
///
/// The first two start on ceiling perches (mode 3), the last on a wall
/// (mode 2); all disable later wall settling and use page 0 / CLUT +2.
/// World poses and texture relocations use the `AreaPlacement` contract.
/// No additional files are requested. The final record is `AREA_PLACEMENT_END`;
/// keep this map overlay loaded while actors borrow the list.
extern AreaPlacement gMapDryfieldGRKitchenVariant1Placements[4];

/// Two Mind Sucklers and five Rats for daytime G & R Kitchen variant 2.
///
/// Mind Sucklers start on walls without later wall settling (mode 2,
/// page 0 / CLUT +2). Rats use behavior row 2 and page +2 / CLUT +4.
/// World poses and texture relocations use the `AreaPlacement` contract.
/// No additional files are requested. The final record is `AREA_PLACEMENT_END`;
/// keep this map overlay loaded while actors borrow the list.
extern AreaPlacement gMapDryfieldGRKitchenVariant2Placements[8];

/// Two hidden Desert Chasers for daytime Water Tower variant 1.
///
/// Entry 1 selects the Water Tower chaser; variant 1 starts hidden and
/// mode 0 selects tuning row 1. Texture offsets are page 0 / CLUT +2.
/// World poses and texture relocations use the `AreaPlacement` contract.
/// No additional files are requested. The final record is `AREA_PLACEMENT_END`;
/// keep this map overlay loaded while actors borrow the list.
extern AreaPlacement gMapDryfieldWaterTowerVariant1Placements[3];

/// Seven Scorpions for daytime Water Tower variant 2.
///
/// The first four wait for the ambush signal (mode 2); the last three
/// roam (mode 0). All use row 0 and page 0 / CLUT +2.
/// World poses and texture relocations use the `AreaPlacement` contract.
/// No additional files are requested. The final record is `AREA_PLACEMENT_END`;
/// keep this map overlay loaded while actors borrow the list.
extern AreaPlacement gMapDryfieldWaterTowerVariant2Placements[8];

/// Five ambush-leading Scorpions for daytime Water Tower variant 3.
///
/// Mode 1 waits for the player to approach, then signals the room pack.
/// All use row 0 and page 0 / CLUT +2.
/// World poses and texture relocations use the `AreaPlacement` contract.
/// No additional files are requested. The final record is `AREA_PLACEMENT_END`;
/// keep this map overlay loaded while actors borrow the list.
extern AreaPlacement gMapDryfieldWaterTowerVariant3Placements[6];

/// Kyle Madigan placement for daytime Water Tank variant 1.
///
/// Entry 140 selects Kyle; the initial world pose is (2531, -12005, -644),
/// yaw 0, with page 0 / CLUT +2. His scene can replace this pose.
/// World poses and texture relocations use the `AreaPlacement` contract.
/// No additional files are requested. The final record is `AREA_PLACEMENT_END`;
/// keep this map overlay loaded while actors borrow the list.
extern AreaPlacement gMapDryfieldWaterTankVariant1Placements[2];

/// Empty placement list for daytime Water Tank variant 2.
///
/// Contains only `AREA_PLACEMENT_END`, so the variant is present but
/// spawns no actors. Keep this map overlay loaded while the layout is used.
extern AreaPlacement gMapDryfieldWaterTankVariant2Placements[1];

/// Scripted and regular Desert Chasers for daytime Breezeway variant 1.
///
/// Entry 101 selects the scripted chaser, which binds itself to the view.
/// Entry 1 selects the regular approaching chaser with tuning row 0
/// (mode 2). Both records retain page 0 / CLUT +2.
/// World poses and texture relocations use the `AreaPlacement` contract.
/// No additional files are requested. The final record is `AREA_PLACEMENT_END`;
/// keep this map overlay loaded while actors borrow the list.
extern AreaPlacement gMapDryfieldBreezewayVariant1Placements[3];

/// Seven Scorpions for daytime Breezeway variant 3.
///
/// The first six wait for an ambush signal (mode 2); the last roams
/// (mode 0). All use row 0 and page 0 / CLUT +2.
/// World poses and texture relocations use the `AreaPlacement` contract.
/// No additional files are requested. The final record is `AREA_PLACEMENT_END`;
/// keep this map overlay loaded while actors borrow the list.
extern AreaPlacement gMapDryfieldBreezewayVariant3Placements[8];

/// Gary Douglas placement for daytime Garage variant 1.
///
/// Entry 106 selects Gary; page 0 / CLUT +2 is also found by his texture
/// lookup. The garage scene can replace the initial world pose.
/// World poses and texture relocations use the `AreaPlacement` contract.
/// No additional files are requested. The final record is `AREA_PLACEMENT_END`;
/// keep this map overlay loaded while actors borrow the list.
extern AreaPlacement gMapDryfieldGarageVariant1Placements[2];

/// Five Mind Sucklers for daytime Garage variant 2.
///
/// Two ceiling perches (mode 3) precede three wall perches (mode 2).
/// All disable later wall settling and use page 0 / CLUT +2.
/// World poses and texture relocations use the `AreaPlacement` contract.
/// No additional files are requested. The final record is `AREA_PLACEMENT_END`;
/// keep this map overlay loaded while actors borrow the list.
extern AreaPlacement gMapDryfieldGarageVariant2Placements[6];

/// Three Mind Sucklers for daytime Garage variant 3.
///
/// All start on ceiling perches, disable later wall settling (mode 3)
/// and use page 0 / CLUT +2.
/// World poses and texture relocations use the `AreaPlacement` contract.
/// No additional files are requested. The final record is `AREA_PLACEMENT_END`;
/// keep this map overlay loaded while actors borrow the list.
extern AreaPlacement gMapDryfieldGarageVariant3Placements[4];

/// Twenty-five wave-entry bats for daytime Driveway variant 1.
///
/// Modes 10..14 activate at wave counters 1..5; the mode-10 bat directs
/// the encounter. Parameter rows 0, 2, 1 and 3 select turn steps
/// 16, 24, 20 and 28. All use page 0 / CLUT +2. Placement order also
/// selects wave timing and the stored pose key; retain all 25 live records.
/// World poses and texture relocations use the `AreaPlacement` contract.
/// No additional files are requested. The final record is `AREA_PLACEMENT_END`;
/// keep this map overlay loaded while actors borrow the list.
extern AreaPlacement gMapDryfieldDrivewayVariant1Placements[26];

/// Empty placement list for daytime Driveway variant 2.
///
/// Contains only `AREA_PLACEMENT_END`, so the variant is present but
/// spawns no actors. Keep this map overlay loaded while the layout is used.
extern AreaPlacement gMapDryfieldDrivewayVariant2Placements[1];

/// Regular Desert Chaser placement for daytime Driveway variant 3.
///
/// Entry 1 starts the approaching chaser (variant 0), with tuning row 1
/// (mode 0) and texture offsets page 0 / CLUT +2.
/// World poses and texture relocations use the `AreaPlacement` contract.
/// No additional files are requested. The final record is `AREA_PLACEMENT_END`;
/// keep this map overlay loaded while actors borrow the list.
extern AreaPlacement gMapDryfieldDrivewayVariant3Placements[2];

/// Empty placement list for daytime Junk Yard variant 1.
///
/// Contains only `AREA_PLACEMENT_END`, so the variant is present but
/// spawns no actors. Keep this map overlay loaded while the layout is used.
extern AreaPlacement gMapDryfieldJunkYardVariant1Placements[1];

/// Four Scorpions for daytime Junk Yard variant 2.
///
/// Three wait for the ambush signal (mode 2); the last waits for the
/// player and signals them (mode 1). All use row 0 and page 0 / CLUT +2.
/// World poses and texture relocations use the `AreaPlacement` contract.
/// No additional files are requested. The final record is `AREA_PLACEMENT_END`;
/// keep this map overlay loaded while actors borrow the list.
extern AreaPlacement gMapDryfieldJunkYardVariant2Placements[5];

/// Gary Douglas placement for daytime Trailer Coach variant 1.
///
/// Entry 106 selects Gary; the initial world pose is (3810, 0, -2130),
/// yaw 512, with page 0 / CLUT +2. Room scripts can replace it.
/// World poses and texture relocations use the `AreaPlacement` contract.
/// No additional files are requested. The final record is `AREA_PLACEMENT_END`;
/// keep this map overlay loaded while actors borrow the list.
extern AreaPlacement gMapDryfieldTrailerCoachGaryPlacements[2];

/// Empty placement list for daytime Motel Balcony variant 1.
///
/// Contains only `AREA_PLACEMENT_END`, so the variant is present but
/// spawns no actors. Keep this map overlay loaded while the layout is used.
extern AreaPlacement gMapDryfieldMotelBalconyVariant1Placements[1];

/// Three Odd Strangers for daytime Motel Balcony variant 2.
///
/// Variant 0x10 selects stalking; mode 0 selects tuning row 1.
/// All use page 0 / CLUT +2.
/// World poses and texture relocations use the `AreaPlacement` contract.
/// No additional files are requested. The final record is `AREA_PLACEMENT_END`;
/// keep this map overlay loaded while actors borrow the list.
extern AreaPlacement gMapDryfieldMotelBalconyVariant2Placements[4];

/// Six roaming Scorpions for daytime Motel Balcony variant 3.
///
/// Mode 0 selects roaming; all use row 0 and page 0 / CLUT +2.
/// World poses and texture relocations use the `AreaPlacement` contract.
/// No additional files are requested. The final record is `AREA_PLACEMENT_END`;
/// keep this map overlay loaded while actors borrow the list.
extern AreaPlacement gMapDryfieldMotelBalconyVariant3Placements[7];

/// Two Brain Stingers for daytime Motel Balcony variant 7.
///
/// Modes 0 and 1 select the two four-point balcony patrol routes.
/// Variant 0 permits targeting; both use page 0 / CLUT +2.
/// World poses and texture relocations use the `AreaPlacement` contract.
/// No additional files are requested. The final record is `AREA_PLACEMENT_END`;
/// keep this map overlay loaded while actors borrow the list.
extern AreaPlacement gMapDryfieldMotelBalconyVariant7Placements[3];

/// Pawn and Rook GOLEMs for daytime Motel Balcony variant 8.
///
/// Entry 56 starts the launcher Pawn idle (page 0 / CLUT +2). Entry 57
/// starts the Rook patrolling (mode 1) for 6000 world units (variant 6),
/// using page +3 / CLUT +5.
/// World poses and texture relocations use the `AreaPlacement` contract.
/// No additional files are requested. The final record is `AREA_PLACEMENT_END`;
/// keep this map overlay loaded while actors borrow the list.
extern AreaPlacement gMapDryfieldMotelBalconyVariant8Placements[3];

/// Scene placeholder placement for daytime Motel Room 6 variant 1.
///
/// Entry 101 selects the bodyless scene placeholder, which immediately
/// releases itself. The separately spawned Kyle body finds this record
/// for its page 0 / CLUT +2 textures; the placeholder's zero pose is retained.
/// World poses and texture relocations use the `AreaPlacement` contract.
/// No additional files are requested. The final record is `AREA_PLACEMENT_END`;
/// keep this map overlay loaded while actors borrow the list.
extern AreaPlacement gMapDryfieldMotelRoom6Variant1Placements[2];

/// Empty placement list for daytime Motel Room 6 variant 2.
///
/// Contains only `AREA_PLACEMENT_END`, so the variant is present but
/// spawns no actors. Keep this map overlay loaded while the layout is used.
extern AreaPlacement gMapDryfieldMotelRoom6Variant2Placements[1];

/// Eight perched bats for daytime Water Hole variant 1.
///
/// The first four take off by dropping (mode 1), the last four by
/// pushing backward (mode 2). Row 2 selects turn step 24; all use
/// page 0 / CLUT +2.
/// World poses and texture relocations use the `AreaPlacement` contract.
/// No additional files are requested. The final record is `AREA_PLACEMENT_END`;
/// keep this map overlay loaded while actors borrow the list.
extern AreaPlacement gMapDryfieldWaterHoleBatPlacements[9];

/// Eight bats and two Rats for daytime Cellar variant 1.
///
/// Six bats take off by dropping (mode 1), two by pushing backward
/// (mode 2); row 2 selects turn step 24, with page 0 / CLUT +2.
/// Rats use behavior row 2 and page +2 / CLUT +4.
/// World poses and texture relocations use the `AreaPlacement` contract.
/// No additional files are requested. The final record is `AREA_PLACEMENT_END`;
/// keep this map overlay loaded while actors borrow the list.
extern AreaPlacement gMapDryfieldCellarVariant1Placements[11];

/// Gray Stalker placement for daytime Underpass variant 1.
///
/// Resource entry 5 selects the room's Gray Stalker recipe, with spawn argument
/// and parameter row zero, base-file index zero, no texture-page relocation
/// and CLUT-row relocation +2. XYZ uses world units and yaw uses 4096 units
/// per turn; saved poses and the actor's arrival selector can replace it.
/// Followed by `AREA_PLACEMENT_END`. Keep this map loaded while the actor
/// borrows the record.
extern AreaPlacement gMapDryfieldUnderpassGrayStalkerPlacements[2];

/// Daytime Dryfield battle rewards for normal, replay and Scavenger runs.
///
/// Twelve rows keyed by stage, area and placement variant, followed by
/// `INVENTORY_BATTLE_REWARD_LIST_END`. Each nonzero slot grants one item pack;
/// slot 3 additionally requires an attached Medicine Wheel. Keep this overlay
/// loaded while the battle-result task searches the list.
extern InventoryBattleReward gMapDryfieldBattleRewards[13];

/// Daytime Dryfield battle rewards for Bounty and Nightmare runs.
///
/// Seven rows keyed by stage, area and placement variant, followed by
/// `INVENTORY_BATTLE_REWARD_LIST_END`. Each nonzero slot grants one item pack;
/// slot 3 additionally requires an attached Medicine Wheel. Keep this overlay
/// loaded while the battle-result task searches the list.
extern InventoryBattleReward gMapDryfieldHardBattleRewards[8];

/// Places movie decode buffers for the daytime Dryfield Gas Station.
///
/// Borrows `location` only for its area ID and disables VRAM staging. Main must
/// first cache dimensions and reserve movie and actor-buffer-0 storage. With
/// P = width * height pixels, places decoded frames at workspace + 3P and + 5P
/// bytes, and VLC outputs at ring + 0x10000 bytes and the workspace base.
/// Each decoded frame occupies 2P bytes; all extents must fit their reserved,
/// word-aligned regions. Other areas retain the preceding buffer pointers.
/// Clears the session reuse fields for actor buffers 0/1; never allocates.
/// Storage remains borrowed by the decoder through playback.
void mapDryfieldSetupMovieBuffers(const GameLocationKey* location);

#endif // INCLUDE_MAPUI_MAP_DRYFIELD_H
