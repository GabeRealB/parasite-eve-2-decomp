#ifndef INCLUDE_MAPUI_MAP_DRYFIELD_FULL_H
#define INCLUDE_MAPUI_MAP_DRYFIELD_FULL_H

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
#include "main/stage_types.h"
#include "main/task_types.h"

/// Highest area ID represented by the night stage's directories, including empty slots.
enum { MAP_DRYFIELD_FULL_AREA_ID_LIMIT = GAME_AREA_DRYFIELD_NIGHT_UNDERPASS };

/// Map-page and marker extents; the marker table adds a page-zero terminator.
enum {
    MAP_DRYFIELD_FULL_PAGE_COUNT   = 3,
    MAP_DRYFIELD_FULL_MARKER_COUNT = 30,
};

/// Columns per area in the flat music table; scene progress selects 0..10.
enum { MAP_DRYFIELD_FULL_MUSIC_COLUMN_COUNT = 11 };

/// Captured-frame and auxiliary-heap RAM regions for Dryfield at night.
///
/// Indexed directly by area ID (0..38), with no terminator. Areas 4, 10, 33
/// and 35..37 are empty; frame transfers require a nonempty slot. Extents
/// count bytes below the resident image workspace, not captured-frame bytes.
/// Slot 0 repeats the MIST parking region used by stages 2..5; its use for
/// this stage is unproven. The map overlay must remain loaded during lookup.
extern GfxImageSlot gMapDryfieldFullImageSlots[MAP_DRYFIELD_FULL_AREA_ID_LIMIT + 1];

/// Area music for Dryfield at night, in flat rows of eleven scene columns.
///
/// Index is area ID * `MAP_DRYFIELD_FULL_MUSIC_COLUMN_COUNT` + column:
/// areas 0..38, columns 0..10. The final zero entry is outside those rows;
/// its role is unproven and it is not a terminator. Entry zero of each row
/// also controls the ambient loop. Keep this overlay loaded during music tasks.
extern StageMusicEntry gMapDryfieldFullAreaMusicEntries[(MAP_DRYFIELD_FULL_AREA_ID_LIMIT + 1) * MAP_DRYFIELD_FULL_MUSIC_COLUMN_COUNT + 1];

/// Scene-indexed countdown music for Dryfield at night.
///
/// `gStageCountdownMusicEntry` directly selects 0..19, without a terminator or
/// bounds check. Entries 12..19 retain sequence zero and start mode zero.
/// Keep this overlay loaded until the music task finishes using its selection.
extern StageMusicEntry gMapDryfieldFullCountdownMusicEntries[20];

/// Availability flags for Dryfield-at-night map pages.
///
/// Indexed by page (1..3); slot 0 is the navigation boundary. All pages use
/// nibble flag 0x16: a nonzero value unlocks them, otherwise visited areas
/// determine availability, including daytime visits. There is no end entry;
/// navigation uses the page count. Keep this overlay loaded during lookup.
extern u8 gMapDryfieldFullPageFlagIds[MAP_DRYFIELD_FULL_PAGE_COUNT + 1];

/// Area-to-map coordinate mappings for Dryfield at night.
///
/// Indexed directly by area ID (0..38); unmapped slots use PAGE_NONE. The extra
/// record at index 39 also has PAGE_NONE, rather than PAGE_END. The retained
/// page-availability walk therefore reads into the adjacent area-shape table
/// until it encounters a page word of 0xFFFF. This array remains 40 records;
/// the neighboring bytes are not part of it. Keep this overlay loaded for use.
extern MenuMapArea gMapDryfieldFullAreas[MAP_DRYFIELD_FULL_AREA_ID_LIMIT + 2];

/// This stage's `Gp_MapMarkTables` entry: the area shapes, indexed by area.
extern MenuMapAreaShape D_map_dryfield_full_80179D10[];

/// Fixed map markers for Dryfield at night, followed by a page-zero terminator.
///
/// Indices 0..29 correspond to `gMapDryfieldFullMarkerFlags`; every marker is
/// independent of area visit bits. Index 29 is controlled directly by story
/// progress. Coordinates are signed map pixels. Keep this overlay loaded for use.
extern MenuMapMarker gMapDryfieldFullMarkers[MAP_DRYFIELD_FULL_MARKER_COUNT + 1];

/// Fixed and objective icons on the Dryfield-at-night map.
///
/// Eight records and a page-zero terminator; areas and pages are one-based.
/// Fixed icons require a visited area and any nonzero condition flag; objective
/// icons compare their condition with the current objective byte. Coordinates
/// are signed map pixels. Keep this overlay loaded while drawing.
extern MenuMapIcon gMapDryfieldFullIcons[9];

/// Area names displayed by the Dryfield-at-night map.
///
/// Indexed by area ID minus one (0..37), without a terminator. Each record is
/// a NUL-terminated ASCII line in 32 bytes; unnamed slots contain empty strings.
/// Keep this overlay loaded while measuring or drawing the text.
extern MenuMapAreaName gMapDryfieldFullAreaNames[MAP_DRYFIELD_FULL_AREA_ID_LIMIT];

/// Placed-object lists and spawn recipes for Dryfield at night.
///
/// Indexed directly by area ID (0..38); empty areas have two NULL pointers.
/// Entry 39 holds `AREA_OBJECT_ROOM_END` for flag-seeding walks. Place lists
/// are borrowed from gameplay, while the spawn recipes belong to this map.
/// The map and any models selected from room overlays must stay loaded while
/// their spawned tasks use them.
extern AreaObjectRoom gMapDryfieldFullObjectRooms[MAP_DRYFIELD_FULL_AREA_ID_LIMIT + 2];

/// Default room-task recipes for the 32 populated Dryfield-at-night areas.
///
/// Each data value is `GP_TASK_LOC_KEY(stage, area, 0)`, so every room of that
/// area shares its callback. Tasks have no body and priority 32. The final
/// `TASK_DESC_END` entry closes the search; keep this overlay and the selected
/// room callback's overlay loaded through spawning and task execution.
extern TaskDesc gMapDryfieldFullRoomTaskDescriptors[33];

/// Packed flag and picture selectors for Dryfield-at-night map markers.
///
/// Indexed in parallel with `gMapDryfieldFullMarkers` (0..29), with no
/// terminator. Low eleven bits select a game-flag nibble; bit eleven selects
/// the alternate picture. Index 29's zero word is bypassed by the story test
/// in `menuMapGetMarkerState`. Keep this overlay loaded during lookup.
extern u16 gMapDryfieldFullMarkerFlags[MAP_DRYFIELD_FULL_MARKER_COUNT];

/// Per-area directories of room lighting for Dryfield at night.
///
/// Indexed by area ID minus one (0..37), then by room ID minus one; neither
/// level has a terminator. Areas 4, 10, 33 and 35..37 have NULL entries.
/// Each nonempty pointer borrows the corresponding loaded room overlay's
/// lighting array; its descriptors and mutable lights must remain loaded.
extern WorldCoordRoomLighting* gMapDryfieldFullRoomLightingTables[MAP_DRYFIELD_FULL_AREA_ID_LIMIT];

extern DirectionWarpEntry* D_map_dryfield_full_8017A80C[];

/// Logical-view search counts for Dryfield-at-night areas 1..38.
///
/// Indexed by area - 1, then room - 1 in that area's borrowed room array.
/// Areas 4, 10, 33 and 35..37 are NULL and cannot be selected. Counts are
/// signed halfword element counts, independent of view-map and camera/image/
/// sprite capacities. Neither level has a terminator. Require in-range
/// populated slots and keep this map and the selected room loaded during lookup.
extern ViewCountTable gMapDryfieldFullViewCountTable;

