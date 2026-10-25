#include "stages/akropolis/rooms/patio.h"

#include "config.h"
#include "../../event_flags.h"
#include "../../room_states.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "gameplay/actor_presentation.h"
#include "gameplay/actor_render.h"
#include "gameplay/captions.h"
#include "gameplay/companion_load.h"
#include "gameplay/direction.h"
#include "gameplay/display.h"
#include "gameplay/effects.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/gameflag.h"
#include "gameplay/items.h"
#include "gameplay/message.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/event_flags.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/random.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"

#include "rooms/room_common.h"
#include "../../../../shared/effects/glow_draw.h"

#include "assets/rooms/akropolis/patio/declarations.inc"

/// The opening's head-aim task, whose state the event scripts set.
static Task* _gAkropolisPatioPlayerHeadAimTask;
/// The cafeteria door's arrival warp and room, kept until its choice is accepted.
static u8 _gAkropolisPatioCafeteriaArrivalWarp;
static u8 _gAkropolisPatioCafeteriaArrivalRoom;

static void _akropolisPatioInitializeRoomTask(Task* task);
static void _akropolisPatioIdleRoomTask(Task* task);

/// Room task states: initialize, idle, release.
static const TaskFuncTable3 _gAkropolisPatioRoomTaskStates = {
    { _akropolisPatioInitializeRoomTask, _akropolisPatioIdleRoomTask, taskKill },
};

static void _akropolisPatioSetBattleActive(s32 battleActive);

static void _akropolisPatioSpawnPlayerTurnLeft(void);
static void _akropolisPatioPlayerTurnLeftTask(Task* task);

static void _akropolisPatioSetPlayerHeadAimState(s32 headAimState);
static void _akropolisPatioEnterPostSwatState(void);
static void _akropolisPatioSetActorControl(u8 actorControl);

static s32  _akropolisPatioResolveRoomTransition(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32  _akropolisPatioHandleRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 command, s32 unusedSecondArg);
static s32  _akropolisPatioRejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedSecondArg);
static s32  _akropolisPatioHandleSoundCue(Task* unusedTask, s32 unusedMessageId, s32 cueIndex, s32 unusedSecondArg);
static void _akropolisPatioCafeteriaDoorSceneTask(Task* task);
static s32  _akropolisPatioHandleRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg);
static void _akropolisPatioRecordSceneChoiceTask(Task* task);
static void _akropolisPatioPlayerHeadAimTask(Task* task);

#include "assets/rooms/akropolis/patio/clips_1.inc"

/// Messages of the room task.
static TaskMessageEntry _gAkropolisPatioRoomMessages[] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _akropolisPatioResolveRoomTransition },
    { ROOM_MESSAGE_COMMAND, _akropolisPatioHandleRoomCommand },
    { ROOM_MESSAGE_USE_KEY_ITEM, _akropolisPatioRejectKeyItemUse },
    { ROOM_MESSAGE_SOUND, _akropolisPatioHandleSoundCue },
    { DIRECTION_MESSAGE_ROOM_ACTION, _akropolisPatioHandleRoomAction },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Scene tasks: the CAP choice (0), the cafeteria door (1) and the opening's head aim (2).
