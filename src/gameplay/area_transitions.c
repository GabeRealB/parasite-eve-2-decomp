#include "gameplay/area_transitions.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "gameplay/companion_load.h"
#include "gameplay/area.h"
#include "area_transitions.h"
#include "gameplay/captions.h"
#include "captions.h"
#include "gameplay/direction.h"
#include "direction.h"
#include "gameplay/direction_input.h"
#include "direction_input.h"
#include "gameplay/loading.h"
#include "loading.h"
#include "menu_map.h"
#include "gameplay/message.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/view.h"
#include "world_collision.h"

#include "main/areas.h"
#include "main/event_flags.h"
#include "main/gameflag.h"
#include "main/gameflow.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/task.h"

#include "mapui/map_akropolis.h"

#include "mapui/map_dryfield.h"

#include "mapui/map_dryfield_full.h"

#include "mapui/map_neo_ark.h"

#include "mapui/map_shelter.h"

/// `area` value that ends an `_AreaMapMarkRec` list.
///
/// The entry's `setMapMark` byte is not read.
enum { AREA_MAP_MARK_END = 0xFF };

/// One area in a per-stage map-mark list.
///
/// `area` selects the area the same way as `GameLocationKey.area`. A nonzero
/// `setMapMark` sets `AREA_SAVED_MAP_MARK` on that area's saved state when the
/// state exists. Zero skips the entry and leaves a stored mark as it is. The
/// stage is chosen by which list holds the record.
typedef struct {
    u8 area;       // 1-based area within the list's stage. AREA_MAP_MARK_END ends the list
    u8 setMapMark; // Nonzero sets AREA_SAVED_MAP_MARK. Zero skips the entry
} _AreaMapMarkRec;
STATIC_ASSERT_SIZEOF(_AreaMapMarkRec, 2);

/// Phases of the facing action, in the order its phase counter passes them.
///
/// The action turns the player to the yaw its trigger gives and then walks
/// them up or down the trigger's stairs. It ends when the climb does, unless
/// the climb carries the player onto a warp trigger, which the last phase
/// then commits.
enum {
    DIRECTION_FACING_PHASE_TURN,        // Start the player's turn to the trigger's yaw
    DIRECTION_FACING_PHASE_AWAIT_TURN,  // Wait for the turn to finish
    DIRECTION_FACING_PHASE_CLIMB,       // Start the stair climb
    DIRECTION_FACING_PHASE_AWAIT_CLIMB, // Follow the climb to its end, or go on once it meets a warp trigger
    DIRECTION_FACING_PHASE_WARP,        // Commit the warp met during the climb and end the action
    DIRECTION_FACING_PHASE_COUNT
};

/// The phase handlers of the facing action, indexed by `DIRECTION_FACING_PHASE_*`.
///
/// The facing action's own handler calls the entry its phase counter picks
/// once per frame, without a range check. The counter starts at
/// `DIRECTION_FACING_PHASE_TURN` when the action is latched and only ever
/// steps forward by one, and the last phase ends the action without stepping
/// it, so the index stays inside the table.
///
/// The array is wrapped in a struct so that the table can be copied by
/// assignment: the action's handler dispatches through a copy of its own.
typedef struct {
    DirectionActionHandler handlers[DIRECTION_FACING_PHASE_COUNT];
} _DirectionFacingPhaseTable;
STATIC_ASSERT_SIZEOF(_DirectionFacingPhaseTable, 0x14);

/// Phases of the warp action, in the order its phase counter passes them.
///
/// The action asks the room whether the trigger's warp may be taken, turns
/// the player to face it and then passes the warp to the room to execute. A
/// warp the room allows goes on to leave the area; a refused one is executed
/// by the room all the same and ends at `DIRECTION_WARP_PHASE_RESOLVE`. The
/// first phase ends the action itself when an event is running, when the
/// warp is refused during a battle, or when the room's answer has it execute
/// the warp at once. Each phase from `DIRECTION_WARP_PHASE_AWAIT_TURN` on
/// also draws the departure fade when the warp has one.
enum {
    DIRECTION_WARP_PHASE_QUERY,       // Ask the room about the warp and start the player's turn to face it
    DIRECTION_WARP_PHASE_AWAIT_TURN,  // Wait for the turn to finish
    DIRECTION_WARP_PHASE_HOLD,        // Pass one frame
    DIRECTION_WARP_PHASE_RESOLVE,     // Have the room carry out the warp and play its sound. A refused warp ends here
    DIRECTION_WARP_PHASE_AWAIT_SOUND, // Wait for the departure sound to finish
    DIRECTION_WARP_PHASE_LEAVE,       // Store the destination as the live location, start the area change and end the action
    DIRECTION_WARP_PHASE_COUNT
};

/// The phase handlers of the warp action, indexed by `DIRECTION_WARP_PHASE_*`.
///
/// The warp action's own handler calls the entry its phase counter picks once
/// per frame, without a range check. The counter starts at
/// `DIRECTION_WARP_PHASE_QUERY` when the action is latched and only ever
/// steps forward by one, and the last phase ends the action without stepping
/// it, so the index stays inside the table.
///
/// The array is wrapped in a struct so that the table can be copied by
/// assignment: the action's handler dispatches through a copy of its own.
typedef struct {
    DirectionActionHandler handlers[DIRECTION_WARP_PHASE_COUNT];
} _DirectionWarpPhaseTable;
STATIC_ASSERT_SIZEOF(_DirectionWarpPhaseTable, 0x18);

