#ifndef INCLUDE_MAPUI_MAP_NEO_ARK_H
#define INCLUDE_MAPUI_MAP_NEO_ARK_H

#include "types.h"

#include "gameplay/area_flags.h"
#include "gameplay/areaplace.h"
#include "gameplay/battle_reward.h"
#include "gameplay/direction.h"
#include "gameplay/map.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/areas.h"
#include "main/gfx_types.h"
#include "main/session_types.h"
#include "main/sound_types.h"
#include "main/stage_types.h"
#include "main/task_types.h"

/// Highest area ID covered by the stage's menu-map and area-music tables.
enum { MAP_NEO_ARK_AREA_ID_LIMIT = GAME_AREA_NEO_ARK_SUBSTATION };

/// Picture pages used by the Shelter/Neo Ark menu map.
enum {
    MAP_NEO_ARK_PAGE_NEO_ARK    = 1,
    MAP_NEO_ARK_PAGE_SHELTER_B6 = 2,
    MAP_NEO_ARK_PAGE_SHELTER_1F = 3,
};

/// Scene columns per area in the flat music table; selection is clamped to 0..9.
enum { MAP_NEO_ARK_MUSIC_COLUMN_COUNT = 10 };

/// Captured-frame and auxiliary-memory regions for the Shelter/Neo Ark stage.
///
/// Indexed directly by area ID, 0..33; all slots are populated. Each extent
/// ends at the resident image workspace. Slot 0 repeats the MIST parking
/// region used by the other later stages; its stage-5 purpose is unproven.
/// Requires the `map_neo_ark` overlay to remain loaded.
extern GfxImageSlot gMapNeoArkImageSlotsByArea[34];

/// Movie-workspace byte reservations indexed directly by live-save area ID.
///
/// Stage 5 reserves 0x21C00 bytes for the Guardroom (area 6) and 0x25800 for
/// the EVE elevator (area 9); zero entries request no allocation. The stored
/// table covers IDs 0..20, not all 33 areas. The CD playback lookup requires
/// a matching movie slot and an in-range area before indexing this table.
extern s32 gMapNeoArkMovieWorkspaceBytesByArea[21];

/// Area music for Shelter/Neo Ark, in flat rows of ten scene columns.
///
/// Index is area ID * `MAP_NEO_ARK_MUSIC_COLUMN_COUNT` + column, covering
/// areas 0..33 and columns 0..9 without a terminator. Each row's column 0
/// also controls the ambient loop. Keep the map overlay loaded while music
/// tasks borrow their selected entries.
extern StageMusicEntry gMapNeoArkAreaMusicEntries[(MAP_NEO_ARK_AREA_ID_LIMIT + 1) * MAP_NEO_ARK_MUSIC_COLUMN_COUNT];

/// Scene-indexed countdown music for Shelter/Neo Ark.
///
/// The scene's music entry directly selects 0..19 without a bounds check.
/// Live sequence entries start immediately; 3..7 request no sequence and
/// 12..19 retain sequence 0 with the view-ready policy. There is no terminator.
/// Keep the map overlay loaded until the music task releases its selection.
extern StageMusicEntry gMapNeoArkCountdownMusicEntries[20];

/// Area-shading flags for the three Shelter/Neo Ark map pages.
///
/// Indexed by page 1..3: Neo Ark uses nibble flag 0x19, Shelter B6 stores 0x80,
/// and Shelter 1F uses nibble flag 0x1A. Shading tests these as nibble IDs;
/// stage 5 disables page navigation, where 0x80 would mean unavailable.
/// Slot 0 is the boundary; slots 4..7 retain zero. Keep the map overlay loaded.
extern u8 gMapNeoArkPageFlagIds[8];

/// Area-to-map coordinate mappings for the Shelter/Neo Ark stage.
///
/// Indexed by area ID 0..33; slots 0, 26 and 31 use `MENU_MAP_AREA_PAGE_NONE`. Index 34
/// closes the table with `MENU_MAP_AREA_PAGE_END`. Maps world X/Z onto signed map pixels
/// using per-axis world units per pixel. Keep the map overlay loaded for use.
extern MenuMapArea gMapNeoArkAreas[MAP_NEO_ARK_AREA_ID_LIMIT + 2];

/// Area-fill shapes for the Shelter/Neo Ark menu map.
///
/// Slots 0..33 are indexed by area ID; slot 0 is empty. Slots 34 and 35
/// substitute the Island and Bridge models on page 1 while the altar switch
/// state is 1. Their stored page bytes (4, 5) are unused by the area walk.
/// Substation retains a no-page, no-model record despite its
/// coordinate mapping on page 1; the intent of that discrepancy is unproven.
/// The map overlay and the selected page's mappic overlay must stay loaded.
extern MenuMapAreaShape gMapNeoArkAreaShapes[MAP_NEO_ARK_AREA_ID_LIMIT + 3];

/// Number of parallel marker and packed-flag entries, excluding the marker terminator.
enum { MAP_NEO_ARK_MARKER_COUNT = 9 };

/// Flag-controlled map markers in signed pixels from the picture centre.
///
/// Entries 0..8 correspond to `gMapNeoArkMarkerFlags`; entry 0 is never drawn,
/// and entries 1..8 have no visited-area gate. The final page-zero record ends
/// the list and has no flag entry. Keep the map overlay loaded during use.
extern MenuMapMarker gMapNeoArkMarkers[MAP_NEO_ARK_MARKER_COUNT + 1];

/// Fixed-picture and telephone icons for the Shelter/Neo Ark map.
///
/// Heliport has a kind-0 fixed picture and a telephone; Nursery has a telephone.
/// All three require their area to have been visited, with no flag gate.
/// Coordinates are signed map pixels; the page-zero record ends the list.
/// The kind-0 picture's subject is unproven. Keep the map overlay loaded.
extern MenuMapIcon gMapNeoArkIcons[4];

/// Map-screen area labels for Shelter/Neo Ark area IDs 1..33.
///
/// Select area - 1; there is no terminator. Each record holds one NUL-terminated
/// line of large-font UI text. Area 31 retains a single space, and the Oval
/// Office label retains its authored byte sequence. Keep the map overlay loaded.
extern MenuMapAreaName gMapNeoArkAreaNames[MAP_NEO_ARK_AREA_ID_LIMIT];

/// Saved-object lists and spawn recipes for Shelter/Neo Ark areas 1..33.
///
/// Indexed directly by area ID: slot 0 is empty and slot 34 contains
/// `AREA_OBJECT_ROOM_END`. NULL lists denote areas without listed objects.
/// The map owns all place/spawn arrays; the vehicular-airlock recipe borrows
/// its room's model and callback while that room overlay is loaded. Flag
/// seeding walks the lists to initialize the stage's saved two-bit states.
extern AreaObjectRoom gMapNeoArkObjectRooms[35];

/// Eligible growth-room updates elapsed on the companion distress timeline.
///
/// Initialized to zero when this map overlay loads; retained across room and
/// actor reloads while the stage stays loaded. The distress task advances it
/// outside CAP playback, events and actor pauses, except during demo scene 11.
/// Companion distress starts at 780 updates and periodic player responses at
/// 1110. Conversation selection and the companion's normal behavior also
/// read it. This is signed update-count storage, not saved game state.
extern s32 gMapNeoArkCompanionDistressTicks;