static TaskDesc _gAkropolisPatioSceneTaskDescs[] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _akropolisPatioRecordSceneChoiceTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _akropolisPatioCafeteriaDoorSceneTask, { .value = 0 } },
    { { { TASK_BODY_NONE, AKROPOLIS_PATIO_HEAD_AIM_TASK_PRIORITY } }, _akropolisPatioPlayerHeadAimTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

#define ROOM_PART _gAkropolisPatioFollowUpPlayerStartPlacement
#include "assets/rooms/akropolis/patio/events.inc"
#include "assets/rooms/akropolis/patio/follow_up_player_bank.inc"

#define ROOM_PART _gAkropolisPatioFollowUpPlayerAnimation47Request
#include "assets/rooms/akropolis/patio/events.inc"
#include "assets/rooms/akropolis/patio/clips_2.inc"
/// The player's animations in the follow-up and the ambush, copied into the equipped weapon's bank from slot 47.
#include "assets/rooms/akropolis/patio/ambush_player_bank.inc"

#define ROOM_PART _gAkropolisPatioAmbushPlayerBankCopyRequest
#include "assets/rooms/akropolis/patio/events.inc"
/// The player's scripted half turn left.
static TaskDesc _gAkropolisPatioPlayerTurnLeftTaskDesc = { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _akropolisPatioPlayerTurnLeftTask, { .value = 0 } };
#define ROOM_PART _gAkropolisPatioAmbushScript
#include "assets/rooms/akropolis/patio/events.inc"
#include "assets/rooms/akropolis/patio/markers.inc"
#include "assets/rooms/akropolis/patio/view_masks.inc"
#include "assets/rooms/akropolis/patio/header.inc"
#include "assets/rooms/akropolis/patio/geometry.inc"
#include "assets/rooms/akropolis/patio/view_boundaries.inc"
#include "assets/rooms/akropolis/patio/action_triggers.inc"
#include "assets/rooms/akropolis/patio/occluders.inc"
#include "assets/rooms/akropolis/patio/placements.inc"
#include "assets/rooms/akropolis/patio/sprites.inc"
#include "assets/rooms/akropolis/patio/lights.inc"
#include "assets/rooms/akropolis/patio/cameras.inc"
#include "assets/rooms/akropolis/patio/surfaces.inc"

STATIC_ASSERT(ARRAY_SIZE(_gAkropolisPatioLightViewMasks) == ARRAY_SIZE(_gAkropolisPatioLightAnchors), akropolisPatioLightMaskCount);

/// Installs the room's messages and sets its actors up for the story so far: before the cafeteria boss, the SWAT
/// member and the Stranger where the opening left them; the opening's Stranger hidden until the opening has
/// played; the ambush's two Strangers held until the ambush has.
static void _akropolisPatioInitializeRoomTask(Task* task)
{
    ActorCommand actorCommand;
    Task*        placedActor;

    task->msgTable = _gAkropolisPatioRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    if (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_CAFETERIA_ROUTE_PROGRESS) < AKROPOLIS_CAFETERIA_ROUTE_BOSS_DEFEATED) {
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState == AKROPOLIS_ROOM_STATE_PATIO_PRE_SWAT_CUTSCENE) {
            TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(AKROPOLIS_PATIO_SWAT_ACTOR), ACTOR_MESSAGE_PLACE, &_gAkropolisPatioRestoredSwatPlacement, 0);
            TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(AKROPOLIS_PATIO_SWAT_ACTOR), ACTOR_COMMAND_MESSAGE_APPLY, &_gAkropolisPatioSwatSlumpCommand, 0);
            taskMessageDispatch(sceneFindPlacedActor(AKROPOLIS_PATIO_SWAT_ACTOR), ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_SHOW, 0);
            _gAkropolisPatioPlayerHeadAimTask = taskSpawnFromTable(_gAkropolisPatioSceneTaskDescs, AKROPOLIS_PATIO_HEAD_AIM_TASK_ENTRY, 0, 0);
        }
        placedActor = sceneFindPlacedActor(AKROPOLIS_PATIO_STRANGER_ACTOR);
        if (placedActor != 0) {
            TASK_MESSAGE_DISPATCH_POINTER(placedActor, ACTOR_MESSAGE_PLACE, &_gAkropolisPatioRestoredStrangerPlacement, 0);
        }
    }
    // The progress is read twice, as the original does.
    if ((gGameSession->location.variant == AKROPOLIS_PATIO_OPENING_VARIANT) && (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_PATIO_CUTSCENE_PROGRESS) <= AKROPOLIS_PATIO_CUTSCENE_START) && (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_PATIO_CUTSCENE_PROGRESS) <= AKROPOLIS_PATIO_CUTSCENE_START)) {
        placedActor = sceneFindPlacedActor(AKROPOLIS_PATIO_STRANGER_ACTOR);
        if (placedActor != 0) {
            TASK_MESSAGE_DISPATCH_POINTER(placedActor, ACTOR_COMMAND_MESSAGE_APPLY, &_gAkropolisPatioHideStrangerCommand, 0);
        }
    }
    if ((gGameSession->location.variant == AKROPOLIS_PATIO_AMBUSH_VARIANT) && (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_PATIO_AMBUSH_SCENE_SEEN) == 0)) {
        actorCommand.context.loc.stage = GAME_STAGE_AKROPOLIS;
        actorCommand.context.loc.area  = AKROPOLIS_ROOM_ID_PATIO;
        actorCommand.command           = 0;
        TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(AKROPOLIS_PATIO_AMBUSH_ACTOR_1), ACTOR_COMMAND_MESSAGE_APPLY, &actorCommand, 0);
        TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(AKROPOLIS_PATIO_AMBUSH_ACTOR_2), ACTOR_COMMAND_MESSAGE_APPLY, &actorCommand, 0);
    }
    task->state = task->state + 1;
}

/// Starts the cafeteria door's choice, and keeps the arrival for when it is accepted.
static inline void _akropolisPatioStartCafeteriaDoorSequence(const RoomEventMsg* request)
{
    taskSpawnFromTable(_gAkropolisPatioSceneTaskDescs, AKROPOLIS_PATIO_CAFETERIA_DOOR_TASK_ENTRY, 0, 0);
    itemSetIdentified(INVENTORY_COLLECTION_ID_PARTHENON_KEY, 1);
    _gAkropolisPatioCafeteriaArrivalWarp = request->warp;
    _gAkropolisPatioCafeteriaArrivalRoom = request->roomState;
}