extern WorldCollisionStageResources D_map_dryfield_full_8017A9D8;

/// Dryfield-at-night directory of per-area room camera arrays.
///
/// `cameras[area - 1][mappedCamera - 1]` selects a borrowed `ViewCamera`.
/// Areas 1..38 are represented; 4, 10, 33 and 35..37 are NULL. Neither level
/// has a terminator, and each pool has its own extent. Require populated,
/// in-range slots. Keep this map loaded during lookup and the selected room
/// loaded until queued camera applications have copied their records.
extern ViewCameraTable gMapDryfieldFullViewCameraTable;

/// Dryfield-at-night directory of room-local logical-view mappings.
///
/// `viewMaps[area - 1][room - 1][view - 1]` yields the unsigned, 1-based
/// camera/image/sprite index. The area directory covers IDs 1..38, with NULL
/// entries for 4, 10, 33 and 35..37; no level has a terminator. Lookups require
/// populated slots and IDs within each borrowed room directory and byte map.
/// Keep this map overlay and the selected room overlay loaded; the mappings
/// are borrowed and may be changed during play.
extern ViewIndexTable gMapDryfieldFullViewIndexTable;

/// Per-area sprite-view directory for Dryfield at night.
///
/// Gameplay selects this single record at `gSpriteAreaTables[stage - 1]`.
/// `areaViews[area - 1][mappedView - 1]` borrows a writable room descriptor.
/// Areas 1..38 are represented; 4, 10, 33 and 35..37 are NULL. Neither level
/// has a terminator. Require populated, in-range slots and keep this map and
/// the selected room overlay loaded while their sprites are allocated or drawn.
extern SpriteAreaTable gMapDryfieldFullSpriteAreaTable;

/// Per-area collision surface-class tables for Dryfield at night.
///
/// Indexed by area ID minus one (0..37), then by surface class (0..7),
/// independently of room variant. Areas 4, 10, 33 and 35..37 have NULL entries;
/// there is no terminator. Populated entries borrow eight property pointers
/// from the selected room overlay. Both overlays must stay loaded for lookup.
extern WorldCollisionSurfaceProperties** gMapDryfieldFullSurfacePropertiesByArea[MAP_DRYFIELD_FULL_AREA_ID_LIMIT];

/// Twelve Scavengers for the night Gas Station scripted encounter.
///
/// World-coordinate XYZ and yaw (4096 units per turn), followed by
/// `AREA_PLACEMENT_END`. Resource keys select this variant's room-owned
/// task recipes; texture offsets use 64-word VRAM columns and CLUT rows.
/// Actors borrow the records, so this map must stay loaded for their lives.
extern AreaPlacement gMapDryfieldFullGasStationVariant1Placements[13];

/// Two Zebra Stalkers for night Gas Station placement variant 2.
///
/// World-coordinate XYZ and yaw (4096 units per turn), followed by
/// `AREA_PLACEMENT_END`. Resource keys select this variant's room-owned
/// task recipes; texture offsets use 64-word VRAM columns and CLUT rows.
/// Actors borrow the records, so this map must stay loaded for their lives.
extern AreaPlacement gMapDryfieldFullGasStationVariant2Placements[3];

/// Two Desert Chasers for night Gas Station placement variant 3.
///
/// World-coordinate XYZ and yaw (4096 units per turn), followed by
/// `AREA_PLACEMENT_END`. Resource keys select this variant's room-owned
/// task recipes; texture offsets use 64-word VRAM columns and CLUT rows.
/// Actors borrow the records, so this map must stay loaded for their lives.
extern AreaPlacement gMapDryfieldFullGasStationVariant3Placements[3];

/// Beam Sword and grenade-launcher Rook GOLEMs for night Gas Station variant 7.
///
/// The sword Rook patrols 5000 world units (`variant` counts thousands);
/// the launcher Rook starts idle. Mode bit 0 enables patrol.
/// World-coordinate XYZ and yaw (4096 units per turn), followed by
/// `AREA_PLACEMENT_END`. Resource keys select this variant's room-owned
/// task recipes; texture offsets use 64-word VRAM columns and CLUT rows.
/// Actors borrow the records, so this map must stay loaded for their lives.
extern AreaPlacement gMapDryfieldFullGasStationVariant7Placements[3];

/// Three Mind Sucklers and one Zebra Stalker for night Gas Station variant 11.
///
/// World-coordinate XYZ and yaw (4096 units per turn), followed by
/// `AREA_PLACEMENT_END`. Resource keys select this variant's room-owned
/// task recipes; texture offsets use 64-word VRAM columns and CLUT rows.
/// Actors borrow the records, so this map must stay loaded for their lives.
extern AreaPlacement gMapDryfieldFullGasStationVariant11Placements[5];

/// Six moths and one Desert Chaser for night Main Street placement variant 1.
///
/// World-coordinate XYZ and yaw (4096 units per turn), followed by
/// `AREA_PLACEMENT_END`. Resource keys select this variant's room-owned
/// task recipes; texture offsets use 64-word VRAM columns and CLUT rows.
/// Actors borrow the records, so this map must stay loaded for their lives.
extern AreaPlacement gMapDryfieldFullMainStreetVariant1Placements[8];

/// Gary Douglas scene placement for night Main Street variant 2.
///
/// World-coordinate XYZ and yaw (4096 units per turn), followed by
/// `AREA_PLACEMENT_END`. Resource keys select this variant's room-owned
/// task recipes; texture offsets use 64-word VRAM columns and CLUT rows.
/// Actors borrow the records, so this map must stay loaded for their lives.
extern AreaPlacement gMapDryfieldFullMainStreetVariant2Placements[2];

/// Two Brain Stingers for night Main Street placement variant 7.
///
/// World-coordinate XYZ and yaw (4096 units per turn), followed by
/// `AREA_PLACEMENT_END`. Resource keys select this variant's room-owned
/// task recipes; texture offsets use 64-word VRAM columns and CLUT rows.
/// Actors borrow the records, so this map must stay loaded for their lives.
extern AreaPlacement gMapDryfieldFullMainStreetVariant7Placements[3];

/// Four moths for night General Store placement variant 1.
///
/// World-coordinate XYZ and yaw (4096 units per turn), followed by
/// `AREA_PLACEMENT_END`. Resource keys select this variant's room-owned
/// task recipes; texture offsets use 64-word VRAM columns and CLUT rows.
/// Actors borrow the records, so this map must stay loaded for their lives.
extern AreaPlacement gMapDryfieldFullGeneralStoreVariant1Placements[5];

/// Three Scavengers for night General Store placement variant 2.
///
/// World-coordinate XYZ and yaw (4096 units per turn), followed by
/// `AREA_PLACEMENT_END`. Resource keys select this variant's room-owned
/// task recipes; texture offsets use 64-word VRAM columns and CLUT rows.
/// Actors borrow the records, so this map must stay loaded for their lives.
extern AreaPlacement gMapDryfieldFullGeneralStoreVariant2Placements[4];

/// Four moths and four scorpions for night General Store placement variant 3.
///
/// World-coordinate XYZ and yaw (4096 units per turn), followed by
/// `AREA_PLACEMENT_END`. Resource keys select this variant's room-owned
/// task recipes; texture offsets use 64-word VRAM columns and CLUT rows.
/// Actors borrow the records, so this map must stay loaded for their lives.
extern AreaPlacement gMapDryfieldFullGeneralStoreVariant3Placements[9];