/// Room-task spawn recipes searched by location for the Shelter/Neo Ark stage.
///
/// The 33 live records use `OBJECT_TASK_ROOM_LOCATION_KEY(stage, area, 0)`, supplying an area
/// default for every room. Records are searched in stored order, not indexed
/// by area ID. Tasks have no body and priority `TASK_PRIORITY_SCENE`; the final `TASK_DESC_END` record
/// is never spawned. Keep the map and selected room overlays loaded through
/// selection; the room's callback must remain loaded throughout its task.
extern TaskDesc gMapNeoArkRoomTaskDescriptors[MAP_NEO_ARK_AREA_ID_LIMIT + 1];

/// Packed game-flag selectors parallel to the nine Shelter/Neo Ark markers.
///
/// Low 11 bits select a flag nibble; `MENU_MAP_MARKER_ALTERNATE_PICTURE` selects
/// the second picture. State 2 draws the marker. No terminator is stored, and
/// marker indices must be 0..8. Keep the map overlay loaded during lookup.
extern u16 gMapNeoArkMarkerFlags[MAP_NEO_ARK_MARKER_COUNT];

/// Per-area room-lighting directories for the 33 Shelter/Neo Ark areas.
///
/// Select area - 1, then room - 1. Every area slot is populated; the nine
/// map-owned arrays cover room 1 only. Other descriptor arrays, light
/// collections and ambient tables are borrowed from the selected room
/// overlay and require it to remain loaded. The map directory stays loaded
/// for the stage; there is no count header or terminator.
extern WorldCoordRoomLighting* gMapNeoArkRoomLightingTables[33];

/// Borrowed warp endpoint arrays for Shelter/Neo Ark.
///
/// Select area - 1 for IDs 1..33, then warp - 1 within that area's own
/// endpoint extent; neither level has a terminator. Every area entry is populated.
/// The map owns this directory. Keep it and the selected room overlay loaded
/// during lookup; endpoints are copied by value and are independent of room ID.
extern DirectionWarpEntry* gMapNeoArkAreaWarpEntries[MAP_NEO_ARK_AREA_ID_LIMIT];

/// Directory of borrowed room view-count arrays for Shelter/Neo Ark areas 1..33.
///
/// Select `viewCounts[area - 1][room - 1]`; every area entry is populated.
/// Counts are signed logical-view search lengths, independent of camera/image
/// capacity. Neither level has a terminator. Keep the map directory and the
/// selected room's count array loaded, and use an in-range 1-based room ID.
extern ViewCountTable gMapNeoArkViewCountTable;

/// Stage collision-resource directory for Shelter/Neo Ark.
///
/// Select `areaRooms[area - 1][room - 1]` with area IDs 1..33 and a room
/// within that area's array. Every area entry is populated.
/// Neither level has a terminator. This map owns the directory and some
/// descriptors; selected room overlays own the mutable grids and linked
/// trigger/occluder storage, which must remain loaded until retired.
extern WorldCollisionStageResources gMapNeoArkWorldCollisionResources;

/// Mapped-view cameras for the 33 Shelter/Neo Ark areas (stage 5).
///
/// Select area - 1, then mapped camera index - 1. Every area slot is populated;
/// the selected room's camera-array extent bounds the mapped index. Neither
/// level has a count header or terminator. The map owns the directory and
/// borrows room camera arrays; keep both overlays loaded during use.
extern ViewCameraTable gMapNeoArkViewCameraTable;

/// Logical-view mapping directory for the 33 Shelter/Neo Ark areas.
///
/// Select area - 1, room - 1 and logical view - 1; the byte is a 1-based
/// camera/image/sprite index. Every area slot is populated. Borrows mutable
/// room directories and maps (including gameplay's identity map); room-owned
/// storage must remain loaded. Map capacities and reverse-search view counts
/// are separate, and none of these directories has a terminator.
extern ViewIndexTable gMapNeoArkViewIndexTable;

/// Sprite-view directory for the Shelter/Neo Ark stage (stage 5).
///
/// Select area - 1 for IDs 1..33, then the mapped one-based view minus one.
/// Every area slot is populated; there is one directory record and no sprite
/// variant slots, counts or terminators. The map owns the pointer directory
/// and borrows mutable view/source/batch resources from the selected room.
/// Keep both overlays loaded while allocating, updating or drawing sprites.
extern SpriteAreaTable gMapNeoArkSpriteAreaTable;

/// Collision and footstep property directories for the 33 Shelter/Neo Ark areas.
///
/// Select area - 1, then surface class 0..7; room variants share that area's
/// class table. Every area supplies eight non-NULL class records. The map
/// owns the directory and borrows tables, records and sound cues from the
/// selected room overlay; both overlays must stay loaded. There is no
/// count header or terminator in the eight-class lookup domain.
extern WorldCollisionSurfaceProperties** gMapNeoArkSurfacePropertiesByArea[33];

/// Terminator-only actor placements for Parking Garage variant 1.
///
/// Paired with that room's empty resource list. Spawns no actors and remains
/// valid while the `map_neo_ark` overlay is loaded.
extern AreaPlacement gMapNeoArkParkingGarageDefaultPlacements[1];

/// Initial Pawn/Rook GOLEM placements for Vehicular Airlock variant 1.
///
/// Resource IDs 20 and 57 select the room's beam-sword Pawn and grenade-launcher
/// Rook recipes. World XYZ and yaw (4096 units per turn) accompany texture-page
/// and CLUT-row offsets. Ends with `AREA_PLACEMENT_END`; actor references require
/// this map overlay to remain loaded.
extern AreaPlacement gMapNeoArkVehicularAirlockDefaultPlacements[3];

/// Initial two-Rook GOLEM placements for Bulwark variant 1.
///
/// Resource IDs 23 and 57 select the room's beam-sword and grenade-launcher
/// recipes. World XYZ and yaw (4096 units per turn) accompany texture-page and
/// CLUT-row offsets. Ends with `AREA_PLACEMENT_END`; actor references require
/// this map overlay to remain loaded.
extern AreaPlacement gMapNeoArkBulwarkDefaultPlacements[3];

/// Jodie and three Soldier B placements for Heliport variant 1.
///
/// Two soldiers carry rifles (mode 1); the last is unarmed (mode 0).
/// Ends with `AREA_PLACEMENT_END`; record order supplies saved placement indices.
/// XYZ use world units and yaw uses 4096 units per turn. Signed texture-page
/// offsets count 64-word VRAM columns and CLUT offsets count rows. All records
/// select base files (fileIdLow 0). Keep the map overlay loaded while loading,
/// spawning or retaining actor references.
extern AreaPlacement gMapNeoArkHeliportVariant1Placements[5];

/// Rupert and three Soldier B placements for Heliport variant 2.
///
/// Two soldiers carry rifles (mode 1); the last is unarmed (mode 0).
/// Ends with `AREA_PLACEMENT_END`; record order supplies saved placement indices.
/// XYZ use world units and yaw uses 4096 units per turn. Signed texture-page
/// offsets count 64-word VRAM columns and CLUT offsets count rows. All records
/// select base files (fileIdLow 0). Keep the map overlay loaded while loading,
/// spawning or retaining actor references.
extern AreaPlacement gMapNeoArkHeliportVariant2Placements[5];