/// The doors: the cafeteria's is locked until the east elevator hall scene, then plays Rupert's scene once after
/// the boss, then opens through a choice; the fountain's is locked until it is unlocked on the cafeteria route.
static s32 _akropolisPatioResolveRoomTransition(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    s32 transitionResult;
    u16 destinationArea;

    *reply = *request;
    if (request->areaId == AKROPOLIS_ROOM_ID_FOUNTAIN) {
        if ((gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_SECURITY_ROOM_PANEL_KEYS) & AKROPOLIS_SECURITY_ROOM_PANEL_KEY_RED_INSERTED) && (request->queryOnly == ROOM_EVENT_EXECUTE)) {
            reply->roomState = AKROPOLIS_PATIO_FOUNTAIN_AFTER_RED_KEY_ROOM;
        }
    }
    if (request->areaId == AKROPOLIS_ROOM_ID_CAFETERIA) {
        if (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_PATIO_CAFETERIA_DOOR_STATE) < AKROPOLIS_PATIO_CAFETERIA_DOOR_UNLOCKED) {
            transitionResult = ROOM_EVENT_STAY;
            if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                capRunCommandWithTransition(AKROPOLIS_PATIO_CAFETERIA_DOOR_CAP_COMMAND);
                gameFlagSetNibble(EVENT_FLAG_AKROPOLIS_PATIO_CAFETERIA_DOOR_STATE, AKROPOLIS_PATIO_CAFETERIA_DOOR_LOCKED);
                gameFlagSetNibbleIfPresent(request->flagId, EVENT_FLAG_MAP_DOOR_LOCKED);
                return ROOM_EVENT_STAY;
            }
            return transitionResult;
        }
        if (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_CAFETERIA_ROUTE_PROGRESS) == AKROPOLIS_CAFETERIA_ROUTE_BOSS_DEFEATED) {
            transitionResult = ROOM_EVENT_ROOM_ACTION;
            if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                if (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_PATIO_CAFETERIA_DOOR_SCENE_SEEN) == 0) {
                    evsStartScriptWithSkip(_gAkropolisPatioCafeteriaDoorScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gAkropolisPatioCafeteriaDoorSkipScript);
                    gameFlagSetNibble(EVENT_FLAG_AKROPOLIS_PATIO_CAFETERIA_DOOR_SCENE_SEEN, 1);
                    return ROOM_EVENT_ROOM_ACTION;
                }
                capRunCommandWithTransition(AKROPOLIS_PATIO_CAFETERIA_DOOR_AFTER_CAP_COMMAND);
                return ROOM_EVENT_ROOM_ACTION;
            }
            return transitionResult;
        }
        if (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_PATIO_CAFETERIA_DOOR_STATE) == AKROPOLIS_PATIO_CAFETERIA_DOOR_UNLOCKED) {
            transitionResult = ROOM_EVENT_ROOM_ACTION;
            if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                // Save the requested arrival for the deferred cafeteria reload.
                _akropolisPatioStartCafeteriaDoorSequence(request);
                return ROOM_EVENT_ROOM_ACTION;
            }
            return transitionResult;
        }
    }
    if ((request->areaId == AKROPOLIS_ROOM_ID_FOUNTAIN) && (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_CAFETERIA_ROUTE_PROGRESS) < AKROPOLIS_CAFETERIA_ROUTE_FOUNTAIN_UNLOCKED)) {
        transitionResult = ROOM_EVENT_STAY;
        if (request->queryOnly == ROOM_EVENT_EXECUTE) {
            gameFlagSetNibbleIfPresent(request->flagId, EVENT_FLAG_MAP_DOOR_LOCKED);
            capRunCommandWithTransition(AKROPOLIS_PATIO_FOUNTAIN_BLOCKED_CAP_COMMAND);
            return ROOM_EVENT_STAY;
        }
        return transitionResult;
    }
    if ((request->queryOnly == ROOM_EVENT_EXECUTE) && (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_PATIO_CUTSCENE_PROGRESS) == AKROPOLIS_PATIO_CUTSCENE_PLAYING)) {
        gameFlagSetNibble(EVENT_FLAG_AKROPOLIS_PATIO_CUTSCENE_PROGRESS, AKROPOLIS_PATIO_CUTSCENE_DONE);
    }
    destinationArea  = request->areaId;
    transitionResult = ROOM_EVENT_DEPART;
    if (destinationArea == AKROPOLIS_ROOM_ID_CAFETERIA) {
        transitionResult = ROOM_EVENT_DEPART;
        if (request->queryOnly == ROOM_EVENT_EXECUTE) {
            if (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_CAFETERIA_ROUTE_PROGRESS) >= AKROPOLIS_CAFETERIA_ROUTE_MIST_CONTACTED) {
                reply->roomState = destinationArea;
            }
            transitionResult = ROOM_EVENT_DEPART;
            if (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_CAFETERIA_ROUTE_PROGRESS) == AKROPOLIS_CAFETERIA_ROUTE_BOSS_DEFEATED) {
                reply->roomState = AKROPOLIS_PATIO_CAFETERIA_AFTER_BOSS_ROOM;
                transitionResult = ROOM_EVENT_DEPART;
            }
        }
    }
    return transitionResult;
}

/// Leaves for the cafeteria through the opened door: the kept arrival, then a reload of the area.
static inline void _akropolisPatioCommitCafeteriaDeparture(Task* task)
{
    enum { CAFETERIA_SPRITE_VARIANT = 1 };

    sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.area      = AKROPOLIS_ROOM_ID_CAFETERIA;
    gDisplayState.spriteVariant                                 = CAFETERIA_SPRITE_VARIANT;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp      = _gAkropolisPatioCafeteriaArrivalWarp;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = _gAkropolisPatioCafeteriaArrivalRoom;
    taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
    taskKill(task);
}

