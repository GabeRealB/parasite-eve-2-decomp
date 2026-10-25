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

/// Live fixed-marker count; the marker records add a page-zero terminator.
enum { MAP_AKROPOLIS_MARKER_COUNT = 14 };

/// Scene columns per area in the flat area-music table.
enum { MAP_AKROPOLIS_MUSIC_COLUMN_COUNT = 8 };

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

/// Area music for Acropolis and MIST, in flat rows of eight scene columns.
///
/// Index is area ID * `MAP_AKROPOLIS_MUSIC_COLUMN_COUNT` + column: areas
/// 0..20, columns 0..7. There is no area-21 row or terminator. Each row's first
/// entry also controls ambient eligibility. Keep this overlay loaded throughout
/// music selection and any music task borrowing an entry.
extern StageMusicEntry gMapAkropolisAreaMusicEntries[(GAME_AREA_MIST_SHOOTING_GALLERY + 1) * MAP_AKROPOLIS_MUSIC_COLUMN_COUNT];

/// Scene-indexed countdown music for Acropolis and MIST.
///
/// `gStageCountdownMusicEntry` selects an entry directly in 0..19, without a
/// bounds check or terminator. Entries 0..12 hold explicit scene selections;
/// entries 13..19 retain sequence zero and start mode zero. Keep this overlay
/// loaded until the music task finishes using its selection.
extern StageMusicEntry gMapAkropolisCountdownMusicEntries[20];

/// Navigation gates for the Acropolis and MIST map pages.
///
/// Indexed by one-based page. Pages 1 and 2 use nibble flag `EVENT_FLAG_MAP_AKROPOLIS_AVAILABLE`,
/// with visited-area fallback when it is zero. Page 3 (MIST) uses 0xFF to
/// suppress page navigation. Slot 0 is the left boundary; slots 4..7 are
/// retained zero bytes outside the used page range. Keep this overlay loaded.
extern u8 gMapAkropolisPageFlagIds[8];

/// Area-to-map coordinate mappings for Acropolis and MIST.
///
/// Indexed directly by area ID (0..20); unmapped slots use
/// `MENU_MAP_AREA_PAGE_NONE`. Record 21 is `MENU_MAP_AREA_PAGE_END` for page
/// availability walks. Origins use world X/Z units, map points use pixels and
/// scales use world units per pixel. Keep this overlay loaded for lookup.
extern MenuMapArea gMapAkropolisAreas[GAME_AREA_MIST_SHOOTING_GALLERY + 2];

/// Area shapes and icon-only page selections for Acropolis and MIST.
///
/// Indexed directly by area ID 0..20; slot zero is hidden. There is no
/// terminator: `gMenuMapAreaShapeCounts` includes slot zero and covers this whole array.
/// Keep this map loaded during drawing, and load the selected page's picture
/// package before dereferencing a shape model from that page.
/// MIST areas 19 and 20 have no shape model; only their icons are drawn.
extern MenuMapAreaShape gMapAkropolisAreaShapes[GAME_AREA_MIST_SHOOTING_GALLERY + 1];

/// Fixed map markers on Acropolis page 1, followed by a page-zero terminator.
///
/// Indices 0..13 correspond to `gMapAkropolisMarkerFlags`; a flag nibble of
/// two shows its marker independently of area visits. Coordinates are signed
/// map pixels. Keep this overlay loaded while drawing.
extern MenuMapMarker gMapAkropolisMarkers[MAP_AKROPOLIS_MARKER_COUNT + 1];

/// Telephone, fixed-picture and objective icons for Acropolis and MIST.
///
/// Five records and a page-zero terminator; areas and pages are one-based.
/// Fixed icons require a visited area and any nonzero condition flag; the
/// objective icon compares its condition with the current objective byte.
/// The kind-zero picture's appearance is unproven. Coordinates are signed
/// map pixels. Keep this overlay loaded while drawing.
extern MenuMapIcon gMapAkropolisIcons[6];