static const TaskFuncTable3 _gDirectionTaskStates;

static const _DirectionWarpPhaseTable _gDirectionWarpPhaseHandlers;

static const _DirectionFacingPhaseTable _gDirectionStairPhaseHandlers;

static inline s32 _areaHasMapMark(const GameLocationKey* key);

static inline s16 _menuMapReadMarkerState(const u16* flagEntries, s16 markerIndex);

static void _directionInitTask(Task* task);

static void _directionUpdateTask(Task* task);

static u8 _viewGetCurrentRoomCount(void);

static void _directionUpdateWarpAction(void);

static void _directionUpdateStairAction(void);

static void _directionClearAction(void);

static void _directionDispatchRoomAction(void);

static void _directionDispatchWeaponCapEvent(void);

static void _directionHoldWarpFrame(void);

static void _directionLeaveWarp(void);

static void _directionStartStairTurn(void);

static void _directionAwaitStairTurn(void);

static void _directionStartStairClimb(void);

static void _areaApplyMapMarkList(s16 stageId, const _AreaMapMarkRec* entry);

/// Returns whether an area's saved map mark is set, as 0 or 1.
///
/// Borrows a key and reads only stage and area, which must index their loaded
/// directories. A missing stage table or saved state returns 0. This tests the
/// stored mark independently of visitation and saved enemy-pose restoration.
static inline s32 _areaHasMapMark(const GameLocationKey* key)
{
    const AreaRecord*     areaRecords;
    const AreaSavedState* areaState;
    s32                   mapMarkFlag;

    areaRecords = gAreaRecordsByStage[key->stage];
    if (areaRecords != NULL) {
        areaState = areaRecords[key->area].savedState;
        if (areaState != NULL) {
            mapMarkFlag = areaState->spawnFlags & AREA_SAVED_MAP_MARK;
            return mapMarkFlag != 0;
        }
    }
    return 0;
}