/// The cafeteria door's choice: choice 2 (the state the choice is resolved in) opens it, plays the door and leaves
/// for the cafeteria; any other gives the player back control.
static void _akropolisPatioCafeteriaDoorSceneTask(Task* task)
{
    enum {
        START          = 0,
        WAIT_CHOICE    = 1,
        RESOLVE_CHOICE = 2,
        WAIT_SOUND     = 3,
    };

    s32 state;

    state = task->state;
    switch (state) {
        case START:
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            capRunCommandWithTransition(AKROPOLIS_PATIO_CAFETERIA_DOOR_CAP_COMMAND);
            task->state = task->state + 1;
            return;
        case WAIT_CHOICE:
            task->state = RESOLVE_CHOICE;
            return;
        case RESOLVE_CHOICE:
            if (capGetVariantKey() == state) {
                gameFlagSetNibble(EVENT_FLAG_AKROPOLIS_PATIO_CAFETERIA_DOOR_STATE, AKROPOLIS_PATIO_CAFETERIA_DOOR_OPENED);
                sndEvtRequestScriptStart(AKROPOLIS_PATIO_SOUND(AKROPOLIS_PATIO_SOUND_CAFETERIA_DOOR_DEPARTURE), 0, 0);
                task->state = task->state + 1;
                return;
            }
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
            taskKill(task);
            return;
        case WAIT_SOUND:
            if (sndScriptHasActiveId(AKROPOLIS_PATIO_SOUND(AKROPOLIS_PATIO_SOUND_CAFETERIA_DOOR_DEPARTURE)) != 0) {
                return;
            }
            _akropolisPatioCommitCafeteriaDeparture(task);
            return;
    }
}

/// Starts the patio's scenes from their triggers: the opening, its follow-up once the Stranger has left, and the
/// ambush on the way back from the cafeteria. The original sets no result; the sender ignores it.
static s32 _akropolisPatioHandleRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    u8 actionId;

    if ((request->actionId == AKROPOLIS_PATIO_ACTION_OPENING) && (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_PATIO_CUTSCENE_PROGRESS) <= AKROPOLIS_PATIO_CUTSCENE_START)) {
        gameFlagSetNibble(EVENT_FLAG_AKROPOLIS_PATIO_CUTSCENE_PROGRESS, AKROPOLIS_PATIO_CUTSCENE_PLAYING);
        evsStartScriptWithSkip(_gAkropolisPatioOpeningScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gAkropolisPatioOpeningSkipScript);
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = AKROPOLIS_PATIO_OPENING_SCENE_EVENT;
        gGameSession->flowFlags                             = (GAME_SESSION_FLOW_SKIP_ENDING_MUSIC | GAME_SESSION_FLOW_HIDE_REEQUIPPED_WEAPON | GAME_SESSION_FLOW_REEQUIP_WEAPON);
    }
    if ((request->actionId == AKROPOLIS_PATIO_ACTION_FOLLOW_UP) && (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_PATIO_CUTSCENE_PROGRESS) == AKROPOLIS_PATIO_CUTSCENE_PLAYING) &&
        (taskMessageDispatch(sceneFindPlacedActor(AKROPOLIS_PATIO_STRANGER_ACTOR), ACTOR_MESSAGE_IS_PRESENT, 0, 0) == 0)) {
        gameFlagSetNibble(EVENT_FLAG_AKROPOLIS_PATIO_CUTSCENE_PROGRESS, AKROPOLIS_PATIO_CUTSCENE_DONE);
        evsStartScriptWithSkip(_gAkropolisPatioFollowUpScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gAkropolisPatioFollowUpSkipScript);
    }
    // The ambush comes after the cafeteria boss; the original compares the progress with the action id, which is the
    // same value (AKROPOLIS_CAFETERIA_ROUTE_BOSS_DEFEATED).
    actionId = request->actionId;
    if ((actionId == AKROPOLIS_PATIO_ACTION_AMBUSH) && (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_PATIO_AMBUSH_SCENE_SEEN) == 0) && (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_CAFETERIA_ROUTE_PROGRESS) == actionId)) {
        gameFlagSetNibble(EVENT_FLAG_AKROPOLIS_PATIO_AMBUSH_SCENE_SEEN, 1);
        evsStartScriptWithSkip(_gAkropolisPatioAmbushScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gAkropolisPatioAmbushSkipScript);
    }
    // No result is set; the sender of a room action ignores it.
}

/// Room commands 1, the gyro, and 2, the civilian in the cafeteria, start their CAP events; 2 only before the cafeteria
/// boss.
static s32 _akropolisPatioHandleRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 command, s32 unusedSecondArg)
{
    s32 reply;

    if (command == AKROPOLIS_PATIO_ROOM_COMMAND_GYRO) {
        capSpawnEventIfIdle(AKROPOLIS_PATIO_ROOM_COMMAND_GYRO, CAP_EVENT_NO_FLAGS);
    }
    reply = 2;
    if (command == AKROPOLIS_PATIO_ROOM_COMMAND_CIVILIAN) {
        reply = gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_CAFETERIA_ROUTE_PROGRESS) < AKROPOLIS_CAFETERIA_ROUTE_BOSS_DEFEATED;
        if (reply != 0) {
            capSpawnEventIfIdle(AKROPOLIS_PATIO_ROOM_COMMAND_CIVILIAN, CAP_EVENT_NO_FLAGS);
            reply = 0;
        }
    }
    return reply;
}

/// No key item can be used here.
static s32 _akropolisPatioRejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Plays room sound cue 3, the cafeteria door's unlock.
static s32 _akropolisPatioHandleSoundCue(Task* unusedTask, s32 unusedMessageId, s32 cueIndex, s32 unusedSecondArg)
{
    if (cueIndex == AKROPOLIS_PATIO_SOUND_CAFETERIA_DOOR_UNLOCK) {
        sndEvtRequestScriptStart(AKROPOLIS_PATIO_SOUND(AKROPOLIS_PATIO_SOUND_CAFETERIA_DOOR_UNLOCK), 0, 0);
    }
    return 0;
}