/// One Knight GOLEM for Airlock placement variant 1.
///
/// Ends with `AREA_PLACEMENT_END`; record order supplies saved placement indices.
/// XYZ use world units and yaw uses 4096 units per turn. Signed texture-page
/// offsets count 64-word VRAM columns and CLUT offsets count rows. All records
/// select base files (fileIdLow 0). Keep the map overlay loaded while loading,
/// spawning or retaining actor references.
extern AreaPlacement gMapNeoArkAirlockKnightPlacements[2];

/// One Bishop GOLEM for Airlock placement variant 7.
///
/// Ends with `AREA_PLACEMENT_END`; record order supplies saved placement indices.
/// XYZ use world units and yaw uses 4096 units per turn. Signed texture-page
/// offsets count 64-word VRAM columns and CLUT offsets count rows. All records
/// select base files (fileIdLow 0). Keep the map overlay loaded while loading,
/// spawning or retaining actor references.
extern AreaPlacement gMapNeoArkAirlockBishopPlacements[2];

/// A bodyless scene-stop placement for Observatory variant 1.
///
/// Entry 101 selects a bodyless task that kills itself; it creates no model.
/// Ends with `AREA_PLACEMENT_END`. Keep the map overlay loaded during use.
extern AreaPlacement gMapNeoArkObservatoryDefaultPlacements[2];

/// Four Watchers for EVE Access Tunnel placement variant 1.
///
/// Weapon variants 2/3 select beam tuning; roll is zero. Sight row 6 gives 7000 world units.
/// Ends with `AREA_PLACEMENT_END`; record order supplies saved placement indices.
/// XYZ use world units and yaw uses 4096 units per turn. Signed texture-page
/// offsets count 64-word VRAM columns and CLUT offsets count rows. All records
/// select base files (fileIdLow 0). Keep the map overlay loaded while loading,
/// spawning or retaining actor references.
extern AreaPlacement gMapNeoArkEveAccessTunnelDefaultPlacements[5];

/// An encounter controller and five Watchers for EVE Access Tunnel variant 11.
///
/// Weapon variants 1/2/3 select beam tuning; roll is zero. Sight row 6 gives 7000 world units.
/// Ends with `AREA_PLACEMENT_END`; record order supplies saved placement indices.
/// XYZ use world units and yaw uses 4096 units per turn. Signed texture-page
/// offsets count 64-word VRAM columns and CLUT offsets count rows. All records
/// select base files (fileIdLow 0). Keep the map overlay loaded while loading,
/// spawning or retaining actor references.
extern AreaPlacement gMapNeoArkEveAccessTunnelEncounterPlacements[7];

/// Two Stingers on separate patrol routes for North Promenade variant 2.
///
/// Modes 0/1 select the room's two routes; variant 0 permits lock-on.
/// Ends with `AREA_PLACEMENT_END`; record order supplies saved placement indices.
/// XYZ use world units and yaw uses 4096 units per turn. Signed texture-page
/// offsets count 64-word VRAM columns and CLUT offsets count rows. All records
/// select base files (fileIdLow 0). Keep the map overlay loaded while loading,
/// spawning or retaining actor references.
extern AreaPlacement gMapNeoArkNorthPromenadeVariant2Placements[3];

/// An idle Pawn GOLEM and two patrolling Pawns for North Promenade variant 3.
///
/// The patrolling Pawns use 5000-world-unit legs (variant 5, mode 1).
/// Ends with `AREA_PLACEMENT_END`; record order supplies saved placement indices.
/// XYZ use world units and yaw uses 4096 units per turn. Signed texture-page
/// offsets count 64-word VRAM columns and CLUT offsets count rows. All records
/// select base files (fileIdLow 0). Keep the map overlay loaded while loading,
/// spawning or retaining actor references.
extern AreaPlacement gMapNeoArkNorthPromenadeVariant3Placements[4];

/// A patrolling Pawn GOLEM and an idle Pawn for North Promenade variant 11.
///
/// The first Pawn uses 4000-world-unit patrol legs (variant 4, mode 1).
/// Ends with `AREA_PLACEMENT_END`; record order supplies saved placement indices.
/// XYZ use world units and yaw uses 4096 units per turn. Signed texture-page
/// offsets count 64-word VRAM columns and CLUT offsets count rows. All records
/// select base files (fileIdLow 0). Keep the map overlay loaded while loading,
/// spawning or retaining actor references.
extern AreaPlacement gMapNeoArkNorthPromenadeVariant11Placements[3];

/// Two reusable Horned Stranger pool actors for Forest Zone variant 1.
///
/// Entry IDs 13/10 bind the room's two separate Horned Stranger packages.
/// Ends with `AREA_PLACEMENT_END`; record order supplies saved placement indices.
/// XYZ use world units and yaw uses 4096 units per turn. Signed texture-page
/// offsets count 64-word VRAM columns and CLUT offsets count rows. All records
/// select base files (fileIdLow 0). Keep the map overlay loaded while loading,
/// spawning or retaining actor references.
extern AreaPlacement gMapNeoArkForestZoneVariant1Placements[3];

/// Two reusable Horned Stranger pool actors for Forest Zone variant 2.
///
/// Ends with `AREA_PLACEMENT_END`; record order supplies saved placement indices.
/// XYZ use world units and yaw uses 4096 units per turn. Signed texture-page
/// offsets count 64-word VRAM columns and CLUT offsets count rows. All records
/// select base files (fileIdLow 0). Keep the map overlay loaded while loading,
/// spawning or retaining actor references.
extern AreaPlacement gMapNeoArkForestZoneVariant2Placements[3];

/// Element at which Forest Zone variant 4 begins in the shared placement stream.
enum { MAP_NEO_ARK_FOREST_ZONE_VARIANT4_PLACEMENT_START = 2 };

/// Shared placement stream for Forest Zone variants 3 and 4.
///
/// Variant 3 starts at element 0: two reusable Horned Strangers, followed by
/// ten entry-38 Beetle records that its resource table does not match.
/// Variant 4 starts at `MAP_NEO_ARK_FOREST_ZONE_VARIANT4_PLACEMENT_START`
/// and spawns those ten Beetles with its own zero-based placement indices.
/// Both walks end at element 12; the unmatched records still advance variant
/// 3's placement cursor. All fileIdLow values are zero, so they queue no
/// additional files. Preserve this order for saved placement keys.
/// XYZ use world units and yaw uses 4096 units per turn. Texture-page offsets
/// count 64-word VRAM columns and CLUT offsets count rows. Keep the map loaded
/// while loading, spawning or retaining actor references.
extern AreaPlacement gMapNeoArkForestZoneVariant3And4Placements[13];

