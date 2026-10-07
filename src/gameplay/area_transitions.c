#include "gameplay/area_transitions.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

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

/// `AREA_MAP_MARK_END`-terminated `_AreaMapMarkRec` lists applied by `Gp_ApplyNewGameAreaFlags` to
/// `Gp_AreaTables[1]`, `[2]`, `[4]` and `[5]`.
extern _AreaMapMarkRec Gp_NewGameFlagsStg1[];

extern _AreaMapMarkRec Gp_NewGameFlagsStg2[];

extern _AreaMapMarkRec Gp_NewGameFlagsStg4[];

extern _AreaMapMarkRec Gp_NewGameFlagsStg5[];

/// The flag entry `table[idx]`: its low 11 bits select a flag nibble, and its
/// bit 0x800 is added onto that nibble's value.
extern const TaskFuncTable3 Gp_DirTaskStates;

static const _DirectionWarpPhaseTable Gp_WarpPhaseFns;

static const _DirectionFacingPhaseTable D_80093990;

static inline s32 _gpGetAreaFlag4(GameLocationKey* key);

static inline s16 _menuMapReadMarkerState(const u16* flagEntries, s16 markerIndex);

static void _directionInitTask(Task* task);

static void Gp_DirTaskState1(Task* task);

static u8 Gp_GetViewCountLo(void);

static void Gp_DirAction0(void);

static void _directionUpdateStairAction(void);

static void _directionClearAction(void);

static void _directionDispatchRoomAction(void);

static void Gp_SpawnEvt1IfCapIdle(void);

static void _directionHoldWarpFrame(void);

static void Gp_CommitSaveLoc(void);

static void _directionStartStairTurn(void);

static void _directionAwaitStairTurn(void);

static void _directionStartStairClimb(void);

static void Gp_ApplyAreaFlag4List(s16 arg0, _AreaMapMarkRec* entry);

static inline s32 _gpGetAreaFlag4(GameLocationKey* key)
{
    AreaRecord*     rec;
    AreaSavedState* areaState;
    s32             val;

    rec = Gp_AreaTables[key->stage];
    if (rec != NULL) {
        areaState = rec[key->area].savedState;
        if (areaState != NULL) {
            val = areaState->spawnFlags & AREA_SAVED_MAP_MARK;
            return val != 0;
        }
    }
    return 0;
}

