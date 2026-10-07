#include "gameplay/area_transitions.h"

#include "common.h"

#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/captions.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "direction_input.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"

#include "main/mc.h"
#include "main/session.h"
#include "main/task.h"

#include "rooms/acropolis_bridge.h"

#include "rooms/acropolis_fountain.h"

#include "rooms/acropolis_helicopter_landing_pad.h"

#include "rooms/shelter_b4_reservoir.h"

#include "rooms/shelter_b4_water_supply.h"

/// Surface classes along one stair flight whose surface changes on the way.
///
/// A stair trigger normally carries a single surface class for its whole
/// flight; one that sets bit 7 of its first parameter selects a flight record
/// instead. Each row gives the `GameActor.surfaceClass` to apply once a given
/// number of steps has been taken, so a climb of `n` steps reads entries 0
/// through `n` of the row for its direction. The rows are borrowed and are
/// only read; most belong to the room that owns the flight.
typedef struct {
    u8* ascent;  // Row for a walk up the flight, indexed by steps taken so far
    u8* descent; // Row for a walk down the flight, indexed the same way
} _DirectionStairSurfaces;
STATIC_ASSERT_SIZEOF(_DirectionStairSurfaces, 8);

extern _DirectionStairSurfaces D_801149FC[];

extern u8 D_801149E8[10];

extern u8 D_801149F4[8];

u8 D_801149E8[10] = {
    4,
    4,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6
};
u8 D_801149F4[8] = {
    4,
    4,
    4,
    4,
    4,
    4,
    4,
    4
};

_DirectionStairSurfaces D_801149FC[5] = {
    { D_acropolis_bridge_80189A9C, &D_acropolis_bridge_80189A9C[12] },
    { D_801149E8, D_801149F4 },
    { D_shelter_b4_water_supply_801826FC, &D_shelter_b4_water_supply_801826FC[16] },
    { &D_shelter_b4_water_supply_801826FC[32], &D_shelter_b4_water_supply_801826FC[48] },
    { D_shelter_b4_reservoir_801850C8, D_shelter_b4_reservoir_801850D8 }
};

/// Ends the latched action and discards any secondary trigger hit.
static inline void _directionClearRequest(void)
{
    D_80114CF8      = 0;
    Gp_DirNibble    = 0;
    Gp_DirByte      = 0;
    Gp_DirFlags     = 0;
    Gp_DirAltNibble = 0;
    Gp_DirAlt       = 0;
    D_80114CD4      = 0;
}

void directionAwaitStairClimb(void)
{
    // FACING parameter0 packs a step count and either a class or a flight index.
    enum {
        DIRECTION_STAIR_INDEXED_SURFACES = 0x80,
        DIRECTION_STAIR_SURFACE_MASK     = 0x70,
        DIRECTION_STAIR_SURFACE_SHIFT    = 4,
        DIRECTION_STAIR_STEP_COUNT_MASK  = 0x0F,
        DIRECTION_STAIR_DESCEND          = 0x100
    };
    Task*      playerTask;
    GameActor* player;
    u8         stairParameter;
    s32        stepsRemaining;
    const u8*  stepSurfaces;

    // Apply the surface at the current step, including the final landing.
    player         = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->work;
    stairParameter = Gp_DirByte;
    if (stairParameter & DIRECTION_STAIR_INDEXED_SURFACES) {
        stepsRemaining = player->scriptMotion.jumpSteps;
        if (Gp_DirFlags & DIRECTION_STAIR_DESCEND) {
            stepSurfaces         = D_801149FC[(stairParameter & DIRECTION_STAIR_SURFACE_MASK) >> DIRECTION_STAIR_SURFACE_SHIFT].descent;
            player->surfaceClass = stepSurfaces[(stairParameter & DIRECTION_STAIR_STEP_COUNT_MASK) - stepsRemaining];
        } else {
            stepSurfaces         = D_801149FC[(stairParameter & DIRECTION_STAIR_SURFACE_MASK) >> DIRECTION_STAIR_SURFACE_SHIFT].ascent;
            player->surfaceClass = stepSurfaces[(stairParameter & DIRECTION_STAIR_STEP_COUNT_MASK) - stepsRemaining];
        }
    } else {
        player->surfaceClass = (stairParameter & DIRECTION_STAIR_SURFACE_MASK) >> DIRECTION_STAIR_SURFACE_SHIFT;
    }

    // Completion releases scripted control; a warp encountered sooner chains on.
    playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    if (taskMessageDispatch(playerTask, GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) == 0) {
        taskMessageDispatch(playerTask, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
        _directionClearRequest();
        D_80114CDD = 0;
    } else if (worldCollisionReadActionHit(&D_80114CD4, &Gp_DirAlt, &Gp_DirAltNibble)) {
        if ((u8)D_80114CD4 == WORLD_COLLISION_TRIGGER_ACTION_WARP) {
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
            Gp_DirPhase++;
        }
    }
}

void directionCommitStairWarp(void)
{
    enum {
        DIRECTION_WARP_ARRIVAL_MASK     = 0x0F,
        DIRECTION_WARP_DEFAULT_ROOM     = 1,
        DIRECTION_AREA_CHANGE_TASK_BANK = 0,
        DIRECTION_AREA_CHANGE_TASK_ID   = 0x11
    };
    Task*         roomTask;
    RoomEventMsg* destination;
    McSaveData*   liveSave;

    roomTask    = gameGetTaskSlot(GAME_TASK_SLOT_ROOM);
    destination = &Gp_WarpLoc;

    // Resolve the warp encountered during the climb within the active stage.
    Gp_WarpLoc.areaId      = Gp_DirAlt;
    destination->warp      = Gp_DirAltNibble & DIRECTION_WARP_ARRIVAL_MASK;
    destination->field_4   = 1;
    destination->room      = DIRECTION_WARP_DEFAULT_ROOM;
    destination->queryOnly = ROOM_EVENT_EXECUTE;
    TASK_MESSAGE_DISPATCH_POINTERS(roomTask, ROOM_EVENT_MESSAGE_RESOLVE, destination, destination);

    // Publish only the resolved destination selectors, then queue the area change.
    liveSave                          = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    liveSave->state.location.loc.area = (u8)Gp_WarpLoc.areaId;
    liveSave->state.location.loc.warp = destination->warp;
    liveSave->state.location.loc.room = destination->room;
    taskSpawn(DIRECTION_AREA_CHANGE_TASK_BANK, DIRECTION_AREA_CHANGE_TASK_ID, 0, 0);

    Gp_DirAltNibble = 0;
    Gp_DirAlt       = 0;
    D_80114CF8      = 0;
    Gp_DirNibble    = 0;
    Gp_DirByte      = 0;
    Gp_DirFlags     = 0;
    D_80114CD4      = 0;
    D_80114CDD      = 0;
}

void directionDispatchCapInteraction(void)
{
    if (gGameSession->eventState == 0) {
        if (capIsBusy() == 0) {
            if (Gp_DirNibble == WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE) {
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_ROOM), ROOM_MESSAGE_COMMAND, Gp_DirByte, 0);
            } else {
                capSpawnEventIfIdle(Gp_DirByte, Gp_DirNibble);
            }
        }
    }
    // A blocked interaction is consumed too; it is never retried by this action.
    _directionClearRequest();
    if (D_80114CDC == 0) {
        gGameSession->dirActionBusy = 0;
    }
}