/// A patrolling Pawn GOLEM and two wounded Horned Strangers for Forest Zone variant 5.
///
/// The Pawn patrols 2000-world-unit legs; Stranger variant 32 selects wounded behavior.
/// Ends with `AREA_PLACEMENT_END`; record order supplies saved placement indices.
/// XYZ use world units and yaw uses 4096 units per turn. Signed texture-page
/// offsets count 64-word VRAM columns and CLUT offsets count rows. All records
/// select base files (fileIdLow 0). Keep the map overlay loaded while loading,
/// spawning or retaining actor references.
extern AreaPlacement gMapNeoArkForestZoneVariant5Placements[4];

/// Two patrolling Pawn GOLEMs for Forest Zone placement variant 11.
///
/// Patrol legs are 5000 and 3000 world units (variants 5/3, mode 1).
/// Ends with `AREA_PLACEMENT_END`; record order supplies saved placement indices.
/// XYZ use world units and yaw uses 4096 units per turn. Signed texture-page
/// offsets count 64-word VRAM columns and CLUT offsets count rows. All records
/// select base files (fileIdLow 0). Keep the map overlay loaded while loading,
/// spawning or retaining actor references.
extern AreaPlacement gMapNeoArkForestZoneVariant11Placements[3];

/// Introduction and continuation Bog Divers for Submarine Tunnel variant 1.
///
/// Modes 2/50 select the introduction/continuation tunnel behavior.
/// Ends with `AREA_PLACEMENT_END`; record order supplies saved placement indices.
/// XYZ use world units and yaw uses 4096 units per turn. Signed texture-page
/// offsets count 64-word VRAM columns and CLUT offsets count rows. All records
/// select base files (fileIdLow 0). Keep the map overlay loaded while loading,
/// spawning or retaining actor references.
extern AreaPlacement gMapNeoArkSubmarineTunnelVariant1Placements[3];

/// Two alternate-palette Amoebas for Submarine Tunnel placement variant 2.
///
/// Mode bit 0 selects alternate combat parameters and adds one CLUT row.
/// Ends with `AREA_PLACEMENT_END`; record order supplies saved placement indices.
/// XYZ use world units and yaw uses 4096 units per turn. Signed texture-page
/// offsets count 64-word VRAM columns and CLUT offsets count rows. All records
/// select base files (fileIdLow 0). Keep the map overlay loaded while loading,
/// spawning or retaining actor references.
extern AreaPlacement gMapNeoArkSubmarineTunnelVariant2Placements[3];

/// Aya and No. 9 walkers for Submarine Tunnel placement variant 3.
///
/// Ends with `AREA_PLACEMENT_END`; record order supplies saved placement indices.
/// XYZ use world units and yaw uses 4096 units per turn. Signed texture-page
/// offsets count 64-word VRAM columns and CLUT offsets count rows. All records
/// select base files (fileIdLow 0). Keep the map overlay loaded while loading,
/// spawning or retaining actor references.
extern AreaPlacement gMapNeoArkSubmarineTunnelVariant3Placements[3];

/// Terminator-only actor placements for Submarine Tunnel variant 4.
///
/// Paired with an empty resource list; the present variant spawns no actors.
/// Ends with `AREA_PLACEMENT_END`. Keep the map overlay loaded during use.
extern AreaPlacement gMapNeoArkSubmarineTunnelVariant4Placements[1];

/// Three water-mode Bog Divers for Pavilion placement variant 1.
///
/// Mode bits 4..5 select waypoint rings 0/1/2; shot-speed row 1 selects 200 units per tick.
/// Ends with `AREA_PLACEMENT_END`; record order supplies saved placement indices.
/// XYZ use world units and yaw uses 4096 units per turn. Signed texture-page
/// offsets count 64-word VRAM columns and CLUT offsets count rows. All records
/// select base files (fileIdLow 0). Keep the map overlay loaded while loading,
/// spawning or retaining actor references.
extern AreaPlacement gMapNeoArkPavilionVariant1Placements[4];

/// Two water-mode Bog Divers and three Black Beetles for Pavilion variant 2.
///
/// Divers use waypoint rings 0/1 and shot-speed row 3 (220 units per tick).
/// Ends with `AREA_PLACEMENT_END`; record order supplies saved placement indices.
/// XYZ use world units and yaw uses 4096 units per turn. Signed texture-page
/// offsets count 64-word VRAM columns and CLUT offsets count rows. All records
/// select base files (fileIdLow 0). Keep the map overlay loaded while loading,
/// spawning or retaining actor references.
extern AreaPlacement gMapNeoArkPavilionVariant2Placements[6];

/// Three water-mode Bog Divers for Pavilion placement variant 3.
///
/// Divers use waypoint rings 0/1/2 and shot-speed row 3 (220 units per tick).
/// Ends with `AREA_PLACEMENT_END`; record order supplies saved placement indices.
/// XYZ use world units and yaw uses 4096 units per turn. Signed texture-page
/// offsets count 64-word VRAM columns and CLUT offsets count rows. All records
/// select base files (fileIdLow 0). Keep the map overlay loaded while loading,
/// spawning or retaining actor references.
extern AreaPlacement gMapNeoArkPavilionVariant3Placements[4];

/// A water-mode Bog Diver and a Mossback for Pavilion placement variant 4.
///
/// The Diver uses waypoint ring 0 and shot-speed row 6 (270 units per tick).
/// Ends with `AREA_PLACEMENT_END`; record order supplies saved placement indices.
/// XYZ use world units and yaw uses 4096 units per turn. Signed texture-page
/// offsets count 64-word VRAM columns and CLUT offsets count rows. All records
/// select base files (fileIdLow 0). Keep the map overlay loaded while loading,
/// spawning or retaining actor references.
extern AreaPlacement gMapNeoArkPavilionVariant4Placements[3];

/// One grounded and three floating wounded Bog Divers for Pavilion variant 5.
///
/// Ends with `AREA_PLACEMENT_END`; record order supplies saved placement indices.
/// XYZ use world units and yaw uses 4096 units per turn. Signed texture-page
/// offsets count 64-word VRAM columns and CLUT offsets count rows. All records
/// select base files (fileIdLow 0). Keep the map overlay loaded while loading,
/// spawning or retaining actor references.
extern AreaPlacement gMapNeoArkPavilionVariant5Placements[5];

/// A patrolling grenade-launcher Rook GOLEM for Pavilion variant 11.
///
/// Variant 3 and mode 1 select 3000-world-unit patrol legs.
/// Ends with `AREA_PLACEMENT_END`; record order supplies saved placement indices.
/// XYZ use world units and yaw uses 4096 units per turn. Signed texture-page
/// offsets count 64-word VRAM columns and CLUT offsets count rows. All records
/// select base files (fileIdLow 0). Keep the map overlay loaded while loading,
/// spawning or retaining actor references.
extern AreaPlacement gMapNeoArkPavilionVariant11Placements[2];

/// Three water-mode Bog Divers for Island placement variant 1.
///
/// Mode bits 4..5 select waypoint rings 0/1/2; shot-speed row 1 selects 200 units per tick.
/// Ends with `AREA_PLACEMENT_END`; record order supplies saved placement indices.
/// XYZ use world units and yaw uses 4096 units per turn. Signed texture-page
/// offsets count 64-word VRAM columns and CLUT offsets count rows. All records
/// select base files (fileIdLow 0). Keep the map overlay loaded while loading,
/// spawning or retaining actor references.
extern AreaPlacement gMapNeoArkIslandVariant1Placements[4];

