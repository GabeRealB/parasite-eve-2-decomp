#include "rooms/acropolis_patio.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "actors/task_tables.h"

#include "gameplay/companion_load.h"
#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effects.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/player_actor.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/random.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_akropolis.h"

#include "rooms/room.h"

#include "rooms/room_common.h"

#include "assets/rooms/akropolis/patio/declarations.inc"

/// Retained bodyless controller for the opening scene's player head aim.
///
/// Spawned on room-1 initialization while story progress is below 2. Script
/// callbacks require this task to be live and set its phase through this handle.
/// Session rebuild discards the task list and heap; this pointer is not cleared
/// and must not be used again until a new qualifying initialization replaces it.
static Task* _gAcropolisPatioPlayerHeadAimTask;
/// Cafeteria arrival ID saved after the door-choice task is spawned.
///
/// This singleton byte is borrowed until the accepted choice commits it to the
/// live save for reload; a later door request overwrites it.
static u8 _gAcropolisPatioCafeteriaArrivalWarp;

/// Cafeteria room selector saved until the accepted door choice requests reload.
///
/// Stores the request's signed room byte as the unsigned saved-location selector,
/// preserving all eight bits. A later door request overwrites this singleton.
static u8 _gAcropolisPatioCafeteriaArrivalRoom;

/// Story thresholds, opening phases and one-shot latches used by the patio receiver.
enum {
    ACROPOLIS_PATIO_STORY_BATTLE_FINISHED    = 2,
    ACROPOLIS_PATIO_STORY_FOLLOW_UP_STARTED  = 3,
    ACROPOLIS_PATIO_STORY_FOUNTAIN_READY     = 5,
    ACROPOLIS_PATIO_OPENING_SCENE_THRESHOLD  = 2,
    ACROPOLIS_PATIO_OPENING_SCENE_STARTED    = 3,
    ACROPOLIS_PATIO_OPENING_SCENE_FINISHED   = 4,
    ACROPOLIS_PATIO_SCENE_UNSEEN             = 0,
    ACROPOLIS_PATIO_SCENE_SEEN               = 1,
    ACROPOLIS_PATIO_SECOND_SECURITY_LOCK_BIT = 2,
    ACROPOLIS_PATIO_DOOR_UNLOCKED            = 2,
    ACROPOLIS_PATIO_DOOR_ATTEMPTED           = 1
};

static void _acropolisPatioInitializeRoomTask(Task* task);
static void _acropolisPatioRoomIdleState(Task* task);

/// Key-item use message handled by the patio's room task.
enum { ACROPOLIS_PATIO_MESSAGE_USE_KEY_ITEM = 0x13F1 };

/// Packed fountain-jet argument fields and the fixed sprite-sheet geometry.
enum {
    ACROPOLIS_PATIO_JET_ANCHOR_MASK   = 0xF,
    ACROPOLIS_PATIO_JET_CELL_SHIFT    = 8,
    ACROPOLIS_PATIO_JET_CELL_MASK     = 3,
    ACROPOLIS_PATIO_JET_SIZE_SHIFT    = 16,
    ACROPOLIS_PATIO_JET_SIZE_MASK     = 0xFFF,
    ACROPOLIS_PATIO_JET_DEFAULT_SIZE  = 640,
    ACROPOLIS_PATIO_JET_MIN_DEPTH     = 0x11,
    ACROPOLIS_PATIO_JET_TEXTURE_PAGE  = 0x2B,
    ACROPOLIS_PATIO_JET_CLUT_Y        = 0x10E,
    ACROPOLIS_PATIO_JET_CELL_SIZE     = 40,
    ACROPOLIS_PATIO_JET_FLICKER_SHIFT = 4
};

/// Room-task states: initialize (0), idle (1), and release the task (2).
///
/// The dispatcher copies all three callbacks to the stack and indexes without
/// a bounds check. Keep the patio and resident task code loaded through dispatch.
static const TaskFuncTable3 _gAcropolisPatioRoomTaskStates = {
    { _acropolisPatioInitializeRoomTask, _acropolisPatioRoomIdleState, taskKill },
};

static void _acropolisPatioSetBattleActive(s32 battleActive);

static void _acropolisPatioSpawnPlayerTurnLeft(void);
static void _acropolisPatioPlayerTurnLeftTask(Task* task);

static void _acropolisPatioSetPlayerHeadAimState(s32 headAimState);
static void _acropolisPatioActivateRoom2(void);
static void _acropolisPatioSetActorControl(u8 actorControl);

static s32  _acropolisPatioResolveRoomTransition(Task* unusedTask, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32  _acropolisPatioHandleCommandMessage(Task* unusedTask, s32 messageId, s32 commandId, s32 unusedArg);
static s32  _acropolisPatioRefuseKeyItemUse(Task* roomTask, s32 messageId, s32 itemId, s32 unusedArg);
static s32  _acropolisPatioHandleSoundMessage(Task* roomTask, s32 messageId, s32 soundCue, s32 unusedArg);
static void _acropolisPatioCafeteriaDoorSceneTask(Task* task);
static s32  _acropolisPatioHandleActionMessage(Task* unusedTask, s32 messageId, const DirectionActionRequest* actionRequest, s32 unusedArg);
static void _acropolisPatioRecordSceneChoiceTask(Task* task);
static void _acropolisPatioPlayerHeadAimTask(Task* task);

/// States selected by the opening scene's player-head-aim callbacks.
enum {
    ACROPOLIS_PATIO_SCRIPT_HEAD_AIM_IDLE        = 1,
    ACROPOLIS_PATIO_SCRIPT_HEAD_AIM_HOLD        = 2,
    ACROPOLIS_PATIO_SCRIPT_HEAD_AIM_START_SLIDE = 3,
};

#include "assets/rooms/akropolis/patio/clips_1.inc"

/// Room receiver's exit, command, key-item, sound and action message handlers.
///
/// Five live entries precede TASK_MESSAGE_TABLE_END; unsupported IDs return 0.
/// The table is read only and borrowed by the room task for its lifetime.
/// Payload types and return meanings follow the selected handler; room actions
/// leave the result word unset and their sender discards it.
static TaskMessageEntry _gAcropolisPatioRoomMessages[] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _acropolisPatioResolveRoomTransition },
    { ROOM_MESSAGE_COMMAND, _acropolisPatioHandleCommandMessage },
    { ACROPOLIS_PATIO_MESSAGE_USE_KEY_ITEM, _acropolisPatioRefuseKeyItemUse },
    { ROOM_MESSAGE_SOUND, _acropolisPatioHandleSoundMessage },
    { DIRECTION_MESSAGE_ROOM_ACTION, _acropolisPatioHandleActionMessage },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Scheduler priorities, clip selectors and blend durations for patio scene records.
enum {
    ACROPOLIS_PATIO_SCENE_TASK_PRIORITY             = 192,
    ACROPOLIS_PATIO_HEAD_AIM_TASK_PRIORITY          = 97,
    ACROPOLIS_PATIO_FOLLOW_UP_PLAYER_ANIMATION_47   = ANIMATION_BANK_BASE_SET_COUNT,
    ACROPOLIS_PATIO_FOLLOW_UP_PLAYER_ANIMATION_49   = ANIMATION_BANK_BASE_SET_COUNT + 2,
    ACROPOLIS_PATIO_FOLLOW_UP_PLAYER_ANIMATION_50   = ANIMATION_BANK_BASE_SET_COUNT + 3,
    ACROPOLIS_PATIO_FOLLOW_UP_PLAYER_ANIMATION_51   = ANIMATION_BANK_BASE_SET_COUNT + 4,
    ACROPOLIS_PATIO_FOLLOW_UP_PLAYER_ANIMATION_52   = ANIMATION_BANK_BASE_SET_COUNT + 5,
    ACROPOLIS_PATIO_FOLLOW_UP_BLEND_FRAMES          = 5,
    ACROPOLIS_PATIO_SWAT_ANIMATION_1                = 1,
    ACROPOLIS_PATIO_SWAT_ANIMATION_2                = 2,
    ACROPOLIS_PATIO_SWAT_ANIMATION_3                = 3,
    ACROPOLIS_PATIO_SWAT_ANIMATION_4                = 4,
    ACROPOLIS_PATIO_SECOND_VISIT_PLAYER_ANIMATION_9 = 9,
    ACROPOLIS_PATIO_SECOND_VISIT_BLEND_FRAMES       = 6,
};

