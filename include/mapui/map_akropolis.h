#ifndef INCLUDE_MAPUI_MAP_AKROPOLIS_H
#define INCLUDE_MAPUI_MAP_AKROPOLIS_H

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

/// Places movie decode buffers for the current Acropolis area.
///
/// Borrows `location` only for its area ID. Main must first cache movie pixel
/// dimensions, reserve the movie workspace and actor-buffer-0 ring/table region,
/// and reset staging flags. The 32-sector ring occupies 0x10000 bytes; all VLC
/// and decoded-frame extents must fit the reserved regions and be word-aligned.
/// Layouts reuse actor buffers, clear their session reuse fields and stage
/// decoded columns in VRAM at (704,0) or (384,256), in words/rows. Other areas
/// retain the preceding buffer pointers. No allocation or bounds check occurs.
/// Storage remains borrowed by the decoder through playback.
void mapAkropolisSetupMovieBuffers(const GameLocationKey* location);

/// Queues the MIST briefing's four-item menu as a display-mode task.
///
/// `unused` is ignored. With zero `retainSelection`, marks all four briefing
/// items collected and clears the Dryfield-map choice; nonzero preserves it.
/// Always returns 1, including when queueing fails. The singleton list and
/// choice storage require this overlay to remain loaded throughout the menu.
s32 mapAkropolisOpenKeyItemMenu(s32 unused, s32 retainSelection);

/// Returns 1 once the briefing menu has opened the Dryfield map, otherwise 0.
///
/// The latch survives menu closure and reopening with nonzero retainSelection.
/// A fresh menu request clears it; opening another item's detail does not.
s32 mapAkropolisWasDryfieldMapSelected(void);

/// Captured-frame and auxiliary-memory regions for Acropolis and MIST areas.
///
/// Indexed directly by area ID, 0..21; slot 0 is empty and cannot be used for
/// frame transfer. Each nonempty extent ends at the resident image workspace,
/// rather than measuring the captured frame. The map overlay must be loaded
/// while selecting a slot; the selected arena is separate borrowed RAM.
extern GfxImageSlot gMapAkropolisImageSlots[GAME_AREA_MIST_R21 + 1];

/// Movie-workspace reservations in bytes for Acropolis and MIST areas.
///
/// Indexed directly by the live save's area ID, 0..20; zero requests no
/// allocation. Only Plaza, Security room, Fountain, Forked road, Observatory,
/// Promenade and Bridge reserve storage. Area 21 has no entry. These are total
/// workspace requests, not decoded-frame sizes; the map overlay must be loaded
/// when the CD subsystem reads them.
extern s32 gMapAkropolisMovieWorkspaceBytesByArea[GAME_AREA_MIST_SHOOTING_GALLERY + 1];

/// Per-map task id tables `Stage_MusicTables` and `Stage_CountdownMusicTables` index.
extern StageMusicEntry D_map_akropolis_8017C1B4[];

extern StageMusicEntry D_map_akropolis_8017C304[];

/// This stage's `Gp_MapFlagIds` entry, indexed by map room id.
extern u8 D_map_akropolis_8017A14C[];

/// This stage's `Gp_MapRecTables` entry: where each room sits on the map, by room.
extern MenuMapArea D_map_akropolis_8017A154[];

/// This stage's `Gp_MapMarkTables` entry: the area shapes, indexed by area.
extern MenuMapAreaShape D_map_akropolis_8017A288[];

/// This stage's `D_8010F0E0` entry, ended by a page of 0.
extern MenuMapMarker D_map_akropolis_8017A330[];

/// This stage's `D_8010F0CC` entry, ended by a page of 0.
extern MenuMapIcon D_map_akropolis_8017A38C[];

/// This stage's `Gp_MapNameTables` entry: the room names, indexed by room - 1.
extern MenuMapAreaName D_map_akropolis_8017A3BC[];

/// This stage's `Gp_Bit2Banks` room table, ended by `AREA_OBJECT_ROOM_END`.
extern AreaObjectRoom D_map_akropolis_8017A7FC[];