/// Two Mossbacks for Island placement variant 2.
///
/// Ends with `AREA_PLACEMENT_END`; record order supplies saved placement indices.
/// XYZ use world units and yaw uses 4096 units per turn. Signed texture-page
/// offsets count 64-word VRAM columns and CLUT offsets count rows. All records
/// select base files (fileIdLow 0). Keep the map overlay loaded while loading,
/// spawning or retaining actor references.
extern AreaPlacement gMapNeoArkIslandVariant2Placements[3];

/// Two grounded and one floating wounded Bog Divers for Island variant 3.
///
/// Ends with `AREA_PLACEMENT_END`; record order supplies saved placement indices.
/// XYZ use world units and yaw uses 4096 units per turn. Signed texture-page
/// offsets count 64-word VRAM columns and CLUT offsets count rows. All records
/// select base files (fileIdLow 0). Keep the map overlay loaded while loading,
/// spawning or retaining actor references.
extern AreaPlacement gMapNeoArkIslandVariant3Placements[4];

/// Terminator-only actor placements for Island variant 4.
///
/// Paired with an empty resource list; the present variant spawns no actors.
/// Ends with `AREA_PLACEMENT_END`. Keep the map overlay loaded during use.
extern AreaPlacement gMapNeoArkIslandVariant4Placements[1];

/// One patrolling Rook GOLEM for Island placement variant 11.
///
/// Its patrol spans 8000 world units; the room recipe selects the grenade launcher.
/// Ends with `AREA_PLACEMENT_END`; transforms and texture offsets use
/// `AreaPlacement` units. Keep the map overlay loaded while actors borrow records.
extern AreaPlacement gMapNeoArkIslandVariant11Placements[2];

/// Ten Black Beetles for Garden placement variant 1.
///
/// Four start idle on the floor and six start wandering.
/// Ends with `AREA_PLACEMENT_END`; transforms and texture offsets use
/// `AreaPlacement` units. Keep the map overlay loaded while actors borrow records.
extern AreaPlacement gMapNeoArkGardenVariant1Placements[11];

/// One scripted footstep walker for Garden placement variant 2.
///
/// Entry 132 binds the room's walker recipe; the character identity is unproven.
/// Ends with `AREA_PLACEMENT_END`; transforms and texture offsets use
/// `AreaPlacement` units. Keep the map overlay loaded while actors borrow records.
extern AreaPlacement gMapNeoArkGardenVariant2Placements[2];

/// One Brute Mossback and six Black Beetles for Garden placement variant 3.
///
/// Two beetles start on walls; four start idle on the floor using a separate texture bank.
/// Ends with `AREA_PLACEMENT_END`; transforms and texture offsets use
/// `AreaPlacement` units. Keep the map overlay loaded while actors borrow records.
extern AreaPlacement gMapNeoArkGardenVariant3Placements[8];

/// Four Amoebae for Garden placement variant 4.
///
/// Each selects the actor's default mode.
/// Ends with `AREA_PLACEMENT_END`; transforms and texture offsets use
/// `AreaPlacement` units. Keep the map overlay loaded while actors borrow records.
extern AreaPlacement gMapNeoArkGardenVariant4Placements[5];

/// One Beta Generator and six Watchers for Power Plant 2 placement variant 1.
///
/// Watcher variants select weapons; sight rows select 5000- or 8000-unit acquisition ranges.
/// Ends with `AREA_PLACEMENT_END`; transforms and texture offsets use
/// `AreaPlacement` units. Keep the map overlay loaded while actors borrow records.
extern AreaPlacement gMapNeoArkPowerPlant2Variant1Placements[8];

/// Four Watchers and one Rook GOLEM for Power Plant 2 placement variant 11.
///
/// The Rook patrol spans 5000 world units; Watchers use a 5000-unit sight range.
/// Ends with `AREA_PLACEMENT_END`; transforms and texture offsets use
/// `AreaPlacement` units. Keep the map overlay loaded while actors borrow records.
extern AreaPlacement gMapNeoArkPowerPlant2Variant11Placements[6];

/// One Bishop GOLEM for Power Plant 2 placement variant 17.
///
/// The room binds its actor-slot-4 recipe.
/// Ends with `AREA_PLACEMENT_END`; transforms and texture offsets use
/// `AreaPlacement` units. Keep the map overlay loaded while actors borrow records.
extern AreaPlacement gMapNeoArkPowerPlant2Variant17Placements[2];

/// One Proto Generator and six Watchers for Power Plant 1 placement variant 1.
///
/// Watchers use 4500- or 8000-unit sight ranges; two select the gun weapon.
/// Ends with `AREA_PLACEMENT_END`; transforms and texture offsets use
/// `AreaPlacement` units. Keep the map overlay loaded while actors borrow records.
extern AreaPlacement gMapNeoArkPowerPlant1Variant1Placements[8];

/// Eight Black Beetles for Power Plant 1 placement variant 2.
///
/// One starts on a wall and seven start on the ceiling.
/// Ends with `AREA_PLACEMENT_END`; transforms and texture offsets use
/// `AreaPlacement` units. Keep the map overlay loaded while actors borrow records.
extern AreaPlacement gMapNeoArkPowerPlant1Variant2Placements[9];

/// Seven Black Beetles for Power Plant 1 placement variant 3.
///
/// Five start idle on the floor, one on a wall and one on the ceiling.
/// Ends with `AREA_PLACEMENT_END`; transforms and texture offsets use
/// `AreaPlacement` units. Keep the map overlay loaded while actors borrow records.
extern AreaPlacement gMapNeoArkPowerPlant1Variant3Placements[8];

/// Four Scorpions and five Black Beetles for Power Plant 1 placement variant 4.
///
/// Scorpions roam with movement row 3; idle floor beetles use a separate texture bank.
/// Ends with `AREA_PLACEMENT_END`; transforms and texture offsets use
/// `AreaPlacement` units. Keep the map overlay loaded while actors borrow records.
extern AreaPlacement gMapNeoArkPowerPlant1Variant4Placements[10];

/// Six ceiling Black Beetles for Power Plant 1 placement variant 11.
///
/// All select the room's actor-slot-1 beetle recipe.
/// Ends with `AREA_PLACEMENT_END`; transforms and texture offsets use
/// `AreaPlacement` units. Keep the map overlay loaded while actors borrow records.
extern AreaPlacement gMapNeoArkPowerPlant1Variant11Placements[7];

/// Two Desert Chasers and five Scorpions for Savanna Zone placement variant 1.
///
/// Chasers start steering with tuning row 2; Scorpions wait as ambush followers with movement row 3.
/// Ends with `AREA_PLACEMENT_END`; transforms and texture offsets use
/// `AreaPlacement` units. Keep the map overlay loaded while actors borrow records.
extern AreaPlacement gMapNeoArkSavannaZoneVariant1Placements[8];

/// Seven wandering floor Black Beetles for Savanna Zone placement variant 2.
///
/// All select the room's actor-slot-1 beetle recipe.
/// Ends with `AREA_PLACEMENT_END`; transforms and texture offsets use
/// `AreaPlacement` units. Keep the map overlay loaded while actors borrow records.
extern AreaPlacement gMapNeoArkSavannaZoneVariant2Placements[8];