/// Bodyless scene controllers: CAP-choice recording, cafeteria door, then head aim.
///
/// Indices 0..2 select callbacks; the fourth record terminates the table and must
/// never be spawned. The callback code must stay loaded for each task's lifetime.
static TaskDesc _gAcropolisPatioSceneTaskDescriptors[] = {
    { { { TASK_BODY_NONE, ACROPOLIS_PATIO_SCENE_TASK_PRIORITY } }, _acropolisPatioRecordSceneChoiceTask, { .value = 0 } },
    { { { TASK_BODY_NONE, ACROPOLIS_PATIO_SCENE_TASK_PRIORITY } }, _acropolisPatioCafeteriaDoorSceneTask, { .value = 0 } },
    { { { TASK_BODY_NONE, ACROPOLIS_PATIO_HEAD_AIM_TASK_PRIORITY } }, _acropolisPatioPlayerHeadAimTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

/// Six follow-up player clips copied to equipped-weapon bank slots 47..52.
///
/// The directory is copied as pointer words; sets and tracks stay borrowed from
/// the patio overlay through playback.

#define ROOM_PART _gAcropolisPatioFollowUpPlayerStartPlacement
#include "assets/rooms/akropolis/patio/events.inc"

#include "assets/rooms/akropolis/patio/acropolis_patio_follow_up_player_bank.inc"

/// Equipped-weapon idle clip selected when the patio scripts take player control.
enum { ACROPOLIS_PATIO_PLAYER_IDLE_ANIMATION = 1 };

enum { ACROPOLIS_PATIO_STRANGER_COMMAND_HIDE = 0 };

/// CAP command slots and actor draw modes used by the patio's event streams.
enum {
    ACROPOLIS_PATIO_CAP_CAFETERIA_DOOR           = 8,
    ACROPOLIS_PATIO_CAP_FOLLOW_UP                = 9,
    ACROPOLIS_PATIO_CAP_OPENING                  = 10,
    ACROPOLIS_PATIO_CAP_SECOND_VISIT             = 12,
    ACROPOLIS_PATIO_ACTOR_DRAW_HIDE              = 0,
    ACROPOLIS_PATIO_ACTOR_DRAW_SHOW              = 1,
    ACROPOLIS_PATIO_LEAP_ACTOR_DRAW_HIDE_RELEASE = 2,
};

#define ROOM_PART _gAcropolisPatioFollowUpPlayerAnimation47Request
#include "assets/rooms/akropolis/patio/events.inc"
#include "assets/rooms/akropolis/patio/clips_2.inc"

/// Second-visit player bank extension: NULL slot 47, then four clips in slots 48..51.
///
/// The directory is copied as pointer words; sets and tracks stay borrowed from
/// the patio overlay through playback. The NULL entry is part of the copy.
#include "assets/rooms/akropolis/patio/acropolis_patio_second_visit_player_bank.inc"

/// Clip selectors, transition duration and commands for the two scene-only Strangers.
enum {
    ACROPOLIS_PATIO_LEAP_ACTOR_1_SCENE_CLIP            = 5,
    ACROPOLIS_PATIO_LEAP_ACTOR_2_SCENE_CLIP            = 6,
    ACROPOLIS_PATIO_LEAP_START_CLIP                    = 7,
    ACROPOLIS_PATIO_LEAP_LANDING_CLIP                  = 8,
    ACROPOLIS_PATIO_LEAP_SCENE_BLEND_FRAMES            = 20,
    ACROPOLIS_PATIO_STRANGER_COMMAND_LOWER_HEAD_WEIGHT = 0,
    ACROPOLIS_PATIO_STRANGER_COMMAND_RAISE_HEAD_WEIGHT = 1,
    ACROPOLIS_PATIO_STRANGER_COMMAND_PRINT_TRANSFORM   = 2,
};

#define ROOM_PART _gAcropolisPatioSecondVisitPlayerBankCopyRequest
#include "assets/rooms/akropolis/patio/events.inc"
/// Bodyless controller that turns the live player left by half a turn over sixteen ticks.
///
/// Read synchronously at spawn; the patio callback and live player must remain
/// available until the controller finishes and releases its task.
static TaskDesc _gAcropolisPatioPlayerTurnLeftTaskDescriptor = { { { TASK_BODY_NONE, ACROPOLIS_PATIO_SCENE_TASK_PRIORITY } }, _acropolisPatioPlayerTurnLeftTask, { .value = 0 } };
#define ROOM_PART _gAcropolisPatioSecondVisitScript
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

/// Visibility bits for the logical camera views that show a patio light.
enum {
    ACROPOLIS_PATIO_LIGHT_VIEW_3 = 1 << (3 - 1),
    ACROPOLIS_PATIO_LIGHT_VIEW_4 = 1 << (4 - 1),
    ACROPOLIS_PATIO_LIGHT_VIEW_5 = 1 << (5 - 1),
    ACROPOLIS_PATIO_LIGHT_VIEW_7 = 1 << (7 - 1),
    ACROPOLIS_PATIO_LIGHT_VIEW_8 = 1 << (8 - 1),
    ACROPOLIS_PATIO_LIGHT_VIEW_9 = 1 << (9 - 1),
};

STATIC_ASSERT(ARRAY_SIZE(_gAcropolisPatioLightViewMasks) == ARRAY_SIZE(_gAcropolisPatioLightAnchors), acropolisPatioLightMaskCount);

/// Door sound requests and map-marker nibbles carried by the patio warp endpoints.
enum {
    ACROPOLIS_PATIO_FOUNTAIN_ARRIVAL_SOUND    = SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_PATIO, 2),
    ACROPOLIS_PATIO_FOUNTAIN_DEPARTURE_SOUND  = SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_PATIO, 1),
    ACROPOLIS_PATIO_FOUNTAIN_BLOCKED_SOUND    = SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_PATIO, 7),
    ACROPOLIS_PATIO_CAFETERIA_ARRIVAL_SOUND   = SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_PATIO, 5),
    ACROPOLIS_PATIO_CAFETERIA_DEPARTURE_SOUND = SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_PATIO, 4),
    ACROPOLIS_PATIO_CAFETERIA_BLOCKED_SOUND   = SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_PATIO, 6),
    ACROPOLIS_PATIO_FOUNTAIN_MAP_FLAG         = 496,
    ACROPOLIS_PATIO_CAFETERIA_MAP_FLAG        = 501,
};

/// Dimensions of the patio grid's X-major cell directory.
enum { ACROPOLIS_PATIO_GRID_CELLS_X = 7,
       ACROPOLIS_PATIO_GRID_CELLS_Z = 4 };

/// Action IDs, CAP slots and packed warp requests used by the patio action quads.
///
/// Warp requests store the patio departure ID in the high nibble and the
/// destination arrival ID in the low nibble. Stair parameters pack surface
/// class 4 above the step count; the control bit selects descent.
enum {
    ACROPOLIS_PATIO_TRIGGER_OPENING_ACTION      = 0,
    ACROPOLIS_PATIO_TRIGGER_FOLLOW_UP_ACTION    = 1,
    ACROPOLIS_PATIO_TRIGGER_SECOND_VISIT_ACTION = 2,
    ACROPOLIS_PATIO_TRIGGER_CAP_COMMAND_1       = 1,
    ACROPOLIS_PATIO_TRIGGER_CAP_COMMAND_2       = 2,
    ACROPOLIS_PATIO_TRIGGER_CAP_COMMAND_5       = 5,
    ACROPOLIS_PATIO_TRIGGER_CAP_COMMAND_7       = 7,
    ACROPOLIS_PATIO_TRIGGER_CAP_COMMAND_11      = 11,
    ACROPOLIS_PATIO_FOUNTAIN_WARP_REQUEST       = (2 << 4) | 1,
    ACROPOLIS_PATIO_CAFETERIA_WARP_REQUEST      = (3 << 4) | 2,
    ACROPOLIS_PATIO_SQUARE_WARP_REQUEST         = (1 << 4) | 5,
    ACROPOLIS_PATIO_STAIR_ASCEND_PARAMETER      = (4 << 4) | 3,
    ACROPOLIS_PATIO_STAIR_DESCEND_PARAMETER     = (4 << 4) | 2,
    ACROPOLIS_PATIO_STAIR_DESCEND_CONTROL       = 0x100,
};