/// This stage's `D_8010FABC` entry: the task each room starts, keyed by
/// `GP_TASK_LOC_KEY` and ended by flags of 0xFFFF.
extern TaskDesc D_map_akropolis_8017A8AC[];

/// This stage's flag table for `menuMapGetMarkerState`.
extern u16 D_map_akropolis_8017AA0C[];

/// Per-area room-lighting arrays for Acropolis and MIST.
///
/// Indexed by area - 1 for areas 1..20, then room - 1 within the borrowed
/// array. There is no terminator or area-21 entry. Each room overlay owns its
/// descriptors and must remain loaded while they are read or updated; the
/// map overlay owns only this area directory.
extern WorldCoordRoomLighting* gMapAkropolisAreaRoomLightingTables[GAME_AREA_MIST_SHOOTING_GALLERY];

extern WorldCollisionStageResources D_map_akropolis_8017AAC8;

extern SpriteAreaTable D_map_akropolis_8017AB1C;

extern DirectionWarpEntry* D_map_akropolis_8017AB20[];

extern ViewCountTable D_map_akropolis_8017ABC0;

extern ViewCameraTable D_map_akropolis_8017AC14;

/// Logical-view mapping directory for Acropolis and MIST areas 1..20.
///
/// Maps area - 1, room - 1 and view - 1 to a 1-based camera/image/sprite index.
/// Has no terminator or area-21 entry. The map overlay owns the area directory;
/// selected room directories and writable maps require their room overlay to
/// remain loaded. Shared identity maps instead belong to gameplay.
extern ViewIndexTable gMapAkropolisViewIndexTable;

/// Per-area collision-surface policy directories for Acropolis and MIST.
///
/// Indexed by area - 1 for areas 1..20, then surface class 0..7, independently
/// of the room variant. There is no terminator or area-21 entry. Each selected
/// class must provide a record in its loaded room overlay; records and footstep
/// tables remain borrowed for that overlay's lifetime.
extern WorldCollisionSurfaceProperties** gMapAkropolisSurfacePropertiesByArea[GAME_AREA_MIST_SHOOTING_GALLERY];

/// Area placement lists, each ended by an entry id of 0xFF. The rooms' area
/// records point at them, one list per location.
extern AreaPlacement D_map_akropolis_8017ACBC[];

extern AreaPlacement D_map_akropolis_8017ACDC[];

extern AreaPlacement D_map_akropolis_8017ACFC[];

extern AreaPlacement D_map_akropolis_8017AD2C[];

extern AreaPlacement D_map_akropolis_8017AD7C[];

extern AreaPlacement D_map_akropolis_8017ADEC[];

extern AreaPlacement D_map_akropolis_8017AE3C[];

extern AreaPlacement D_map_akropolis_8017AE6C[];

extern AreaPlacement D_map_akropolis_8017AE9C[];

extern AreaPlacement D_map_akropolis_8017AEDC[];

extern AreaPlacement D_map_akropolis_8017AF1C[];

extern AreaPlacement D_map_akropolis_8017AF3C[];

extern AreaPlacement D_map_akropolis_8017AF6C[];

extern AreaPlacement D_map_akropolis_8017AFCC[];

extern AreaPlacement D_map_akropolis_8017AFEC[];

extern AreaPlacement D_map_akropolis_8017B01C[];

extern AreaPlacement D_map_akropolis_8017B04C[];

extern AreaPlacement D_map_akropolis_8017B0DC[];

extern AreaPlacement D_map_akropolis_8017B0FC[];

extern AreaPlacement D_map_akropolis_8017B11C[];

extern AreaPlacement D_map_akropolis_8017B17C[];

extern AreaPlacement D_map_akropolis_8017B1AC[];

extern AreaPlacement D_map_akropolis_8017B1DC[];

/// One Stranger and three Rats for forked-road variant 3.
///
/// Entry keys 10 and 7 select the Stranger and Rat recipes.
/// XYZ uses world units; yaw uses 4096 units per turn. `fileIdLow` is zero
/// (base resources). The final `AREA_PLACEMENT_END` record is included in the
/// extent; keep the map overlay loaded while actors borrow these records.
extern AreaPlacement gMapAkropolisForkedRoadVariant3Placements[5];