/// Initial Acropolis/M.I.S.T. map marks, ended by AREA_MAP_MARK_END.
///
/// True choices set the area's saved map-mark bit; false choices leave it
/// unchanged. The final zero record after the terminator is unread and its
/// purpose is unproven; retain it with the list's twenty-record extent.
static _AreaMapMarkRec _gAreaAcropolisNewGameMapMarks[20] = {
    { GAME_AREA_AKROPOLIS_SQUARE, false },
    { GAME_AREA_AKROPOLIS_EAST_ELEVATOR_HALL, false },
    { GAME_AREA_AKROPOLIS_PATIO, true },
    { GAME_AREA_AKROPOLIS_CAFETERIA, false },
    { GAME_AREA_AKROPOLIS_PLAZA, false },
    { GAME_AREA_AKROPOLIS_SECURITY_ROOM, false },
    { GAME_AREA_AKROPOLIS_HALLWAY, true },
    { GAME_AREA_AKROPOLIS_FOUNTAIN, false },
    { GAME_AREA_AKROPOLIS_FORKED_ROAD, false },
    { GAME_AREA_AKROPOLIS_OBSERVATORY, true },
    { GAME_AREA_AKROPOLIS_PROMENADE, true },
    { GAME_AREA_AKROPOLIS_SANCTUARY, false },
    { GAME_AREA_AKROPOLIS_ROOF_GARDEN, false },
    { GAME_AREA_AKROPOLIS_BRIDGE, true },
    { GAME_AREA_AKROPOLIS_FIRE_ESCAPE, true },
    { GAME_AREA_AKROPOLIS_HELICOPTER_LANDING_PAD, false },
    { GAME_AREA_MIST_R18, false },
    { GAME_AREA_MIST_PARKING, false },
    { AREA_MAP_MARK_END, 0 },
    { 0, 0 },
};
/// Initial daytime Dryfield map marks, ended by AREA_MAP_MARK_END.
///
/// True choices set the saved map-mark bit; false choices preserve it. The
/// final zero record after the terminator is unread and has an unproven role.
static _AreaMapMarkRec _gAreaDryfieldNewGameMapMarks[28] = {
    { GAME_AREA_DRYFIELD_GAS_STATION, false },
    { GAME_AREA_DRYFIELD_MAIN_STREET, false },
    { GAME_AREA_DRYFIELD_GENERAL_STORE, false },
    { GAME_AREA_DRYFIELD_BACK_STREET, true },
    { GAME_AREA_DRYFIELD_SOUVENIR_SHOP, true },
    { GAME_AREA_DRYFIELD_WAREHOUSE, false },
    { GAME_AREA_DRYFIELD_DILAPIDATED_HOUSE, false },
    { GAME_AREA_DRYFIELD_MOTEL_ROOM_1, false },
    { GAME_AREA_DRYFIELD_MOTEL_ROOM_2, true },
    { GAME_AREA_DRYFIELD_PARKING_LOT, false },
    { GAME_AREA_DRYFIELD_TOILET, false },
    { GAME_AREA_DRYFIELD_SALOON_G_R, true },
    { GAME_AREA_DRYFIELD_G_R_KITCHEN, true },
    { GAME_AREA_DRYFIELD_WATER_TOWER, true },
    { GAME_AREA_DRYFIELD_WATER_TANK, false },
    { GAME_AREA_DRYFIELD_BREEZEWAY, false },
    { GAME_AREA_DRYFIELD_FACTORY, false },
    { GAME_AREA_DRYFIELD_GARAGE, false },
    { GAME_AREA_DRYFIELD_DRIVEWAY, false },
    { GAME_AREA_DRYFIELD_JUNK_YARD, false },
    { GAME_AREA_DRYFIELD_TRAILER_COACH, false },
    { GAME_AREA_DRYFIELD_MOTEL_BALCONY, false },
    { GAME_AREA_DRYFIELD_MOTEL_ROOM_6, false },
    { GAME_AREA_DRYFIELD_WATER_HOLE, true },
    { GAME_AREA_DRYFIELD_CELLAR, true },
    { GAME_AREA_DRYFIELD_UNDERPASS, false },
    { AREA_MAP_MARK_END, 0 },
    { 0, 0 },
};
/// Initial Mine/Shelter map marks, ended by AREA_MAP_MARK_END.
///
/// True choices set the saved map-mark bit; false choices preserve it. These
/// area IDs belong to stage 4, including Shelter floors 1F through B4.
static _AreaMapMarkRec _gAreaMineShelterNewGameMapMarks[46] = {
    { GAME_AREA_MINE_MESA, false },
    { GAME_AREA_MINE_CAVERN, false },
    { GAME_AREA_MINE_TUNNEL_ENTRANCE, true },
    { GAME_AREA_MINE_TUNNEL, false },
    { GAME_AREA_MINE_GORGE, false },
    { GAME_AREA_MINE_REFUGE, false },
    { GAME_AREA_MINE_FORKED_TUNNEL, false },
    { GAME_AREA_MINE_SECRET_PASSAGE, false },
    { GAME_AREA_SHELTER_B1_ELEVATOR_HALL, false },
    { GAME_AREA_SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY, true },
    { GAME_AREA_SHELTER_B1_STOREROOM, true },
    { GAME_AREA_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY, true },
    { GAME_AREA_SHELTER_B1_ARMORY, false },
    { GAME_AREA_SHELTER_B1_SLEEPING_QUARTERS, true },
    { GAME_AREA_SHELTER_B1_MAIN_CORRIDOR, true },
    { GAME_AREA_SHELTER_B1_STERILIZATION_ROOM, false },
    { GAME_AREA_SHELTER_B1_POD_ACCESS_TUNNEL, false },
    { GAME_AREA_SHELTER_B1_CONTROL_ROOM, true },
    { GAME_AREA_SHELTER_B1_ACCESS_TUNNEL, true },
    { GAME_AREA_SHELTER_B1_UNDERGROUND_PARKING, false },
    { GAME_AREA_SHELTER_B1_GOLEM_FREEZER_1, false },
    { GAME_AREA_SHELTER_B2_POD_BOTTOM, false },
    { GAME_AREA_SHELTER_B1_POD_SERVICE_GANTRY, false },
    { GAME_AREA_SHELTER_B1_TRANSFER_TUNNEL, true },
    { GAME_AREA_SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL, true },
    { GAME_AREA_SHELTER_B2_ELEVATOR, false },
    { GAME_AREA_SHELTER_B2_ELEVATOR_HALL, true },
    { GAME_AREA_SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY, true },
    { GAME_AREA_SHELTER_B2_OPERATING_ROOM, true },
    { GAME_AREA_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY, false },
    { GAME_AREA_SHELTER_B2_LABORATORY, false },
    { GAME_AREA_SHELTER_B2_BREEDING_ROOM, true },
    { GAME_AREA_SHELTER_B2_MAIN_CORRIDOR, true },
    { GAME_AREA_SHELTER_B2_SEPTIC_TANK, false },
    { GAME_AREA_SHELTER_B2_POD_ACCESS_TUNNEL, true },
    { GAME_AREA_SHELTER_B3_DUMPING_HOLE, true },
    { GAME_AREA_SHELTER_B3_GARBAGE_INCINERATOR, false },
    { GAME_AREA_SHELTER_B3_INCINERATOR_CONTROL_ROOM, false },
    { GAME_AREA_SHELTER_B3_ELEVATOR_HALL, true },
    { GAME_AREA_SHELTER_B4_LOWER_SEWER, true },
    { GAME_AREA_SHELTER_B4_UPPER_SEWER, true },
    { GAME_AREA_SHELTER_B4_RESERVOIR, true },
    { GAME_AREA_SHELTER_B4_WATER_SUPPLY, true },
    { GAME_AREA_SHELTER_R47, false },
    { GAME_AREA_SHELTER_R48, false },
    { AREA_MAP_MARK_END, 0 },
};
/// Initial Shelter/Neo Ark map marks, ended by AREA_MAP_MARK_END.
///
/// True choices set the saved map-mark bit; false choices preserve it. These
/// area IDs belong to stage 5, including Shelter 1F and B6 and Neo Ark.
static _AreaMapMarkRec _gAreaShelterNeoArkNewGameMapMarks[34] = {
    { GAME_AREA_SHELTER_1F_PARKING_GARAGE, false },
    { GAME_AREA_SHELTER_1F_VEHICULAR_AIRLOCK, true },
    { GAME_AREA_SHELTER_1F_BULWARK, true },
    { GAME_AREA_SHELTER_1F_HELIPORT, false },
    { GAME_AREA_SHELTER_1F_AIRLOCK, true },
    { GAME_AREA_SHELTER_1F_GUARDROOM, false },
    { GAME_AREA_NEO_ARK_OBSERVATORY, false },
    { GAME_AREA_NEO_ARK_EVE_ACCESS_TUNNEL, true },
    { GAME_AREA_NEO_ARK_EVE_ELEVATOR, false },
    { GAME_AREA_NEO_ARK_NORTH_PROMENADE, false },
    { GAME_AREA_NEO_ARK_FOREST_ZONE, false },
    { GAME_AREA_NEO_ARK_SUBMARINE_TUNNEL, false },
    { GAME_AREA_NEO_ARK_PAVILION, true },
    { GAME_AREA_NEO_ARK_ISLAND, true },
    { GAME_AREA_NEO_ARK_GARDEN, true },
    { GAME_AREA_NEO_ARK_POWER_PLANT_2, true },
    { GAME_AREA_NEO_ARK_POWER_PLANT_1, true },
    { GAME_AREA_NEO_ARK_SAVANNA_ZONE, true },
    { GAME_AREA_NEO_ARK_SOUTH_PROMENADE, false },
    { GAME_AREA_NEO_ARK_ALTAR, false },
    { GAME_AREA_NEO_ARK_SHRINE, false },
    { GAME_AREA_SHELTER_B6_NURSERY, false },
    { GAME_AREA_SHELTER_B6_GROWTH_ROOM, false },
    { GAME_AREA_SHELTER_B6_CORRIDOR, true },
    { GAME_AREA_SHELTER_B6_TRAINING_ROOM, true },
    { GAME_AREA_NEO_ARK_R26, false },
    { GAME_AREA_NEO_ARK_BRIDGE, true },
    { GAME_AREA_SHELTER_1F_TENT, false },
    { GAME_AREA_NEO_ARK_WOODLAND_PATH, false },
    { GAME_AREA_NEO_ARK_SUBMARINE_GALLERY, true },
    { GAME_AREA_NEO_ARK_R31, false },
    { GAME_AREA_NEO_ARK_PYRAMID, true },
    { GAME_AREA_NEO_ARK_SUBSTATION, false },
    { AREA_MAP_MARK_END, 0 },
};