/// Area names displayed by the Acropolis and MIST map.
///
/// Indexed by area ID minus one for areas 1..21, without a terminator. Each
/// record is a NUL-terminated ASCII line in 32 bytes; MIST areas 18 and 21
/// have empty names. Keep this overlay loaded while measuring or drawing.
extern MenuMapAreaName gMapAkropolisAreaNames[GAME_AREA_MIST_R21];

/// Placed-object state and spawn directory for Acropolis and MIST areas 0..20.
///
/// Indexed directly by area ID; NULL slots contain no placed objects. Slot 21
/// is `AREA_OBJECT_ROOM_END`, not an area entry. Flag initialization walks the
/// directory through that terminator; spawning selects a populated area slot.
/// Place lists belong to this map overlay; spawn recipes may borrow models and
/// callbacks from the selected room overlay. Keep both loaded while in use.
/// Flag-index publication requires an index listed in a place list: its search
/// uses a different terminator and cannot safely handle a miss in this
/// directory.
extern AreaObjectRoom gMapAkropolisObjectRooms[GAME_AREA_MIST_SHOOTING_GALLERY + 2];

/// Room-controller recipes for twenty Acropolis and MIST areas.
///
/// Metadata uses `OBJECT_TASK_ROOM_LOCATION_KEY(stage, area, room)`: room zero supplies an
/// area default, while the West elevator hall and MIST briefing select room 1.
/// Plaza has no recipe. Bodyless tasks run at priority `TASK_PRIORITY_SCENE`; the final
/// `TASK_DESC_END` entry closes lookup. Keep this map and the selected room
/// callback's overlay loaded through spawning and task execution.
extern TaskDesc gMapAkropolisRoomTaskDescriptors[21];

/// Packed flag and picture selectors for the Acropolis fixed map markers.
///
/// Indices 0..13 correspond to `gMapAkropolisMarkers`, excluding its ending
/// record. Low 11 bits select a game-flag nibble; bit 11 selects the alternate
/// picture. Every entry here selects the ordinary picture. There is no
/// terminator. Keep this overlay loaded while reading marker state.
extern u16 gMapAkropolisMarkerFlags[MAP_AKROPOLIS_MARKER_COUNT];

/// Per-area room-lighting arrays for Acropolis and MIST.
///
/// Indexed by area - 1 for areas 1..20, then room - 1 within the borrowed
/// array. There is no terminator or area-21 entry. Each room overlay owns its
/// descriptors and must remain loaded while they are read or updated; the
/// map overlay owns only this area directory.
extern WorldCoordRoomLighting* gMapAkropolisAreaRoomLightingTables[GAME_AREA_MIST_SHOOTING_GALLERY];

/// Collision room-resource directory for Acropolis and MIST areas 1..20.
///
/// Indexed by area - 1, then room - 1 within that area's own extent. There
/// is no terminator or area-21 entry. The map owns the area directory and
/// borrows room-overlay grid, trigger and occluder descriptors. Keep this map
/// loaded during lookup and each selected room's resources loaded until unbound.
extern WorldCollisionStageResources gMapAkropolisWorldCollisionResources;

/// Sprite-view directory for Acropolis and MIST areas 1..20.
///
/// Indexed by area - 1, independently of room and placement variant. There is
/// no terminator or area-21 entry. The mapped 1-based view index selects an
/// area's sprite descriptor at index minus one; its source/batch/clipping lists
/// stay borrowed from the room overlay. Both map and selected room overlays
/// must remain loaded during lookup.
extern SpriteAreaTable gMapAkropolisSpriteAreaTable;

/// Warp-endpoint arrays for Acropolis and MIST areas 1..20.
///
/// Indexed by area - 1, then warp - 1 within that area's own extent; room
/// variants share the area's endpoints. There is no terminator or area-21
/// entry. Endpoints supply departure facing/sounds and arrival placements/view.
/// Keep this map and the selected room overlay loaded while copying an entry.
extern DirectionWarpEntry* gMapAkropolisAreaWarpEntries[GAME_AREA_MIST_SHOOTING_GALLERY];