/// One Desert Chaser and five Maggots for Savanna Zone placement variant 3.
///
/// Maggots use ambush-follower mode and movement row 1; the Chaser starts steering with tuning row 2.
/// Ends with `AREA_PLACEMENT_END`; transforms and texture offsets use
/// `AreaPlacement` units. Keep the map overlay loaded while actors borrow records.
extern AreaPlacement gMapNeoArkSavannaZoneVariant3Placements[7];

/// Five Scorpions and one Desert Chaser for Savanna Zone placement variant 4.
///
/// Four Scorpions wait for the ambush leader, all using movement row 0; the Chaser uses a separate texture bank.
/// Ends with `AREA_PLACEMENT_END`; transforms and texture offsets use
/// `AreaPlacement` units. Keep the map overlay loaded while actors borrow records.
extern AreaPlacement gMapNeoArkSavannaZoneVariant4Placements[7];

/// Two Pawn GOLEMs for Savanna Zone placement variant 5.
///
/// The first patrols 3000 world units; the second starts idle with the other actor-slot recipe.
/// Ends with `AREA_PLACEMENT_END`; transforms and texture offsets use
/// `AreaPlacement` units. Keep the map overlay loaded while actors borrow records.
extern AreaPlacement gMapNeoArkSavannaZoneVariant5Placements[3];

/// One Pawn and one Rook GOLEM for Savanna Zone placement variant 11.
///
/// The Pawn patrols 3000 world units; the Rook starts idle with a separate texture bank.
/// Ends with `AREA_PLACEMENT_END`; transforms and texture offsets use
/// `AreaPlacement` units. Keep the map overlay loaded while actors borrow records.
extern AreaPlacement gMapNeoArkSavannaZoneVariant11Placements[3];

/// Terminator-only placements for South Promenade placement variant 2.
///
/// Paired with an empty resource list; the present variant spawns no actors.
/// Ends with `AREA_PLACEMENT_END`; transforms and texture offsets use
/// `AreaPlacement` units. Keep the map overlay loaded while actors borrow records.
extern AreaPlacement gMapNeoArkSouthPromenadeVariant2Placements[1];

/// Terminator-only placements for South Promenade placement variant 3.
///
/// Paired with a distinct empty resource list; the present variant spawns no actors.
/// Ends with `AREA_PLACEMENT_END`; transforms and texture offsets use
/// `AreaPlacement` units. Keep the map overlay loaded while actors borrow records.
extern AreaPlacement gMapNeoArkSouthPromenadeVariant3Placements[1];

/// Six Black Beetles for South Promenade placement variant 4.
///
/// One starts idle on the floor, four on walls and one on the ceiling.
/// Ends with `AREA_PLACEMENT_END`; transforms and texture offsets use
/// `AreaPlacement` units. Keep the map overlay loaded while actors borrow records.
extern AreaPlacement gMapNeoArkSouthPromenadeVariant4Placements[7];

/// Two Pawn GOLEMs for South Promenade placement variant 5.
///
/// One patrols 5000 world units; the other starts idle.
/// Ends with `AREA_PLACEMENT_END`; transforms and texture offsets use
/// `AreaPlacement` units. Keep the map overlay loaded while actors borrow records.
extern AreaPlacement gMapNeoArkSouthPromenadeVariant5Placements[3];

/// Two Pawn GOLEMs and one Rook for South Promenade placement variant 11.
///
/// The beam-sword Pawns patrol 6000 and 4000 world units; the beam-sword Rook starts idle.
/// Ends with `AREA_PLACEMENT_END`; transforms and texture offsets use
/// `AreaPlacement` units. Keep the map overlay loaded while actors borrow records.
extern AreaPlacement gMapNeoArkSouthPromenadeVariant11Placements[4];

/// Three script-hidden Black Beetles for Shrine placement variant 1.
///
/// Their shrine mode waits for the room script to reveal them.
/// Ends with `AREA_PLACEMENT_END`; transforms and texture offsets use
/// `AreaPlacement` units. Keep the map overlay loaded while actors borrow records.
extern AreaPlacement gMapNeoArkShrineVariant1Placements[4];

/// Four roaming Scorpions for Shrine placement variant 2.
///
/// All use movement row 0 and initial yaw 0.
/// Ends with `AREA_PLACEMENT_END`; transforms and texture offsets use
/// `AreaPlacement` units. Keep the map overlay loaded while actors borrow records.
extern AreaPlacement gMapNeoArkShrineVariant2Placements[5];

/// Four roaming Scorpions for Shrine placement variant 3.
///
/// All use movement row 0 and initial yaw 3072 (three quarters of a turn).
/// Ends with `AREA_PLACEMENT_END`; transforms and texture offsets use
/// `AreaPlacement` units. Keep the map overlay loaded while actors borrow records.
extern AreaPlacement gMapNeoArkShrineVariant3Placements[5];

/// Four Scorpions and four Black Beetles for Shrine placement variant 4.
///
/// Scorpions roam with movement row 3; wandering floor beetles use a separate texture bank.
/// Ends with `AREA_PLACEMENT_END`; transforms and texture offsets use
/// `AreaPlacement` units. Keep the map overlay loaded while actors borrow records.
extern AreaPlacement gMapNeoArkShrineVariant4Placements[9];

/// Two beam-sword Pawn GOLEMs for Shrine placement variant 5.
///
/// One starts idle and one patrols 3000 world units.
/// Ends with `AREA_PLACEMENT_END`; transforms and texture offsets use
/// `AreaPlacement` units. Keep the map overlay loaded while actors borrow records.
extern AreaPlacement gMapNeoArkShrineVariant5Placements[3];

/// One Knight GOLEM for Shrine placement variant 7.
///
/// The room binds its actor-slot-4 recipe.
/// Ends with `AREA_PLACEMENT_END`; transforms and texture offsets use
/// `AreaPlacement` units. Keep the map overlay loaded while actors borrow records.
extern AreaPlacement gMapNeoArkShrineVariant7Placements[2];

/// One Bishop GOLEM for Shrine placement variant 11.
///
/// Ends with `AREA_PLACEMENT_END`. XYZ use world units, yaw uses 4096 units
/// per turn, and signed texture-page/CLUT-row offsets bind the room resources.
/// Spawn parameters remain actor-specific; the map overlay must outlive actors
/// borrowing these records.
extern AreaPlacement gMapNeoArkShrineVariant11Placements[2];

/// Kyle, two Pawn GOLEMs and the second scene body for Nursery variant 1.
///
/// Ends with `AREA_PLACEMENT_END`. XYZ use world units, yaw uses 4096 units
/// per turn, and signed texture-page/CLUT-row offsets bind the room resources.
/// Spawn parameters remain actor-specific; the map overlay must outlive actors
/// borrowing these records.
extern AreaPlacement gMapNeoArkNurseryDefaultPlacements[5];

/// Terminator-only actor placements for Growth Room variant 1.
///
/// The nonempty scene resource list is separate; this list spawns no actors.
/// The map overlay must remain loaded while the room borrows it.
extern AreaPlacement gMapNeoArkGrowthRoomDefaultPlacements[1];

