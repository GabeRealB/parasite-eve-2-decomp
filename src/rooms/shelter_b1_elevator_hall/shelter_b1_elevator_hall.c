#include "rooms/shelter_b1_elevator_hall.h"

#include "types.h"

#include "shelter_b1_elevator_hall_private.h"

#include "gameplay/companion_load.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/direction.h"
#include "gameplay/display.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/message.h"
#include "gameplay/scene_combat.h"

#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_shelter.h"

#include "rooms/room_common.h"

extern RoomEventMsg D_shelter_b1_elevator_hall_801849F8;

static void _shelterB1ElevatorHallInitRoomTask(Task* task);
static void _shelterB1ElevatorHallIdleRoomTask(Task* task);

RoomEventMsg D_shelter_b1_elevator_hall_801849F8;

#include "../../shared/shelter_elevator_task.inc.c"

s32 shelterB1ElevatorHallResolveRoomTransition(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    enum { TRANSITION_REFUSED          = 0,
           TRANSITION_DIRECT           = 1,
           TRANSITION_DEFERRED         = 2,
           TRANSITION_REFUSAL_FLAG     = 2,
           CAP_COMMAND_CORRIDOR_LOCKED = 2,
           CAP_COMMAND_ELEVATOR_LOCKED = 1,
           CAP_COMMAND_ELEVATOR_RIDE   = 4 };

    *reply = *request;
    mapShelterRoomVariantResolve(request, reply);
    if (request->areaId == GAME_AREA_SHELTER_B1_MAIN_CORRIDOR && gameFlagGetNibble(GAME_FLAG_B1_CORRIDOR_ELEVATOR_HALL_UNLOCKED) == 0) {
        if (request->queryOnly == ROOM_EVENT_EXECUTE) {
            gameFlagSetNibbleIfPresent(request->flagId, TRANSITION_REFUSAL_FLAG);
            capRunCommandWithTransition(CAP_COMMAND_CORRIDOR_LOCKED);
        }
        return TRANSITION_REFUSED;
    }
    if (request->areaId == GAME_AREA_SHELTER_B2_ELEVATOR) {
        if (gameFlagGetNibble(GAME_FLAG_SHELTER_ELEVATOR_ENABLED) == 0) {
            if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                gameFlagSetNibbleIfPresent(request->flagId, TRANSITION_REFUSAL_FLAG);
                capRunCommandWithTransition(CAP_COMMAND_ELEVATOR_LOCKED);
            }
        } else {
            if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                capRunCommand(CAP_COMMAND_ELEVATOR_RIDE, CAP_PLAYBACK_IN_PLACE);
                taskSpawnFromTable(&D_shelter_b1_elevator_hall_80182CAC, 0, SOUND_SHELTER_B1_ELEVATOR_RIDE, 0);
            }
        }
        return TRANSITION_REFUSED;
    }
    if (request->areaId == GAME_AREA_MINE_SECRET_PASSAGE) {
        if (request->queryOnly == ROOM_EVENT_EXECUTE) {
            // Keep the resolved destination until the passage choice and fade finish.
            D_shelter_b1_elevator_hall_801849F8.warp              = (u8)reply->areaId;
            D_shelter_b1_elevator_hall_801849F8.field_4           = reply->warp;
            ((u8*)&D_shelter_b1_elevator_hall_801849F8.areaId)[1] = reply->room;
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            taskSpawnFromTable(&D_shelter_b1_elevator_hall_80182CE8, 0, 0, 0);
        }
        return TRANSITION_DEFERRED;
    }
    return TRANSITION_DIRECT;
}

/// The room task's state table, dispatched by
/// `shelterB1ElevatorHallRoomTask` from a stack copy.
static const TaskFuncTable3 D_shelter_b1_elevator_hall_8017D5D8 = {
    {
        _shelterB1ElevatorHallInitRoomTask,
        _shelterB1ElevatorHallIdleRoomTask,
        taskKill,
    },
};