void Gp_RunDirAction(void)
{
    void (*fns[2])(s32, s32) = { func_acropolis_fountain_8017DA78, func_acropolis_helicopter_landing_pad_8017EF60 };

    if (gGameSession->eventState != 0) {
        D_80114CF8      = 0;
        Gp_DirNibble    = 0;
        Gp_DirByte      = 0;
        Gp_DirFlags     = 0;
        Gp_DirAltNibble = 0;
        Gp_DirAlt       = 0;
        D_80114CD4      = 0;
    } else {
        fns[(Gp_DirFlags >> 8) & 0x7F](Gp_DirByte, Gp_DirNibble);
        D_80114CF8      = 0;
        Gp_DirNibble    = 0;
        Gp_DirByte      = 0;
        Gp_DirFlags     = 0;
        Gp_DirAltNibble = 0;
        Gp_DirAlt       = 0;
        D_80114CD4      = 0;
    }
}

void areaApplySavedUpdates(const AreaApplyRec* records)
{
    // Stored save modes accepted by the record's two optional mode groups.
    enum {
        AREA_APPLY_SAVE_MODE_NORMAL_REPLAY = 0,
        AREA_APPLY_SAVE_MODE_BOUNTY        = 1,
        AREA_APPLY_SAVE_MODE_SCAVENGER     = 2,
        AREA_APPLY_SAVE_MODE_NIGHTMARE     = 3,
        AREA_APPLY_DEFAULT_ROOM            = 1
    };
    GameLocationKey        location;
    AreaRecord*            areaRecords;
    AreaSavedState*        savedState;
    const GameLocationKey* currentLocation;
    s32                    recordIndex;
    s32                    stageId;
    s32                    modePolicy;
    s8                     shouldApply;
    s8                     saveMode;

    shouldApply     = 0;
    currentLocation = &gGameSession->location.loc;
    for (recordIndex = 0; records[recordIndex].stage != AREA_APPLY_END; recordIndex++) {
        stageId        = records[recordIndex].stage;
        areaRecords    = Gp_AreaTables[stageId];
        location.stage = stageId;
        location.area  = records[recordIndex].area;
        location.room  = AREA_APPLY_DEFAULT_ROOM;
        location.view  = currentLocation->view;
        modePolicy     = records[recordIndex].policy & AREA_APPLY_MODE_MASK;
        if (modePolicy == AREA_APPLY_MODE_ALWAYS) {
            shouldApply = 1;
        } else {
            saveMode = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.gameMode;
            if (saveMode == AREA_APPLY_SAVE_MODE_NORMAL_REPLAY || saveMode == AREA_APPLY_SAVE_MODE_SCAVENGER) {
                if (modePolicy == AREA_APPLY_MODE_REPLAY_SCAVENGER) {
                    shouldApply = 1;
                }
            } else if (saveMode == AREA_APPLY_SAVE_MODE_BOUNTY || saveMode == AREA_APPLY_SAVE_MODE_NIGHTMARE) {
                if (modePolicy == AREA_APPLY_MODE_BOUNTY_NIGHTMARE) {
                    shouldApply = 1;
                }
            } else {
                shouldApply = 0;
            }
        }
        if (shouldApply) {
            // Changing a placement layout also removes this area's saved enemy poses.
            areaSetPlacementVariant(&location, records[recordIndex].variant, AREA_VARIANT_RESET_ALWAYS);
            if (areaRecords != NULL) {
                savedState = areaRecords[records[recordIndex].area].savedState;
                if (savedState != NULL) {
                    if (records[recordIndex].policy & AREA_APPLY_MAP_MARK_MASK) {
                        savedState->spawnFlags |= AREA_SAVED_MAP_MARK;
                    } else {
                        savedState->spawnFlags &= (u8)~AREA_SAVED_MAP_MARK;
                    }
                }
            }
        }
        shouldApply = 0;
    }
}