/// Logical-view search counts for Acropolis and MIST areas 1..20.
///
/// Indexed by area - 1, then room - 1 in the borrowed room-overlay array.
/// Counts are signed halfword element counts, independent of camera, image
/// and sprite capacities. There is no terminator or area-21 entry. Keep this
/// map and the selected room overlay loaded during lookup.
extern ViewCountTable gMapAkropolisViewCountTable;

/// Camera directory for Acropolis and MIST areas 1..20.
///
/// Indexed by area - 1, independently of room and placement variant. There is
/// no terminator or area-21 entry. The mapped 1-based view index selects a
/// camera at index minus one. Camera transforms and projection distances stay
/// borrowed from the room overlay. Both map and selected room overlays must
/// remain loaded during lookup.
extern ViewCameraTable gMapAkropolisViewCameraTable;

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

/// One Grinning Stranger for the Square opening layout.
///
/// XYZ use world units; yaw uses 4096 units per turn. `fileIdLow` is zero (base
/// resources). The extent includes `AREA_PLACEMENT_END`; keep the map overlay
/// loaded while actors borrow records.
extern AreaPlacement gMapAkropolisSquareVariant1Placements[2];

/// One camera-parented view figure for East elevator hall variant 1.
///
/// XYZ use world units; yaw uses 4096 units per turn. `fileIdLow` is zero (base
/// resources). The extent includes `AREA_PLACEMENT_END`; keep the map overlay
/// loaded while actors borrow records.
extern AreaPlacement gMapAkropolisEastElevatorHallVariant1Placements[2];

/// One SWAT member and one dormant Grinning Stranger for Patio variant 1.
///
/// Entry 107 selects SWAT; entry 19 uses dormant selector 4 and default tuning.
/// XYZ use world units; yaw uses 4096 units per turn. `fileIdLow` is zero (base
/// resources). The extent includes `AREA_PLACEMENT_END`; keep the map overlay
/// loaded while actors borrow records.
extern AreaPlacement gMapAkropolisPatioVariant1Placements[3];

/// Two scripted Grinning Strangers and two dormant Grinning Strangers for Patio
/// variant 2.
///
/// Entry 10 is scripted and ignores its stored selectors; entry 19 uses dormant
/// selector 4 and first tuning 2. XYZ use world units; yaw uses 4096 units per
/// turn. `fileIdLow` is zero (base resources). The extent includes
/// `AREA_PLACEMENT_END`; keep the map overlay loaded while actors borrow
/// records.
extern AreaPlacement gMapAkropolisPatioVariant2Placements[5];

/// Three Moths and three Rats for Patio variant 3.
///
/// Entry keys 7/8 select separate recipes in the same actor package. Rat tuning
/// row 1 and Moth tuning row 0 are within their eight-row tables. XYZ use world
/// units; yaw uses 4096 units per turn. `fileIdLow` is zero (base resources).
/// The extent includes `AREA_PLACEMENT_END`; keep the map overlay loaded while
/// actors borrow records.
extern AreaPlacement gMapAkropolisPatioVariant3Placements[7];

/// Two dormant Strangers and two Moths for Patio variant 4.
///
/// Entry 10 uses dormant selector 4; entry 8 uses Moth tuning row 1 of eight.
/// XYZ use world units; yaw uses 4096 units per turn. `fileIdLow` is zero (base
/// resources). The extent includes `AREA_PLACEMENT_END`; keep the map overlay
/// loaded while actors borrow records.
extern AreaPlacement gMapAkropolisPatioVariant4Placements[5];