/// Sprite-atlas pages: opaque 8-bit textures at VRAM Y 0, sharing CLUT (0, 255).
enum {
    ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_0 = getTPage(1, 0, 832, 0),
    ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_1 = getTPage(1, 0, 896, 0),
    ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2 = getTPage(1, 0, 960, 0),
    ACROPOLIS_PATIO_SPRITE_CLUT           = getClut(0, 255),
    ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB    = 128,
    ACROPOLIS_PATIO_SPRITE_OPAQUE         = 0,
};

/// Cue-2 footstep bases in the loaded type-1 bank; cue 1 uses the next entry.
enum {
    ACROPOLIS_PATIO_CLASS4_WALK_SOUND = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x01,
    ACROPOLIS_PATIO_CLASS4_RUN_SOUND  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x03,
    ACROPOLIS_PATIO_CLASS4_JUMP_SOUND = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x11,
};

/// Registers the patio room receiver and restores its story-dependent actor setup.
///
/// Requires a live state-0 task and loaded room resources. Before story progress
/// 2, room 1 restores actor 0 and starts the player's head-aim controller; actor 1
/// is restored when present. Placement variants select additional actor commands.
/// Publishes `GAME_TASK_SLOT_ROOM` and advances to idle. The repeated opening-
/// progress reads are retained; command storage is borrowed through dispatch.
static void _acropolisPatioInitializeRoomTask(Task* task)
{
    enum {
        ACROPOLIS_PATIO_PLAYER_HEAD_AIM_TASK = 2,
    };

    ActorCommand actorCommand;
    Task*        placedActor;

    task->msgTable = _gAcropolisPatioRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    if (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_CAFETERIA_ROUTE_PROGRESS) < ACROPOLIS_PATIO_STORY_BATTLE_FINISHED) {
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState == 1) {
            TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(0), ACTOR_MESSAGE_PLACE, &_gAcropolisPatioRestoredSwatPlacement, 0);
            TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(0), ACTOR_COMMAND_MESSAGE_APPLY, &_gAcropolisPatioSwatAnimation3Command, 0);
            taskMessageDispatch(sceneFindPlacedActor(0), ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
            _gAcropolisPatioPlayerHeadAimTask = taskSpawnFromTable(_gAcropolisPatioSceneTaskDescriptors, ACROPOLIS_PATIO_PLAYER_HEAD_AIM_TASK, 0, 0);
        }
        placedActor = sceneFindPlacedActor(1);
        if (placedActor != 0) {
            TASK_MESSAGE_DISPATCH_POINTER(placedActor, ACTOR_MESSAGE_PLACE, &_gAcropolisPatioRestoredStrangerPlacement, 0);
        }
    }
    if ((gGameSession->location.variant == 1) && (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_PATIO_CUTSCENE_PROGRESS) < ACROPOLIS_PATIO_OPENING_SCENE_THRESHOLD) && (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_PATIO_CUTSCENE_PROGRESS) < ACROPOLIS_PATIO_OPENING_SCENE_THRESHOLD)) {
        placedActor = sceneFindPlacedActor(1);
        if (placedActor != 0) {
            TASK_MESSAGE_DISPATCH_POINTER(placedActor, ACTOR_COMMAND_MESSAGE_APPLY, &_gAcropolisPatioHideStrangerCommand, 0);
        }
    }
    // Commands are borrowed only through these synchronous dispatches.
    if ((gGameSession->location.variant == 2) && (gameFlagGetNibble(GAME_FLAG_AKROPOLIS_PATIO_026) == ACROPOLIS_PATIO_SCENE_UNSEEN)) {
        actorCommand.context.loc.stage = GAME_STAGE_AKROPOLIS;
        actorCommand.context.loc.area  = GAME_AREA_AKROPOLIS_PATIO;
        actorCommand.command           = 0;
        TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(2), ACTOR_COMMAND_MESSAGE_APPLY, &actorCommand, 0);
        TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(3), ACTOR_COMMAND_MESSAGE_APPLY, &actorCommand, 0);
    }
    task->state = task->state + 1;
}

/// Starts the cafeteria-door choice and records its deferred arrival selectors.
///
/// Borrows a complete transition request for this call; retains no pointer.
/// Marks the Parthenon Key identified and copies warp/room bytes after the
/// spawn, even on allocation failure. The singleton arrival must remain intact
/// until the accepted door scene commits it to the live save for reload.
static inline void _acropolisPatioStartCafeteriaDoorSequence(const RoomEventMsg* request)
{
    enum {
        ACROPOLIS_PATIO_CAFETERIA_DOOR_TASK = 1,
    };

    taskSpawnFromTable(_gAcropolisPatioSceneTaskDescriptors, ACROPOLIS_PATIO_CAFETERIA_DOOR_TASK, 0, 0);
    itemSetIdentified(INVENTORY_COLLECTION_ID_PARTHENON_KEY, 1);
    _gAcropolisPatioCafeteriaArrivalWarp = request->warp;
    _gAcropolisPatioCafeteriaArrivalRoom = request->roomState;
}