_AreaMapMarkRec Gp_NewGameFlagsStg1[20] = {
    { 1, 0 },
    { 2, 0 },
    { 3, 1 },
    { 4, 0 },
    { 5, 0 },
    { 6, 0 },
    { 7, 1 },
    { 8, 0 },
    { 9, 0 },
    { 10, 1 },
    { 11, 1 },
    { 12, 0 },
    { 13, 0 },
    { 14, 1 },
    { 15, 1 },
    { 16, 0 },
    { 18, 0 },
    { 19, 0 },
    { AREA_MAP_MARK_END, 0 },
    { 0, 0 },
};
_AreaMapMarkRec Gp_NewGameFlagsStg2[28] = {
    { 1, 0 },
    { 2, 0 },
    { 3, 0 },
    { 5, 1 },
    { 6, 1 },
    { 7, 0 },
    { 9, 0 },
    { 11, 0 },
    { 12, 1 },
    { 15, 0 },
    { 16, 0 },
    { 18, 1 },
    { 19, 1 },
    { 20, 1 },
    { 21, 0 },
    { 22, 0 },
    { 23, 0 },
    { 24, 0 },
    { 25, 0 },
    { 26, 0 },
    { 27, 0 },
    { 29, 0 },
    { 30, 0 },
    { 32, 1 },
    { 34, 1 },
    { 38, 0 },
    { AREA_MAP_MARK_END, 0 },
    { 0, 0 },
};
_AreaMapMarkRec Gp_NewGameFlagsStg4[46] = {
    { 1, 0 },
    { 2, 0 },
    { 3, 1 },
    { 4, 0 },
    { 5, 0 },
    { 6, 0 },
    { 7, 0 },
    { 8, 0 },
    { 9, 0 },
    { 10, 1 },
    { 11, 1 },
    { 12, 1 },
    { 13, 0 },
    { 14, 1 },
    { 15, 1 },
    { 16, 0 },
    { 17, 0 },
    { 18, 1 },
    { 19, 1 },
    { 20, 0 },
    { 21, 0 },
    { 22, 0 },
    { 23, 0 },
    { 24, 1 },
    { 25, 1 },
    { 26, 0 },
    { 27, 1 },
    { 28, 1 },
    { 29, 1 },
    { 30, 0 },
    { 31, 0 },
    { 32, 1 },
    { 33, 1 },
    { 34, 0 },
    { 35, 1 },
    { 39, 1 },
    { 40, 0 },
    { 41, 0 },
    { 42, 1 },
    { 43, 1 },
    { 44, 1 },
    { 45, 1 },
    { 46, 1 },
    { 47, 0 },
    { 48, 0 },
    { AREA_MAP_MARK_END, 0 },
};
_AreaMapMarkRec Gp_NewGameFlagsStg5[34] = {
    { 1, 0 },
    { 2, 1 },
    { 3, 1 },
    { 4, 0 },
    { 5, 1 },
    { 6, 0 },
    { 7, 0 },
    { 8, 1 },
    { 9, 0 },
    { 10, 0 },
    { 11, 0 },
    { 12, 0 },
    { 13, 1 },
    { 14, 1 },
    { 15, 1 },
    { 16, 1 },
    { 17, 1 },
    { 18, 1 },
    { 19, 0 },
    { 20, 0 },
    { 21, 0 },
    { 22, 0 },
    { 23, 0 },
    { 24, 1 },
    { 25, 1 },
    { 26, 0 },
    { 27, 1 },
    { 28, 0 },
    { 29, 0 },
    { 30, 1 },
    { 31, 0 },
    { 32, 1 },
    { 33, 0 },
    { AREA_MAP_MARK_END, 0 },
};

void Gp_ApplyNewGameAreaFlags(void)
{
    {
        AreaRecord*      tbl;
        AreaSavedState*  areaState;
        _AreaMapMarkRec* entry;

        entry = Gp_NewGameFlagsStg1;
        tbl   = Gp_AreaTables[1];
        if (tbl != NULL) {
            for (; entry->area != AREA_MAP_MARK_END; entry++) {
                if (entry->setMapMark != 0) {
                    areaState = tbl[entry->area].savedState;
                    if (areaState != NULL) {
                        areaState->spawnFlags |= AREA_SAVED_MAP_MARK;
                    }
                }
            }
        }
    }
    {
        AreaRecord*      tbl;
        AreaSavedState*  areaState;
        _AreaMapMarkRec* entry;

        entry = Gp_NewGameFlagsStg2;
        tbl   = Gp_AreaTables[2];
        if (tbl != NULL) {
            for (; entry->area != AREA_MAP_MARK_END; entry++) {
                if (entry->setMapMark != 0) {
                    areaState = tbl[entry->area].savedState;
                    if (areaState != NULL) {
                        areaState->spawnFlags |= AREA_SAVED_MAP_MARK;
                    }
                }
            }
        }
    }
    {
        AreaRecord*      tbl;
        AreaSavedState*  areaState;
        _AreaMapMarkRec* entry;

        entry = Gp_NewGameFlagsStg4;
        tbl   = Gp_AreaTables[4];
        if (tbl != NULL) {
            for (; entry->area != AREA_MAP_MARK_END; entry++) {
                if (entry->setMapMark != 0) {
                    areaState = tbl[entry->area].savedState;
                    if (areaState != NULL) {
                        areaState->spawnFlags |= AREA_SAVED_MAP_MARK;
                    }
                }
            }
        }
    }
    {
        AreaRecord*      tbl;
        AreaSavedState*  areaState;
        _AreaMapMarkRec* entry;

        entry = Gp_NewGameFlagsStg5;
        tbl   = Gp_AreaTables[5];
        if (tbl != NULL) {
            for (; entry->area != AREA_MAP_MARK_END; entry++) {
                if (entry->setMapMark != 0) {
                    areaState = tbl[entry->area].savedState;
                    if (areaState != NULL) {
                        areaState->spawnFlags |= AREA_SAVED_MAP_MARK;
                    }
                }
            }
        }
    }
}