/// Plays CAP 6; its choice 1 makes the Akropolis map available. Never spawned, and the patio's file has no CAP 6.
static void _akropolisPatioRecordSceneChoiceTask(Task* task)
{
    enum {
        START         = 0,
        WAIT_CHOICE   = 1,
        RECORD_CHOICE = 2,
    };

    s32 state;

    state = task->state;
    switch (state) {
        case START:
            capRunCommandWithTransition(AKROPOLIS_PATIO_SCENE_CHOICE_CAP_COMMAND);
            task->state = task->state + 1;
            return;
        case WAIT_CHOICE:
            task->state = RECORD_CHOICE;
            return;
        case RECORD_CHOICE:
            if (capGetVariantKey() == 1) {
                gameFlagSetNibble(EVENT_FLAG_MAP_AKROPOLIS_AVAILABLE, 1);
            }
            taskKill(task);
            return;
    }
}

/// Turns the player's head to a point in the opening, holding it there (state 2) or sliding it along Z (3, then 4).
static void _akropolisPatioPlayerHeadAimTask(Task* task)
{
    enum {
        INIT        = 0,
        IDLE        = 1,
        HOLD        = 2,
        START_SLIDE = 3,
        SLIDE       = 4,
        MAX_YAW     = ACTOR_TRANSFORM_ANGLE_TURN / 8,
        MAX_PITCH   = ACTOR_TRANSFORM_ANGLE_TURN / 16,
    };

    GfxCoord focus;        // World point carried in the translation; the other fields are unused
    byte     unused[0x28]; // Frame space no instruction touches; what the original declared here is unproven
    Task*    playerTask;
    s32      slideDistance;

    playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);

    // The head turn reads nothing of its target frame but the translation, a
    // world point, so the rest of `focus` stays uninitialized.
    focus.coord.t[0] = AKROPOLIS_PATIO_HEAD_AIM_X;
    focus.coord.t[1] = 0;
    focus.coord.t[2] = AKROPOLIS_PATIO_HEAD_AIM_Z;

    switch (task->state) {
        case INIT:
            task->spawnArg1.value = 0;
            task->state           = task->state + 1;
            return;
        case IDLE:
            return;
        case HOLD:
            animationAimHeadAtPoint(playerTask, &focus, MAX_YAW, MAX_PITCH, ONE);
            return;
        case START_SLIDE:
            task->spawnArg1.value = 0;
            animationAimHeadAtPoint(playerTask, &focus, MAX_YAW, MAX_PITCH, ONE);
            task->state = task->state + 1;
            return;
        case SLIDE:
            slideDistance         = task->spawnArg1.value + AKROPOLIS_PATIO_HEAD_AIM_SLIDE_STEP;
            task->spawnArg1.value = slideDistance;
            if (slideDistance >= AKROPOLIS_PATIO_HEAD_AIM_SLIDE_DISTANCE + 1) {
                task->spawnArg1.value = AKROPOLIS_PATIO_HEAD_AIM_SLIDE_DISTANCE;
            }
            focus.coord.t[2] -= task->spawnArg1.value;
            animationAimHeadAtPoint(playerTask, &focus, MAX_YAW, MAX_PITCH, ONE);
            return;
    }
}

/// Sets the head-aim task's state, for the event scripts.
static void _akropolisPatioSetPlayerHeadAimState(s32 headAimState)
{
    _gAkropolisPatioPlayerHeadAimTask->state = headAimState;
}

/// Moves the patio to its state after the SWAT scene.
static void _akropolisPatioEnterPostSwatState(void)
{
    gGameSession->location.roomState = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = AKROPOLIS_ROOM_STATE_PATIO_POST_SWAT_CUTSCENE;
    gGameSession->roomObjsDirty                                                                    = true;
}

/// Sets whether the scene's actors run, for the event scripts.
static void _akropolisPatioSetActorControl(u8 actorControl)
{
    gSceneCombatState.actorControl = actorControl;
}

/// The room task's idle state.
static void _akropolisPatioIdleRoomTask(Task* unusedTask)
{
    // Removing this unused reservation changes the original call frame.
    byte retainedStackSpace[0x10];
}

void akropolisPatioRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers = _gAkropolisPatioRoomTaskStates;
    stateHandlers.funcs[task->state](task);
}

/// Pauses the patio's actors, or resumes them and starts the battle, for the event scripts.
static void _akropolisPatioSetBattleActive(s32 battleActive)
{
    if (battleActive != 0) {
        gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
        sceneEngageBattle(1);
        return;
    }
    gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
}

/// Starts the player's half turn left, for the event scripts.
static void _akropolisPatioSpawnPlayerTurnLeft(void)
{
    taskSpawnFromTable(&_gAkropolisPatioPlayerTurnLeftTaskDesc, 0, 0, 0);
}