/// Resolves patio exits, including the scripted cafeteria-door departure.
///
/// Copies complete borrowed request/reply records, which may alias. Returns
/// 0 for a blocked exit, 1 for an immediate transition or 2 for a deferred CAP/
/// event/door sequence. Queries suppress effects and leave destination selectors
/// copied. Execution gates cafeteria/fountain exits on story flags, identifies
/// the Parthenon Key before the door sequence and saves its arrival for reload.
/// Allowed cafeteria rooms follow progress; receiver and message ID are unused.
static s32 _acropolisPatioResolveRoomTransition(Task* unusedTask, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    enum {
        ACROPOLIS_PATIO_TRANSITION_BLOCKED  = 0,
        ACROPOLIS_PATIO_TRANSITION_ALLOWED  = 1,
        ACROPOLIS_PATIO_TRANSITION_DEFERRED = 2,
    };

    s32 transitionResult;
    u16 destinationArea;

    *reply = *request;
    if (request->areaId == GAME_AREA_AKROPOLIS_FOUNTAIN) {
        if ((gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_SECURITY_ROOM_PANEL_KEYS) & ACROPOLIS_PATIO_SECOND_SECURITY_LOCK_BIT) && (request->queryOnly == ROOM_EVENT_EXECUTE)) {
            reply->roomState = 2;
        }
    }
    if (request->areaId == GAME_AREA_AKROPOLIS_CAFETERIA) {
        if (gameFlagGetNibble(GAME_FLAG_PATIO_CAFETERIA_DOOR_STATE) < ACROPOLIS_PATIO_DOOR_UNLOCKED) {
            transitionResult = ACROPOLIS_PATIO_TRANSITION_BLOCKED;
            if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                capRunCommandWithTransition(3);
                gameFlagSetNibble(GAME_FLAG_PATIO_CAFETERIA_DOOR_STATE, ACROPOLIS_PATIO_DOOR_ATTEMPTED);
                gameFlagSetNibbleIfPresent(request->flagId, 2);
                return ACROPOLIS_PATIO_TRANSITION_BLOCKED;
            }
            return transitionResult;
        }
        if (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_CAFETERIA_ROUTE_PROGRESS) == ACROPOLIS_PATIO_STORY_BATTLE_FINISHED) {
            transitionResult = ACROPOLIS_PATIO_TRANSITION_DEFERRED;
            if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                if (gameFlagGetNibble(GAME_FLAG_PATIO_CAFETERIA_DOOR_SCENE_SEEN) == ACROPOLIS_PATIO_SCENE_UNSEEN) {
                    evsStartScriptWithSkip(_gAcropolisPatioCafeteriaDoorScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gAcropolisPatioCafeteriaDoorSkipScript);
                    gameFlagSetNibble(GAME_FLAG_PATIO_CAFETERIA_DOOR_SCENE_SEEN, ACROPOLIS_PATIO_SCENE_SEEN);
                    return ACROPOLIS_PATIO_TRANSITION_DEFERRED;
                }
                capRunCommandWithTransition(8);
                return ACROPOLIS_PATIO_TRANSITION_DEFERRED;
            }
            return transitionResult;
        }
        if (gameFlagGetNibble(GAME_FLAG_PATIO_CAFETERIA_DOOR_STATE) == ACROPOLIS_PATIO_DOOR_UNLOCKED) {
            transitionResult = ACROPOLIS_PATIO_TRANSITION_DEFERRED;
            if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                // Save the requested arrival for the deferred cafeteria reload.
                _acropolisPatioStartCafeteriaDoorSequence(request);
                return ACROPOLIS_PATIO_TRANSITION_DEFERRED;
            }
            return transitionResult;
        }
    }
    if ((request->areaId == GAME_AREA_AKROPOLIS_FOUNTAIN) && (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_CAFETERIA_ROUTE_PROGRESS) < ACROPOLIS_PATIO_STORY_FOUNTAIN_READY)) {
        transitionResult = ACROPOLIS_PATIO_TRANSITION_BLOCKED;
        if (request->queryOnly == ROOM_EVENT_EXECUTE) {
            gameFlagSetNibbleIfPresent(request->flagId, 2);
            capRunCommandWithTransition(4);
            return ACROPOLIS_PATIO_TRANSITION_BLOCKED;
        }
        return transitionResult;
    }
    if ((request->queryOnly == ROOM_EVENT_EXECUTE) && (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_PATIO_CUTSCENE_PROGRESS) == ACROPOLIS_PATIO_OPENING_SCENE_STARTED)) {
        gameFlagSetNibble(EVENT_FLAG_AKROPOLIS_PATIO_CUTSCENE_PROGRESS, ACROPOLIS_PATIO_OPENING_SCENE_FINISHED);
    }
    destinationArea  = request->areaId;
    transitionResult = ACROPOLIS_PATIO_TRANSITION_ALLOWED;
    if (destinationArea == GAME_AREA_AKROPOLIS_CAFETERIA) {
        transitionResult = ACROPOLIS_PATIO_TRANSITION_ALLOWED;
        if (request->queryOnly == ROOM_EVENT_EXECUTE) {
            if (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_CAFETERIA_ROUTE_PROGRESS) >= ACROPOLIS_PATIO_STORY_FOLLOW_UP_STARTED) {
                reply->roomState = destinationArea;
            }
            transitionResult = ACROPOLIS_PATIO_TRANSITION_ALLOWED;
            if (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_CAFETERIA_ROUTE_PROGRESS) == ACROPOLIS_PATIO_STORY_BATTLE_FINISHED) {
                reply->roomState = 3;
                transitionResult = ACROPOLIS_PATIO_TRANSITION_ALLOWED;
            }
        }
    }
    return transitionResult;
}

/// Transfers the accepted door arrival to captured-frame reload and releases its task.
///
/// Requires the deferred warp/room bytes and a live bodyless door-scene task.
/// Stops nonambient sound before committing the cafeteria destination to the
/// live save; room-byte assignment retains its bit pattern in the unsigned saved
/// selector. Selects sprite variant 1 and kills this task after requesting
/// captured-frame reload, including when reload allocation fails.
static inline void _acropolisPatioCommitCafeteriaDeparture(Task* task)
{
    enum { ACROPOLIS_PATIO_CAFETERIA_SPRITE_VARIANT = 1 };

    sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.area      = GAME_AREA_AKROPOLIS_CAFETERIA;
    gDisplayState.spriteVariant                                 = ACROPOLIS_PATIO_CAFETERIA_SPRITE_VARIANT;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp      = _gAcropolisPatioCafeteriaArrivalWarp;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = _gAcropolisPatioCafeteriaArrivalRoom;
    taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
    taskKill(task);
}

/// Resolves the cafeteria-door choice and reloads the saved destination on acceptance.
///
/// Starts bodyless in state 0 after the transition resolver records warp/room.
/// Holds player control for CAP command 3. Choice key 2 marks the door open and
/// plays patio sound 4; another choice restores control and kills the task.
/// After the sound ends, stops nonambient sound and starts captured-frame reload
/// into the cafeteria, transferring the saved arrival before releasing this task.
static void _acropolisPatioCafeteriaDoorSceneTask(Task* task)
{
    enum {
        ACROPOLIS_PATIO_DOOR_SOUND_CUE = 4,
        ACROPOLIS_PATIO_DOOR_OPEN      = 3,
    };

    enum {
        ACROPOLIS_PATIO_DOOR_START          = 0,
        ACROPOLIS_PATIO_DOOR_WAIT_CHOICE    = 1,
        ACROPOLIS_PATIO_DOOR_RESOLVE_CHOICE = 2,
        ACROPOLIS_PATIO_DOOR_WAIT_SOUND     = 3,
        ACROPOLIS_PATIO_DOOR_CAP_COMMAND    = 3,
    };

    s32 state;

    state = task->state;
    switch (state) {
        case ACROPOLIS_PATIO_DOOR_START:
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            capRunCommandWithTransition(ACROPOLIS_PATIO_DOOR_CAP_COMMAND);
            task->state = task->state + 1;
            return;
        case ACROPOLIS_PATIO_DOOR_WAIT_CHOICE:
            task->state = ACROPOLIS_PATIO_DOOR_RESOLVE_CHOICE;
            return;
        case ACROPOLIS_PATIO_DOOR_RESOLVE_CHOICE:
            if (capGetVariantKey() == state) {
                gameFlagSetNibble(GAME_FLAG_PATIO_CAFETERIA_DOOR_STATE, ACROPOLIS_PATIO_DOOR_OPEN);
                sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_PATIO, ACROPOLIS_PATIO_DOOR_SOUND_CUE), 0, 0);
                task->state = task->state + 1;
                return;
            }
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
            taskKill(task);
            return;
        case ACROPOLIS_PATIO_DOOR_WAIT_SOUND:
            if (sndScriptHasActiveId(SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_PATIO, ACROPOLIS_PATIO_DOOR_SOUND_CUE)) != 0) {
                return;
            }
            _acropolisPatioCommitCafeteriaDeparture(task);
            return;
    }
}