/// Two Strangers for Patio variant 5.
///
/// XYZ use world units; yaw uses 4096 units per turn. `fileIdLow` is zero (base
/// resources). The extent includes `AREA_PLACEMENT_END`; keep the map overlay
/// loaded while actors borrow records.
extern AreaPlacement gMapAkropolisPatioVariant5Placements[3];

/// Two unmatched stored placements for Patio variant 10.
///
/// Entry 9 has no matching recipe in this variant's empty resource list.
/// Neither record spawns an actor or requests an additional file; stored
/// selectors are retained without a proven actor interpretation. XYZ use world
/// units; yaw uses 4096 units per turn. `fileIdLow` is zero (base resources).
/// The extent includes `AREA_PLACEMENT_END`; keep the map overlay loaded while
/// actors borrow records.
extern AreaPlacement gMapAkropolisPatioVariant10Placements[3];

/// Stranger, woman and Rupert placements for the Cafeteria opening scene.
///
/// Entry 10 starts hidden or restored (selector 1) with largest movement
/// tuning; entry keys 29/102 select the scripted woman and Rupert. XYZ use
/// world units; yaw uses 4096 units per turn. `fileIdLow` is zero (base
/// resources). The extent includes `AREA_PLACEMENT_END`; keep the map overlay
/// loaded while actors borrow records.
extern AreaPlacement gMapAkropolisCafeteriaVariant1Placements[4];

/// Stranger, scripted Grinning Stranger and Rupert placements for the Cafeteria
/// battle scene.
///
/// Entry 10 starts hidden or restored (selector 1) with largest movement
/// tuning; entry keys 19/102 select the scripted battle form and Rupert. XYZ
/// use world units; yaw uses 4096 units per turn. `fileIdLow` is zero (base
/// resources). The extent includes `AREA_PLACEMENT_END`; keep the map overlay
/// loaded while actors borrow records.
extern AreaPlacement gMapAkropolisCafeteriaVariant2Placements[4];

/// One dormant stalking Odd Stranger for Cafeteria variant 3.
///
/// Variant 0x14 combines stalking with dormant selector 4. Mode 2 selects the
/// actor's first combat-tuning row. XYZ use world units; yaw uses 4096 units per
/// turn. `fileIdLow` is zero (base resources). The extent includes
/// `AREA_PLACEMENT_END`; keep the map overlay loaded while actors borrow records.
extern AreaPlacement gMapAkropolisCafeteriaVariant3Placements[2];

/// Two stalking Odd Strangers for Cafeteria variant 4.
///
/// Variants 0x14/0x10 combine stalking with dormant/patrol opening states.
/// Mode 2 selects the first combat-tuning row for both actors. XYZ use world
/// units; yaw uses 4096 units per turn. `fileIdLow` is zero (base resources).
/// The extent includes `AREA_PLACEMENT_END`; keep the
/// map overlay loaded while actors borrow records.
extern AreaPlacement gMapAkropolisCafeteriaVariant4Placements[3];

/// Three patrolling and two idle Bone Sucklers for Cafeteria variant 5.
///
/// Entry 12 uses variant 0 for patrol and 1 for idle; modes and tuning rows are
/// zero. XYZ use world units; yaw uses 4096 units per turn. `fileIdLow` is zero
/// (base resources). The extent includes `AREA_PLACEMENT_END`; keep the map
/// overlay loaded while actors borrow records.
extern AreaPlacement gMapAkropolisCafeteriaVariant5Placements[6];

/// One patrolling Rook GOLEM for Cafeteria variant 7.
///
/// Entry 57 uses patrol mode 1 and variant 3 for a 3000-world-unit patrol. XYZ
/// use world units; yaw uses 4096 units per turn. `fileIdLow` is zero (base
/// resources). The extent includes `AREA_PLACEMENT_END`; keep the map overlay
/// loaded while actors borrow records.
extern AreaPlacement gMapAkropolisCafeteriaVariant7Placements[2];

