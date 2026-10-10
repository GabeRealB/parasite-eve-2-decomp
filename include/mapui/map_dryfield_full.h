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

extern ViewCountTable D_map_dryfield_full_8017A93C;

extern WorldCollisionStageResources D_map_dryfield_full_8017A9D8;

extern ViewCameraTable D_map_dryfield_full_8017AA74;

/// Dryfield-at-night directory of room-local logical-view mappings.
///
/// `viewMaps[area - 1][room - 1][view - 1]` yields the unsigned, 1-based
/// camera/image/sprite index. The area directory covers IDs 1..38, with NULL
/// entries for 4, 10, 33 and 35..37; no level has a terminator. Lookups require
/// populated slots and IDs within each borrowed room directory and byte map.
/// Keep this map overlay and the selected room overlay loaded; the mappings
/// are borrowed and may be changed during play.
extern ViewIndexTable gMapDryfieldFullViewIndexTable;

extern SpriteAreaTable D_map_dryfield_full_8017ABAC;

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

extern AreaPlacement D_map_dryfield_full_8017B4A8[];

extern AreaPlacement D_map_dryfield_full_8017B508[];

extern AreaPlacement D_map_dryfield_full_8017B558[];

extern AreaPlacement D_map_dryfield_full_8017B5D8[];

extern AreaPlacement D_map_dryfield_full_8017B648[];

extern AreaPlacement D_map_dryfield_full_8017B6A8[];

extern AreaPlacement D_map_dryfield_full_8017B708[];

extern AreaPlacement D_map_dryfield_full_8017B798[];

extern AreaPlacement D_map_dryfield_full_8017B7D8[];

extern AreaPlacement D_map_dryfield_full_8017B858[];

extern AreaPlacement D_map_dryfield_full_8017B8C8[];

extern AreaPlacement D_map_dryfield_full_8017B938[];

extern AreaPlacement D_map_dryfield_full_8017B998[];

extern AreaPlacement D_map_dryfield_full_8017BA08[];

extern AreaPlacement D_map_dryfield_full_8017BA38[];

extern AreaPlacement D_map_dryfield_full_8017BAB8[];

extern AreaPlacement D_map_dryfield_full_8017BB28[];

extern AreaPlacement D_map_dryfield_full_8017BB78[];

extern AreaPlacement D_map_dryfield_full_8017BBE8[];

extern AreaPlacement D_map_dryfield_full_8017BC08[];

extern AreaPlacement D_map_dryfield_full_8017BCC8[];

extern AreaPlacement D_map_dryfield_full_8017BD38[];

extern AreaPlacement D_map_dryfield_full_8017BDF8[];

extern AreaPlacement D_map_dryfield_full_8017BE58[];

extern AreaPlacement D_map_dryfield_full_8017BE78[];

extern AreaPlacement D_map_dryfield_full_8017BEA8[];

extern AreaPlacement D_map_dryfield_full_8017BF08[];

extern AreaPlacement D_map_dryfield_full_8017BF88[];

extern AreaPlacement D_map_dryfield_full_8017C038[];

extern AreaPlacement D_map_dryfield_full_8017C0B8[];

extern AreaPlacement D_map_dryfield_full_8017C158[];

extern AreaPlacement D_map_dryfield_full_8017C198[];

extern AreaPlacement D_map_dryfield_full_8017C1B8[];

extern AreaPlacement D_map_dryfield_full_8017C228[];

extern AreaPlacement D_map_dryfield_full_8017C238[];

extern AreaPlacement D_map_dryfield_full_8017C288[];

extern AreaPlacement D_map_dryfield_full_8017C2A8[];

extern AreaPlacement D_map_dryfield_full_8017C378[];

extern AreaPlacement D_map_dryfield_full_8017C448[];

extern AreaPlacement D_map_dryfield_full_8017C4E8[];

extern AreaPlacement D_map_dryfield_full_8017C538[];

extern AreaPlacement D_map_dryfield_full_8017C568[];

extern AreaPlacement D_map_dryfield_full_8017C578[];

extern AreaPlacement D_map_dryfield_full_8017C5B8[];

extern AreaPlacement D_map_dryfield_full_8017C698[];

extern AreaPlacement D_map_dryfield_full_8017C768[];

extern AreaPlacement D_map_dryfield_full_8017C7F8[];

extern AreaPlacement D_map_dryfield_full_8017C828[];

extern AreaPlacement D_map_dryfield_full_8017C878[];

extern AreaPlacement D_map_dryfield_full_8017C8B8[];

extern AreaPlacement D_map_dryfield_full_8017C8E8[];

extern AreaPlacement D_map_dryfield_full_8017C948[];

extern AreaPlacement D_map_dryfield_full_8017C968[];

extern AreaPlacement D_map_dryfield_full_8017C9C8[];

extern AreaPlacement D_map_dryfield_full_8017CA28[];

extern AreaPlacement D_map_dryfield_full_8017CA88[];

extern AreaPlacement D_map_dryfield_full_8017CAC8[];

extern AreaPlacement D_map_dryfield_full_8017CAF8[];

extern AreaPlacement D_map_dryfield_full_8017CB58[];

extern AreaPlacement D_map_dryfield_full_8017CB98[];

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