/// Starts the patio's opening, follow-up or second-visit action script once eligible.
///
/// Borrows a complete read-only `DirectionActionRequest` through synchronous
/// dispatch and retains no pointer. Actions 0/1 advance opening progress; action
/// 1 also waits for placed actor 1 to leave. Action 2 latches the second-visit
/// event. The receiver, ID and second word are unused. The binary leaves the
/// return word unset; the room-action sender ignores it.
static s32 _acropolisPatioHandleActionMessage(Task* unusedTask, s32 messageId, const DirectionActionRequest* actionRequest, s32 unusedArg)
{
    enum {
        ACROPOLIS_PATIO_ACTION_OPENING      = 0,
        ACROPOLIS_PATIO_ACTION_FOLLOW_UP    = 1,
        ACROPOLIS_PATIO_ACTION_SECOND_VISIT = 2,
    };

    u8 actionId;

    if ((actionRequest->actionId == ACROPOLIS_PATIO_ACTION_OPENING) && (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_PATIO_CUTSCENE_PROGRESS) < ACROPOLIS_PATIO_OPENING_SCENE_THRESHOLD)) {
        gameFlagSetNibble(EVENT_FLAG_AKROPOLIS_PATIO_CUTSCENE_PROGRESS, ACROPOLIS_PATIO_OPENING_SCENE_STARTED);
        evsStartScriptWithSkip(_gAcropolisPatioOpeningScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gAcropolisPatioOpeningSkipScript);
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = 3;
        gGameSession->flowFlags                             = (GAME_SESSION_FLOW_SKIP_ENDING_MUSIC | GAME_SESSION_FLOW_HIDE_REEQUIPPED_WEAPON | GAME_SESSION_FLOW_REEQUIP_WEAPON);
    }
    if ((actionRequest->actionId == ACROPOLIS_PATIO_ACTION_FOLLOW_UP) && (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_PATIO_CUTSCENE_PROGRESS) == ACROPOLIS_PATIO_OPENING_SCENE_STARTED) &&
        (taskMessageDispatch(sceneFindPlacedActor(1), ACTOR_MESSAGE_IS_PRESENT, 0, 0) == 0)) {
        gameFlagSetNibble(EVENT_FLAG_AKROPOLIS_PATIO_CUTSCENE_PROGRESS, ACROPOLIS_PATIO_OPENING_SCENE_FINISHED);
        evsStartScriptWithSkip(_gAcropolisPatioFollowUpScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gAcropolisPatioFollowUpSkipScript);
    }
    actionId = actionRequest->actionId;
    if ((actionId == ACROPOLIS_PATIO_ACTION_SECOND_VISIT) && (gameFlagGetNibble(GAME_FLAG_AKROPOLIS_PATIO_026) == ACROPOLIS_PATIO_SCENE_UNSEEN) && (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_CAFETERIA_ROUTE_PROGRESS) == actionId)) {
        gameFlagSetNibble(GAME_FLAG_AKROPOLIS_PATIO_026, ACROPOLIS_PATIO_SCENE_SEEN);
        evsStartScriptWithSkip(_gAcropolisPatioSecondVisitScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gAcropolisPatioSecondVisitSkipScript);
    }
    // No result is set; the sender of a room action ignores it.
}
/// Handles patio CAP event commands 1 and 2.
///
/// Command 1 tries event 1 and returns 2. Command 2 tries event 2 only before
/// story progress 2 and returns 0 either way; other commands return 2. Event
/// admission failures are ignored. Receiver, ID and second word are unused.
static s32 _acropolisPatioHandleCommandMessage(Task* unusedTask, s32 messageId, s32 commandId, s32 unusedArg)
{
    enum {
        ACROPOLIS_PATIO_COMMAND_EVENT_1 = 1,
        ACROPOLIS_PATIO_COMMAND_EVENT_2 = 2,
    };

    s32 reply;

    if (commandId == ACROPOLIS_PATIO_COMMAND_EVENT_1) {
        capSpawnEventIfIdle(ACROPOLIS_PATIO_COMMAND_EVENT_1, CAP_EVENT_NO_FLAGS);
    }
    reply = 2;
    if (commandId == ACROPOLIS_PATIO_COMMAND_EVENT_2) {
        reply = gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_CAFETERIA_ROUTE_PROGRESS) < ACROPOLIS_PATIO_STORY_BATTLE_FINISHED;
        if (reply != 0) {
            capSpawnEventIfIdle(ACROPOLIS_PATIO_COMMAND_EVENT_2, CAP_EVENT_NO_FLAGS);
            reply = 0;
        }
    }
    return reply;
}

/// Refuses every key-item-use request (message 0x13F1), returning zero to the inventory.
///
/// The receiver, selected item ID and second payload word are unused.
static s32 _acropolisPatioRefuseKeyItemUse(Task* roomTask, s32 messageId, s32 itemId, s32 unusedArg)
{
    enum { ACROPOLIS_PATIO_KEY_ITEM_REFUSED = 0 };

    return ACROPOLIS_PATIO_KEY_ITEM_REFUSED;
}

/// Handles the room sound message: cue 3 queues area-bank entry 3; all cues return zero.
///
/// Queue admission failures are ignored. The receiver and second payload word are unused.
static s32 _acropolisPatioHandleSoundMessage(Task* roomTask, s32 messageId, s32 soundCue, s32 unusedArg)
{
    enum { ACROPOLIS_PATIO_SOUND_CUE = 3 };

    if (soundCue == ACROPOLIS_PATIO_SOUND_CUE) {
        sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_PATIO, ACROPOLIS_PATIO_SOUND_CUE), 0, 0);
    }
    return 0;
}
/// Runs patio CAP command 6 and records choice key 1 in the shared scene flag.
///
/// Starts bodyless in state 0. Advances through one waiting tick, then tests
/// the CAP variant key, conditionally sets `EVENT_FLAG_MAP_AKROPOLIS_AVAILABLE` and kills the task.
/// The flag's wider story meaning is unproven; other choice keys leave it alone.
static void _acropolisPatioRecordSceneChoiceTask(Task* task)
{
    enum {
        ACROPOLIS_PATIO_CAP6_START         = 0,
        ACROPOLIS_PATIO_CAP6_WAIT_CHOICE   = 1,
        ACROPOLIS_PATIO_CAP6_RECORD_CHOICE = 2,
        ACROPOLIS_PATIO_CAP6_COMMAND       = 6,
    };

    s32 state;

    state = task->state;
    switch (state) {
        case ACROPOLIS_PATIO_CAP6_START:
            capRunCommandWithTransition(ACROPOLIS_PATIO_CAP6_COMMAND);
            task->state = task->state + 1;
            return;
        case ACROPOLIS_PATIO_CAP6_WAIT_CHOICE:
            task->state = ACROPOLIS_PATIO_CAP6_RECORD_CHOICE;
            return;
        case ACROPOLIS_PATIO_CAP6_RECORD_CHOICE:
            if (capGetVariantKey() == 1) {
                gameFlagSetNibble(EVENT_FLAG_MAP_AKROPOLIS_AVAILABLE, 1);
            }
            taskKill(task);
            return;
    }
}

/// Aims the player's head at a scripted world point, optionally sliding it along Z.
///
/// States 0/1 initialize and idle, 2 holds the point (-8000, 0, 900), 3 starts
/// the slide, and 4 moves it 50 units per tick to Z -3196. `spawnArg1.value`
/// stores the slide distance (0..4096). Requires the live player's five-part
/// head chain; the task neither owns nor releases the player.
static void _acropolisPatioPlayerHeadAimTask(Task* task)
{
    enum {
        ACROPOLIS_PATIO_HEAD_AIM_INIT           = 0,
        ACROPOLIS_PATIO_HEAD_AIM_IDLE           = 1,
        ACROPOLIS_PATIO_HEAD_AIM_HOLD           = 2,
        ACROPOLIS_PATIO_HEAD_AIM_START_SLIDE    = 3,
        ACROPOLIS_PATIO_HEAD_AIM_SLIDE          = 4,
        ACROPOLIS_PATIO_HEAD_AIM_MAX_YAW        = ACTOR_TRANSFORM_ANGLE_TURN / 8,
        ACROPOLIS_PATIO_HEAD_AIM_MAX_PITCH      = ACTOR_TRANSFORM_ANGLE_TURN / 16,
        ACROPOLIS_PATIO_HEAD_AIM_SLIDE_STEP     = 50,
        ACROPOLIS_PATIO_HEAD_AIM_SLIDE_DISTANCE = 4096
    };

    GfxCoord focus;        // World point carried in the translation; the other fields are unused
    byte     unused[0x28]; // Frame space no instruction touches; what the original declared here is unproven
    Task*    playerTask;
    s32      slideDistance;

    playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);

    // The head turn reads nothing of its target frame but the translation, a
    // world point, so the rest of `focus` stays uninitialised.
    focus.coord.t[0] = -0x1F40;
    focus.coord.t[1] = 0;
    focus.coord.t[2] = 0x384;

    switch (task->state) {
        case ACROPOLIS_PATIO_HEAD_AIM_INIT:
            task->spawnArg1.value = 0;
            task->state           = task->state + 1;
            return;
        case ACROPOLIS_PATIO_HEAD_AIM_IDLE:
            return;
        case ACROPOLIS_PATIO_HEAD_AIM_HOLD:
            animationAimHeadAtPoint(playerTask, &focus, ACROPOLIS_PATIO_HEAD_AIM_MAX_YAW, ACROPOLIS_PATIO_HEAD_AIM_MAX_PITCH, ONE);
            return;
        case ACROPOLIS_PATIO_HEAD_AIM_START_SLIDE:
            task->spawnArg1.value = 0;
            animationAimHeadAtPoint(playerTask, &focus, ACROPOLIS_PATIO_HEAD_AIM_MAX_YAW, ACROPOLIS_PATIO_HEAD_AIM_MAX_PITCH, ONE);
            task->state = task->state + 1;
            return;
        case ACROPOLIS_PATIO_HEAD_AIM_SLIDE:
            slideDistance         = task->spawnArg1.value + ACROPOLIS_PATIO_HEAD_AIM_SLIDE_STEP;
            task->spawnArg1.value = slideDistance;
            if (slideDistance >= ACROPOLIS_PATIO_HEAD_AIM_SLIDE_DISTANCE + 1) {
                task->spawnArg1.value = ACROPOLIS_PATIO_HEAD_AIM_SLIDE_DISTANCE;
            }
            focus.coord.t[2] -= task->spawnArg1.value;
            animationAimHeadAtPoint(playerTask, &focus, ACROPOLIS_PATIO_HEAD_AIM_MAX_YAW, ACROPOLIS_PATIO_HEAD_AIM_MAX_PITCH, ONE);
            return;
    }
}