/// Three rats and two Scavengers for night General Store placement variant 21.
///
/// World-coordinate XYZ and yaw (4096 units per turn), followed by
/// `AREA_PLACEMENT_END`. Resource keys select this variant's room-owned
/// task recipes; texture offsets use 64-word VRAM columns and CLUT rows.
/// Actors borrow the records, so this map must stay loaded for their lives.
extern AreaPlacement gMapDryfieldFullGeneralStoreVariant21Placements[6];

/// Six scorpions for night Back Street placement variant 1.
///
/// World-coordinate XYZ and yaw (4096 units per turn), followed by
/// `AREA_PLACEMENT_END`. Resource keys select this variant's room-owned
/// task recipes; texture offsets use 64-word VRAM columns and CLUT rows.
/// Actors borrow the records, so this map must stay loaded for their lives.
extern AreaPlacement gMapDryfieldFullBackStreetVariant1Placements[7];

/// Five Mind Sucklers and one Desert Chaser for night Back Street variant 3.
///
/// World-coordinate XYZ and yaw (4096 units per turn), followed by
/// `AREA_PLACEMENT_END`. Resource keys select this variant's room-owned
/// task recipes; texture offsets use 64-word VRAM columns and CLUT rows.
/// Actors borrow the records, so this map must stay loaded for their lives.
extern AreaPlacement gMapDryfieldFullBackStreetVariant3Placements[7];

/// Two Desert Chasers and five moths for night Back Street placement variant 4.
///
/// World-coordinate XYZ and yaw (4096 units per turn), followed by
/// `AREA_PLACEMENT_END`. Resource keys select this variant's room-owned
/// task recipes; texture offsets use 64-word VRAM columns and CLUT rows.
/// Actors borrow the records, so this map must stay loaded for their lives.
extern AreaPlacement gMapDryfieldFullBackStreetVariant4Placements[8];

/// One patrolling grenade-launcher Rook GOLEM for night Back Street variant 7.
///
/// Mode bit 0 enables patrol; `variant` 12 gives a 12000-world-unit leg.
/// World-coordinate XYZ and yaw (4096 units per turn), followed by
/// `AREA_PLACEMENT_END`. Resource keys select this variant's room-owned
/// task recipes; texture offsets use 64-word VRAM columns and CLUT rows.
/// Actors borrow the records, so this map must stay loaded for their lives.
extern AreaPlacement gMapDryfieldFullBackStreetVariant7Placements[2];

/// Fourteen perched bats for night Back Street placement variant 21.
///
/// World-coordinate XYZ and yaw (4096 units per turn), followed by
/// `AREA_PLACEMENT_END`. Resource keys select this variant's room-owned
/// task recipes; texture offsets use 64-word VRAM columns and CLUT rows.
/// Actors borrow the records, so this map must stay loaded for their lives.
extern AreaPlacement gMapDryfieldFullBackStreetVariant21Placements[15];

/// Four moths for night Souvenir Shop placement variant 1.
///
/// World-coordinate XYZ and yaw (4096 units per turn), followed by
/// `AREA_PLACEMENT_END`. Resource keys select this variant's room-owned
/// task recipes; texture offsets use 64-word VRAM columns and CLUT rows.
/// Actors borrow the records, so this map must stay loaded for their lives.
extern AreaPlacement gMapDryfieldFullSouvenirShopVariant1Placements[5];

/// Four Scavengers for night Souvenir Shop placement variant 3.
///
/// World-coordinate XYZ and yaw (4096 units per turn), followed by
/// `AREA_PLACEMENT_END`. Resource keys select this variant's room-owned
/// task recipes; texture offsets use 64-word VRAM columns and CLUT rows.
/// Actors borrow the records, so this map must stay loaded for their lives.
extern AreaPlacement gMapDryfieldFullSouvenirShopVariant3Placements[5];

/// Seven moths and three scorpions for night Souvenir Shop placement variant 11.
///
/// World-coordinate XYZ and yaw (4096 units per turn), followed by
/// `AREA_PLACEMENT_END`. Resource keys select this variant's room-owned
/// task recipes; texture offsets use 64-word VRAM columns and CLUT rows.
/// Actors borrow the records, so this map must stay loaded for their lives.
extern AreaPlacement gMapDryfieldFullSouvenirShopVariant11Placements[11];

/// Two scorpions for night Warehouse placement variant 1.
///
/// World-coordinate XYZ and yaw (4096 units per turn), followed by
/// `AREA_PLACEMENT_END`. Resource keys select this variant's room-owned
/// task recipes; texture offsets use 64-word VRAM columns and CLUT rows.
/// Actors borrow the records, so this map must stay loaded for their lives.
extern AreaPlacement gMapDryfieldFullWarehouseVariant1Placements[3];

/// Six Blood Sucklers for night Warehouse placement variant 3.
///
/// World-coordinate XYZ and yaw (4096 units per turn), followed by
/// `AREA_PLACEMENT_END`. Resource keys select this variant's room-owned
/// task recipes; texture offsets use 64-word VRAM columns and CLUT rows.
/// Actors borrow the records, so this map must stay loaded for their lives.
extern AreaPlacement gMapDryfieldFullWarehouseVariant3Placements[7];

/// Two Scavengers and three Scorpions for night Warehouse placement variant 11.
///
/// World-coordinate XYZ and yaw (4096 units per turn). Entry keys select
/// the paired room-owned resource recipes; fileIdLow = 0 uses their base files.
/// Texture offsets count 64-word VRAM columns and CLUT rows. The final record
/// is `AREA_PLACEMENT_END`; actors borrow the live records, so keep this map
/// overlay loaded until they end.
///
/// Scorpions use tuning row 2.
extern AreaPlacement gMapDryfieldFullWarehouseVariant11Placements[6];

/// Aya and three Pawn GOLEM walkers for night R08 placement variant 1.
///
/// World-coordinate XYZ and yaw (4096 units per turn). Entry keys select
/// the paired room-owned resource recipes; fileIdLow = 0 uses their base files.
/// Texture offsets count 64-word VRAM columns and CLUT rows. The final record
/// is `AREA_PLACEMENT_END`; actors borrow the live records, so keep this map
/// overlay loaded until they end.
///
/// The Pawn GOLEMs use the body recipe of their paired walker tasks.
extern AreaPlacement gMapDryfieldFullR08Variant1Placements[5];

/// Two Blood Sucklers and five Scorpions for night Dilapidated House placement variant 3.
///
/// World-coordinate XYZ and yaw (4096 units per turn). Entry keys select
/// the paired room-owned resource recipes; fileIdLow = 0 uses their base files.
/// Texture offsets count 64-word VRAM columns and CLUT rows. The final record
/// is `AREA_PLACEMENT_END`; actors borrow the live records, so keep this map
/// overlay loaded until they end.
///
/// Blood Sucklers patrol; Scorpions use tuning row 1.
extern AreaPlacement gMapDryfieldFullDilapidatedHouseVariant3Placements[8];

/// Two idle Blood Sucklers and four Scavengers for night Dilapidated House placement variant 4.
///
/// World-coordinate XYZ and yaw (4096 units per turn). Entry keys select
/// the paired room-owned resource recipes; fileIdLow = 0 uses their base files.
/// Texture offsets count 64-word VRAM columns and CLUT rows. The final record
/// is `AREA_PLACEMENT_END`; actors borrow the live records, so keep this map
/// overlay loaded until they end.
extern AreaPlacement gMapDryfieldFullDilapidatedHouseVariant4Placements[7];