/// Sets the saved map marks selected by one stage's new-game list.
///
/// `stageId` selects the stage directory directly (the callers use 1, 2, 4, 5).
/// Borrows a readable `AREA_MAP_MARK_END`-terminated list of valid 1-based area
/// IDs. Only nonzero list choices set `AREA_SAVED_MAP_MARK`; zero choices and
/// other saved flags survive. Missing directories and saved states are skipped.
static inline void _areaApplyNewGameMapMarksForStage(s16 stageId, const _AreaMapMarkRec* entry)
{
    const AreaRecord* areaRecords;
    AreaSavedState*   areaState;

    areaRecords = gAreaRecordsByStage[stageId];
    if (areaRecords != NULL) {
        for (; entry->area != AREA_MAP_MARK_END; entry++) {
            if (entry->setMapMark != 0) {
                areaState = areaRecords[entry->area].savedState;
                if (areaState != NULL) {
                    areaState->spawnFlags |= AREA_SAVED_MAP_MARK;
                }
            }
        }
    }
}

void areaApplyNewGameMapMarks(void)
{
    _areaApplyNewGameMapMarksForStage(GAME_STAGE_AKROPOLIS, _gAreaAcropolisNewGameMapMarks);
    _areaApplyNewGameMapMarksForStage(GAME_STAGE_DRYFIELD, _gAreaDryfieldNewGameMapMarks);
    _areaApplyNewGameMapMarksForStage(GAME_STAGE_MINE_SHELTER, _gAreaMineShelterNewGameMapMarks);
    _areaApplyNewGameMapMarksForStage(GAME_STAGE_SHELTER_NEO_ARK, _gAreaShelterNeoArkNewGameMapMarks);
}