void Gp_RebuildAreaIdBits(void)
{
    GameLocationKey  key;
    GameLocationKey* sess;
    s32              count;
    s32              i;
    u8               stage;

    sess      = &gGameSession->location.loc;
    stage     = sess->stage;
    key.room  = 1;
    key.view  = 2;
    key.stage = stage;
    if (gGameSession->location.loc.stage - 1 < 5) {
        count = Gp_AreaIdCounts[sess->stage - 1];
        for (i = 1; i <= count; i++) {
            key.area = i;
            if (_gpGetAreaFlag4(&key) == 1) {
                if (areaIsSavedPoseRestoreEnabled(&key) == 1) {
                    if (key.area <= 32) {
                        Gp_AreaIdBits[0] &= ~(1 << (key.area - 1));
                    } else {
                        Gp_AreaIdBits[1] &= ~(1 << (key.area - 33));
                    }
                } else {
                    if (key.area <= 32) {
                        Gp_AreaIdBits[0] |= 1 << (key.area - 1);
                    } else {
                        Gp_AreaIdBits[1] |= 1 << (key.area - 33);
                    }
                }
            } else {
                if (key.area <= 32) {
                    Gp_AreaIdBits[0] &= ~(1 << (key.area - 1));
                } else {
                    Gp_AreaIdBits[1] &= ~(1 << (key.area - 33));
                }
            }
        }
    }
}

/// The flag entry `table[idx]`: its low 11 bits select a flag nibble, and its
/// bit 0x800 is added onto that nibble's value.
const TaskFuncTable3 Gp_DirTaskStates = { {
    _directionInitTask,
    Gp_DirTaskState1,
    taskKill,
} };

const DirectionActionTable Gp_DirActionFns = { {
    [WORLD_COLLISION_TRIGGER_ACTION_WARP]       = Gp_DirAction0,
    [WORLD_COLLISION_TRIGGER_ACTION_FACING]     = _directionUpdateStairAction,
    [WORLD_COLLISION_TRIGGER_ACTION_CAP]        = directionDispatchCapInteraction,
    [WORLD_COLLISION_TRIGGER_ACTION_CALLBACK]   = Gp_RunDirAction,
    [WORLD_COLLISION_TRIGGER_ACTION_CLEAR]      = _directionClearAction,
    [WORLD_COLLISION_TRIGGER_ACTION_ROOM]       = _directionDispatchRoomAction,
    [WORLD_COLLISION_TRIGGER_ACTION_CAP_WEAPON] = Gp_SpawnEvt1IfCapIdle,
} };

static const _DirectionWarpPhaseTable Gp_WarpPhaseFns = { {
    [DIRECTION_WARP_PHASE_QUERY]       = Gp_SetupDirWarp,
    [DIRECTION_WARP_PHASE_AWAIT_TURN]  = Gp_FadeDirWaitMsg,
    [DIRECTION_WARP_PHASE_HOLD]        = _directionHoldWarpFrame,
    [DIRECTION_WARP_PHASE_RESOLVE]     = Gp_CommitWarp,
    [DIRECTION_WARP_PHASE_AWAIT_SOUND] = Gp_WarpPhase4,
    [DIRECTION_WARP_PHASE_LEAVE]       = Gp_CommitSaveLoc,
} };