/// Two police-officer placements for Plaza variant 1.
///
/// Entry keys 109/108 select officers 2/1. Low file components 2/1 request
/// their additional resources; their root placements are borrowed by model
/// swaps. XYZ use world units; yaw uses 4096 units per turn. The extent
/// includes `AREA_PLACEMENT_END`; keep the map overlay loaded while actors
/// borrow records.
extern AreaPlacement gMapAkropolisPlazaVariant1Placements[3];

/// Rupert and SWAT placements for Security room variant 1.
///
/// Entry keys 102/110 select Rupert/SWAT, independent of entry 110's meaning in
/// the East elevator hall. XYZ use world units; yaw uses 4096 units per turn.
/// `fileIdLow` is zero (base resources). The extent includes
/// `AREA_PLACEMENT_END`; keep the map overlay loaded while actors borrow
/// records.
extern AreaPlacement gMapAkropolisSecurityRoomVariant1Placements[3];

/// Eight Rats for Hallway variant 1.
///
/// Entry 7 uses Rat tuning row 0 of eight. XYZ use world units; yaw uses 4096
/// units per turn. `fileIdLow` is zero (base resources). The extent includes
/// `AREA_PLACEMENT_END`; keep the map overlay loaded while actors borrow
/// records.
extern AreaPlacement gMapAkropolisHallwayVariant1Placements[9];

/// One stalking Odd Stranger for Hallway variant 2.
///
/// Variant 0x10 enables stalking. Mode 0 selects default combat tuning. XYZ use
/// world units; yaw uses 4096 units per turn. `fileIdLow` is zero (base
/// resources). The extent includes `AREA_PLACEMENT_END`; keep the map overlay
/// loaded while actors borrow records.
extern AreaPlacement gMapAkropolisHallwayVariant2Placements[2];

/// One stalking Odd Stranger for Hallway variant 3.
///
/// Variant 0x10 enables stalking. This distinct layout retains the same
/// placement bytes as variant 2; mode 0 selects default combat tuning. XYZ use
/// world units; yaw uses 4096 units per turn. `fileIdLow` is zero (base
/// resources). The extent includes `AREA_PLACEMENT_END`; keep the map overlay
/// loaded while actors borrow records.
extern AreaPlacement gMapAkropolisHallwayVariant3Placements[2];

/// One stalking Odd Stranger and four Rats for Hallway variant 4.
///
/// The Stranger uses stalking selector 0x10 and first tuning 2. Rats use tuning
/// row 1 of eight, with separate texture-page and CLUT offsets. XYZ use world
/// units; yaw uses 4096 units per turn. `fileIdLow` is zero (base resources).
/// The extent includes `AREA_PLACEMENT_END`; keep the map overlay loaded while
/// actors borrow records.
extern AreaPlacement gMapAkropolisHallwayVariant4Placements[6];

/// Two Mossbacks for Fountain variant 2.
///
/// Entry 11 uses ordinary Mossback awareness selector 0. XYZ use world units;
/// yaw uses 4096 units per turn. `fileIdLow` is zero (base resources). The
/// extent includes `AREA_PLACEMENT_END`; keep the map overlay loaded while
/// actors borrow records.
extern AreaPlacement gMapAkropolisFountainVariant2Placements[3];

/// Two large Mossbacks for Fountain variant 7.
///
/// Entry 49 selects the large Mossback recipe with awareness selector 0. XYZ
/// use world units; yaw uses 4096 units per turn. `fileIdLow` is zero (base
/// resources). The extent includes `AREA_PLACEMENT_END`; keep the map overlay
/// loaded while actors borrow records.
extern AreaPlacement gMapAkropolisFountainVariant7Placements[3];

/// One dormant Grinning Stranger for Forked road variant 2.
///
/// Entry 19 uses dormant selector 4 and default pursuit tuning. XYZ use world
/// units; yaw uses 4096 units per turn. `fileIdLow` is zero (base resources).
/// The extent includes `AREA_PLACEMENT_END`; keep the map overlay loaded while
/// actors borrow records.
extern AreaPlacement gMapAkropolisForkedRoadVariant2Placements[2];

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