void shelterB1ElevatorHallMineTransitTask(Task* task)
{
    enum { MINE_TRANSIT_START_CAP,
           MINE_TRANSIT_WAIT_CAP,
           MINE_TRANSIT_CHECK_CHOICE,
           MINE_TRANSIT_SETTLE_ACTORS,
           MINE_TRANSIT_START_FADE,
           MINE_TRANSIT_WAIT_SOUND,
           MINE_TRANSIT_RELOAD,
           CAP_COMMAND_MINE_PASSAGE           = 5,
           CAP_VARIANT_MINE_PASSAGE_CONFIRMED = 0xA,
           ACTOR_SETTLE_FRAMES                = 3,
           FADE_RAMP_FRAMES                   = 30,
           FADE_TASK_BANK                     = 1,
           FADE_TASK_SLOT                     = 0x31,
           SPRITE_RESOURCE_VARIANT            = 1 };
    s16 remainingFrames;

    switch (task->state) {
        case MINE_TRANSIT_START_CAP:
            capRunCommand(CAP_COMMAND_MINE_PASSAGE, CAP_PLAYBACK_IN_PLACE);
            task->state++;
            break;
        case MINE_TRANSIT_WAIT_CAP:
            if (capIsBusy() != 0) {
                break;
            }
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
            task->state++;
            break;
        case MINE_TRANSIT_CHECK_CHOICE:
            if (capGetVariantKey() != CAP_VARIANT_MINE_PASSAGE_CONFIRMED) {
                taskKill(task);
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
                gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
                break;
            }
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
            task->killCountdown            = ACTOR_SETTLE_FRAMES;
            task->state++;
            break;
        case MINE_TRANSIT_SETTLE_ACTORS:
            // Delay escape accounting by three task ticks after pausing actors.
            remainingFrames     = (u16)task->killCountdown - 1;
            task->killCountdown = remainingFrames;
            if (remainingFrames != 0) {
                break;
            }
            sceneQueueBattleEscapeResult();
            task->state++;
            break;
        case MINE_TRANSIT_START_FADE:
            D_shelter_b1_elevator_hall_801849F0.fade.blend      = SCREEN_FADE_SUBTRACT;
            D_shelter_b1_elevator_hall_801849F0.fade.phase      = SCREEN_FADE_RUNNING;
            D_shelter_b1_elevator_hall_801849F0.fade.rampFrames = FADE_RAMP_FRAMES;
            taskSpawn(FADE_TASK_BANK, FADE_TASK_SLOT, 0, &D_shelter_b1_elevator_hall_801849F0.fade);
            sndEvtRequestScriptStart(SOUND_SHELTER_B1_ELEV_HALL_MINE_TRANSIT, 0, 0);
            task->state++;
            break;
        case MINE_TRANSIT_WAIT_SOUND:
            if (sndScriptHasActiveId(SOUND_SHELTER_B1_ELEV_HALL_MINE_TRANSIT) == 0) {
                task->state++;
            }
            break;
        case MINE_TRANSIT_RELOAD:
            // Commit the saved destination only after the transit sound has ended.
            sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
            gDisplayState.spriteVariant                                = SPRITE_RESOURCE_VARIANT;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area = D_shelter_b1_elevator_hall_801849F8.warp;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp = D_shelter_b1_elevator_hall_801849F8.field_4;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = ((u8*)&D_shelter_b1_elevator_hall_801849F8.areaId)[1];
            taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_SKIP_BATTLE_ESCAPE, 0);
            taskKill(task);
            break;
    }
}

s32 shelterB1ElevatorHallRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 secondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

s32 shelterB1ElevatorHallIgnoreCommandMessage(Task* task, s32 messageId, s32 commandId, s32 commandMode)
{
    return 0;
}

s32 shelterB1ElevatorHallIgnoreActionMessage(Task* task, s32 messageId, const DirectionActionRequest* actionRequest, s32 secondArg)
{
    return 0;
}

s32 shelterB1ElevatorHallPlaySoundCueMessage(Task* task, s32 messageId, s32 cueId, s32 secondArg)
{
    enum { SOUND_CUE_CONFIRM       = 6,
           SOUND_CUE_ELEVATOR_RIDE = 8 };

    switch (cueId) {
        case SOUND_CUE_CONFIRM:
            sndEvtRequestScriptStart(SOUND_SYSTEM_CONFIRM, 0, 0);
            break;
        case SOUND_CUE_ELEVATOR_RIDE:
            sndEvtRequestScriptStart(SOUND_SHELTER_B1_ELEVATOR_RIDE, 0, 0);
            break;
    }
    return 0;
}

/// Registers the hall's room-message receiver and records its first visit.
///
/// State 0 installs the borrowed message table and publishes the live task in
/// `GAME_TASK_SLOT_ROOM`. First entry sets the visited nibble and objective
/// byte; later entries preserve the objective. Advances to the idle state.
static void _shelterB1ElevatorHallInitRoomTask(Task* task)
{
    enum { ROOM_UNVISITED        = 0,
           ROOM_VISITED          = 1,
           FIRST_VISIT_OBJECTIVE = 0x1D };

    task->msgTable = D_shelter_b1_elevator_hall_80182CB8;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    if (gameFlagGetNibble(GAME_FLAG_SHELTER_B1_ELEVATOR_HALL_VISITED) == ROOM_UNVISITED) {
        gameFlagSetNibble(GAME_FLAG_SHELTER_B1_ELEVATOR_HALL_VISITED, ROOM_VISITED);
        gameFlagSetPackedByte(GAME_FLAG_CURRENT_OBJECTIVE, FIRST_VISIT_OBJECTIVE);
    }
    task->state++;
}

/// Keeps the initialized room task available for messages without frame work.
///
/// State 1 leaves the task and its state unchanged until external teardown.
static void _shelterB1ElevatorHallIdleRoomTask(Task* task)
{
}

void shelterB1ElevatorHallRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = D_shelter_b1_elevator_hall_8017D5D8;
    stateHandlers.funcs[task->state](task);
}