/// Five Scavengers for night Dilapidated House placement variant 21.
///
/// World-coordinate XYZ and yaw (4096 units per turn). Entry keys select
/// the paired room-owned resource recipes; fileIdLow = 0 uses their base files.
/// Texture offsets count 64-word VRAM columns and CLUT rows. The final record
/// is `AREA_PLACEMENT_END`; actors borrow the live records, so keep this map
/// overlay loaded until they end.
extern AreaPlacement gMapDryfieldFullDilapidatedHouseVariant21Placements[6];

/// Two idle Blood Sucklers and three Scavengers for night Motel Room 1 placement variant 1.
///
/// World-coordinate XYZ and yaw (4096 units per turn). Entry keys select
/// the paired room-owned resource recipes; fileIdLow = 0 uses their base files.
/// Texture offsets count 64-word VRAM columns and CLUT rows. The final record
/// is `AREA_PLACEMENT_END`; actors borrow the live records, so keep this map
/// overlay loaded until they end.
extern AreaPlacement gMapDryfieldFullMotelRoom1Variant1Placements[6];

/// Five Scorpions and three Moths for night Motel Room 1 placement variant 2.
///
/// World-coordinate XYZ and yaw (4096 units per turn). Entry keys select
/// the paired room-owned resource recipes; fileIdLow = 0 uses their base files.
/// Texture offsets count 64-word VRAM columns and CLUT rows. The final record
/// is `AREA_PLACEMENT_END`; actors borrow the live records, so keep this map
/// overlay loaded until they end.
///
/// Scorpions use tuning row 1; Moths use pursuit-speed row 3.
extern AreaPlacement gMapDryfieldFullMotelRoom1Variant2Placements[9];

/// Three Scavengers for night Motel Room 1 placement variant 21.
///
/// World-coordinate XYZ and yaw (4096 units per turn). Entry keys select
/// the paired room-owned resource recipes; fileIdLow = 0 uses their base files.
/// Texture offsets count 64-word VRAM columns and CLUT rows. The final record
/// is `AREA_PLACEMENT_END`; actors borrow the live records, so keep this map
/// overlay loaded until they end.
extern AreaPlacement gMapDryfieldFullMotelRoom1Variant21Placements[4];

/// Four Blood Sucklers and three Scorpions for night Motel Room 2 placement variant 2.
///
/// World-coordinate XYZ and yaw (4096 units per turn). Entry keys select
/// the paired room-owned resource recipes; fileIdLow = 0 uses their base files.
/// Texture offsets count 64-word VRAM columns and CLUT rows. The final record
/// is `AREA_PLACEMENT_END`; actors borrow the live records, so keep this map
/// overlay loaded until they end.
///
/// Blood Sucklers patrol; Scorpions use tuning row 1.
extern AreaPlacement gMapDryfieldFullMotelRoom2Variant2Placements[8];

/// Six idle Blood Sucklers for night Motel Room 2 placement variant 3.
///
/// World-coordinate XYZ and yaw (4096 units per turn). Entry keys select
/// the paired room-owned resource recipes; fileIdLow = 0 uses their base files.
/// Texture offsets count 64-word VRAM columns and CLUT rows. The final record
/// is `AREA_PLACEMENT_END`; actors borrow the live records, so keep this map
/// overlay loaded until they end.
extern AreaPlacement gMapDryfieldFullMotelRoom2Variant3Placements[7];

/// Two Scavengers and four Moths for night Motel Room 2 placement variant 21.
///
/// World-coordinate XYZ and yaw (4096 units per turn). Entry keys select
/// the paired room-owned resource recipes; fileIdLow = 0 uses their base files.
/// Texture offsets count 64-word VRAM columns and CLUT rows. The final record
/// is `AREA_PLACEMENT_END`; actors borrow the live records, so keep this map
/// overlay loaded until they end.
///
/// Moths use pursuit-speed row 4.
extern AreaPlacement gMapDryfieldFullMotelRoom2Variant21Placements[7];

/// Four Blood Sucklers and an Odd Stranger for night Motel Room 3 placement variant 1.
///
/// World-coordinate XYZ and yaw (4096 units per turn). Entry keys select
/// the paired room-owned resource recipes; fileIdLow = 0 uses their base files.
/// Texture offsets count 64-word VRAM columns and CLUT rows. The final record
/// is `AREA_PLACEMENT_END`; actors borrow the live records, so keep this map
/// overlay loaded until they end.
///
/// Two Blood Sucklers patrol and two start idle. The Odd Stranger retains
/// mode 0x10: its low nibble selects the middle tuning row.
extern AreaPlacement gMapDryfieldFullMotelRoom3Variant1Placements[6];

/// Three Blood Sucklers and three Scavengers for night Motel Room 3 placement variant 2.
///
/// World-coordinate XYZ and yaw (4096 units per turn). Entry keys select
/// the paired room-owned resource recipes; fileIdLow = 0 uses their base files.
/// Texture offsets count 64-word VRAM columns and CLUT rows. The final record
/// is `AREA_PLACEMENT_END`; actors borrow the live records, so keep this map
/// overlay loaded until they end.
///
/// Blood Sucklers patrol; Scavengers carry the force-posed variant bit.
extern AreaPlacement gMapDryfieldFullMotelRoom3Variant2Placements[7];

/// Two Odd Strangers for night Motel Room 4 placement variant 1.
///
/// World-coordinate XYZ and yaw (4096 units per turn). Entry keys select
/// the paired room-owned resource recipes; fileIdLow = 0 uses their base files.
/// Texture offsets count 64-word VRAM columns and CLUT rows. The final record
/// is `AREA_PLACEMENT_END`; actors borrow the live records, so keep this map
/// overlay loaded until they end.
///
/// Both retain mode 0x10: its low nibble selects the middle tuning row.
extern AreaPlacement gMapDryfieldFullMotelRoom4Variant1Placements[3];

/// Four Blood Sucklers and three Moths for night Motel Room 4 placement variant 2.
///
/// World-coordinate XYZ and yaw (4096 units per turn). Entry keys select
/// the paired room-owned resource recipes; fileIdLow = 0 uses their base files.
/// Texture offsets count 64-word VRAM columns and CLUT rows. The final record
/// is `AREA_PLACEMENT_END`; actors borrow the live records, so keep this map
/// overlay loaded until they end.
///
/// Blood Sucklers patrol; Moths use pursuit-speed row 3.
extern AreaPlacement gMapDryfieldFullMotelRoom4Variant2Placements[8];

/// Two Desert Chasers and four Moths for night Parking Lot placement variant 2.
///
/// World-coordinate XYZ and yaw (4096 units per turn). Entry keys select
/// the paired room-owned resource recipes; fileIdLow = 0 uses their base files.
/// Texture offsets count 64-word VRAM columns and CLUT rows. The final record
/// is `AREA_PLACEMENT_END`; actors borrow the live records, so keep this map
/// overlay loaded until they end.
///
/// Desert Chasers select tuning row 2; Moths use pursuit-speed row 2.
extern AreaPlacement gMapDryfieldFullParkingLotVariant2Placements[7];

/// Four wave-staged Scavengers for night Parking Lot placement variant 3.
///
/// World-coordinate XYZ and yaw (4096 units per turn). Entry keys select
/// the paired room-owned resource recipes; fileIdLow = 0 uses their base files.
/// Texture offsets count 64-word VRAM columns and CLUT rows. The final record
/// is `AREA_PLACEMENT_END`; actors borrow the live records, so keep this map
/// overlay loaded until they end.
///
/// All respond to either wave cue. Three start on the first entrance-cue tick;
/// one retains the random 1..32-tick delay. The battle cue replaces XYZ using
/// placement indices 0..3 in the actor's four-entry parking-lot position table.
extern AreaPlacement gMapDryfieldFullParkingLotVariant3Placements[5];