/// Selects the opening scene's player-head-aim phase without resetting its progress.
///
/// The retained aim task must already be spawned and live. Script choices are
/// 1 idle, 2 hold the fixed world point, and 3 begin its Z slide. The task itself
/// resets the slide distance when phase 3 next runs; this setter only stores
/// the supplied signed-word state.
static void _acropolisPatioSetPlayerHeadAimState(s32 headAimState)
{
    _gAcropolisPatioPlayerHeadAimTask->state = headAimState;
}

/// Selects room 2 in the live and saved location and requests room-object relinking.
static void _acropolisPatioActivateRoom2(void)
{
    enum { ACROPOLIS_PATIO_ROOM_AFTER_SCENE = 2 };

    gGameSession->location.roomState = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = ACROPOLIS_PATIO_ROOM_AFTER_SCENE;
    gGameSession->roomObjsDirty                                                                    = true;
}

/// Sets the scene's actor update/visibility control from an event-script byte.
///
/// Accepts `SCENE_COMBAT_ACTORS_*`; the patio scripts use paused and running.
static void _acropolisPatioSetActorControl(u8 actorControl)
{
    gSceneCombatState.actorControl = actorControl;
}

/// Keeps the room task alive without per-frame work; `task` is unused.
static void _acropolisPatioRoomIdleState(Task* task)
{
    byte unused[0x10]; // Untouched local storage retained for the original stack frame; its role is unproven
}

void acropolisPatioRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers = _gAcropolisPatioRoomTaskStates;
    stateHandlers.funcs[task->state](task);
}

/// Pauses the patio's actors or resumes them and engages battle.
///
/// Event scripts pass zero to pause, or nonzero to resume actor updates and
/// latch an idle battle as engaged. Pausing leaves the battle phase intact.
/// Requires the live scene-combat state; no task or payload is retained.
static void _acropolisPatioSetBattleActive(s32 battleActive)
{
    if (battleActive != 0) {
        gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
        sceneEngageBattle(1);
        return;
    }
    gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
}
/// Starts the live player's half-turn left for the patio event script.
///
/// Allocates a bodyless controller; its sixteen ticks update the player's
/// yaw before teardown. The player must remain live through those updates.
/// Allocation failure leaves no controller and is ignored by the script.
static void _acropolisPatioSpawnPlayerTurnLeft(void)
{
    taskSpawnFromTable(&_gAcropolisPatioPlayerTurnLeftTaskDescriptor, 0, 0, 0);
}

