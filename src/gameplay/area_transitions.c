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

/// `Gp_AreaTables[1]`, `[2]`, `[4]`, `[5]`. Splat labels the later slots as
/// their own symbols; `Gp_ApplyNewGameAreaFlags` loads each as an `AreaRecord*`.
#define Gp_AreaTableStg1 Gp_AreaTables[1]

#define Gp_AreaTableStg2 Gp_AreaTables[2]

#define Gp_AreaTableStg4 Gp_AreaTables[4]

#define Gp_AreaTableStg5 Gp_AreaTables[5]

/// `AREA_MAP_MARK_END`-terminated `_AreaMapMarkRec` lists applied by `Gp_ApplyNewGameAreaFlags` to
/// `Gp_AreaTableStg1` / `Gp_AreaTableStg2` / `Gp_AreaTableStg4` / `Gp_AreaTableStg5` (stages 1, 2,
/// 4, 5 of `Gp_AreaTables`).
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

static inline s16 _gpStageFlagNibble(u16* table, s16 idx);

static void Gp_InitDirState(Task* arg0);

static void Gp_DirTaskState1(Task* task);

static u8 Gp_GetViewCountLo(void);

static void Gp_DirAction0(void);

static void Gp_DirAction1(void);

static void Gp_ClearDirCursor(void);

static void Gp_PostMsg13EF(void);

static void Gp_SpawnEvt1IfCapIdle(void);

static void Gp_FadeDirAdvance(void);

static void Gp_CommitSaveLoc(void);

static void Gp_MsgPlayer3EE(void);

static void Gp_MsgPlayer3F0(void);

static void Gp_MsgPlayer3EF(void);

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
        tbl   = Gp_AreaTableStg1;
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
        tbl   = Gp_AreaTableStg2;
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
        tbl   = Gp_AreaTableStg4;
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
        tbl   = Gp_AreaTableStg5;
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
                if (Gp_GetAreaFlag2(&key) == 1) {
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
    Gp_InitDirState,
    Gp_DirTaskState1,
    taskKill,
} };

const DirectionActionTable Gp_DirActionFns = { {
    [WORLD_COLLISION_TRIGGER_ACTION_WARP]       = Gp_DirAction0,
    [WORLD_COLLISION_TRIGGER_ACTION_FACING]     = Gp_DirAction1,
    [WORLD_COLLISION_TRIGGER_ACTION_CAP]        = Gp_PostDirIfCapIdle,
    [WORLD_COLLISION_TRIGGER_ACTION_CALLBACK]   = Gp_RunDirAction,
    [WORLD_COLLISION_TRIGGER_ACTION_CLEAR]      = Gp_ClearDirCursor,
    [WORLD_COLLISION_TRIGGER_ACTION_ROOM]       = Gp_PostMsg13EF,
    [WORLD_COLLISION_TRIGGER_ACTION_CAP_WEAPON] = Gp_SpawnEvt1IfCapIdle,
} };

static const _DirectionWarpPhaseTable Gp_WarpPhaseFns = { {
    [DIRECTION_WARP_PHASE_QUERY]       = Gp_SetupDirWarp,
    [DIRECTION_WARP_PHASE_AWAIT_TURN]  = Gp_FadeDirWaitMsg,
    [DIRECTION_WARP_PHASE_HOLD]        = Gp_FadeDirAdvance,
    [DIRECTION_WARP_PHASE_RESOLVE]     = Gp_CommitWarp,
    [DIRECTION_WARP_PHASE_AWAIT_SOUND] = Gp_WarpPhase4,
    [DIRECTION_WARP_PHASE_LEAVE]       = Gp_CommitSaveLoc,
} };

static const _DirectionFacingPhaseTable D_80093990 = { {
    [DIRECTION_FACING_PHASE_TURN]        = Gp_MsgPlayer3EE,
    [DIRECTION_FACING_PHASE_AWAIT_TURN]  = Gp_MsgPlayer3F0,
    [DIRECTION_FACING_PHASE_CLIMB]       = Gp_MsgPlayer3EF,
    [DIRECTION_FACING_PHASE_AWAIT_CLIMB] = Gp_MsgPlayerDirFacing,
    [DIRECTION_FACING_PHASE_WARP]        = Gp_CommitDirWarp,
} };

static inline s16 _gpStageFlagNibble(u16* table, s16 idx)
{
    return gameFlagGetNibble(table[idx] & 0x7FF) + (table[idx] & 0x800);
}