static const _DirectionFacingPhaseTable D_80093990 = { {
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
        MENU_MAP_ACROPOLIS_MARKER_COUNT            = 14,
        MENU_MAP_DRYFIELD_MARKER_COUNT             = 29,
        MENU_MAP_DRYFIELD_NIGHT_MARKER_COUNT       = 30,
        MENU_MAP_SHELTER_MARKER_COUNT              = 30,
        MENU_MAP_NEO_ARK_MARKER_COUNT              = 9,
        MENU_MAP_DRYFIELD_NIGHT_STORY_MARKER       = 29,
        MENU_MAP_SHELTER_ALTERNATE_PICTURE_CHAPTER = 6
    };

    switch (gGameSession->location.loc.stage) {
        case GAME_STAGE_ACROPOLIS:
            if (markerIndex >= MENU_MAP_ACROPOLIS_MARKER_COUNT) {
                break;
            }
            return _menuMapReadMarkerState(D_map_akropolis_8017AA0C, markerIndex);
        case GAME_STAGE_DRYFIELD:
            if (markerIndex >= MENU_MAP_DRYFIELD_MARKER_COUNT) {
                break;
            }
            return _menuMapReadMarkerState(D_map_dryfield_8017A824, markerIndex);
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
            return _menuMapReadMarkerState(D_map_dryfield_full_8017A738, markerIndex);
        case GAME_STAGE_MINE_SHELTER:
            if (markerIndex >= MENU_MAP_SHELTER_MARKER_COUNT) {
                break;
            }
            if (markerIndex == 0 && gameFlagGetNibble(GAME_FLAG_STORY_CHAPTER) == MENU_MAP_SHELTER_ALTERNATE_PICTURE_CHAPTER) {
                return gameFlagGetNibble(D_map_shelter_8017AD88[0] & MENU_MAP_MARKER_FLAG_ID_MASK) + MENU_MAP_MARKER_ALTERNATE_PICTURE;
            }
            return _menuMapReadMarkerState(D_map_shelter_8017AD88, markerIndex);
        case GAME_STAGE_SHELTER_NEO_ARK:
            if (markerIndex >= MENU_MAP_NEO_ARK_MARKER_COUNT) {
                break;
            }
            return _menuMapReadMarkerState(D_map_neo_ark_8017A9A0, markerIndex);
    }
    return MENU_MAP_MARKER_STATE_UNAVAILABLE;
}

void Gp_ClearAreaFlag4(GameLocationKey* key)
{
    AreaRecord*     rec;
    AreaSavedState* areaState;

    rec = Gp_AreaTables[key->stage];
    if (rec != NULL) {
        areaState = rec[key->area].savedState;
        if (areaState != NULL) {
            areaState->spawnFlags &= 0xFF ^ AREA_SAVED_MAP_MARK;
        }
    }
}

/// Resets direction action history and the area map-mark cache, then enables per-frame updates.
///
/// Runs once for a new direction task. Interaction activation is held off for
/// ten direction updates. The legacy D_80114CE0 initialization is retained;
/// that value has no observed reader.
static void _directionInitTask(Task* task)
{
    enum { DIRECTION_INITIAL_INTERACTION_DELAY_UPDATES = 10 };

    D_80114CDE       = SCENE_COMBAT_BATTLE_IDLE;
    D_80114CDD       = 0;
    Gp_DirFlags      = 0;
    D_80114CD0       = 0;
    D_80114CDC       = 0;
    Gp_DirByte       = 0;
    Gp_DirNibble     = 0;
    Gp_DirPhase      = 0;
    D_80114CF8       = 0;
    D_80114CE0       = 1;
    Gp_AreaIdBits[0] = 0;
    Gp_AreaIdBits[1] = 0;
    D_80114D08       = DIRECTION_INITIAL_INTERACTION_DELAY_UPDATES;
    task->state++;
}