void menuMapRebuildMarkedAreaBits(void)
{
    enum { MENU_MAP_STAGE_COUNT            = ARRAY_SIZE(gMenuMapStageAreaCounts.markedAreaIdLimits),
           MENU_MAP_AREA_BITS_PER_WORD     = 32,
           MENU_MAP_SECOND_WORD_FIRST_AREA = 33 };
    GameLocationKey        areaKey;
    const GameLocationKey* currentLocation;
    s32                    areaCount;
    s32                    areaId;
    u8                     stageId;

    /// Clears the map-screen cache bit for an area ID in 1..64.
    ///
    /// The argument must be a side-effect-free area expression: it is evaluated
    /// in the comparison and again for the shift. Captures `gMenuMapMarkedAreaBits` and
    /// the local word-boundary constants; use as a standalone statement.
#define MENU_MAP_CLEAR_MARKED_AREA_BIT(areaId)                                                  \
    {                                                                                           \
        if ((areaId) <= MENU_MAP_AREA_BITS_PER_WORD) {                                          \
            gMenuMapMarkedAreaBits[0] &= ~(1U << ((areaId) - 1));                               \
        } else {                                                                                \
            gMenuMapMarkedAreaBits[1] &= ~(1U << ((areaId) - MENU_MAP_SECOND_WORD_FIRST_AREA)); \
        }                                                                                       \
    }

    currentLocation = &gGameSession->location;
    stageId         = currentLocation->stage;
    // Retain the room/view initialization; both predicates read only stage and area.
    areaKey.roomState = 1;
    areaKey.view      = 2;
    areaKey.stage     = stageId;
    // Refresh only this stage's area bits; higher bits retain their prior values.
    if (gGameSession->location.stage - GAME_STAGE_AKROPOLIS < MENU_MAP_STAGE_COUNT) {
        areaCount = gMenuMapStageAreaCounts.markedAreaIdLimits[currentLocation->stage - GAME_STAGE_AKROPOLIS];
        for (areaId = 1; areaId <= areaCount; areaId++) {
            areaKey.area = areaId;
            if (_areaHasMapMark(&areaKey) == 1) {
                if (areaIsSavedPoseRestoreEnabled(&areaKey) == 1) {
                    MENU_MAP_CLEAR_MARKED_AREA_BIT(areaKey.area);
                } else {
                    if (areaKey.area <= MENU_MAP_AREA_BITS_PER_WORD) {
                        gMenuMapMarkedAreaBits[0] |= 1U << (areaKey.area - 1);
                    } else {
                        gMenuMapMarkedAreaBits[1] |= 1U << (areaKey.area - MENU_MAP_SECOND_WORD_FIRST_AREA);
                    }
                }
            } else {
                MENU_MAP_CLEAR_MARKED_AREA_BIT(areaKey.area);
            }
        }
    }
#undef MENU_MAP_CLEAR_MARKED_AREA_BIT
}

/// Direction-task lifecycle handlers: 0 initialize, 1 update, 2 kill.
///
/// Initialization advances to 1; update holds that state. `Task::state` must
/// stay in 0..2. Dispatch copies the complete table and requires a live player.
static const TaskFuncTable3 _gDirectionTaskStates = { {
    _directionInitTask,
    _directionUpdateTask,
    taskKill,
} };

const DirectionActionTable gDirectionActionHandlers = { {
    [WORLD_COLLISION_TRIGGER_ACTION_WARP]       = _directionUpdateWarpAction,
    [WORLD_COLLISION_TRIGGER_ACTION_FACING]     = _directionUpdateStairAction,
    [WORLD_COLLISION_TRIGGER_ACTION_CAP]        = directionDispatchCapInteraction,
    [WORLD_COLLISION_TRIGGER_ACTION_CALLBACK]   = directionDispatchCallbackAction,
    [WORLD_COLLISION_TRIGGER_ACTION_CLEAR]      = _directionClearAction,
    [WORLD_COLLISION_TRIGGER_ACTION_ROOM]       = _directionDispatchRoomAction,
    [WORLD_COLLISION_TRIGGER_ACTION_CAP_WEAPON] = _directionDispatchWeaponCapEvent,
} };

/// Warp-action handlers indexed by `DIRECTION_WARP_PHASE_*` (0..5).
///
/// A copied `_DirectionWarpPhaseTable` dispatches the latched phase. Query can
/// end the action immediately; later phases advance at most once per update,
/// and LEAVE ends the action without advancing beyond the last entry.
static const _DirectionWarpPhaseTable _gDirectionWarpPhaseHandlers = { {
    [DIRECTION_WARP_PHASE_QUERY]       = directionQueryWarp,
    [DIRECTION_WARP_PHASE_AWAIT_TURN]  = directionAwaitWarpTurn,
    [DIRECTION_WARP_PHASE_HOLD]        = _directionHoldWarpFrame,
    [DIRECTION_WARP_PHASE_RESOLVE]     = directionResolveWarp,
    [DIRECTION_WARP_PHASE_AWAIT_SOUND] = directionAwaitWarpSound,
    [DIRECTION_WARP_PHASE_LEAVE]       = _directionLeaveWarp,
} };

/// Stair-action phases: turn, await turn, climb, await climb, then finish a met warp.
///
/// Indexed by the latched phase in 0..4 through a whole-table copy. Each phase
/// advances at most once; the final phase ends the action without advancing.
static const _DirectionFacingPhaseTable _gDirectionStairPhaseHandlers = { {
    [DIRECTION_FACING_PHASE_TURN]        = _directionStartStairTurn,
    [DIRECTION_FACING_PHASE_AWAIT_TURN]  = _directionAwaitStairTurn,
    [DIRECTION_FACING_PHASE_CLIMB]       = _directionStartStairClimb,
    [DIRECTION_FACING_PHASE_AWAIT_CLIMB] = directionAwaitStairClimb,
    [DIRECTION_FACING_PHASE_WARP]        = directionCommitStairWarp,
} };

/// Reads a map marker's flag nibble and preserves its alternate-picture bit.
///
/// Borrows a loaded packed u16 table: the low 11 bits select a game-flag nibble,
/// and bit 11 selects the alternate picture. `markerIndex` must be within the
/// table's extent; there is no bounds check or terminator.
static inline s16 _menuMapReadMarkerState(const u16* flagEntries, s16 markerIndex)
{
    return gameFlagGetNibble(flagEntries[markerIndex] & MENU_MAP_MARKER_FLAG_ID_MASK) + (flagEntries[markerIndex] & MENU_MAP_MARKER_ALTERNATE_PICTURE);
}