/// Masked Eve, Mossback and Eve controller/body placements for Corridor variant 1.
///
/// Ends with `AREA_PLACEMENT_END`. XYZ use world units, yaw uses 4096 units
/// per turn, and signed texture-page/CLUT-row offsets bind the room resources.
/// Spawn parameters remain actor-specific; the map overlay must outlive actors
/// borrowing these records.
extern AreaPlacement gMapNeoArkCorridorDefaultPlacements[5];

/// Masked Eve, Kyle, Stinger and Eve controller/body placements for Training Room variant 1.
///
/// Ends with `AREA_PLACEMENT_END`. XYZ use world units, yaw uses 4096 units
/// per turn, and signed texture-page/CLUT-row offsets bind the room resources.
/// Spawn parameters remain actor-specific; the map overlay must outlive actors
/// borrowing these records.
extern AreaPlacement gMapNeoArkTrainingRoomDefaultPlacements[6];

/// Three water-mode Bog Divers for Bridge placement variant 1.
///
/// Ends with `AREA_PLACEMENT_END`. XYZ use world units, yaw uses 4096 units
/// per turn, and signed texture-page/CLUT-row offsets bind the room resources.
/// Spawn parameters remain actor-specific; the map overlay must outlive actors
/// borrowing these records.
extern AreaPlacement gMapNeoArkBridgeVariant1Placements[4];

/// Two water-mode Bog Divers and three Black Beetles for Bridge variant 2.
///
/// Ends with `AREA_PLACEMENT_END`. XYZ use world units, yaw uses 4096 units
/// per turn, and signed texture-page/CLUT-row offsets bind the room resources.
/// Spawn parameters remain actor-specific; the map overlay must outlive actors
/// borrowing these records.
extern AreaPlacement gMapNeoArkBridgeVariant2Placements[6];

/// A water-mode Bog Diver and a Mossback for Bridge variant 3.
///
/// Ends with `AREA_PLACEMENT_END`. XYZ use world units, yaw uses 4096 units
/// per turn, and signed texture-page/CLUT-row offsets bind the room resources.
/// Spawn parameters remain actor-specific; the map overlay must outlive actors
/// borrowing these records.
extern AreaPlacement gMapNeoArkBridgeVariant3Placements[3];

/// Two water-mode Bog Divers and a Mossback for Bridge variant 4.
///
/// Ends with `AREA_PLACEMENT_END`. XYZ use world units, yaw uses 4096 units
/// per turn, and signed texture-page/CLUT-row offsets bind the room resources.
/// Spawn parameters remain actor-specific; the map overlay must outlive actors
/// borrowing these records.
extern AreaPlacement gMapNeoArkBridgeVariant4Placements[4];

/// One grounded and two floating wounded Bog Divers for Bridge variant 5.
///
/// Ends with `AREA_PLACEMENT_END`. XYZ use world units, yaw uses 4096 units
/// per turn, and signed texture-page/CLUT-row offsets bind the room resources.
/// Spawn parameters remain actor-specific; the map overlay must outlive actors
/// borrowing these records.
extern AreaPlacement gMapNeoArkBridgeVariant5Placements[4];

/// Two beam-sword Rook GOLEMs for Bridge placement variant 11.
///
/// Patrol spans are 6000 and 8000 world units, encoded in variant 6/8.
/// Ends with `AREA_PLACEMENT_END`. XYZ use world units, yaw uses 4096 units
/// per turn, and signed texture-page/CLUT-row offsets bind the room resources.
/// Spawn parameters remain actor-specific; the map overlay must outlive actors
/// borrowing these records.
extern AreaPlacement gMapNeoArkBridgeVariant11Placements[3];

/// Two beam-sword Rook GOLEMs for Bridge placement variant 12.
///
/// Patrol spans are 6000 and 8000 world units, encoded in variant 6/8.
/// Ends with `AREA_PLACEMENT_END`. XYZ use world units, yaw uses 4096 units
/// per turn, and signed texture-page/CLUT-row offsets bind the room resources.
/// Spawn parameters remain actor-specific; the map overlay must outlive actors
/// borrowing these records.
extern AreaPlacement gMapNeoArkBridgeVariant12Placements[3];

/// Soldiers C, B and A for Tent placement variant 1.
///
/// Ends with `AREA_PLACEMENT_END`. XYZ use world units, yaw uses 4096 units
/// per turn, and signed texture-page/CLUT-row offsets bind the room resources.
/// Spawn parameters remain actor-specific; the map overlay must outlive actors
/// borrowing these records.
extern AreaPlacement gMapNeoArkTentDefaultPlacements[4];

/// Two reusable Horned Stranger pool actors for Woodland Path variant 1.
///
/// Ends with `AREA_PLACEMENT_END`. XYZ use world units, yaw uses 4096 units
/// per turn, and signed texture-page/CLUT-row offsets bind the room resources.
/// Spawn parameters remain actor-specific; the map overlay must outlive actors
/// borrowing these records.
extern AreaPlacement gMapNeoArkWoodlandPathVariant1Placements[3];

/// Two reusable Horned Stranger pool actors for Woodland Path variant 2.
///
/// Ends with `AREA_PLACEMENT_END`. XYZ use world units, yaw uses 4096 units
/// per turn, and signed texture-page/CLUT-row offsets bind the room resources.
/// Spawn parameters remain actor-specific; the map overlay must outlive actors
/// borrowing these records.
extern AreaPlacement gMapNeoArkWoodlandPathVariant2Placements[3];

/// Two reusable Horned Stranger pool actors for Woodland Path variant 3.
///
/// Ends with `AREA_PLACEMENT_END`. XYZ use world units, yaw uses 4096 units
/// per turn, and signed texture-page/CLUT-row offsets bind the room resources.
/// Spawn parameters remain actor-specific; the map overlay must outlive actors
/// borrowing these records.
extern AreaPlacement gMapNeoArkWoodlandPathVariant3Placements[3];

/// Four wounded Horned Strangers for Woodland Path placement variant 4.
///
/// Ends with `AREA_PLACEMENT_END`. XYZ use world units, yaw uses 4096 units
/// per turn, and signed texture-page/CLUT-row offsets bind the room resources.
/// Spawn parameters remain actor-specific; the map overlay must outlive actors
/// borrowing these records.
extern AreaPlacement gMapNeoArkWoodlandPathVariant4Placements[5];

/// Sea Diver placement and a suppressed Bog Diver for Submarine Gallery variant 1.
///
/// The Bog Diver record sets spawn-rejection bit 16 and creates no actor.
/// Ends with `AREA_PLACEMENT_END`. XYZ use world units, yaw uses 4096 units
/// per turn, and signed texture-page/CLUT-row offsets bind the room resources.
/// Spawn parameters remain actor-specific; the map overlay must outlive actors
/// borrowing these records.
extern AreaPlacement gMapNeoArkSubmarineGalleryVariant1Placements[3];

/// Three water-mode Bog Divers for Submarine Gallery placement variant 2.
///
/// Ends with `AREA_PLACEMENT_END`. XYZ use world units, yaw uses 4096 units
/// per turn, and signed texture-page/CLUT-row offsets bind the room resources.
/// Spawn parameters remain actor-specific; the map overlay must outlive actors
/// borrowing these records.
extern AreaPlacement gMapNeoArkSubmarineGalleryVariant2Placements[4];