/// Turns the live player left by half a turn over 16 task ticks, then kills itself.
///
/// Yaw uses 4096 units per turn. States 0/1 start and turn; any other state
/// kills this bodyless controller. `killCountdown` holds the remaining ticks.
/// Start in state 0 with player yaw in [-2048, 2048).
static void _acropolisPatioPlayerTurnLeftTask(Task* task)
{
    enum {
        ACROPOLIS_PATIO_PLAYER_TURN_INIT     = 0,
        ACROPOLIS_PATIO_PLAYER_TURN_STEP     = 1,
        ACROPOLIS_PATIO_PLAYER_TURN_TICKS    = 16,
        ACROPOLIS_PATIO_PLAYER_TURN_YAW_STEP = ACTOR_TRANSFORM_ANGLE_HALF_TURN / ACROPOLIS_PATIO_PLAYER_TURN_TICKS
    };

    GameActor* playerActor;
    s16        yaw;

    playerActor = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->work;

    switch (task->state) {
        case ACROPOLIS_PATIO_PLAYER_TURN_INIT:
            task->killCountdown = ACROPOLIS_PATIO_PLAYER_TURN_TICKS;
            task->state++;
            /* fallthrough */
        case ACROPOLIS_PATIO_PLAYER_TURN_STEP:
            yaw = playerActor->rotation.vy - ACROPOLIS_PATIO_PLAYER_TURN_YAW_STEP;
            if (yaw < -ACTOR_TRANSFORM_ANGLE_HALF_TURN) {
                yaw = playerActor->rotation.vy + (ACTOR_TRANSFORM_ANGLE_TURN - ACROPOLIS_PATIO_PLAYER_TURN_YAW_STEP);
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

void acropolisPatioSpawnLightEffectsTask(Task* task)
{
    enum { LIGHT_EMITTER_INIT     = 0,
           LARGE_LIGHT_END        = 3,
           CEILING_LIGHT_END      = 7,
           MOTES_PER_LARGE_LIGHT  = 3,
           LARGE_LIGHT_ARGUMENT   = (768 << ACROPOLIS_PATIO_JET_SIZE_SHIFT) | (2 << ACROPOLIS_PATIO_JET_CELL_SHIFT),
           CEILING_LIGHT_ARGUMENT = 512 << ACROPOLIS_PATIO_JET_SIZE_SHIFT,
           WALL_LIGHT_ARGUMENT    = 1 << ACROPOLIS_PATIO_JET_CELL_SHIFT };
    GfxCoord*   parentCoord;
    EffectWork* scratchWork;
    s32         anchorIndex;
    s32         moteIndex;

/// Chooses a parent-relative mote position around a large-light anchor.
///
/// Requires a writable SVECTOR pointer and an anchor index 0..2. Both arguments
/// must be stable and side-effect-free: position is evaluated six times and
/// anchorIndex three times. Advances gRandomLcgState three times in XYZ order,
/// with jitter -1023..1024 coordinate units and s16 narrowing before and after
/// anchor addition. Replaces the previous XYZ, preserving pad; the following
/// effect spawn copies the chosen position.
/// Use as a statement inside a braced block.
#define ACROPOLIS_PATIO_CHOOSE_LIGHT_MOTE_POSITION(position, anchorIndex)                    \
    {                                                                                        \
        enum { MOTE_JITTER_CENTRE = 1024,                                                    \
               MOTE_JITTER_MASK   = 2047 };                                                    \
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;    \
        (position)->vx  = MOTE_JITTER_CENTRE - ((gRandomLcgState >> 16) & MOTE_JITTER_MASK); \
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;    \
        (position)->vy  = MOTE_JITTER_CENTRE - ((gRandomLcgState >> 16) & MOTE_JITTER_MASK); \
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;    \
        (position)->vz  = MOTE_JITTER_CENTRE - ((gRandomLcgState >> 16) & MOTE_JITTER_MASK); \
        (position)->vx += _gAcropolisPatioLightAnchors[(anchorIndex)].vx;                    \
        (position)->vy += _gAcropolisPatioLightAnchors[(anchorIndex)].vy;                    \
        (position)->vz += _gAcropolisPatioLightAnchors[(anchorIndex)].vz;                    \
    }

    scratchWork = task->spawnArg2.pointer;
    parentCoord = task->extra.coordBody->coord;

    if (task->state == LIGHT_EMITTER_INIT) {
        // Seed all fixed light glows before allocating their moving points.
        for (anchorIndex = 0; anchorIndex < LARGE_LIGHT_END; anchorIndex++) {
            effectSpawn(EFFECT_ACROPOLIS_PATIO_FOUNTAIN_JET, parentCoord, anchorIndex + LARGE_LIGHT_ARGUMENT, &_gAcropolisPatioLightAnchors[anchorIndex]);
        }
        for (anchorIndex = LARGE_LIGHT_END; anchorIndex < CEILING_LIGHT_END; anchorIndex++) {
            effectSpawn(EFFECT_ACROPOLIS_PATIO_FOUNTAIN_JET, parentCoord, anchorIndex + CEILING_LIGHT_ARGUMENT, &_gAcropolisPatioLightAnchors[anchorIndex]);
        }
        for (anchorIndex = CEILING_LIGHT_END; anchorIndex < ARRAY_SIZE(_gAcropolisPatioLightAnchors); anchorIndex++) {
            effectSpawn(EFFECT_ACROPOLIS_PATIO_FOUNTAIN_JET, parentCoord, anchorIndex + WALL_LIGHT_ARGUMENT, &_gAcropolisPatioLightAnchors[anchorIndex]);
        }
        task->state++;
        // Each child snapshots XYZ; these point tasks never read the retained offset pointer.
        for (anchorIndex = 0; anchorIndex < LARGE_LIGHT_END; anchorIndex++) {
            for (moteIndex = 0; moteIndex < MOTES_PER_LARGE_LIGHT; moteIndex++) {
                ACROPOLIS_PATIO_CHOOSE_LIGHT_MOTE_POSITION(&scratchWork->move, anchorIndex);
                effectSpawn(EFFECT_ACROPOLIS_PATIO_FOUNTAIN_MIST, parentCoord, anchorIndex, &scratchWork->move);
            }
        }
    }
#undef ACROPOLIS_PATIO_CHOOSE_LIGHT_MOTE_POSITION
}

/// Positions a patio light glow as a square around its projected screen centre.
///
/// Borrows a writable packet and a separate, read-only `projection` for this
/// call. Only `screenPos` and `halfExtent` need initialization in the projection;
/// both count whole pixels, with a nonnegative half-extent. Centre plus/minus
/// extent must fit signed 32-bit arithmetic. Each edge narrows to signed 16 bits
/// without clipping. Vertices 0..3 are top-left, top-right, bottom-left and
/// bottom-right. Writes only the eight coordinate halfwords.
static inline void _acropolisPatioSetLightGlowBounds(POLY_FT4* glowQuad, const RoomGlowSpriteScratch* projection)
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

/// Projects a patio light's cached view position into a flickering additive glow.
///
/// `composedCoord->workm.t` supplies signed game coordinates, narrowed to s16
/// before projection through `GsWSMATRIX`. The caller supplies GTE projection
/// settings and a complete, word-aligned `spriteScratch` block. FLAG is not
/// tested. `glowWork->angle` selects cell/CLUT 0..2, `scale` is the size scale
/// (1..4095), and `period` is resting grey (48, 64 or 80); odd animation frames
/// add 16. Screen half-extent is scale * 39 / (SZ3 / 4), in integer pixels.
///
/// Reserves one `POLY_FT4` even when depth is below `ACROPOLIS_PATIO_JET_MIN_DEPTH`.
/// Accepted depths wrap into the current 1024-tag ordering table. The arena and
/// sprite texture/CLUT must remain live until GPU completion; inputs and scratch
/// are borrowed only for this call. Changes GTE state and uses the texture page's
/// additive blend mode without allocating a separate draw-mode command.
static inline void _acropolisPatioDrawLightGlow(const GfxCoord* composedCoord, const EffectWork* glowWork, RoomGlowSpriteScratch* spriteScratch)
{
    enum {
        ACROPOLIS_PATIO_LIGHT_GLOW_CLUT_STRIDE             = 16,
        ACROPOLIS_PATIO_LIGHT_GLOW_FRAME_PARITY_MASK       = 1,
        ACROPOLIS_PATIO_LIGHT_GLOW_DEPTH_TO_OT_BYTES_SHIFT = 2
    };

    POLY_FT4* glowQuad;
    u8        greyLevel;

    // Project the narrowed cached view translation into the sprite's centre.
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
    if (spriteScratch->otz >= ACROPOLIS_PATIO_JET_MIN_DEPTH) {
        greyLevel       = glowWork->period + (((u8)gDisplayState.animFrame & ACROPOLIS_PATIO_LIGHT_GLOW_FRAME_PARITY_MASK) << ACROPOLIS_PATIO_JET_FLICKER_SHIFT);
        glowQuad->tpage = ACROPOLIS_PATIO_JET_TEXTURE_PAGE;
        setRGB0(glowQuad, greyLevel, greyLevel, greyLevel);
        setSemiTrans(glowQuad, true);
        setClut(glowQuad, glowWork->angle * ACROPOLIS_PATIO_LIGHT_GLOW_CLUT_STRIDE, ACROPOLIS_PATIO_JET_CLUT_Y);
        // Texture endpoints include the final texel of the fixed 40-by-40 cell.
        setUVWH(glowQuad, glowWork->angle * ACROPOLIS_PATIO_JET_CELL_SIZE, 0,
                ACROPOLIS_PATIO_JET_CELL_SIZE - 1, ACROPOLIS_PATIO_JET_CELL_SIZE - 1);

        spriteScratch->halfExtent = (glowWork->scale * (ACROPOLIS_PATIO_JET_CELL_SIZE - 1)) / spriteScratch->otz;
        _acropolisPatioSetLightGlowBounds(glowQuad, spriteScratch);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(
                    (((u32)spriteScratch->otz << gDisplayState.otDepthShift) >> ACROPOLIS_PATIO_LIGHT_GLOW_DEPTH_TO_OT_BYTES_SHIFT) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK),
                glowQuad);
    }
}

void acropolisPatioFountainJetTask(Task* task)
{
    enum { ACROPOLIS_PATIO_JET_INIT = 0 };

    RoomGlowSpriteScratch* block;
    EffectWork*            work;
    GfxCoord*              coord;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN &&
        ((_gAcropolisPatioLightViewMasks[task->spawnArg1.value & ACROPOLIS_PATIO_JET_ANCHOR_MASK] >> (gGameSession->location.view - 1)) & 1)) {
        actorRenderComposeCoord(coord);
        block = SCRATCH_STACK_RESERVE_BLOCK(RoomGlowSpriteScratch);
        if (task->state == ACROPOLIS_PATIO_JET_INIT) {
            // Decode the fixed sheet cell and size only on the first visible tick.
            u8 levels[3] = { 0x50, 0x30, 0x40 };

            if (task->spawnArg1.value & (ACROPOLIS_PATIO_JET_SIZE_MASK << ACROPOLIS_PATIO_JET_SIZE_SHIFT)) {
                work->scale = (task->spawnArg1.value >> ACROPOLIS_PATIO_JET_SIZE_SHIFT) & ACROPOLIS_PATIO_JET_SIZE_MASK;
            } else {
                work->scale = ACROPOLIS_PATIO_JET_DEFAULT_SIZE;
            }
            work->angle           = (task->spawnArg1.value >> ACROPOLIS_PATIO_JET_CELL_SHIFT) & ACROPOLIS_PATIO_JET_CELL_MASK;
            task->spawnArg1.value = task->spawnArg1.value & ACROPOLIS_PATIO_JET_ANCHOR_MASK;
            work->period          = levels[work->angle];
            task->state           = task->state + 1;
        }
        _acropolisPatioDrawLightGlow(coord, work, block);
        SCRATCH_STACK_RELEASE_BLOCK(RoomGlowSpriteScratch);
    }
}

/// Retained nonzero alignment byte after the fountain's three grey-level constants.
///
/// The grey-level initializer loads exactly three bytes; this fourth byte is
/// never read. Keep its value and emission position to preserve the aligned
/// start of the following text section.
static const u8 _gAcropolisPatioLightGreyAlignmentByte = 0xF2;

/// Projects a cached light-mote position into a random-grey, average-blended pixel.
///
/// Reads only `cachedCoord->workm.t`, in signed game units narrowed to s16.
/// This cache is the snapshot composed before the caller moved the local node;
/// a dirty composition stamp does not invalidate that snapshot for this draw.
/// Requires configured GTE projection settings, `GsWSMATRIX`, and a complete,
/// word-aligned `tileScratch` block. FLAG is not tested; depth is SZ3 / 4.
///
/// The word-aligned arena needs one `TILE_1` even on depth rejection and one
/// additional `DR_TPAGE` at `EFFECT_POINT_TILE_MIN_DEPTH` or further. Only an
/// accepted depth advances the shared LCG, selecting grey 0..191. Queues the
/// blend command before the pixel in the current 1024-tag ordering table and
/// leaves average draw mode active. Packet storage lives until GPU completion;
/// neither input nor scratch is retained. Changes GTE state.
static inline void _acropolisPatioDrawLightMote(const GfxCoord* cachedCoord, EffectPointTileScratch* tileScratch)
{
    enum {
        ACROPOLIS_PATIO_LIGHT_MOTE_GREY_LEVELS             = 192,
        ACROPOLIS_PATIO_LIGHT_MOTE_DEPTH_TO_OT_BYTES_SHIFT = 2
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
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        greyLevel       = gRandomLcgState >> 16;
        greyLevel      %= ACROPOLIS_PATIO_LIGHT_MOTE_GREY_LEVELS;
        setRGB0(moteTile, greyLevel, greyLevel, greyLevel);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(
                    (((u32)tileScratch->depth << gDisplayState.otDepthShift) >> ACROPOLIS_PATIO_LIGHT_MOTE_DEPTH_TO_OT_BYTES_SHIFT) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK),
                moteTile);
        // Prepending at the same depth makes the GPU set blending before the tile.
        gpuSetPrimitiveBlendMode(moteTile, GPU_BLEND_AVERAGE, tileScratch->depth);
    }
}