s16 menuMapGetMarkerState(s16 markerIndex)
{
    enum {
        MENU_MAP_ACROPOLIS_MARKER_COUNT            = ARRAY_SIZE(gMapAkropolisMarkerFlags),
        MENU_MAP_DRYFIELD_MARKER_COUNT             = ARRAY_SIZE(gMapDryfieldMarkerFlags),
        MENU_MAP_DRYFIELD_NIGHT_MARKER_COUNT       = ARRAY_SIZE(gMapDryfieldFullMarkerFlags),
        MENU_MAP_SHELTER_MARKER_COUNT              = ARRAY_SIZE(gMapShelterMarkerFlags),
        MENU_MAP_NEO_ARK_MARKER_COUNT              = ARRAY_SIZE(gMapNeoArkMarkerFlags),
        MENU_MAP_DRYFIELD_NIGHT_STORY_MARKER       = 29,
        MENU_MAP_SHELTER_ALTERNATE_PICTURE_CHAPTER = 6
    };

    switch (gGameSession->location.stage) {
        case GAME_STAGE_AKROPOLIS:
            if (markerIndex >= MENU_MAP_ACROPOLIS_MARKER_COUNT) {
                break;
            }
            return _menuMapReadMarkerState(gMapAkropolisMarkerFlags, markerIndex);
        case GAME_STAGE_DRYFIELD:
            if (markerIndex >= MENU_MAP_DRYFIELD_MARKER_COUNT) {
                break;
            }
            return _menuMapReadMarkerState(gMapDryfieldMarkerFlags, markerIndex);
        case GAME_STAGE_DRYFIELD_NIGHT:
            if (markerIndex >= MENU_MAP_DRYFIELD_NIGHT_MARKER_COUNT) {
                break;
            }
            // This marker is controlled directly by story progress, not its table word.
            if (markerIndex == MENU_MAP_DRYFIELD_NIGHT_STORY_MARKER) {
                if (gameFlagGetNibble(GAME_FLAG_07F) == 0) {
                    return 0;
                }
                return MENU_MAP_MARKER_STATE_VISIBLE + MENU_MAP_MARKER_ALTERNATE_PICTURE;
            }
            return _menuMapReadMarkerState(gMapDryfieldFullMarkerFlags, markerIndex);
        case GAME_STAGE_MINE_SHELTER:
            if (markerIndex >= MENU_MAP_SHELTER_MARKER_COUNT) {
                break;
            }
            if (markerIndex == 0 && gameFlagGetNibble(EVENT_FLAG_STORY_CHAPTER) == MENU_MAP_SHELTER_ALTERNATE_PICTURE_CHAPTER) {
                return gameFlagGetNibble(gMapShelterMarkerFlags[0] & MENU_MAP_MARKER_FLAG_ID_MASK) + MENU_MAP_MARKER_ALTERNATE_PICTURE;
            }
            return _menuMapReadMarkerState(gMapShelterMarkerFlags, markerIndex);
        case GAME_STAGE_SHELTER_NEO_ARK:
            if (markerIndex >= MENU_MAP_NEO_ARK_MARKER_COUNT) {
                break;
            }
            return _menuMapReadMarkerState(gMapNeoArkMarkerFlags, markerIndex);
    }
    return MENU_MAP_MARKER_STATE_UNAVAILABLE;
}

void areaClearMapMark(const GameLocationKey* key)
{
    const AreaRecord* areaRecords;
    AreaSavedState*   areaState;

    areaRecords = gAreaRecordsByStage[key->stage];
    if (areaRecords != NULL) {
        areaState = areaRecords[key->area].savedState;
        if (areaState != NULL) {
            areaState->spawnFlags &= (u8)~AREA_SAVED_MAP_MARK;
        }
    }
}

/// Resets direction action history and the area map-mark cache, then enables per-frame updates.
///
/// Runs once for a new direction task. Interaction activation is held off for
/// ten eligible idle direction updates. The legacy D_80114CE0 initialization is retained;
/// that value has no observed reader.
static void _directionInitTask(Task* task)
{
    enum { DIRECTION_INITIAL_INTERACTION_DELAY_UPDATES = 10 };

    gDirectionPreviousBattlePhase     = SCENE_COMBAT_BATTLE_IDLE;
    gDirectionStairBattleFinished     = false;
    gDirectionActionControl           = 0;
    gDirectionPreviousActionControl   = 0;
    gDirectionActionControlChanged    = false;
    gDirectionActionParameter0        = 0;
    gDirectionActionParameter1        = 0;
    gDirectionActionPhase             = 0;
    gDirectionActionActive            = false;
    D_80114CE0                        = 1;
    gMenuMapMarkedAreaBits[0]         = 0;
    gMenuMapMarkedAreaBits[1]         = 0;
    gDirectionInteractionRearmUpdates = DIRECTION_INITIAL_INTERACTION_DELAY_UPDATES;
    task->state++;
}

/// Consumes view-boundary hits before updating the direction action each frame.
///
/// Runs in direction-task state 1 while a player exists. Boundary hits may
/// request a saved view before the action reads the session and trigger state.
/// The task argument is unused; this callback retains state 1.
static void _directionUpdateTask(Task* task)
{
    worldCollisionConsumeViewBoundaryHits();
    directionUpdateAction();
}