/// Five Scorpions and a Desert Chaser for night Parking Lot placement variant 4.
///
/// World-coordinate XYZ and yaw (4096 units per turn). Entry keys select
/// the paired room-owned resource recipes; fileIdLow = 0 uses their base files.
/// Texture offsets count 64-word VRAM columns and CLUT rows. The final record
/// is `AREA_PLACEMENT_END`; actors borrow the live records, so keep this map
/// overlay loaded until they end.
///
/// Two Scorpions use ambush-follower mode and three roam, all with tuning
/// row 1. The Desert Chaser selects tuning row 2.
extern AreaPlacement gMapDryfieldFullParkingLotVariant4Placements[7];

/// A Bishop GOLEM for night Parking Lot placement variant 7.
///
/// World-coordinate XYZ and yaw (4096 units per turn). Entry keys select
/// the paired room-owned resource recipes; fileIdLow = 0 uses their base files.
/// Texture offsets count 64-word VRAM columns and CLUT rows. The final record
/// is `AREA_PLACEMENT_END`; actors borrow the live records, so keep this map
/// overlay loaded until they end.
extern AreaPlacement gMapDryfieldFullParkingLotVariant7Placements[2];

/// Eleven Bats for night Parking Lot placement variant 21.
///
/// World-coordinate XYZ and yaw (4096 units per turn). Entry keys select
/// the paired room-owned resource recipes; fileIdLow = 0 uses their base files.
/// Texture offsets count 64-word VRAM columns and CLUT rows. The final record
/// is `AREA_PLACEMENT_END`; actors borrow the live records, so keep this map
/// overlay loaded until they end.
///
/// Bats start wandering and use turn-rate row 6.
extern AreaPlacement gMapDryfieldFullParkingLotVariant21Placements[12];

/// Six Blood Sucklers for night Toilet placement variant 1.
///
/// World-coordinate XYZ and yaw (4096 units per turn). Entry keys select
/// the paired room-owned resource recipes; fileIdLow = 0 uses their base files.
/// Texture offsets count 64-word VRAM columns and CLUT rows. The final record
/// is `AREA_PLACEMENT_END`; actors borrow the live records, so keep this map
/// overlay loaded until they end.
///
/// All patrol from the same XYZ position with separately authored yaw.
extern AreaPlacement gMapDryfieldFullToiletVariant1Placements[7];

/// Six Rats and five Moths for night Toilet placement variant 3.
///
/// World-coordinate XYZ and yaw (4096 units per turn). Entry keys select
/// the paired room-owned resource recipes; fileIdLow = 0 uses their base files.
/// Texture offsets count 64-word VRAM columns and CLUT rows. The final record
/// is `AREA_PLACEMENT_END`; actors borrow the live records, so keep this map
/// overlay loaded until they end.
///
/// Rats use tuning row 3; Moths use pursuit-speed row 2.
extern AreaPlacement gMapDryfieldFullToiletVariant3Placements[12];

/// Five Scavengers for night Toilet placement variant 11.
///
/// World-coordinate XYZ and yaw (4096 units per turn). Entry keys select
/// the paired room-owned resource recipes; fileIdLow = 0 uses their base files.
/// Texture offsets count 64-word VRAM columns and CLUT rows. The final record
/// is `AREA_PLACEMENT_END`; actors borrow the live records, so keep this map
/// overlay loaded until they end.
extern AreaPlacement gMapDryfieldFullToiletVariant11Placements[6];

/// Kyle Madigan for night Saloon G/R placement variant 1.
///
/// World-coordinate XYZ and yaw (4096 units per turn). Entry keys select
/// the paired room-owned resource recipes; fileIdLow = 0 uses their base files.
/// Texture offsets count 64-word VRAM columns and CLUT rows. The final record
/// is `AREA_PLACEMENT_END`; actors borrow the live records, so keep this map
/// overlay loaded until they end.
///
/// The live placement has zero XYZ, yaw and spawn selectors.
extern AreaPlacement gMapDryfieldFullSaloonGRVariant1Placements[2];

/// Two Blood Sucklers for night Saloon G/R placement variant 2.
///
/// World-coordinate XYZ and yaw (4096 units per turn). Entry keys select
/// the paired room-owned resource recipes; fileIdLow = 0 uses their base files.
/// Texture offsets count 64-word VRAM columns and CLUT rows. The final record
/// is `AREA_PLACEMENT_END`; actors borrow the live records, so keep this map
/// overlay loaded until they end.
///
/// Both patrol from zero XYZ and yaw; neither record is a terminator.
extern AreaPlacement gMapDryfieldFullSaloonGRVariant2Placements[3];

/// A Blood Suckler and four Scavengers for night Saloon G/R placement variant 3.
///
/// World-coordinate XYZ and yaw (4096 units per turn). Entry keys select
/// the paired room-owned resource recipes; fileIdLow = 0 uses their base files.
/// Texture offsets count 64-word VRAM columns and CLUT rows. The final record
/// is `AREA_PLACEMENT_END`; actors borrow the live records, so keep this map
/// overlay loaded until they end.
///
/// The Blood Suckler starts patrolling.
extern AreaPlacement gMapDryfieldFullSaloonGRVariant3Placements[6];

/// Two Scavengers and five Rats for night G/R Kitchen placement variant 1.
///
/// World-coordinate XYZ and yaw (4096 units per turn). Entry keys select
/// the paired room-owned resource recipes; fileIdLow = 0 uses their base files.
/// Texture offsets count 64-word VRAM columns and CLUT rows. The final record
/// is `AREA_PLACEMENT_END`; actors borrow the live records, so keep this map
/// overlay loaded until they end.
///
/// Rats use tuning row 3.
extern AreaPlacement gMapDryfieldFullGRKitchenVariant1Placements[8];

/// An idle Blood Suckler, seven Rats and two Moths for night G/R Kitchen placement variant 3.
///
/// World-coordinate XYZ and yaw (4096 units per turn). Entry keys select
/// the paired room-owned resource recipes; fileIdLow = 0 uses their base files.
/// Texture offsets count 64-word VRAM columns and CLUT rows. The final record
/// is `AREA_PLACEMENT_END`; actors borrow the live records, so keep this map
/// overlay loaded until they end.
///
/// Rats use tuning row 3; Moths use pursuit-speed row 2.
extern AreaPlacement gMapDryfieldFullGRKitchenVariant3Placements[11];

/// Four idle Blood Sucklers and three Scavengers for night G/R Kitchen placement variant 11.
///
/// World-coordinate XYZ and yaw (4096 units per turn). Entry keys select
/// the paired room-owned resource recipes; fileIdLow = 0 uses their base files.
/// Texture offsets count 64-word VRAM columns and CLUT rows. The final record
/// is `AREA_PLACEMENT_END`; actors borrow the live records, so keep this map
/// overlay loaded until they end.
extern AreaPlacement gMapDryfieldFullGRKitchenVariant11Placements[8];

/// Six Moths and three Scavengers for night Water Tower placement variant 1.
///
/// World-coordinate XYZ and yaw (4096 units per turn). Entry keys select
/// the paired room-owned resource recipes; fileIdLow = 0 uses their base files.
/// Texture offsets count 64-word VRAM columns and CLUT rows. The final record
/// is `AREA_PLACEMENT_END`; actors borrow the live records, so keep this map
/// overlay loaded until they end.
///
/// Moths use pursuit-speed row 2.
extern AreaPlacement gMapDryfieldFullWaterTowerVariant1Placements[10];