/// Four Maggot leap entrances for forked-road variant 4.
///
/// Entry 55 uses leap mode 10, with entrance slots 3, 0, 2 and 1.
/// XYZ uses world units; yaw uses 4096 units per turn. `fileIdLow` is zero
/// (base resources). The final `AREA_PLACEMENT_END` record is included in the
/// extent; keep the map overlay loaded while actors borrow these records.
extern AreaPlacement gMapAkropolisForkedRoadVariant4Placements[5];

/// One patrolling Pawn GOLEM and three Rats for forked-road variant 7.
///
/// Entry 20 patrols 3000 world units; entry 7 uses tuning row 7.
/// XYZ uses world units; yaw uses 4096 units per turn. `fileIdLow` is zero
/// (base resources). The final `AREA_PLACEMENT_END` record is included in the
/// extent; keep the map overlay loaded while actors borrow these records.
extern AreaPlacement gMapAkropolisForkedRoadVariant7Placements[5];

/// Four Caterpillar leap entrances for forked-road variant 8.
///
/// Entry 26 uses leap mode 10, entrance slots 3, 0, 2 and 1, and tuning row 7.
/// XYZ uses world units; yaw uses 4096 units per turn. `fileIdLow` is zero
/// (base resources). The final `AREA_PLACEMENT_END` record is included in the
/// extent; keep the map overlay loaded while actors borrow these records.
extern AreaPlacement gMapAkropolisForkedRoadVariant8Placements[5];

/// One scripted dormant Odd Stranger for forked-road variant 9.
///
/// Entry 18 uses variant selector 4 and the default pursuit tuning.
/// XYZ uses world units; yaw uses 4096 units per turn. `fileIdLow` is zero
/// (base resources). The final `AREA_PLACEMENT_END` record is included in the
/// extent; keep the map overlay loaded while actors borrow these records.
extern AreaPlacement gMapAkropolisForkedRoadVariant9Placements[2];

/// Six Rats and five Moths for forked-road variant 10.
///
/// Entry keys 7 and 8 select separate recipes from the same actor library.
/// Their tuning rows remain in 0..7.
/// XYZ uses world units; yaw uses 4096 units per turn. `fileIdLow` is zero
/// (base resources). The final `AREA_PLACEMENT_END` record is included in the
/// extent; keep the map overlay loaded while actors borrow these records.
extern AreaPlacement gMapAkropolisForkedRoadVariant10Placements[12];

/// Six Amoebas for forked-road variant 11.
///
/// Entry 24 alternates normal and alternate parameter/palette variants.
/// XYZ uses world units; yaw uses 4096 units per turn. `fileIdLow` is zero
/// (base resources). The final `AREA_PLACEMENT_END` record is included in the
/// extent; keep the map overlay loaded while actors borrow these records.
extern AreaPlacement gMapAkropolisForkedRoadVariant11Placements[7];

/// Six Scorpions for forked-road variant 12.
///
/// Entry 25 starts one roaming Scorpion and five proximity-triggered ambush
/// leaders, using tuning rows 0..5.
/// XYZ uses world units; yaw uses 4096 units per turn. `fileIdLow` is zero
/// (base resources). The final `AREA_PLACEMENT_END` record is included in the
/// extent; keep the map overlay loaded while actors borrow these records.
extern AreaPlacement gMapAkropolisForkedRoadVariant12Placements[7];

/// Four Caterpillars for forked-road variant 13.
///
/// Entry 26 starts dormant, aimed and two hanging-leader placements.
/// XYZ uses world units; yaw uses 4096 units per turn. `fileIdLow` is zero
/// (base resources). The final `AREA_PLACEMENT_END` record is included in the
/// extent; keep the map overlay loaded while actors borrow these records.
extern AreaPlacement gMapAkropolisForkedRoadVariant13Placements[5];

