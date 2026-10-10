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

/// This stage's `Gp_MapMarkTables` entry: the area shapes, indexed by area.
extern MenuMapAreaShape D_map_neo_ark_8017A110[];

/// This stage's `D_8010F0E0` entry, ended by a page of 0.
extern MenuMapMarker D_map_neo_ark_8017A230[];

/// Fixed-picture and telephone icons for the Shelter/Neo Ark map.
///
/// Heliport has a kind-0 fixed picture and a telephone; Nursery has a telephone.
/// All three require their area to have been visited, with no flag gate.
/// Coordinates are signed map pixels; the page-zero record ends the list.
/// The kind-0 picture's subject is unproven. Keep the map overlay loaded.
extern MenuMapIcon gMapNeoArkIcons[4];

/// This stage's `Gp_MapNameTables` entry: the room names, indexed by room - 1.
extern MenuMapAreaName D_map_neo_ark_8017A28C[];

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

/// This stage's `D_8010FABC` entry: the task each room starts, keyed by
/// `GP_TASK_LOC_KEY` and ended by flags of 0xFFFF.
extern TaskDesc D_map_neo_ark_8017A804[];

/// This stage's flag table for `menuMapGetMarkerState`.
extern u16 D_map_neo_ark_8017A9A0[];

/// Per-area room-lighting directories for the 33 Shelter/Neo Ark areas.
///
/// Select area - 1, then room - 1. Every area slot is populated; the nine
/// map-owned arrays cover room 1 only. Other descriptor arrays, light
/// collections and ambient tables are borrowed from the selected room
/// overlay and require it to remain loaded. The map directory stays loaded
/// for the stage; there is no count header or terminator.
extern WorldCoordRoomLighting* gMapNeoArkRoomLightingTables[33];

/// This stage's entries in `Gp_WarpTables`, `Gp_ViewCountTables`,
/// `Gp_RoomObjTables`, `Gp_ViewTables` and `gSpriteAreaTables`: each leads to
/// one pointer per room, into that room's package or, for some rooms, at a
/// record this overlay holds itself.
extern DirectionWarpEntry* D_map_neo_ark_8017AA80[];

extern ViewCountTable D_map_neo_ark_8017AB88;

extern WorldCollisionStageResources D_map_neo_ark_8017ACA0;

extern ViewCameraTable D_map_neo_ark_8017AD28;

/// Logical-view mapping directory for the 33 Shelter/Neo Ark areas.
///
/// Select area - 1, room - 1 and logical view - 1; the byte is a 1-based
/// camera/image/sprite index. Every area slot is populated. Borrows mutable
/// room directories and maps (including gameplay's identity map); room-owned
/// storage must remain loaded. Map capacities and reverse-search view counts
/// are separate, and none of these directories has a terminator.
extern ViewIndexTable gMapNeoArkViewIndexTable;

extern SpriteAreaTable D_map_neo_ark_8017AE38;

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

/// Area placement lists, each ended by an entry id of 0xFF. The rooms' area
/// records point at them, one list per location; one location's list runs on
/// into the next one's.
extern AreaPlacement D_map_neo_ark_8017AF30[];

extern AreaPlacement D_map_neo_ark_8017AF80[];

extern AreaPlacement D_map_neo_ark_8017AFD0[];

extern AreaPlacement D_map_neo_ark_8017AFF0[];

extern AreaPlacement D_map_neo_ark_8017B010[];

extern AreaPlacement D_map_neo_ark_8017B030[];

extern AreaPlacement D_map_neo_ark_8017B080[];

extern AreaPlacement D_map_neo_ark_8017B0F0[];

extern AreaPlacement D_map_neo_ark_8017B120[];

extern AreaPlacement D_map_neo_ark_8017B160[];

extern AreaPlacement D_map_neo_ark_8017B190[];

extern AreaPlacement D_map_neo_ark_8017B1C0[];

extern AreaPlacement D_map_neo_ark_8017B1F0[];

extern AreaPlacement D_map_neo_ark_8017B210[];

extern AreaPlacement D_map_neo_ark_8017B2C0[];