void acropolisPatioFountainMistTask(Task* task)
{
    enum {
        ACROPOLIS_PATIO_MIST_INIT            = 0,
        ACROPOLIS_PATIO_MIST_DRIFT           = 0,
        ACROPOLIS_PATIO_MIST_GATHER          = 1,
        ACROPOLIS_PATIO_MIST_DRIFT_CENTRE    = 16,
        ACROPOLIS_PATIO_MIST_DRIFT_MASK      = 31,
        ACROPOLIS_PATIO_MIST_AIM_SAMPLE_MASK = 3,
        ACROPOLIS_PATIO_MIST_GATHER_WEIGHT   = ONE / 128,
        ACROPOLIS_PATIO_MIST_JITTER_MASK     = 15,
        ACROPOLIS_PATIO_MIST_JITTER_CENTRE   = 8,
        ACROPOLIS_PATIO_MIST_DRIFT_CHANCE    = 120,
        ACROPOLIS_PATIO_MIST_GATHER_CHANCE   = 60
    };

    EffectWork*             work;
    GfxCoord*               coord;
    EffectPointTileScratch* tileScratch;
    SVECTOR*                velocity;
    SVECTOR*                anchors;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN &&
        ((_gAcropolisPatioLightViewMasks[task->spawnArg1.value] >> (gGameSession->location.view - 1)) & 1)) {
        tileScratch = SCRATCH_STACK_RESERVE_BLOCK(EffectPointTileScratch);
        actorRenderComposeCoord(coord);
        if (task->state == ACROPOLIS_PATIO_MIST_INIT) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->move.vx   = ACROPOLIS_PATIO_MIST_DRIFT_CENTRE - ((gRandomLcgState >> 16) & ACROPOLIS_PATIO_MIST_DRIFT_MASK);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->move.vy   = ACROPOLIS_PATIO_MIST_DRIFT_CENTRE - ((gRandomLcgState >> 16) & ACROPOLIS_PATIO_MIST_DRIFT_MASK);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->move.vz   = ACROPOLIS_PATIO_MIST_DRIFT_CENTRE - ((gRandomLcgState >> 16) & ACROPOLIS_PATIO_MIST_DRIFT_MASK);
            task->state++;
        }
        // Gather re-aims on a random quarter of ticks, rather than periodically.
        if (work->index != ACROPOLIS_PATIO_MIST_DRIFT) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if (((gRandomLcgState >> 16) & ACROPOLIS_PATIO_MIST_AIM_SAMPLE_MASK) == 0) {
                anchors       = _gAcropolisPatioLightAnchors;
                velocity      = &work->move;
                work->move.vx = (u16)anchors[task->spawnArg1.value].vx -
                                (u16)coord->coord.t[0];
                work->move.vy = (u16)anchors[task->spawnArg1.value].vy -
                                (u16)coord->coord.t[1];
                work->move.vz = (u16)anchors[task->spawnArg1.value].vz -
                                (u16)coord->coord.t[2];
                VectorNormalSS(velocity, velocity);
                gte_lddp(ACROPOLIS_PATIO_MIST_GATHER_WEIGHT);
                gte_ldsv(velocity);
                gte_gpf12();
                gte_stsv(velocity);
            }
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->move.vx  -= ((gRandomLcgState >> 16) & ACROPOLIS_PATIO_MIST_JITTER_MASK) - ACROPOLIS_PATIO_MIST_JITTER_CENTRE;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->move.vy  -= ((gRandomLcgState >> 16) & ACROPOLIS_PATIO_MIST_JITTER_MASK) - ACROPOLIS_PATIO_MIST_JITTER_CENTRE;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->move.vz  -= ((gRandomLcgState >> 16) & ACROPOLIS_PATIO_MIST_JITTER_MASK) - ACROPOLIS_PATIO_MIST_JITTER_CENTRE;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if ((u16)((gRandomLcgState >> 16) % ACROPOLIS_PATIO_MIST_DRIFT_CHANCE) == 0) {
                work->index = ACROPOLIS_PATIO_MIST_DRIFT;
            }
        } else {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->move.vx   = ACROPOLIS_PATIO_MIST_DRIFT_CENTRE - ((gRandomLcgState >> 16) & ACROPOLIS_PATIO_MIST_DRIFT_MASK);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->move.vy   = ACROPOLIS_PATIO_MIST_DRIFT_CENTRE - ((gRandomLcgState >> 16) & ACROPOLIS_PATIO_MIST_DRIFT_MASK);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->move.vz   = ACROPOLIS_PATIO_MIST_DRIFT_CENTRE - ((gRandomLcgState >> 16) & ACROPOLIS_PATIO_MIST_DRIFT_MASK);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if ((u16)((gRandomLcgState >> 16) % ACROPOLIS_PATIO_MIST_GATHER_CHANCE) == 0) {
                work->index = ACROPOLIS_PATIO_MIST_GATHER;
            }
        }
        coord->coord.t[0]  += work->move.vx;
        coord->coord.t[1]  += work->move.vy;
        coord->coord.t[2]  += work->move.vz;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        _acropolisPatioDrawLightMote(coord, tileScratch);
        SCRATCH_STACK_RELEASE_BLOCK(EffectPointTileScratch);
    }
}