/// Three Zebra Stalkers for night Water Tower placement variant 2.
///
/// World-coordinate XYZ and yaw (4096 units per turn). Entry keys select
/// the paired room-owned resource recipes; `fileIdLow = 0` uses their base files.
/// Texture offsets count 64-word VRAM columns and CLUT rows. The final record
/// is `AREA_PLACEMENT_END`; actors borrow the live records, so keep this map
/// overlay loaded until they end.
///
/// All start on the floor with ceiling perching disabled.
extern AreaPlacement gMapDryfieldFullWaterTowerVariant2Placements[4];

/// One dangling Mad Chaser for night Water Tower placement variant 11.
///
/// World-coordinate XYZ and yaw (4096 units per turn). Entry keys select
/// the paired room-owned resource recipes; `fileIdLow = 0` uses their base files.
/// Texture offsets count 64-word VRAM columns and CLUT rows. The final record
/// is `AREA_PLACEMENT_END`; actors borrow the live records, so keep this map
/// overlay loaded until they end.
///
/// The authored spawn transform is (450, -3700, 4400), yaw 2600.
extern AreaPlacement gMapDryfieldFullWaterTowerVariant11Placements[2];

/// Four Scorpions and two Zebra Stalkers for night Water Tower placement variant 21.
///
/// World-coordinate XYZ and yaw (4096 units per turn). Entry keys select
/// the paired room-owned resource recipes; `fileIdLow = 0` uses their base files.
/// Texture offsets count 64-word VRAM columns and CLUT rows. The final record
/// is `AREA_PLACEMENT_END`; actors borrow the live records, so keep this map
/// overlay loaded until they end.
///
/// Scorpions use signalled ambush mode and tuning row 3; Stalkers cannot perch on ceilings.
extern AreaPlacement gMapDryfieldFullWaterTowerVariant21Placements[7];

/// Empty placement list for night Water Tank placement variant 1.
///
/// Only `AREA_PLACEMENT_END`; the paired resource list is also empty.
/// Keep this map overlay loaded while the room variant borrows the list.
extern AreaPlacement gMapDryfieldFullWaterTankVariant1Placements[1];

/// The handover figure and three Mind Sucklers for night Water Tank placement variant 10.
///
/// World-coordinate XYZ and yaw (4096 units per turn). Entry keys select
/// the paired room-owned resource recipes; `fileIdLow = 0` uses their base files.
/// Texture offsets count 64-word VRAM columns and CLUT rows. The final record
/// is `AREA_PLACEMENT_END`; actors borrow the live records, so keep this map
/// overlay loaded until they end.
///
/// The Sucklers select the scripted airborne variant, which ignores the stored
/// perch-mode bits. All use base resource files.
extern AreaPlacement gMapDryfieldFullWaterTankVariant10Placements[5];

/// The handover figure for night Water Tank placement variant 11.
///
/// World-coordinate XYZ and yaw (4096 units per turn). Entry keys select
/// the paired room-owned resource recipes; `fileIdLow = 0` uses their base files.
/// Texture offsets count 64-word VRAM columns and CLUT rows. The final record
/// is `AREA_PLACEMENT_END`; actors borrow the live records, so keep this map
/// overlay loaded until they end.
///
/// Its initial transform is (1110, -12000, -2500), yaw 1820.
extern AreaPlacement gMapDryfieldFullWaterTankVariant11Placements[2];

/// Twelve bats for night Breezeway placement variant 1.
///
/// World-coordinate XYZ and yaw (4096 units per turn). Entry keys select
/// the paired room-owned resource recipes; `fileIdLow = 0` uses their base files.
/// Texture offsets count 64-word VRAM columns and CLUT rows. The final record
/// is `AREA_PLACEMENT_END`; actors borrow the live records, so keep this map
/// overlay loaded until they end.
///
/// Six push backward from their perches and six drop from them; all use turn-rate row 4.
extern AreaPlacement gMapDryfieldFullBreezewayVariant1Placements[13];

/// One Desert Chaser and eleven Moths for night Breezeway placement variant 3.
///
/// World-coordinate XYZ and yaw (4096 units per turn). Entry keys select
/// the paired room-owned resource recipes; `fileIdLow = 0` uses their base files.
/// Texture offsets count 64-word VRAM columns and CLUT rows. The final record
/// is `AREA_PLACEMENT_END`; actors borrow the live records, so keep this map
/// overlay loaded until they end.
///
/// Moths use pursuit-speed row 2.
extern AreaPlacement gMapDryfieldFullBreezewayVariant3Placements[13];

/// Three Scorpions and six Mind Sucklers for night Breezeway placement variant 21.
///
/// World-coordinate XYZ and yaw (4096 units per turn). Entry keys select
/// the paired room-owned resource recipes; `fileIdLow = 0` uses their base files.
/// Texture offsets count 64-word VRAM columns and CLUT rows. The final record
/// is `AREA_PLACEMENT_END`; actors borrow the live records, so keep this map
/// overlay loaded until they end.
///
/// Scorpions use signalled ambush mode and tuning row 3; Sucklers start on ceilings and cannot return to walls.
extern AreaPlacement gMapDryfieldFullBreezewayVariant21Placements[10];

/// Four active Scavengers for night Garage placement variant 1.
///
/// World-coordinate XYZ and yaw (4096 units per turn). Entry keys select
/// the paired room-owned resource recipes; `fileIdLow = 0` uses their base files.
/// Texture offsets count 64-word VRAM columns and CLUT rows. The final record
/// is `AREA_PLACEMENT_END`; actors borrow the live records, so keep this map
/// overlay loaded until they end.
extern AreaPlacement gMapDryfieldFullGarageVariant1Placements[5];

/// Gary Douglas and Flint for night Garage placement variant 2.
///
/// World-coordinate XYZ and yaw (4096 units per turn). Entry keys select
/// the paired room-owned resource recipes; `fileIdLow = 0` uses their base files.
/// Texture offsets count 64-word VRAM columns and CLUT rows. The final record
/// is `AREA_PLACEMENT_END`; actors borrow the live records, so keep this map
/// overlay loaded until they end.
///
/// Their texture page/CLUT row offsets are respectively 0/2 and 2/4.
extern AreaPlacement gMapDryfieldFullGarageVariant2Placements[3];

/// Empty placement list for night Garage placement variant 3's companion scene.
///
/// Only `AREA_PLACEMENT_END`; room initialization restores the companion separately.
/// Keep this map overlay loaded while the room variant borrows the list.
extern AreaPlacement gMapDryfieldFullGarageVariant3Placements[1];

/// Three Zebra Stalkers for night Garage placement variant 4.
///
/// World-coordinate XYZ and yaw (4096 units per turn). Entry keys select
/// the paired room-owned resource recipes; `fileIdLow = 0` uses their base files.
/// Texture offsets count 64-word VRAM columns and CLUT rows. The final record
/// is `AREA_PLACEMENT_END`; actors borrow the live records, so keep this map
/// overlay loaded until they end.
///
/// All use ordinary floor entrances with ceiling perching disabled.
extern AreaPlacement gMapDryfieldFullGarageVariant4Placements[4];

/// Thirteen bats for night Driveway placement variant 1.
///
/// World-coordinate XYZ and yaw (4096 units per turn). Entry keys select
/// the paired room-owned resource recipes; `fileIdLow = 0` uses their base files.
/// Texture offsets count 64-word VRAM columns and CLUT rows. The final record
/// is `AREA_PLACEMENT_END`; actors borrow the live records, so keep this map
/// overlay loaded until they end.
///
/// Three push backward from perches and ten start wandering; all use turn-rate row 4.
extern AreaPlacement gMapDryfieldFullDrivewayVariant1Placements[14];