/// Six Bats for forked-road variant 14.
///
/// Entry 37 selects perch-drop, perch-back and wandering starts;
/// tuning rows 0..5 select the turn rates.
/// XYZ uses world units; yaw uses 4096 units per turn. `fileIdLow` is zero
/// (base resources). The final `AREA_PLACEMENT_END` record is included in the
/// extent; keep the map overlay loaded while actors borrow these records.
extern AreaPlacement gMapAkropolisForkedRoadVariant14Placements[7];

/// Two Mind Sucklers for forked-road variant 15.
///
/// Entry 15 starts one unperched and one perched enemy.
/// XYZ uses world units; yaw uses 4096 units per turn. `fileIdLow` is zero
/// (base resources). The final `AREA_PLACEMENT_END` record is included in the
/// extent; keep the map overlay loaded while actors borrow these records.
extern AreaPlacement gMapAkropolisForkedRoadVariant15Placements[3];

/// Six Black Beetles for forked-road variant 16.
///
/// Entry 38 alternates idle and wandering floor starts.
/// XYZ uses world units; yaw uses 4096 units per turn. `fileIdLow` is zero
/// (base resources). The final `AREA_PLACEMENT_END` record is included in the
/// extent; keep the map overlay loaded while actors borrow these records.
extern AreaPlacement gMapAkropolisForkedRoadVariant16Placements[7];

/// One Scavenger for forked-road variant 20.
///
/// Entry 16 selects its default spawn recipe.
/// XYZ uses world units; yaw uses 4096 units per turn. `fileIdLow` is zero
/// (base resources). The final `AREA_PLACEMENT_END` record is included in the
/// extent; keep the map overlay loaded while actors borrow these records.
extern AreaPlacement gMapAkropolisForkedRoadVariant20Placements[2];

/// One Sucklerceph and one Skull Stalker for forked-road variant 21.
///
/// Entry keys 46 and 47 select the two separate task recipes.
/// XYZ uses world units; yaw uses 4096 units per turn. `fileIdLow` is zero
/// (base resources). The final `AREA_PLACEMENT_END` record is included in the
/// extent; keep the map overlay loaded while actors borrow these records.
extern AreaPlacement gMapAkropolisForkedRoadVariant21Placements[3];

/// One Slouch and one Creeping Stranger for forked-road variant 22.
///
/// Entry keys 70 and 72 select the spawned recipes; the resource list also
/// loads entries 71 and 73 without corresponding placements.
/// XYZ uses world units; yaw uses 4096 units per turn. `fileIdLow` is zero
/// (base resources). The final `AREA_PLACEMENT_END` record is included in the
/// extent; keep the map overlay loaded while actors borrow these records.
extern AreaPlacement gMapAkropolisForkedRoadVariant22Placements[3];

/// Two Grinning Strangers for observatory variant 1.
///
/// Entry 19 starts one patrol and one scripted dormant actor (selector 4).
/// XYZ uses world units; yaw uses 4096 units per turn. `fileIdLow` is zero
/// (base resources). The final `AREA_PLACEMENT_END` record is included in the
/// extent; keep the map overlay loaded while actors borrow these records.
extern AreaPlacement gMapAkropolisObservatoryVariant1Placements[3];

/// Two standard Mossbacks for observatory variant 2.
///
/// Entry 11 selects the normal-sized body; variant selectors 1 and 0
/// choose run-or-near and ordinary proximity awareness.
/// XYZ uses world units; yaw uses 4096 units per turn. `fileIdLow` is zero
/// (base resources). The final `AREA_PLACEMENT_END` record is included in the
/// extent; keep the map overlay loaded while actors borrow these records.
extern AreaPlacement gMapAkropolisObservatoryVariant2Placements[3];

/// Nine scripted Bat entrances for observatory variant 3.
///
/// Entry 37 uses scripted entrance mode 30 and turn-rate row 0.
/// XYZ uses world units; yaw uses 4096 units per turn. `fileIdLow` is zero
/// (base resources). The final `AREA_PLACEMENT_END` record is included in the
/// extent; keep the map overlay loaded while actors borrow these records.
extern AreaPlacement gMapAkropolisObservatoryVariant3Placements[10];