static void Gp_DirTaskState1(Task* task)
{
    worldCollisionConsumeViewBoundaryHits();
    func_800AD6BC();
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
    states     = Gp_DirTaskStates;
    if (playerTask != NULL) {
        states.funcs[task->state](task);
    }
}

static u8 Gp_GetViewCountLo(void)
{
    GameSession*    session;
    ViewCountTable* tbl;

    session = gGameSession;
    tbl     = Gp_ViewCountTables[session->location.loc.stage - 1];
    return (u8)tbl->viewCounts[session->location.loc.area - 1][session->location.loc.room - 1];
}

static void Gp_DirAction0(void)
{
    _DirectionWarpPhaseTable phaseTable;

    phaseTable = Gp_WarpPhaseFns;
    phaseTable.handlers[(s16)Gp_DirPhase]();
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

    phaseTable = D_80093990;
    if (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_FINISHED) {
        if (D_80114CDE == SCENE_COMBAT_BATTLE_ENGAGED) {
            D_80114CDD = D_80114CDE;
        }
    }
    if (D_80114CDD != 0) {
        gSceneCombatState.signals.bytes.endDelayFrames = SCENE_COMBAT_END_DELAY_FRAMES;
    }
    phaseTable.handlers[(s16)Gp_DirPhase]();
}

/// Ends the active direction action and discards both latched trigger hits.
///
/// Leaves the session busy flag for the next direction update to release.
static void _directionClearAction(void)
{
    _directionClearTriggerParameters();
    D_80114CF8 = 0;
}

/// Dispatches a room action while events and CAP playback are idle, then consumes it.
///
/// Requires a live room task when the gates permit dispatch. The room borrows
/// the four-byte request only during synchronous dispatch; the second word is
/// zero and the reply is ignored. Blocked requests are also discarded. A newly
/// entered trigger keeps the session busy flag until the next direction update.
static void _directionDispatchRoomAction(void)
{
    DirectionActionRequest request;
    Task*                  roomTask;

    if (gGameSession->eventState == 0) {
        if (capIsBusy() == 0) {
            request.control  = Gp_DirFlags;
            request.actionId = Gp_DirByte;
            request.argument = Gp_DirNibble;
            roomTask         = gameGetTaskSlot(GAME_TASK_SLOT_ROOM);
            TASK_MESSAGE_DISPATCH_POINTER(roomTask, DIRECTION_MESSAGE_ROOM_ACTION, &request, 0);
        }
    }
    // Consume the request even when an event or CAP playback blocks dispatch.
    _directionClearTriggerParameters();
    D_80114CF8 = 0;
    if (D_80114CDC == 0) {
        gGameSession->dirActionBusy = 0;
    }
}

static void Gp_SpawnEvt1IfCapIdle(void)
{
    if (gGameSession->eventState == 0) {
        if (capIsBusy() == 0) {
            Gp_SpawnEvt1(Gp_DirByte, Gp_DirNibble);
        }
    }
    D_80114CF8      = 0;
    Gp_DirNibble    = 0;
    Gp_DirByte      = 0;
    Gp_DirFlags     = 0;
    Gp_DirAltNibble = 0;
    Gp_DirAlt       = 0;
    D_80114CD4      = 0;
}

/// Draws the active departure fade before stepping its shade toward full subtraction.
///
/// Uses the low byte as the shade and preserves the signed-halfword zero gate
/// of this phase. The active ramp starts at 30 and stays within 30..255.
static inline void _directionStepDepartureFade(void)
{
    enum { DIRECTION_DEPARTURE_FADE_STEP = 30,
           DIRECTION_DEPARTURE_FADE_MAX  = 255 };
    u8  fadeShade;
    s16 activeShade;

    activeShade = (s16)Gp_DirFadeLevel;
    if (activeShade != 0) {
        fadeShade = (u8)Gp_DirFadeLevel;
        fadeDrawOverlay(fadeShade, fadeShade, fadeShade, GPU_BLEND_SUBTRACT);
        Gp_DirFadeLevel += DIRECTION_DEPARTURE_FADE_STEP;
        if ((s16)Gp_DirFadeLevel >= DIRECTION_DEPARTURE_FADE_MAX + 1) {
            Gp_DirFadeLevel = DIRECTION_DEPARTURE_FADE_MAX;
        }
    }
}