/// Turns the player left by half a turn, in equal steps.
static void _akropolisPatioPlayerTurnLeftTask(Task* task)
{
    enum {
        INIT     = 0,
        STEP     = 1,
        YAW_STEP = ACTOR_TRANSFORM_ANGLE_HALF_TURN / AKROPOLIS_PATIO_PLAYER_TURN_TICKS
    };

    GameActor* playerActor;
    s16        yaw;

    playerActor = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->work;

    switch (task->state) {
        case INIT:
            task->killCountdown = AKROPOLIS_PATIO_PLAYER_TURN_TICKS;
            task->state++;
            /* fallthrough */
        case STEP:
            yaw = playerActor->rotation.vy - YAW_STEP;
            if (yaw < -ACTOR_TRANSFORM_ANGLE_HALF_TURN) {
                yaw = playerActor->rotation.vy + (ACTOR_TRANSFORM_ANGLE_TURN - YAW_STEP);
            }
            playerActor->rotation.vy = yaw;
            task->killCountdown--;
            if (task->killCountdown > 0) {
                break;
            }
            /* fallthrough */
        default:
            taskKill(task);
            break;
    }
}

void akropolisPatioSpawnLightEffectsTask(Task* task)
{
    enum { LIGHT_EMITTER_INIT     = 0,
           LARGE_LIGHT_END        = 3,
           CEILING_LIGHT_END      = 7,
           MOTES_PER_LARGE_LIGHT  = 3,
           LARGE_LIGHT_ARGUMENT   = (768 << AKROPOLIS_PATIO_LIGHT_GLOW_SIZE_SHIFT) | (2 << AKROPOLIS_PATIO_LIGHT_GLOW_CELL_SHIFT),
           CEILING_LIGHT_ARGUMENT = 512 << AKROPOLIS_PATIO_LIGHT_GLOW_SIZE_SHIFT,
           WALL_LIGHT_ARGUMENT    = 1 << AKROPOLIS_PATIO_LIGHT_GLOW_CELL_SHIFT };
    GfxCoord*   parentCoord;
    EffectWork* scratchWork;
    s32         anchorIndex;
    s32         moteIndex;

    scratchWork = task->spawnArg2.pointer;
    parentCoord = task->extra.coordBody->coord;

    if (task->state == LIGHT_EMITTER_INIT) {
        // Seed all fixed light glows before allocating their moving points.
        for (anchorIndex = 0; anchorIndex < LARGE_LIGHT_END; anchorIndex++) {
            effectSpawn(EFFECT_AKROPOLIS_PATIO_LIGHT_GLOW, parentCoord, anchorIndex + LARGE_LIGHT_ARGUMENT, &_gAkropolisPatioLightAnchors[anchorIndex]);
        }
        for (anchorIndex = LARGE_LIGHT_END; anchorIndex < CEILING_LIGHT_END; anchorIndex++) {
            effectSpawn(EFFECT_AKROPOLIS_PATIO_LIGHT_GLOW, parentCoord, anchorIndex + CEILING_LIGHT_ARGUMENT, &_gAkropolisPatioLightAnchors[anchorIndex]);
        }
        for (anchorIndex = CEILING_LIGHT_END; anchorIndex < ARRAY_SIZE(_gAkropolisPatioLightAnchors); anchorIndex++) {
            effectSpawn(EFFECT_AKROPOLIS_PATIO_LIGHT_GLOW, parentCoord, anchorIndex + WALL_LIGHT_ARGUMENT, &_gAkropolisPatioLightAnchors[anchorIndex]);
        }
        task->state++;
        // Each child snapshots XYZ; these point tasks never read the retained offset pointer.
        for (anchorIndex = 0; anchorIndex < LARGE_LIGHT_END; anchorIndex++) {
            for (moteIndex = 0; moteIndex < MOTES_PER_LARGE_LIGHT; moteIndex++) {
                // A random point within 1024 units of the light.
                {
                    enum { MOTE_JITTER_CENTER = 1024,
                           MOTE_JITTER_MASK   = 2047 };
                    RANDOM_LCG_ADVANCE();
                    scratchWork->move.vx = MOTE_JITTER_CENTER - (RANDOM_LCG_DRAW() & MOTE_JITTER_MASK);
                    RANDOM_LCG_ADVANCE();
                    scratchWork->move.vy = MOTE_JITTER_CENTER - (RANDOM_LCG_DRAW() & MOTE_JITTER_MASK);
                    RANDOM_LCG_ADVANCE();
                    scratchWork->move.vz  = MOTE_JITTER_CENTER - (RANDOM_LCG_DRAW() & MOTE_JITTER_MASK);
                    scratchWork->move.vx += _gAkropolisPatioLightAnchors[anchorIndex].vx;
                    scratchWork->move.vy += _gAkropolisPatioLightAnchors[anchorIndex].vy;
                    scratchWork->move.vz += _gAkropolisPatioLightAnchors[anchorIndex].vz;
                }
                effectSpawn(EFFECT_AKROPOLIS_PATIO_LIGHT_MOTE, parentCoord, anchorIndex, &scratchWork->move);
            }
        }
    }
}

/// Sets a glow's quad to the square around its projected center.
static inline void _akropolisPatioSetLightGlowBounds(POLY_FT4* glowQuad, const RoomGlowSpriteScratch* projection)
{
    s16 left;
    s16 right;
    s16 top;
    s16 bottom;

    left         = projection->screenPos.vx - projection->halfExtent;
    glowQuad->x2 = left;
    glowQuad->x0 = left;
    right        = projection->screenPos.vx + projection->halfExtent;
    glowQuad->x3 = right;
    glowQuad->x1 = right;
    top          = projection->screenPos.vy - projection->halfExtent;
    glowQuad->y1 = top;
    glowQuad->y0 = top;
    bottom       = projection->screenPos.vy + projection->halfExtent;
    glowQuad->y3 = bottom;
    glowQuad->y2 = bottom;
}