/// Two large Mossbacks for observatory variant 7.
///
/// Entry 49 selects the larger body through the same actor task as entry 11;
/// variant selectors 1 and 0 choose its awareness behavior.
/// XYZ uses world units; yaw uses 4096 units per turn. `fileIdLow` is zero
/// (base resources). The final `AREA_PLACEMENT_END` record is included in the
/// extent; keep the map overlay loaded while actors borrow these records.
extern AreaPlacement gMapAkropolisObservatoryVariant7Placements[3];

/// One unmatched placement record for observatory variant 10.
///
/// Its entry key 18 has no match in the selected resource list (key 13),
/// so this record spawns no actor. The reason for that pairing is unproven.
/// XYZ uses world units; yaw uses 4096 units per turn. `fileIdLow` is zero
/// (base resources). The final `AREA_PLACEMENT_END` record is included in the
/// extent; keep the map overlay loaded while actors borrow these records.
extern AreaPlacement gMapAkropolisObservatoryVariant10Placements[2];

/// Two Mossbacks for promenade variant 1.
///
/// Entry 11 uses the actor-301100 recipe with run-or-near awareness.
/// XYZ uses world units; yaw uses 4096 units per turn. `fileIdLow` is zero
/// (base resources). The final `AREA_PLACEMENT_END` record is included in the
/// extent; keep the map overlay loaded while actors borrow these records.
extern AreaPlacement gMapAkropolisPromenadeVariant1Placements[3];

/// Four Rats and three Moths for promenade variant 2.
///
/// Entry keys 7 and 8 select Rat tuning row 1 and Moth tuning row 0.
/// XYZ uses world units; yaw uses 4096 units per turn. `fileIdLow` is zero
/// (base resources). The final `AREA_PLACEMENT_END` record is included in the
/// extent; keep the map overlay loaded while actors borrow these records.
extern AreaPlacement gMapAkropolisPromenadeVariant2Placements[8];

/// Four dormant Maggots and six Moths for promenade variant 3.
///
/// Entry keys 55 and 8 select Maggot tuning row 0 and Moth tuning row 1.
/// XYZ uses world units; yaw uses 4096 units per turn. `fileIdLow` is zero
/// (base resources). The final `AREA_PLACEMENT_END` record is included in the
/// extent; keep the map overlay loaded while actors borrow these records.
extern AreaPlacement gMapAkropolisPromenadeVariant3Placements[11];

/// One Knight GOLEM for promenade variant 7.
///
/// Entry 22 selects its default spawn recipe.
/// XYZ uses world units; yaw uses 4096 units per turn. `fileIdLow` is zero
/// (base resources). The final `AREA_PLACEMENT_END` record is included in the
/// extent; keep the map overlay loaded while actors borrow these records.
extern AreaPlacement gMapAkropolisPromenadeVariant7Placements[2];

/// Four dormant Caterpillars and six Moths for promenade variant 8.
///
/// Entry keys 26 and 8 both use tuning row 7.
/// XYZ uses world units; yaw uses 4096 units per turn. `fileIdLow` is zero
/// (base resources). The final `AREA_PLACEMENT_END` record is included in the
/// extent; keep the map overlay loaded while actors borrow these records.
extern AreaPlacement gMapAkropolisPromenadeVariant8Placements[11];

/// No. 9 GOLEM and Rupert placements for sanctuary variant 1.
///
/// Entry keys 27 and 102 select the two actors; Rupert uses texture-page
/// offset 4 and CLUT-row offset 6.
/// XYZ uses world units; yaw uses 4096 units per turn. `fileIdLow` is zero
/// (base resources). The final `AREA_PLACEMENT_END` record is included in the
/// extent; keep the map overlay loaded while actors borrow these records.
extern AreaPlacement gMapAkropolisSanctuaryNo9AndRupertPlacements[3];