/// Twelve perched bats for night Driveway placement variant 3.
///
/// World-coordinate XYZ and yaw (4096 units per turn). Entry keys select
/// the paired room-owned resource recipes; `fileIdLow = 0` uses their base files.
/// Texture offsets count 64-word VRAM columns and CLUT rows. The final record
/// is `AREA_PLACEMENT_END`; actors borrow the live records, so keep this map
/// overlay loaded until they end.
///
/// All push backward from their perches, in groups of four using turn-rate rows 5, 4 and 3.
extern AreaPlacement gMapDryfieldFullDrivewayVariant3Placements[13];

/// Three Scorpions and five Mind Sucklers for night Driveway placement variant 21.
///
/// World-coordinate XYZ and yaw (4096 units per turn). Entry keys select
/// the paired room-owned resource recipes; `fileIdLow = 0` uses their base files.
/// Texture offsets count 64-word VRAM columns and CLUT rows. The final record
/// is `AREA_PLACEMENT_END`; actors borrow the live records, so keep this map
/// overlay loaded until they end.
///
/// Scorpions use signalled ambush mode and tuning row 3; Sucklers start on walls and cannot return to walls.
extern AreaPlacement gMapDryfieldFullDrivewayVariant21Placements[9];

/// Two scripted-entrance Zebra Stalkers for night Junk Yard placement variant 1.
///
/// World-coordinate XYZ and yaw (4096 units per turn). Entry keys select
/// the paired room-owned resource recipes; `fileIdLow = 0` uses their base files.
/// Texture offsets count 64-word VRAM columns and CLUT rows. The final record
/// is `AREA_PLACEMENT_END`; actors borrow the live records, so keep this map
/// overlay loaded until they end.
///
/// The first uses the junk-yard entrance and the second the second drop entrance; neither can perch on ceilings.
extern AreaPlacement gMapDryfieldFullJunkYardVariant1Placements[3];

/// Two Mind Sucklers and two Desert Chasers for night Junk Yard placement variant 2.
///
/// World-coordinate XYZ and yaw (4096 units per turn). Entry keys select
/// the paired room-owned resource recipes; `fileIdLow = 0` uses their base files.
/// Texture offsets count 64-word VRAM columns and CLUT rows. The final record
/// is `AREA_PLACEMENT_END`; actors borrow the live records, so keep this map
/// overlay loaded until they end.
///
/// Sucklers start on walls and cannot return to walls; Chasers select tuning row 2.
extern AreaPlacement gMapDryfieldFullJunkYardVariant2Placements[5];

/// Three Zebra Stalkers for night Junk Yard placement variant 4.
///
/// World-coordinate XYZ and yaw (4096 units per turn). Entry keys select
/// the paired room-owned resource recipes; `fileIdLow = 0` uses their base files.
/// Texture offsets count 64-word VRAM columns and CLUT rows. The final record
/// is `AREA_PLACEMENT_END`; actors borrow the live records, so keep this map
/// overlay loaded until they end.
///
/// All use ordinary floor entrances with ceiling perching disabled.
extern AreaPlacement gMapDryfieldFullJunkYardVariant4Placements[4];

/// A patrolling Pawn GOLEM and a Rook GOLEM for night Junk Yard placement variant 7.
///
/// World-coordinate XYZ and yaw (4096 units per turn). Entry keys select
/// the paired room-owned resource recipes; `fileIdLow = 0` uses their base files.
/// Texture offsets count 64-word VRAM columns and CLUT rows. The final record
/// is `AREA_PLACEMENT_END`; actors borrow the live records, so keep this map
/// overlay loaded until they end.
///
/// The Pawn uses resource key 56 and 4000-unit patrol legs; the Rook uses Beam Sword resource key 23 and starts idle.
extern AreaPlacement gMapDryfieldFullJunkYardVariant7Placements[3];

/// Four Mind Sucklers and one Zebra Stalker for night Junk Yard placement variant 21.
///
/// World-coordinate XYZ and yaw (4096 units per turn). Entry keys select
/// the paired room-owned resource recipes; `fileIdLow = 0` uses their base files.
/// Texture offsets count 64-word VRAM columns and CLUT rows. The final record
/// is `AREA_PLACEMENT_END`; actors borrow the live records, so keep this map
/// overlay loaded until they end.
///
/// Sucklers start on walls and cannot return to walls; the Stalker cannot perch on ceilings.
extern AreaPlacement gMapDryfieldFullJunkYardVariant21Placements[6];

/// Gary Douglas for night Trailer Coach placement variant 1.
///
/// World-coordinate XYZ and yaw (4096 units per turn). Entry keys select
/// the paired room-owned resource recipes; `fileIdLow = 0` uses their base files.
/// Texture offsets count 64-word VRAM columns and CLUT rows. The final record
/// is `AREA_PLACEMENT_END`; actors borrow the live records, so keep this map
/// overlay loaded until they end.
///
/// His initial transform is (3810, 0, -2130), yaw 512; resource key 106 selects the room's Gary recipe.
extern AreaPlacement gMapDryfieldFullTrailerCoachVariant1Placements[2];

/// Two Odd Strangers and three Moths for night Motel Room 5 placement variant 1.
///
/// World-coordinate XYZ and yaw (4096 units per turn). Entry keys select
/// the paired room-owned resource recipes; `fileIdLow = 0` uses their base files.
/// Texture offsets count 64-word VRAM columns and CLUT rows. The final record
/// is `AREA_PLACEMENT_END`; actors borrow the live records, so keep this map
/// overlay loaded until they end.
///
/// Odd Strangers switch from pursuit to stalking; Moths use pursuit-speed row 2.
extern AreaPlacement gMapDryfieldFullMotelRoom5Variant1Placements[6];

/// Five posed Scavengers for night Motel Room 5 placement variant 2.
///
/// World-coordinate XYZ and yaw (4096 units per turn). Entry keys select
/// the paired room-owned resource recipes; `fileIdLow = 0` uses their base files.
/// Texture offsets count 64-word VRAM columns and CLUT rows. The final record
/// is `AREA_PLACEMENT_END`; actors borrow the live records, so keep this map
/// overlay loaded until they end.
///
/// The actor-defined variant bit forces posed behavior while its running behavior remains eligible.
extern AreaPlacement gMapDryfieldFullMotelRoom5Variant2Placements[6];

/// Five active Scavengers for night Motel Balcony placement variant 1.
///
/// World-coordinate XYZ and yaw (4096 units per turn). Entry keys select
/// the paired room-owned resource recipes; `fileIdLow = 0` uses their base files.
/// Texture offsets count 64-word VRAM columns and CLUT rows. The final record
/// is `AREA_PLACEMENT_END`; actors borrow the live records, so keep this map
/// overlay loaded until they end.
extern AreaPlacement gMapDryfieldFullMotelBalconyVariant1Placements[6];

/// Burner, Gary Douglas and Flint for night Motel Balcony placement variant 2.
///
/// World-coordinate XYZ and yaw (4096 units per turn). Entry keys select
/// the paired room-owned resource recipes; `fileIdLow = 0` uses their base files.
/// Texture offsets count 64-word VRAM columns and CLUT rows. The final record
/// is `AREA_PLACEMENT_END`; actors borrow the live records, so keep this map
/// overlay loaded until they end.
///
/// Gary and Flint have zero initial XYZ/yaw and share texture offsets 4/6; both zero-transform records are live placements.
extern AreaPlacement gMapDryfieldFullMotelBalconyVariant2Placements[4];