/// Empty bridge variant-2 placement list, containing only `AREA_PLACEMENT_END`.
///
/// Retains its own map-owned address alongside the variant's empty resources.
extern AreaPlacement gMapAkropolisBridgeVariant2Placements[1];

/// Empty bridge variant-3 placement list, containing only `AREA_PLACEMENT_END`.
///
/// Retains a separate map-owned address alongside the variant's empty resources.
extern AreaPlacement gMapAkropolisBridgeVariant3Placements[1];

/// Stranger placement shared by Fire escape variants 1..3.
///
/// Entry 10 selects the room's actor recipe. XYZ uses world units; yaw uses
/// 4096 units per turn. The extent includes `AREA_PLACEMENT_END`; keep this
/// map loaded while the spawned actor borrows its placement.
extern AreaPlacement gMapAkropolisFireEscapePlacements[2];

/// No. 9 GOLEM placement for Helicopter landing pad variant 1.
///
/// Entry 27 selects the room's recipe. The extent includes `AREA_PLACEMENT_END`;
/// keep this map loaded while the spawned actor borrows its placement.
extern AreaPlacement gMapAkropolisHelicopterLandingPadVariant1Placements[2];

/// No. 9, Rupert and helicopter placements for landing-pad variant 2.
///
/// Entry IDs 27, 144 and 254 match the room's recipes. All use base resources;
/// texture-page and CLUT-row offsets belong to each placement. The extent
/// includes `AREA_PLACEMENT_END`; keep this map loaded while actors borrow it.
extern AreaPlacement gMapAkropolisHelicopterLandingPadVariant2Placements[4];

/// Rupert and Baldwin placements shared by MIST briefing variants 1 and 3.
///
/// Entry IDs 144 and 105 select the room's recipes. The extent includes
/// `AREA_PLACEMENT_END`; keep this map loaded while actors borrow the records.
extern AreaPlacement gMapAkropolisMistR18Placements[3];

/// Pierce and Jodie placements shared by MIST parking variants 1 and 2.
///
/// Entry IDs 143 and 115 select the room's recipes. XYZ uses world units; yaw
/// uses 4096 units per turn. The extent includes `AREA_PLACEMENT_END`; keep
/// this map loaded while actors borrow the records.
extern AreaPlacement gMapAkropolisMistParkingPlacements[3];

/// Gallery mount and Pierce placements for shooting-gallery variant 1.
///
/// Entry IDs 76 and 143 select the room's recipes. The mount's mode 0xFF
/// rejects its spawn; the record retains its placement identity. XYZ uses
/// world units and yaw uses 4096 units per turn. The extent includes
/// `AREA_PLACEMENT_END`; keep this map loaded while actors borrow the records.
extern AreaPlacement gMapAkropolisMistShootingGalleryPlacements[3];

/// Acropolis battle rewards for normal, replay and Scavenger runs.
///
/// Eleven layout-keyed rows followed by `INVENTORY_BATTLE_REWARD_LIST_END`.
/// Nonzero item slots grant one pack; slot 3 requires a Medicine Wheel.
/// Keys exclude the room and view. Keep this overlay loaded during lookup.
extern InventoryBattleReward gMapAkropolisBattleRewards[12];

/// Acropolis battle rewards for Bounty and Nightmare runs.
///
/// Five layout-keyed rows followed by `INVENTORY_BATTLE_REWARD_LIST_END`.
/// Nonzero item slots grant one pack; slot 3 requires a Medicine Wheel.
/// Keys exclude the room and view. Keep this overlay loaded during lookup.
extern InventoryBattleReward gMapAkropolisHardBattleRewards[6];

#endif // INCLUDE_MAPUI_MAP_AKROPOLIS_H