/// Projects a light's position and draws its flickering glow there.
static inline void _akropolisPatioDrawLightGlow(const GfxCoord* composedCoord, const EffectWork* glowWork, RoomGlowSpriteScratch* spriteScratch)
{
    enum {
        CLUT_STRIDE       = 16,
        FRAME_PARITY_MASK = 1,
    };

    POLY_FT4* glowQuad;
    u8        greyLevel;

    // Project the narrowed cached view translation into the sprite's center.
    spriteScratch->worldPos.vx = composedCoord->workm.t[0];
    spriteScratch->worldPos.vy = composedCoord->workm.t[1];
    spriteScratch->worldPos.vz = composedCoord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&spriteScratch->worldPos);
    gte_rtps();
    // Consume a packet even when the depth test will reject the projection.
    glowQuad       = gGpuPrimCursor;
    gGpuPrimCursor = glowQuad + 1;
    setPolyFT4(glowQuad);
    gte_stsxy(&spriteScratch->screenPos);
    gte_stszotz(&spriteScratch->otz);
    if (spriteScratch->otz >= GLOW_MIN_DEPTH) {
        greyLevel       = glowWork->period + (((u8)gDisplayState.animFrame & FRAME_PARITY_MASK) << AKROPOLIS_PATIO_LIGHT_GLOW_FLICKER_SHIFT);
        glowQuad->tpage = GLOW_FLARE_TEXTURE_PAGE;
        setRGB0(glowQuad, greyLevel, greyLevel, greyLevel);
        setSemiTrans(glowQuad, true);
        setClut(glowQuad, glowWork->angle * CLUT_STRIDE, AKROPOLIS_PATIO_LIGHT_GLOW_CLUT_Y);
        // Texture endpoints include the final texel of the fixed 40-by-40 cell.
        setUVWH(glowQuad, glowWork->angle * AKROPOLIS_PATIO_LIGHT_GLOW_CELL_SIZE, 0,
                AKROPOLIS_PATIO_LIGHT_GLOW_CELL_SIZE - 1, AKROPOLIS_PATIO_LIGHT_GLOW_CELL_SIZE - 1);

        spriteScratch->halfExtent = (glowWork->scale * (AKROPOLIS_PATIO_LIGHT_GLOW_CELL_SIZE - 1)) / spriteScratch->otz;
        _akropolisPatioSetLightGlowBounds(glowQuad, spriteScratch);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(spriteScratch->otz),
                glowQuad);
    }
}

void akropolisPatioLightGlowTask(Task* task)
{
    enum { LIGHT_GLOW_INIT = 0 };

    RoomGlowSpriteScratch* block;
    EffectWork*            work;
    GfxCoord*              coord;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN &&
        ((_gAkropolisPatioLightViewMasks[task->spawnArg1.value & AKROPOLIS_PATIO_LIGHT_GLOW_ANCHOR_MASK] >> (gGameSession->location.view - 1)) & 1)) {
        actorRenderComposeCoord(coord);
        block = SCRATCH_STACK_RESERVE_BLOCK(RoomGlowSpriteScratch);
        if (task->state == LIGHT_GLOW_INIT) {
            // Decode the fixed sheet cell and size only on the first visible tick.
            u8 levels[3] = { 0x50, 0x30, 0x40 };

            if (task->spawnArg1.value & (AKROPOLIS_PATIO_LIGHT_GLOW_SIZE_MASK << AKROPOLIS_PATIO_LIGHT_GLOW_SIZE_SHIFT)) {
                work->scale = (task->spawnArg1.value >> AKROPOLIS_PATIO_LIGHT_GLOW_SIZE_SHIFT) & AKROPOLIS_PATIO_LIGHT_GLOW_SIZE_MASK;
            } else {
                work->scale = AKROPOLIS_PATIO_LIGHT_GLOW_DEFAULT_SIZE;
            }
            work->angle           = (task->spawnArg1.value >> AKROPOLIS_PATIO_LIGHT_GLOW_CELL_SHIFT) & AKROPOLIS_PATIO_LIGHT_GLOW_CELL_MASK;
            task->spawnArg1.value = task->spawnArg1.value & AKROPOLIS_PATIO_LIGHT_GLOW_ANCHOR_MASK;
            work->period          = levels[work->angle];
            task->state           = task->state + 1;
        }
        _akropolisPatioDrawLightGlow(coord, work, block);
        SCRATCH_STACK_RELEASE_BLOCK(RoomGlowSpriteScratch);
    }
}

/// A fourth byte after the glow's three grey levels, never read; it keeps the next section's alignment.
static const u8 _gAkropolisPatioLightGreyAlignmentByte = 0xF2;