s16 Gp_LookupStageFlag(s16 idx)
{
    switch (gGameSession->location.loc.stage) {
        case GAME_STAGE_ACROPOLIS:
            if (idx >= 0xE) {
                break;
            }
            return _gpStageFlagNibble(D_map_akropolis_8017AA0C, idx);
        case GAME_STAGE_DRYFIELD:
            if (idx >= 0x1D) {
                break;
            }
            return _gpStageFlagNibble(D_map_dryfield_8017A824, idx);
        case GAME_STAGE_DRYFIELD_NIGHT:
            if (idx >= 0x1E) {
                break;
            }
            if (idx == 0x1D) {
                if (gameFlagGetNibble(GAME_FLAG_07F) == 0) {
                    return 0;
                }
                return 0x802;
            }
            return _gpStageFlagNibble(D_map_dryfield_full_8017A738, idx);
        case GAME_STAGE_MINE_SHELTER:
            if (idx >= 0x1E) {
                break;
            }
            if (idx == 0 && gameFlagGetNibble(GAME_FLAG_STORY_CHAPTER) == 6) {
                return gameFlagGetNibble(D_map_shelter_8017AD88[0] & 0x7FF) + 0x800;
            }
            return _gpStageFlagNibble(D_map_shelter_8017AD88, idx);
        case GAME_STAGE_SHELTER_NEO_ARK:
            if (idx >= 9) {
                break;
            }
            return _gpStageFlagNibble(D_map_neo_ark_8017A9A0, idx);
    }
    return -1;
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

static void Gp_InitDirState(Task* arg0)
{
    D_80114CDE       = 0;
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
    D_80114D08       = 0xA;
    arg0->state++;
}

static void Gp_DirTaskState1(Task* task)
{
    worldCollisionConsumeViewBoundaryHits();
    func_800AD6BC();
}

s32 Gp_YawToPosXZ(Task* arg0, SVECTOR* arg1)
{
    SVECTOR   vec;
    GfxCoord* coord;

    coord  = arg0->extra.tmd->coords;
    vec.vx = arg1->vx - coord->coord.t[0];
    vec.vy = 0;
    vec.vz = arg1->vz - coord->coord.t[2];
    VectorNormalSS(&vec, &vec);
    return ratan2(vec.vx, vec.vz) & 0xFFF;
}

void func_800AEE8C(Task* arg0)
{
    TaskFuncTable3 sp;
    Task*          playerTask;

    playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    sp         = Gp_DirTaskStates;
    if (playerTask != NULL) {
        sp.funcs[arg0->state](arg0);
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

static void Gp_DirAction1(void)
{
    _DirectionFacingPhaseTable phaseTable;

    phaseTable = D_80093990;
    if (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_FINISHED) {
        if (D_80114CDE == 1) {
            D_80114CDD = D_80114CDE;
        }
    }
    if (D_80114CDD != 0) {
        gSceneCombatState.signals.bytes.endDelayFrames = SCENE_COMBAT_END_DELAY_FRAMES;
    }
    phaseTable.handlers[(s16)Gp_DirPhase]();
}

static void Gp_ClearDirCursor(void)
{
    Gp_DirNibble    = 0;
    Gp_DirByte      = 0;
    Gp_DirFlags     = 0;
    Gp_DirAltNibble = 0;
    Gp_DirAlt       = 0;
    D_80114CD4      = 0;
    D_80114CF8      = 0;
}

static void Gp_PostMsg13EF(void)
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
    Gp_DirNibble    = 0;
    Gp_DirByte      = 0;
    Gp_DirFlags     = 0;
    Gp_DirAltNibble = 0;
    Gp_DirAlt       = 0;
    D_80114CD4      = 0;
    D_80114CF8      = 0;
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

static void Gp_FadeDirAdvance(void)
{
    u8 fade;

    if (*(s16*)&Gp_DirFadeLevel != 0) {
        fade = *(u8*)&Gp_DirFadeLevel;
        fadeDrawOverlay(fade, fade, fade, GPU_BLEND_SUBTRACT);
        Gp_DirFadeLevel += 0x1E;
        if ((s16)Gp_DirFadeLevel >= 0x100) {
            Gp_DirFadeLevel = 0xFF;
        }
    }
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

static void Gp_MsgPlayer3EE(void)
{
    ActorTransform sp;
    Task*          playerTask;

    playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    if (gGameSession->eventState != 0) {
        D_80114CF8      = 0;
        Gp_DirNibble    = 0;
        Gp_DirByte      = 0;
        Gp_DirFlags     = 0;
        Gp_DirAltNibble = 0;
        Gp_DirAlt       = 0;
        D_80114CD4      = 0;
        D_80114CDD      = 0;
    } else {
        sp.rot.vx = 0;
        sp.rot.vz = 0;
        sp.rot.vy = Gp_DirNibble << 4;
        TASK_MESSAGE_DISPATCH_POINTER(playerTask, 0x3EE, &sp, 0);
        Gp_DirPhase++;
    }
}

static void Gp_MsgPlayer3F0(void)
{
    if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) == 0) {
        Gp_DirPhase++;
    }
}

static void Gp_MsgPlayer3EF(void)
{
    GameActorStairClimb climb;
    Task*               playerTask;

    playerTask      = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    climb.descend   = (Gp_DirFlags >> 8) & 1;
    climb.stepCount = Gp_DirByte & 0xF;
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