s32 actorAngleTaskYawTowardPoint(const Task* modelTask, const SVECTOR* targetPoint)
{
    SVECTOR         delta;
    const GfxCoord* rootCoord;

    // Narrow before normalization so offsets retain the signed halfword wrap.
    rootCoord = modelTask->extra.tmd->coords;
    delta.vx  = targetPoint->vx - rootCoord->coord.t[0];
    delta.vy  = 0;
    delta.vz  = targetPoint->vz - rootCoord->coord.t[2];
    VectorNormalSS(&delta, &delta);
    return ratan2(delta.vx, delta.vz) & ACTOR_TRANSFORM_ANGLE_MASK;
}

void directionTask(Task* task)
{
    TaskFuncTable3 states;
    Task*          playerTask;

    playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    states     = _gDirectionTaskStates;
    if (playerTask != NULL) {
        states.funcs[task->state](task);
    }
}

/// Returns the low byte of the current room's signed logical-view count.
///
/// Requires a live session with stage 1..5, valid one-based area/room indices
/// in a populated count directory, and the owning map and room overlays loaded.
/// There is no range or NULL check. The byte conversion retains truncation.
static u8 _viewGetCurrentRoomCount(void)
{
    const GameSession*    session;
    const ViewCountTable* viewCountTable;

    session        = gGameSession;
    viewCountTable = gViewCountTables[session->location.stage - 1];
    return (u8)viewCountTable->viewCounts[session->location.area - 1][session->location.roomState - 1];
}

/// Runs one phase of the latched warp action through its copied handler table.
///
/// Requires gDirectionActionPhase in 0..5. Query, turn-wait, hold, resolve and sound-wait
/// advance at most once; the leave phase ends the action without advancing.
/// Dispatch preserves the signed-halfword interpretation and performs no check.
static void _directionUpdateWarpAction(void)
{
    _DirectionWarpPhaseTable phaseTable;

    phaseTable = _gDirectionWarpPhaseHandlers;
    phaseTable.handlers[(s16)gDirectionActionPhase]();
}

/// Runs the current stair phase and holds the battle-end delay until the action finishes.
///
/// Requires a latched stair trigger and a phase in 0..4. A battle ending during
/// the action latches its engaged phase; subsequent frames refresh the end delay.
/// The turn, wait, climb and climb-wait phases advance at most once; the final
/// warp phase ends the action without advancing, keeping dispatch in bounds.
static void _directionUpdateStairAction(void)
{
    _DirectionFacingPhaseTable phaseTable;

    phaseTable = _gDirectionStairPhaseHandlers;
    if (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_FINISHED) {
        if (gDirectionPreviousBattlePhase == SCENE_COMBAT_BATTLE_ENGAGED) {
            gDirectionStairBattleFinished = gDirectionPreviousBattlePhase;
        }
    }
    if (gDirectionStairBattleFinished != 0) {
        gSceneCombatState.signals.bytes.endDelayFrames = SCENE_COMBAT_END_DELAY_FRAMES;
    }
    phaseTable.handlers[(s16)gDirectionActionPhase]();
}

/// Ends the active direction action and discards both latched trigger hits.
///
/// Leaves the session busy flag for the next direction update to release.
static void _directionClearAction(void)
{
    _directionClearTriggerParameters();
    gDirectionActionActive = false;
}

/// Dispatches a room action while events and CAP playback are idle, then consumes it.
///
/// Requires a live room task when the gates permit dispatch. The room borrows
/// the four-byte request only during synchronous dispatch; the second word is
/// zero and the reply is ignored. Blocked requests are also discarded. A changed
/// control word keeps the session busy flag until the next direction update.
static void _directionDispatchRoomAction(void)
{
    DirectionActionRequest request;
    Task*                  roomTask;

    if (gGameSession->eventState == 0) {
        if (capIsBusy() == 0) {
            request.control  = gDirectionActionControl;
            request.actionId = gDirectionActionParameter0;
            request.argument = gDirectionActionParameter1;
            roomTask         = gameGetTaskSlot(GAME_TASK_SLOT_ROOM);
            TASK_MESSAGE_DISPATCH_POINTER(roomTask, DIRECTION_MESSAGE_ROOM_ACTION, &request, 0);
        }
    }
    // Consume the request even when an event or CAP playback blocks dispatch.
    _directionClearTriggerParameters();
    gDirectionActionActive = false;
    if (gDirectionActionControlChanged == 0) {
        gGameSession->dirActionBusy = 0;
    }
}

/// Consumes a weapon-presentation CAP trigger, dispatching only while events and CAP are idle.
///
/// The latched command byte and flag byte follow `capSpawnWeaponEvent`'s
/// resource and player lifetime contract. Allocation failure or either busy
/// gate still discards the primary request and secondary hit. The session's
/// direction-busy flag and phase remain for the direction task to handle.
static void _directionDispatchWeaponCapEvent(void)
{
    if (gGameSession->eventState == 0) {
        if (capIsBusy() == 0) {
            capSpawnWeaponEvent(gDirectionActionParameter0, gDirectionActionParameter1);
        }
    }
    // Consume blocked requests too, releasing activity before the parameters.
    gDirectionActionActive = false;
    _directionClearTriggerParameters();
}

/// Draws and steps the departure fade, then hands the warp to its resolve phase next frame.
///
/// This hold phase advances unconditionally, adding one frame after the turn
/// finishes. A nonzero fade draws its current low byte before increasing by
/// 30 toward 255; zero leaves the fade disabled.
static void _directionHoldWarpFrame(void)
{
    _directionStepDepartureFade();
    gDirectionActionPhase++;
}