/// Draws and steps the departure fade, then hands the warp to its resolve phase next frame.
///
/// This hold phase advances unconditionally, adding one frame after the turn
/// finishes. A nonzero fade draws its current low byte before increasing by
/// 30 toward 255; zero leaves the fade disabled.
static void _directionHoldWarpFrame(void)
{
    _directionStepDepartureFade();
    Gp_DirPhase++;
}

static void Gp_CommitSaveLoc(void)
{
    u8 fade;

    if (*(s16*)&Gp_DirFadeLevel != 0) {
        fade = *(u8*)&Gp_DirFadeLevel;
        fadeDrawOverlay(fade, fade, fade, GPU_BLEND_SUBTRACT);
    }
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area = (u8)Gp_WarpLoc.areaId;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp = Gp_WarpLoc.warp;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = Gp_WarpLoc.room;
    taskSpawn(0, 0x11, 0, 0);
    D_80114CF8   = 0;
    Gp_DirNibble = 0;
    Gp_DirByte   = 0;
    Gp_DirFlags  = 0;
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
        D_80114CF8 = 0;
        _directionClearTriggerParameters();
        D_80114CDD = 0;
    } else {
        facing.rot.vx = 0;
        facing.rot.vz = 0;
        facing.rot.vy = Gp_DirNibble << DIRECTION_STAIR_YAW_SHIFT;
        TASK_MESSAGE_DISPATCH_POINTER(playerTask, GAME_ACTOR_MESSAGE_TURN_TO_YAW, &facing, 0);
        Gp_DirPhase++;
    }
}

/// Waits for the player's scripted turn to settle before starting the stair climb.
///
/// Requires the player used by the preceding turn phase. Takes no payload and
/// advances one phase only when scripted motion is no longer pending.
static void _directionAwaitStairTurn(void)
{
    if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) == 0) {
        Gp_DirPhase++;
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
    climb.descend   = (Gp_DirFlags >> DIRECTION_STAIR_DESCEND_SHIFT) & DIRECTION_STAIR_DESCEND_MASK;
    climb.stepCount = Gp_DirByte & DIRECTION_STAIR_STEP_COUNT_MASK;
    TASK_MESSAGE_DISPATCH_POINTER(playerTask, GAME_ACTOR_MESSAGE_CLIMB_STAIRS, &climb, 0);
    Gp_DirAltNibble = 0;
    Gp_DirAlt       = 0;
    D_80114CD4      = 0;
    Gp_DirPhase++;
}

void Gp_SetCurAreaFlag4(void)
{
    GameLocationKey* key;
    AreaRecord*      rec;
    AreaSavedState*  areaState;

    key = &gGameSession->location.loc;
    rec = Gp_AreaTables[key->stage];
    if (rec != NULL) {
        areaState = rec[key->area].savedState;
        if (areaState != NULL) {
            areaState->spawnFlags |= AREA_SAVED_MAP_MARK;
        }
    }
}

static void Gp_ApplyAreaFlag4List(s16 arg0, _AreaMapMarkRec* entry)
{
    AreaRecord*     rec;
    AreaSavedState* areaState;

    rec = Gp_AreaTables[arg0];
    if (rec != NULL) {
        for (; entry->area != AREA_MAP_MARK_END; entry++) {
            if (entry->setMapMark != 0) {
                areaState = rec[entry->area].savedState;
                if (areaState != NULL) {
                    areaState->spawnFlags |= AREA_SAVED_MAP_MARK;
                }
            }
        }
    }
}
