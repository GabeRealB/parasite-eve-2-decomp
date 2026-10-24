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

#include "stages/akropolis/rooms/bridge.h"

#include "stages/akropolis/rooms/fountain.h"

#include "stages/akropolis/rooms/helicopter_landing_pad.h"

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

/// Water-hole surface classes for ascent, indexed by completed stair steps.
///
/// The five-step flight reads entries 0..5, including its landing. Entries
/// 6..9 are retained but have no established reader; the class changes from
/// 4 to 6 after two steps. These bytes live with gameplay, not the room overlay.
static u8 _gDirectionWaterHoleStairAscentSurfaces[10] = {
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
/// Water-hole surface classes for descent, indexed by completed stair steps.
///
/// The two-step descent reads entries 0..2, including its landing, all class 4.
/// Entries 3..7 are retained but have no established reader.
static u8 _gDirectionWaterHoleStairDescentSurfaces[8] = {
    4,
    4,
    4,
    4,
    4,
    4,
    4,
    4
};

/// Stair-flight surface rows selected by bits 4..6 of an indexed stair parameter.
///
/// Slots 0..4 are the Acropolis bridge, Dryfield water hole, two water-supply
/// flights and the reservoir. Selectors 5..7 have no record. Each direction's
/// row must cover the trigger's step count plus the landing. Borrowed room
/// rows must remain loaded through the climb; the water-hole rows live here.
static _DirectionStairSurfaces _gDirectionStairSurfaceFlights[5] = {
    { gAkropolisBridgeStairSurfaceRows[0], gAkropolisBridgeStairSurfaceRows[1] },
    { _gDirectionWaterHoleStairAscentSurfaces, _gDirectionWaterHoleStairDescentSurfaces },
    { gShelterB4WaterSupplyStairSurfaceRows[0], gShelterB4WaterSupplyStairSurfaceRows[1] },
    { gShelterB4WaterSupplyStairSurfaceRows[2], gShelterB4WaterSupplyStairSurfaceRows[3] },
    { gShelterB4ReservoirStairAscentSurfaces, gShelterB4ReservoirStairDescentSurfaces }
};

/// Releases action activity, then discards the primary parameters and secondary hit.
///
/// Used on stair-climb completion and after consuming a CAP interaction.
/// The phase, trigger-control change latch and session busy flag are left for callers.
static inline void _directionClearRequest(void)
{
    gDirectionActionActive = false;
    _directionClearTriggerParameters();
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
    stairParameter = gDirectionActionParameter0;
    if (stairParameter & DIRECTION_STAIR_INDEXED_SURFACES) {
        stepsRemaining = player->scriptMotion.jumpSteps;
        if (gDirectionActionControl & DIRECTION_STAIR_DESCEND) {
            stepSurfaces         = _gDirectionStairSurfaceFlights[(stairParameter & DIRECTION_STAIR_SURFACE_MASK) >> DIRECTION_STAIR_SURFACE_SHIFT].descent;
            player->surfaceClass = stepSurfaces[(stairParameter & DIRECTION_STAIR_STEP_COUNT_MASK) - stepsRemaining];
        } else {
            stepSurfaces         = _gDirectionStairSurfaceFlights[(stairParameter & DIRECTION_STAIR_SURFACE_MASK) >> DIRECTION_STAIR_SURFACE_SHIFT].ascent;
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
        gDirectionStairBattleFinished = false;
    } else if (worldCollisionReadActionHit(&gDirectionSecondaryActionControl, &gDirectionSecondaryActionParameter0, &gDirectionSecondaryActionParameter1)) {
        if ((u8)gDirectionSecondaryActionControl == WORLD_COLLISION_TRIGGER_ACTION_WARP) {
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
            gDirectionActionPhase++;
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
    destination = &gDirectionWarpRequest;

    // Resolve the warp encountered during the climb within the active stage.
    gDirectionWarpRequest.areaId = gDirectionSecondaryActionParameter0;
    destination->warp            = gDirectionSecondaryActionParameter1 & DIRECTION_WARP_ARRIVAL_MASK;
    destination->field_4         = 1;
    destination->roomState       = DIRECTION_WARP_DEFAULT_ROOM;
    destination->queryOnly       = ROOM_EVENT_EXECUTE;
    TASK_MESSAGE_DISPATCH_POINTERS(roomTask, ROOM_EVENT_MESSAGE_RESOLVE, destination, destination);

    // Publish only the resolved destination selectors, then queue the area change.
    liveSave                           = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    liveSave->state.location.area      = (u8)gDirectionWarpRequest.areaId;
    liveSave->state.location.warp      = destination->warp;
    liveSave->state.location.roomState = destination->roomState;
    taskSpawn(DIRECTION_AREA_CHANGE_TASK_BANK, DIRECTION_AREA_CHANGE_TASK_ID, 0, 0);

    gDirectionSecondaryActionParameter1 = 0;
    gDirectionSecondaryActionParameter0 = 0;
    gDirectionActionActive              = false;
    gDirectionActionParameter1          = 0;
    gDirectionActionParameter0          = 0;
    gDirectionActionControl             = 0;
    gDirectionSecondaryActionControl    = 0;
    gDirectionStairBattleFinished       = false;
}

void directionDispatchCapInteraction(void)
{
    if (gGameSession->eventState == 0) {
        if (capIsBusy() == 0) {
            if (gDirectionActionParameter1 == WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE) {
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_ROOM), ROOM_MESSAGE_COMMAND, gDirectionActionParameter0, 0);
            } else {
                capSpawnEventIfIdle(gDirectionActionParameter0, gDirectionActionParameter1);
            }
        }
    }
    // A blocked interaction is consumed too; it is never retried by this action.
    _directionClearRequest();
    if (gDirectionActionControlChanged == 0) {
        gGameSession->dirActionBusy = 0;
    }
}

void directionDispatchCallbackAction(void)
{
    enum {
        DIRECTION_CALLBACK_INDEX_SHIFT = 8,
        DIRECTION_CALLBACK_INDEX_MASK  = 0x7F,
    };
    void (*callbacks[])(s32 parameter0, s32 parameter1) = { akropolisFountainStartClimb, akropolisHelicopterLandingPadStartExit };

    if (gGameSession->eventState != 0) {
        _directionClearRequest();
    } else {
        callbacks[(gDirectionActionControl >> DIRECTION_CALLBACK_INDEX_SHIFT) & DIRECTION_CALLBACK_INDEX_MASK](gDirectionActionParameter0, gDirectionActionParameter1);
        _directionClearRequest();
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
    const AreaRecord*      areaRecords;
    AreaSavedState*        savedState;
    const GameLocationKey* currentLocation;
    s32                    recordIndex;
    s32                    stageId;
    s32                    modePolicy;
    s8                     shouldApply;
    s8                     saveMode;

    shouldApply     = 0;
    currentLocation = &gGameSession->location;
    for (recordIndex = 0; records[recordIndex].stage != AREA_APPLY_END; recordIndex++) {
        stageId            = records[recordIndex].stage;
        areaRecords        = gAreaRecordsByStage[stageId];
        location.stage     = stageId;
        location.area      = records[recordIndex].area;
        location.roomState = AREA_APPLY_DEFAULT_ROOM;
        location.view      = currentLocation->view;
        modePolicy         = records[recordIndex].policy & AREA_APPLY_MODE_MASK;
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