/// Stores the resolved warp destination and requests a reload from the live save.
///
/// The final warp phase requires the room's resolved destination for the active
/// stage. Only area, arrival and room are copied; the area's selector narrows to
/// a byte. Draws the departure shade without advancing it, then starts the
/// frame-capturing reload. Task allocation failure still consumes the primary
/// action. The phase and secondary trigger remain for later updates to clear.
static void _directionLeaveWarp(void)
{
    u8  fadeShade;
    s16 activeShade;

    activeShade = (s16)gDirectionWarpDepartureShade;
    if (activeShade != 0) {
        fadeShade = (u8)gDirectionWarpDepartureShade;
        fadeDrawOverlay(fadeShade, fadeShade, fadeShade, GPU_BLEND_SUBTRACT);
    }
    // Publish the destination before the reload task can consume the live save.
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.area      = (u8)gDirectionWarpRequest.areaId;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp      = gDirectionWarpRequest.warp;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = gDirectionWarpRequest.roomState;
    taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
    gDirectionActionActive     = false;
    gDirectionActionParameter1 = 0;
    gDirectionActionParameter0 = 0;
    gDirectionActionControl    = 0;
}

/// Starts the player's scripted turn to the stair trigger's byte-encoded yaw.
///
/// Requires a live player. The trigger yaw uses 256 units per turn and is
/// expanded to 4096 units for a synchronously borrowed ActorTransform. Only
/// rotation is initialized because the turn handler does not read position.
/// An active event instead discards the action and its battle-finished latch.
static void _directionStartStairTurn(void)
{
    enum { DIRECTION_STAIR_YAW_SHIFT = 4 };
    ActorTransform facing;
    Task*          playerTask;

    playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    if (gGameSession->eventState != 0) {
        gDirectionActionActive = false;
        _directionClearTriggerParameters();
        gDirectionStairBattleFinished = false;
    } else {
        facing.rot.vx = 0;
        facing.rot.vz = 0;
        facing.rot.vy = gDirectionActionParameter1 << DIRECTION_STAIR_YAW_SHIFT;
        TASK_MESSAGE_DISPATCH_POINTER(playerTask, GAME_ACTOR_MESSAGE_TURN_TO_YAW, &facing, 0);
        gDirectionActionPhase++;
    }
}

/// Waits for the player's scripted turn to settle before starting the stair climb.
///
/// Requires the player used by the preceding turn phase. Takes no payload and
/// advances one phase only when scripted motion is no longer pending.
static void _directionAwaitStairTurn(void)
{
    if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) == 0) {
        gDirectionActionPhase++;
    }
}

/// Starts the player's stair climb and clears secondary hits before tracking the flight.
///
/// Requires the preceding turn to have finished. The trigger's first parameter
/// supplies a positive low-nibble step count (1..15); control bit 8 selects
/// descent. The player copies both words synchronously and keeps no request
/// pointer. Surface selection still uses the retained primary parameters.
static void _directionStartStairClimb(void)
{
    enum {
        DIRECTION_STAIR_DESCEND_SHIFT   = 8,
        DIRECTION_STAIR_DESCEND_MASK    = 1,
        DIRECTION_STAIR_STEP_COUNT_MASK = 0xF
    };
    GameActorStairClimb climb;
    Task*               playerTask;

    playerTask      = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    climb.descend   = (gDirectionActionControl >> DIRECTION_STAIR_DESCEND_SHIFT) & DIRECTION_STAIR_DESCEND_MASK;
    climb.stepCount = gDirectionActionParameter0 & DIRECTION_STAIR_STEP_COUNT_MASK;
    TASK_MESSAGE_DISPATCH_POINTER(playerTask, GAME_ACTOR_MESSAGE_CLIMB_STAIRS, &climb, 0);
    gDirectionSecondaryActionParameter1 = 0;
    gDirectionSecondaryActionParameter0 = 0;
    gDirectionSecondaryActionControl    = 0;
    gDirectionActionPhase++;
}

void areaSetCurrentMapMark(void)
{
    const GameLocationKey* key;
    const AreaRecord*      areaRecords;
    AreaSavedState*        areaState;

    key         = &gGameSession->location;
    areaRecords = gAreaRecordsByStage[key->stage];
    if (areaRecords != NULL) {
        areaState = areaRecords[key->area].savedState;
        if (areaState != NULL) {
            areaState->spawnFlags |= AREA_SAVED_MAP_MARK;
        }
    }
}

/// Sets saved map marks selected by one stage's terminated list.
///
/// Borrows the list synchronously; `AREA_MAP_MARK_END` ends it, and zero
/// `setMapMark` entries preserve existing marks. Missing tables and saved state
/// are skipped. `stageId` and each live area must index their loaded directories.
static void _areaApplyMapMarkList(s16 stageId, const _AreaMapMarkRec* entry)
{
    const AreaRecord* areaRecords;
    AreaSavedState*   areaState;

    areaRecords = gAreaRecordsByStage[stageId];
    if (areaRecords != NULL) {
        for (; entry->area != AREA_MAP_MARK_END; entry++) {
            if (entry->setMapMark != 0) {
                areaState = areaRecords[entry->area].savedState;
                if (areaState != NULL) {
                    areaState->spawnFlags |= AREA_SAVED_MAP_MARK;
                }
            }
        }
    }
}