/// Projects a mote's position and draws it as one pixel of random grey.
static inline void _akropolisPatioDrawLightMote(const GfxCoord* cachedCoord, EffectPointTileScratch* tileScratch)
{
    enum {
        LIGHT_MOTE_GREY_LEVELS = 192,
    };

    TILE_1* moteTile;
    u32     greyLevel;

    // Draw the pre-movement view cache; the dirty stamp defers recomposition.
    tileScratch->viewPoint.vx = cachedCoord->workm.t[0];
    tileScratch->viewPoint.vy = cachedCoord->workm.t[1];
    tileScratch->viewPoint.vz = cachedCoord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&tileScratch->viewPoint);
    gte_rtps();
    // Reserve the tile before rejecting depth, without consuming random state.
    moteTile       = gGpuPrimCursor;
    gGpuPrimCursor = moteTile + 1;
    setTile1(moteTile);
    gte_stsxy(&moteTile->x0);
    gte_stszotz(&tileScratch->depth);
    if (tileScratch->depth >= EFFECT_POINT_TILE_MIN_DEPTH) {
        RANDOM_LCG_ADVANCE();
        greyLevel  = RANDOM_LCG_DRAW();
        greyLevel %= LIGHT_MOTE_GREY_LEVELS;
        setRGB0(moteTile, greyLevel, greyLevel, greyLevel);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(tileScratch->depth),
                moteTile);
        // Prepending at the same depth makes the GPU set blending before the tile.
        gpuSetPrimitiveBlendMode(moteTile, GPU_BLEND_AVERAGE, tileScratch->depth);
    }
}

void akropolisPatioLightMoteTask(Task* task)
{
    enum {
        INIT            = 0,
        DRIFT           = 0,
        GATHER          = 1,
        DRIFT_CENTER    = 16,
        DRIFT_MASK      = 31,
        AIM_SAMPLE_MASK = 3,
        GATHER_WEIGHT   = ONE / 128,
        JITTER_MASK     = 15,
        JITTER_CENTER   = 8,
        DRIFT_CHANCE    = 120,
        GATHER_CHANCE   = 60
    };

    EffectWork*             work;
    GfxCoord*               coord;
    EffectPointTileScratch* tileScratch;
    SVECTOR*                velocity;
    SVECTOR*                anchors;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN &&
        ((_gAkropolisPatioLightViewMasks[task->spawnArg1.value] >> (gGameSession->location.view - 1)) & 1)) {
        tileScratch = SCRATCH_STACK_RESERVE_BLOCK(EffectPointTileScratch);
        actorRenderComposeCoord(coord);
        if (task->state == INIT) {
            RANDOM_LCG_ADVANCE();
            work->move.vx = DRIFT_CENTER - (RANDOM_LCG_DRAW() & DRIFT_MASK);
            RANDOM_LCG_ADVANCE();
            work->move.vy = DRIFT_CENTER - (RANDOM_LCG_DRAW() & DRIFT_MASK);
            RANDOM_LCG_ADVANCE();
            work->move.vz = DRIFT_CENTER - (RANDOM_LCG_DRAW() & DRIFT_MASK);
            task->state++;
        }
        // Gather re-aims on a random quarter of ticks, rather than periodically.
        if (work->index != DRIFT) {
            RANDOM_LCG_ADVANCE();
            if ((RANDOM_LCG_DRAW() & AIM_SAMPLE_MASK) == 0) {
                anchors       = _gAkropolisPatioLightAnchors;
                velocity      = &work->move;
                work->move.vx = (u16)anchors[task->spawnArg1.value].vx -
                                (u16)coord->coord.t[0];
                work->move.vy = (u16)anchors[task->spawnArg1.value].vy -
                                (u16)coord->coord.t[1];
                work->move.vz = (u16)anchors[task->spawnArg1.value].vz -
                                (u16)coord->coord.t[2];
                VectorNormalSS(velocity, velocity);
                gte_lddp(GATHER_WEIGHT);
                gte_ldsv(velocity);
                gte_gpf12();
                gte_stsv(velocity);
            }
            RANDOM_LCG_ADVANCE();
            work->move.vx -= (RANDOM_LCG_DRAW() & JITTER_MASK) - JITTER_CENTER;
            RANDOM_LCG_ADVANCE();
            work->move.vy -= (RANDOM_LCG_DRAW() & JITTER_MASK) - JITTER_CENTER;
            RANDOM_LCG_ADVANCE();
            work->move.vz -= (RANDOM_LCG_DRAW() & JITTER_MASK) - JITTER_CENTER;
            RANDOM_LCG_ADVANCE();
            if ((u16)(RANDOM_LCG_DRAW() % DRIFT_CHANCE) == 0) {
                work->index = DRIFT;
            }
        } else {
            RANDOM_LCG_ADVANCE();
            work->move.vx = DRIFT_CENTER - (RANDOM_LCG_DRAW() & DRIFT_MASK);
            RANDOM_LCG_ADVANCE();
            work->move.vy = DRIFT_CENTER - (RANDOM_LCG_DRAW() & DRIFT_MASK);
            RANDOM_LCG_ADVANCE();
            work->move.vz = DRIFT_CENTER - (RANDOM_LCG_DRAW() & DRIFT_MASK);
            RANDOM_LCG_ADVANCE();
            if ((u16)(RANDOM_LCG_DRAW() % GATHER_CHANCE) == 0) {
                work->index = GATHER;
            }
        }
        coord->coord.t[0]  += work->move.vx;
        coord->coord.t[1]  += work->move.vy;
        coord->coord.t[2]  += work->move.vz;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        _akropolisPatioDrawLightMote(coord, tileScratch);
        SCRATCH_STACK_RELEASE_BLOCK(EffectPointTileScratch);
    }
}