/// A floating wounded Bog Diver for Submarine Gallery placement variant 3.
///
/// Ends with `AREA_PLACEMENT_END`. XYZ use world units, yaw uses 4096 units
/// per turn, and signed texture-page/CLUT-row offsets bind the room resources.
/// Spawn parameters remain actor-specific; the map overlay must outlive actors
/// borrowing these records.
extern AreaPlacement gMapNeoArkSubmarineGalleryVariant3Placements[2];

/// A water-mode Bog Diver for Submarine Gallery placement variant 11.
///
/// Ends with `AREA_PLACEMENT_END`. XYZ use world units, yaw uses 4096 units
/// per turn, and signed texture-page/CLUT-row offsets bind the room resources.
/// Spawn parameters remain actor-specific; the map overlay must outlive actors
/// borrowing these records.
extern AreaPlacement gMapNeoArkSubmarineGalleryVariant11Placements[2];

/// Kyle and Aya scene walkers for room 31 placement variant 1.
///
/// Ends with `AREA_PLACEMENT_END`. XYZ use world units, yaw uses 4096 units
/// per turn, and signed texture-page/CLUT-row offsets bind the room resources.
/// Spawn parameters remain actor-specific; the map overlay must outlive actors
/// borrowing these records.
extern AreaPlacement gMapNeoArkR31DefaultPlacements[3];

/// Nine floor/wall Black Beetles for Pyramid placement variant 1.
///
/// Ends with `AREA_PLACEMENT_END`. XYZ use world units, yaw uses 4096 units
/// per turn, and signed texture-page/CLUT-row offsets bind the room resources.
/// Spawn parameters remain actor-specific; the map overlay must outlive actors
/// borrowing these records.
extern AreaPlacement gMapNeoArkPyramidVariant1Placements[10];

/// Six ambush Maggots for Pyramid placement variant 2.
///
/// Ends with `AREA_PLACEMENT_END`. XYZ use world units, yaw uses 4096 units
/// per turn, and signed texture-page/CLUT-row offsets bind the room resources.
/// Spawn parameters remain actor-specific; the map overlay must outlive actors
/// borrowing these records.
extern AreaPlacement gMapNeoArkPyramidVariant2Placements[7];

/// Five Black Beetles and three ambush Maggots for Pyramid variant 3.
///
/// Ends with `AREA_PLACEMENT_END`. XYZ use world units, yaw uses 4096 units
/// per turn, and signed texture-page/CLUT-row offsets bind the room resources.
/// Spawn parameters remain actor-specific; the map overlay must outlive actors
/// borrowing these records.
extern AreaPlacement gMapNeoArkPyramidVariant3Placements[9];

/// Two Pawn GOLEMs and a wounded Horned Stranger for Pyramid variant 4.
///
/// Ends with `AREA_PLACEMENT_END`. XYZ use world units, yaw uses 4096 units
/// per turn, and signed texture-page/CLUT-row offsets bind the room resources.
/// Spawn parameters remain actor-specific; the map overlay must outlive actors
/// borrowing these records.
extern AreaPlacement gMapNeoArkPyramidVariant4Placements[4];

/// Beam-sword and grenade-launcher Rook GOLEMs for Pyramid variant 11.
///
/// Ends with `AREA_PLACEMENT_END`. XYZ use world units, yaw uses 4096 units
/// per turn, and signed texture-page/CLUT-row offsets bind the room resources.
/// Spawn parameters remain actor-specific; the map overlay must outlive actors
/// borrowing these records.
extern AreaPlacement gMapNeoArkPyramidVariant11Placements[3];

/// Shelter/Neo Ark battle rewards for normal, replay and Scavenger runs.
///
/// Search the 28 live rows by stage, area and placement variant, granting the
/// first match; room/view do not participate. Zero item slots award nothing,
/// and slot 3 requires a Medicine Wheel. The final key is
/// `INVENTORY_BATTLE_REWARD_LIST_END`. Keep the map overlay loaded during lookup.
extern InventoryBattleReward gMapNeoArkBattleRewards[29];

/// Shelter/Neo Ark battle rewards for Bounty and Nightmare runs.
///
/// Five live rows use the same location and item-slot contract as
/// `gMapNeoArkBattleRewards`; the sixth is `INVENTORY_BATTLE_REWARD_LIST_END`.
/// Keep the map overlay loaded during lookup.
extern InventoryBattleReward gMapNeoArkHardBattleRewards[6];

/// Places movie decode buffers for the Guardroom or Eve Elevator area.
///
/// Borrows only `location->area`. Main must first cache dimensions, reserve the
/// movie workspace and actor-buffer-0 ring/table region, and reset staging.
/// Each decoded frame occupies width * height * 2 bytes. The Guardroom uses
/// two consecutive decoded frames after the workspace's VLC buffer; the Eve
/// Elevator shares one output frame, places both VLC buffers after the
/// 0x10000-byte ring, and stages columns at VRAM (384,256), in words/rows.
/// All extents must fit their reserved word-aligned storage. Other areas retain
/// prior pointers. Clears session fields for reused actor buffers; never allocates.
/// Storage remains borrowed by the decoder through playback.
void mapNeoArkSetupMovieBuffers(const GameLocationKey* location);

/// Applies Neo Ark's area-dependent ducking to sequence 0x4F's song volume.
///
/// `fullVolume` is the zero-extended 16-bit product of sequence-table gain and
/// requested MIDI gain. Substation waits until its counter reaches 121 volume
/// updates, then steps down by 768 per call toward half volume; Power Plant 2
/// starts at half, waits for 241 updates, then steps up toward full volume.
/// The fixed signed step is unchecked; target tests view the cached level as
/// u32. Other areas reset both counters and use `fullVolume`. Counters count calls,
/// not frames.
///
/// Combines the selected level with the low-byte master gain divided by 127,
/// then applies the borrowed song ramp and returns the SPU playback level.
/// Gain products use unsigned 32-bit arithmetic. Uses singleton resident
/// counters; requires this map overlay and a live ramp for the call.
s32 mapNeoArkUpdateMusicVolume(u32 fullVolume, u8 areaId, LinInterp* ramp);

/// Resolves a destination's progress-dependent room variant in the Neo Ark stage.
///
/// `request` borrows a `RoomEventMsg` with a stage-5 `areaId` and `queryOnly` choice.
/// On `ROOM_EVENT_EXECUTE`, updates only `reply->roomState` for the observatory,
/// pavilion, altar, shrine and pyramid. Queries and other areas leave the reply
/// untouched. Room IDs are 1-based; the caller must initialize the reply's
/// remaining fields (room handlers copy the request first).
///
/// Both arguments may point to the same record. Neither pointer is retained;
/// game flags are only read. Always returns 1, without testing passage gates.
/// The `map_neo_ark` overlay must remain loaded for the call.
s32 mapNeoArkResolveRoomVariant(RoomEventMsg* request, RoomEventMsg* reply);

#endif // INCLUDE_MAPUI_MAP_NEO_ARK_H