/// Two Zebra Stalkers for night Motel Balcony placement variant 3.
///
/// World-coordinate XYZ and yaw (4096 units per turn). Entry keys select
/// the paired room-owned resource recipes; `fileIdLow = 0` uses their base files.
/// Texture offsets count 64-word VRAM columns and CLUT rows. The final record
/// is `AREA_PLACEMENT_END`; actors borrow the live records, so keep this map
/// overlay loaded until they end.
///
/// Both use ordinary floor entrances with ceiling perching disabled and `fileIdLow = 0` (base resource files).
extern AreaPlacement gMapDryfieldFullMotelBalconyVariant3Placements[3];

/// Five Mind Sucklers for night Motel Balcony placement variant 11.
///
/// World-coordinate XYZ and yaw (4096 units per turn). Entry keys select
/// the paired room-owned resource recipes; `fileIdLow = 0` uses their base files.
/// Texture offsets count 64-word VRAM columns and CLUT rows. The final record
/// is `AREA_PLACEMENT_END`; actors borrow the live records, so keep this map
/// overlay loaded until they end.
///
/// All start on walls, cannot return to walls and use `fileIdLow = 0` (base resource files).
extern AreaPlacement gMapDryfieldFullMotelBalconyVariant11Placements[6];

/// Three scripted-entrance Zebra Stalkers for night Motel Loft placement variant 1.
///
/// World-coordinate XYZ and yaw (4096 units per turn). Entry keys select
/// the paired room-owned resource recipes; `fileIdLow = 0` uses their base files.
/// Texture offsets count 64-word VRAM columns and CLUT rows. The final record
/// is `AREA_PLACEMENT_END`; actors borrow the live records, so keep this map
/// overlay loaded until they end.
///
/// The first uses the first drop entrance and the other two wait for the group trigger; none can perch on ceilings.
extern AreaPlacement gMapDryfieldFullMotelLoftVariant1Placements[4];

/// One Zebra Stalker and four Scavengers for night Motel Loft placement variant 3.
///
/// World-coordinate XYZ and yaw (4096 units per turn). Entry keys select
/// the paired room-owned resource recipes; `fileIdLow = 0` uses their base files.
/// Texture offsets count 64-word VRAM columns and CLUT rows. The final record
/// is `AREA_PLACEMENT_END`; actors borrow the live records, so keep this map
/// overlay loaded until they end.
///
/// The Stalker cannot perch on ceilings; Scavengers use the room's actor-301600 recipe.
extern AreaPlacement gMapDryfieldFullMotelLoftVariant3Placements[6];

/// Seven posed Scavengers for night Motel Loft placement variant 4.
///
/// Ends with `AREA_PLACEMENT_END`; actors borrow these records from this overlay.
extern AreaPlacement gMapDryfieldFullMotelLoftVariant4Placements[8];

/// Six active Scavengers for night Motel Loft placement variant 11.
///
/// Ends with `AREA_PLACEMENT_END`; actors borrow these records from this overlay.
extern AreaPlacement gMapDryfieldFullMotelLoftVariant11Placements[7];

/// One Zebra Stalker with a water entrance and no ceiling perch, for night Water Hole variant 1.
///
/// Ends with `AREA_PLACEMENT_END`; the actor borrows its record from this overlay.
extern AreaPlacement gMapDryfieldFullWaterHoleVariant1Placements[2];

/// Two ordinary Mossbacks for night Water Hole placement variant 2.
///
/// Ends with `AREA_PLACEMENT_END`; actors borrow these records from this overlay.
extern AreaPlacement gMapDryfieldFullWaterHoleVariant2Placements[3];

/// Empty placement list for night Water Hole variant 10's ending scene.
///
/// Only `AREA_PLACEMENT_END`; the variant still loads its scene controller,
/// which room initialization starts separately. Keep this overlay loaded for lookup.
extern AreaPlacement gMapDryfieldFullWaterHoleVariant10Placements[1];

/// Three Zebra Stalkers without ceiling perches, for night Water Hole variant 21.
///
/// Ends with `AREA_PLACEMENT_END`; actors borrow these records from this overlay.
extern AreaPlacement gMapDryfieldFullWaterHoleVariant21Placements[4];

/// Five active Scavengers for night Cellar placement variant 1.
///
/// Ends with `AREA_PLACEMENT_END`; actors borrow these records from this overlay.
extern AreaPlacement gMapDryfieldFullCellarVariant1Placements[6];

/// Six wandering bats and five rats for night Cellar placement variant 3.
///
/// Bats use turn-rate row 4. Ends with `AREA_PLACEMENT_END`; actors borrow
/// these records from this overlay.
extern AreaPlacement gMapDryfieldFullCellarVariant3Placements[12];

/// Two Scavengers and eight perched bats for night Cellar placement variant 11.
///
/// Bats use turn-rate row 5. Ends with `AREA_PLACEMENT_END`; actors borrow
/// these records from this overlay.
extern AreaPlacement gMapDryfieldFullCellarVariant11Placements[11];

/// One Gray Stalker for night Underpass placement variant 1.
///
/// Ends with `AREA_PLACEMENT_END`; the actor borrows its record from this overlay.
extern AreaPlacement gMapDryfieldFullUnderpassVariant1Placements[2];

/// Five active Scavengers for night Underpass placement variant 2.
///
/// Ends with `AREA_PLACEMENT_END`; actors borrow these records from this overlay.
extern AreaPlacement gMapDryfieldFullUnderpassVariant2Placements[6];

/// Three Mossbacks with movement-sensitive awareness, for night Underpass variant 3.
///
/// Ends with `AREA_PLACEMENT_END`; actors borrow these records from this overlay.
extern AreaPlacement gMapDryfieldFullUnderpassVariant3Placements[4];

/// One Knight GOLEM for night Underpass placement variant 7.
///
/// Ends with `AREA_PLACEMENT_END`; the actor borrows its record from this overlay.
extern AreaPlacement gMapDryfieldFullUnderpassVariant7Placements[2];

/// Seven ceiling-perched Mind Sucklers barred from wall reperching, for night Underpass variant 11.
///
/// Ends with `AREA_PLACEMENT_END`; actors borrow these records from this overlay.
extern AreaPlacement gMapDryfieldFullUnderpassVariant11Placements[8];

/// Dryfield-at-night battle rewards for normal, replay and Scavenger runs.
///
/// Twenty-two location-keyed rows and `INVENTORY_BATTLE_REWARD_LIST_END`.
/// Each nonzero item slot grants one pack; slot 3 requires a Medicine Wheel.
/// Keys select stage 3, area and placement variant, independent of room/view.
/// Keep this overlay loaded during reward lookup.
extern InventoryBattleReward gMapDryfieldFullBattleRewards[23];

/// Dryfield-at-night battle rewards for Bounty and Nightmare runs.
///
/// Eight location-keyed rows and `INVENTORY_BATTLE_REWARD_LIST_END`.
/// Each nonzero item slot grants one pack; slot 3 requires a Medicine Wheel.
/// Keys select stage 3, area and placement variant, independent of room/view.
/// Keep this overlay loaded during reward lookup.
extern InventoryBattleReward gMapDryfieldFullHardBattleRewards[9];

/// Resolves the night Junk Yard's destination room from event progress.
///
/// On ROOM_EVENT_EXECUTE for that area, writes only reply->room to the event
/// progress nibble plus 1 (1..16). The flag must select a room present in the
/// destination's tables. Queries and other areas leave the reply untouched;
/// initialize it before calling. The borrowed records may alias and are never
/// retained. Always returns 1, without testing passage gates. Requires this
/// map overlay to remain loaded for the call.
s32 mapDryfieldFullResolveRoomVariant(const RoomEventMsg* request, RoomEventMsg* reply);

#endif // INCLUDE_MAPUI_MAP_DRYFIELD_FULL_H