/// Rupert placement and an unused record for sanctuary variant 2.
///
/// The zero-key first record matches no recipe and retains Rupert's
/// placement index 1. Entry 102 uses texture-page offset 4 and CLUT offset 6.
/// XYZ uses world units; yaw uses 4096 units per turn. `fileIdLow` is zero
/// (base resources). The final `AREA_PLACEMENT_END` record is included in the
/// extent; keep the map overlay loaded while actors borrow these records.
extern AreaPlacement gMapAkropolisSanctuaryRupertPlacements[3];

/// One view-parented figure and four Maggot drop entrances for roof-garden variant 1.
///
/// Entry 110 selects the figure; entry 55 selects the room-bound Maggots.
/// Drop mode 11 uses entrance slots 0..3 and tuning row 0.
/// XYZ uses world units; yaw uses 4096 units per turn. `fileIdLow` is zero
/// (base resources). The final `AREA_PLACEMENT_END` record is included in the
/// extent; keep the map overlay loaded while actors borrow these records.
extern AreaPlacement gMapAkropolisRoofGardenVariant1Placements[6];

/// Four hanging Maggots and three Moths for roof-garden variant 2.
///
/// Entry 55 starts three followers and one leader; entry 8 uses Moth row 1.
/// XYZ uses world units; yaw uses 4096 units per turn. `fileIdLow` is zero
/// (base resources). The final `AREA_PLACEMENT_END` record is included in the
/// extent; keep the map overlay loaded while actors borrow these records.
extern AreaPlacement gMapAkropolisRoofGardenVariant2Placements[8];

/// One view-parented figure and four Caterpillar drop entrances for roof-garden variant 7.
///
/// Entry 110 selects the figure; entry 26 selects the room-bound Caterpillars.
/// Drop mode 11 uses entrance slots 0..3 and tuning row 7.
/// XYZ uses world units; yaw uses 4096 units per turn. `fileIdLow` is zero
/// (base resources). The final `AREA_PLACEMENT_END` record is included in the
/// extent; keep the map overlay loaded while actors borrow these records.
extern AreaPlacement gMapAkropolisRoofGardenVariant7Placements[6];

/// Four hanging Caterpillars and three Moths for roof-garden variant 8.
///
/// Entry 26 starts three followers and one leader; both actor kinds use row 7.
/// XYZ uses world units; yaw uses 4096 units per turn. `fileIdLow` is zero
/// (base resources). The final `AREA_PLACEMENT_END` record is included in the
/// extent; keep the map overlay loaded while actors borrow these records.
extern AreaPlacement gMapAkropolisRoofGardenVariant8Placements[8];

/// Six independently indexed enemies for the bridge's default variant.
///
/// All use resource key 41 and the room's enemy recipe. Repeated poses retain
/// separate placement identities for the room's indexed movement and scenes.
/// XYZ uses world units; yaw uses 4096 units per turn. `fileIdLow` is zero
/// (base resources). The final `AREA_PLACEMENT_END` record is included in the
/// extent; keep the map overlay loaded while actors borrow these records.
extern AreaPlacement gMapAkropolisBridgeDefaultPlacements[7];

extern AreaPlacement D_map_akropolis_8017BCEC[];

extern AreaPlacement D_map_akropolis_8017BCFC[];

extern AreaPlacement D_map_akropolis_8017BD0C[];

extern AreaPlacement D_map_akropolis_8017BD2C[];

extern AreaPlacement D_map_akropolis_8017BD4C[];

extern AreaPlacement D_map_akropolis_8017BD8C[];

extern AreaPlacement D_map_akropolis_8017BDBC[];

extern AreaPlacement D_map_akropolis_8017BDEC[];

/// This stage's `D_8010F9F4` and `D_8010FA0C` entries: items awarded
/// for battles won at a location, each list ended by a key of
/// `INVENTORY_BATTLE_REWARD_LIST_END`.
extern InventoryBattleReward D_map_akropolis_8017C0DC[];

extern InventoryBattleReward D_map_akropolis_8017C16C[];

#endif // INCLUDE_MAPUI_MAP_AKROPOLIS_H