extern AreaPlacement D_map_neo_ark_8017B300[];

extern AreaPlacement D_map_neo_ark_8017B330[];

extern AreaPlacement D_map_neo_ark_8017B360[];

extern AreaPlacement D_map_neo_ark_8017B390[];

extern AreaPlacement D_map_neo_ark_8017B3C0[];

extern AreaPlacement D_map_neo_ark_8017B3D0[];

extern AreaPlacement D_map_neo_ark_8017B410[];

extern AreaPlacement D_map_neo_ark_8017B470[];

extern AreaPlacement D_map_neo_ark_8017B4B0[];

extern AreaPlacement D_map_neo_ark_8017B4E0[];

extern AreaPlacement D_map_neo_ark_8017B530[];

extern AreaPlacement D_map_neo_ark_8017B550[];

extern AreaPlacement D_map_neo_ark_8017B590[];

extern AreaPlacement D_map_neo_ark_8017B5C0[];

extern AreaPlacement D_map_neo_ark_8017B600[];

extern AreaPlacement D_map_neo_ark_8017B610[];

extern AreaPlacement D_map_neo_ark_8017B630[];

extern AreaPlacement D_map_neo_ark_8017B6E0[];

extern AreaPlacement D_map_neo_ark_8017B700[];

extern AreaPlacement D_map_neo_ark_8017B780[];

extern AreaPlacement D_map_neo_ark_8017B7D0[];

extern AreaPlacement D_map_neo_ark_8017B850[];

extern AreaPlacement D_map_neo_ark_8017B8B0[];

extern AreaPlacement D_map_neo_ark_8017B8D0[];

extern AreaPlacement D_map_neo_ark_8017B950[];

extern AreaPlacement D_map_neo_ark_8017B9E0[];

extern AreaPlacement D_map_neo_ark_8017BA60[];

extern AreaPlacement D_map_neo_ark_8017BB00[];

extern AreaPlacement D_map_neo_ark_8017BB70[];

extern AreaPlacement D_map_neo_ark_8017BBF0[];

extern AreaPlacement D_map_neo_ark_8017BC70[];

extern AreaPlacement D_map_neo_ark_8017BCE0[];

extern AreaPlacement D_map_neo_ark_8017BD50[];

extern AreaPlacement D_map_neo_ark_8017BD80[];

extern AreaPlacement D_map_neo_ark_8017BDB0[];

extern AreaPlacement D_map_neo_ark_8017BDC0[];

extern AreaPlacement D_map_neo_ark_8017BDD0[];

extern AreaPlacement D_map_neo_ark_8017BE40[];

extern AreaPlacement D_map_neo_ark_8017BE70[];

extern AreaPlacement D_map_neo_ark_8017BEB0[];

extern AreaPlacement D_map_neo_ark_8017BEF0[];

extern AreaPlacement D_map_neo_ark_8017BF40[];

extern AreaPlacement D_map_neo_ark_8017BF90[];

extern AreaPlacement D_map_neo_ark_8017C020[];

extern AreaPlacement D_map_neo_ark_8017C050[];

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

/// This stage's `D_8010F9F4` and `D_8010FA0C` entries: items awarded
/// for battles won at a location, each list ended by a key of
/// `INVENTORY_BATTLE_REWARD_LIST_END`.
extern InventoryBattleReward D_map_neo_ark_8017C9B0[];

extern InventoryBattleReward D_map_neo_ark_8017CB0C[];

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
/// On `ROOM_EVENT_EXECUTE`, updates only `reply->room` for the observatory,
/// pavilion, altar, shrine and pyramid. Queries and other areas leave the reply
/// untouched. Room IDs are 1-based; the caller must initialize the reply's
/// remaining fields (room handlers copy the request first).
///
/// Both arguments may point to the same record. Neither pointer is retained;
/// game flags are only read. Always returns 1, without testing passage gates.
/// The `map_neo_ark` overlay must remain loaded for the call.
s32 mapNeoArkResolveRoomVariant(RoomEventMsg* request, RoomEventMsg* reply);

#endif // INCLUDE_MAPUI_MAP_NEO_ARK_H
