#include "rooms/acropolis_fire_escape.h"
#include "../../stages/akropolis/event_flags.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "actors/task_tables.h"

#include "gameplay/direction_input.h"
#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/area_transitions.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/player_actor.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effects.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/random.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/stage_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/text.h"
#include "main/tmd_types.h"
#include "main/ui.h"
#include "main/ui_types.h"

#include "mapui/map_akropolis.h"

#include "rooms/akropolis_square.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/room/room_cutscene.h"
#include "../../shared/effects/glow_draw.h"

#include "assets/rooms/akropolis/fire_escape/declarations.inc"

static void _roomCutsceneTask(Task* task);

static void _roomCutsceneSoundTask(Task* task);

/// The save's `companionType` byte under a symbol of its own; the cutscene's
/// end reads it through this name rather than through `gMcSaveData`.

/// View saved when the cutscene starts and restored when it ends.

/// Object whose `field_4A` bit 0x40 the message task clears each frame while
/// the slot-4 task does not answer message 0x7D6.

/// Last requested fire-escape ambience level, in whole percent (0, 15, 30 or 100).
///
/// Initialized to silence when the ambience task starts. Updated even if the
/// sound queue rejects a request, so equal levels suppress duplicate requests.
static s32 _gAcropolisFireEscapeRequestedAmbiencePercent;

/// The cutscene's sound task, killed when the scene is skipped.
extern Task* gRoomCutsceneSoundTask;

/// Persistent playback request for the repeat fire-escape scene in view 9.
///
/// A scene command fills all fields before spawning the bodyless runner.
/// The runner borrows this singleton through completion; keep the overlay
/// loaded and do not overwrite the request while that scene is live.
RoomCutsceneRec _gAcropolisFireEscapeRepeatScene;

#include "../../shared/room/telephone.h"

/// Key-item request handled by this room; the payload is an item ID.
enum { ACROPOLIS_FIRE_ESCAPE_MESSAGE_USE_KEY_ITEM = 0x13F1 };

/// Initializes a glow quad with RGB at vertex 2 and a black rim elsewhere.
///
/// `quad` must be a stable, side-effect-free pointer to a writable POLY_G4;
/// SDK setters evaluate it repeatedly. Each colour expression is evaluated
/// once and narrows to a byte on assignment. This void expression sequences
/// the header and rim stores before reading the centre colour. Geometry and
/// queueing remain the caller's responsibility; no identifiers are captured.
#define ACROPOLIS_FIRE_ESCAPE_INIT_GLOW_QUAD(quad, red, green, blue) \
    ((void)(setPolyG4((quad)),                                       \
            setRGB0((quad), 0, 0, 0),                                \
            setRGB1((quad), 0, 0, 0),                                \
            setRGB2((quad), (red), (green), (blue)),                 \
            setRGB3((quad), 0, 0, 0)))

static void _acropolisFireEscapeInitializeRoomTask(Task* task);
static void _acropolisFireEscapeDisableAbsentActorInteraction(Task* unusedTask);

static s32 _acropolisFireEscapeHandleRoomCommand(Task* unusedTask, s32 messageId, s32 command, s32 unusedArg);
static s32 _acropolisFireEscapeResolveRoomTransition(Task* unusedTask, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32 _acropolisFireEscapeRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedArg);
static s32 _acropolisFireEscapeIgnoreSoundMessage(Task* task, s32 messageId, s32 soundCommand, s32 unusedArg);

static void _acropolisFireEscapeAmbienceTask(Task* task);

#include "../../shared/room/telephone_data.inc.c"

/// This carrier's bodyless scene runner and timed sound task, followed by END.
///
/// Entry SCENE borrows a RoomCutsceneRec in spawnArg2 and takes the follow-up
/// CAP command in spawnArg1; SOUND takes a packed sound request in spawnArg2.
/// Both run at priority 32. Keep callback code and the scene record loaded
/// until completion; spawning copies the recipe and retains no table pointer.
static TaskDesc _gRoomCutsceneTaskDescs[ROOM_CUTSCENE_TASK_DESCRIPTOR_COUNT] = {
    { { { TASK_BODY_NONE, ROOM_CUTSCENE_TASK_PRIORITY } }, _roomCutsceneTask, { .value = 0 } },
    { { { TASK_BODY_NONE, ROOM_CUTSCENE_TASK_PRIORITY } }, _roomCutsceneSoundTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

/// Synchronous room handlers for transitions, key-item use, commands and sound.
///
/// Installed on the room task as a borrowed, read-only table. The final END
/// record makes unsupported IDs return zero. Transition payloads borrow a
/// request/reply pair; the other handlers receive integer words. Keep this
/// table and its callback code loaded while the room task can receive messages.
static TaskMessageEntry _gAcropolisFireEscapeRoomMessages[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _acropolisFireEscapeResolveRoomTransition },
    { ACROPOLIS_FIRE_ESCAPE_MESSAGE_USE_KEY_ITEM, _acropolisFireEscapeRejectKeyItemUse },
    { ROOM_MESSAGE_COMMAND, _acropolisFireEscapeHandleRoomCommand },
    { ROOM_MESSAGE_SOUND, _acropolisFireEscapeIgnoreSoundMessage },
    { TASK_MESSAGE_TABLE_END, NULL },
};

enum { ACROPOLIS_FIRE_ESCAPE_AMBIENCE_TASK_PRIORITY = 32 };

/// Bodyless view-dependent ambience recipe at index 0, followed by END.
///
/// Spawning copies the recipe synchronously; both payload words are ignored.
/// The task uses the room's requested-level singleton and must finish before
/// its room callback code is unloaded.
static TaskDesc _gAcropolisFireEscapeAmbienceTaskDescriptors[2] = {
    {
        .header   = { .fields = { TASK_BODY_NONE, ACROPOLIS_FIRE_ESCAPE_AMBIENCE_TASK_PRIORITY } },
        .callback = _acropolisFireEscapeAmbienceTask,
        .data     = { .value = 0 },
    },
    { .header = { .fields = { TASK_DESC_END, 0 } } },
};
#include "assets/rooms/akropolis/fire_escape/samples.inc"
#include "assets/rooms/akropolis/fire_escape/header.inc"
#include "assets/rooms/akropolis/fire_escape/geometry.inc"
#include "assets/rooms/akropolis/fire_escape/view_boundaries.inc"
#include "assets/rooms/akropolis/fire_escape/action_triggers.inc"
#include "assets/rooms/akropolis/fire_escape/occluders.inc"
#include "assets/rooms/akropolis/fire_escape/placements.inc"
#include "assets/rooms/akropolis/fire_escape/lights.inc"
#include "assets/rooms/akropolis/fire_escape/sprites.inc"
#include "assets/rooms/akropolis/fire_escape/cameras.inc"
#include "assets/rooms/akropolis/fire_escape/surfaces.inc"

enum { ACROPOLIS_FIRE_ESCAPE_BRIDGE_DOOR_MAP_FLAG = 490 };

/// Stair controls: bit 8 selects descent; parameter0 packs surface class above the step count.
enum {
    ACROPOLIS_FIRE_ESCAPE_STAIR_DESCEND            = 0x100,
    ACROPOLIS_FIRE_ESCAPE_STAIR_CLASS0_NINE_STEPS  = (0 << 4) | 9,
    ACROPOLIS_FIRE_ESCAPE_STAIR_CLASS1_TEN_STEPS   = (1 << 4) | 10,
    ACROPOLIS_FIRE_ESCAPE_STAIR_CLASS1_THREE_STEPS = (1 << 4) | 3,
};

/// Type-1 cue-2 bases used by this room's surface footstep records.
enum {
    ACROPOLIS_FIRE_ESCAPE_FOOTSTEP_DEFAULT_WALK = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x0D,
    ACROPOLIS_FIRE_ESCAPE_FOOTSTEP_DEFAULT_RUN  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x0F,
    ACROPOLIS_FIRE_ESCAPE_FOOTSTEP_CLASS1_WALK  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x15,
    ACROPOLIS_FIRE_ESCAPE_FOOTSTEP_CLASS1_RUN   = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x17,
    ACROPOLIS_FIRE_ESCAPE_FOOTSTEP_CLASS1_JUMP  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x19,
};

Task* gRoomCutsceneSoundTask;

/// Retained bytes following this room's telephone menu title terminator.
///
/// Two-byte string-literal binding for `telephone.inc.c`; unused by label drawing.
#define TELEPHONE_MENU_TITLE_TRAILING_BYTES "\x01\x00"
#include "../../shared/room/telephone.inc.c"
#undef TELEPHONE_MENU_TITLE_TRAILING_BYTES

void acropolisFireEscapeTelephoneMenuTask(Task* task)
{
    _telephoneMenuTask(task);
}

#include "../../shared/room/telephone_panels.inc.c"

#include "../../shared/room/room_cutscene_task.inc.c"

/// Room-task handlers for state 0 initialization, state 1 interaction checks and state 2 teardown.
///
/// Dispatch copies these three callback pointers by value before selecting a
/// state in 0..2, without a bounds check. Keep the room callbacks loaded through
/// the selected call; teardown may release the task.
static const TaskFuncTable3 _gAcropolisFireEscapeRoomStateHandlers = {
    .funcs = {
        _acropolisFireEscapeInitializeRoomTask,
        _acropolisFireEscapeDisableAbsentActorInteraction,
        taskKill,
    },
};

/// Starts the repeat fire-escape scene using the room's persistent playback record.
///
/// The record and scene resources must remain loaded and unchanged until the
/// runner finishes; its follow-up command advances the room dialogue.
static inline void _acropolisFireEscapeStartRepeatScene(void)
{
    enum {
        ACROPOLIS_FIRE_ESCAPE_SCENE_VIEW              = 9,
        ACROPOLIS_FIRE_ESCAPE_SCENE_CAP_SLOT          = 1,
        ACROPOLIS_FIRE_ESCAPE_SCENE_CAP_FILE          = 1,
        ACROPOLIS_FIRE_ESCAPE_SCENE_PLAY              = 0,
        ACROPOLIS_FIRE_ESCAPE_SCENE_TASK              = 0,
        ACROPOLIS_FIRE_ESCAPE_SCENE_FOLLOW_UP_COMMAND = 3,
        ACROPOLIS_FIRE_ESCAPE_SCENE_START_SOUND       = SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_FIRE_ESCAPE, 1),
        ACROPOLIS_FIRE_ESCAPE_SCENE_END_SOUND         = SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_FIRE_ESCAPE, 4),
        ACROPOLIS_FIRE_ESCAPE_SCENE_PLAYBACK_SOUND    = SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_FIRE_ESCAPE, 7),
        ACROPOLIS_FIRE_ESCAPE_SCENE_AFTER_SOUND       = SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_FIRE_ESCAPE, 8),
    };

    _gAcropolisFireEscapeRepeatScene.view            = ACROPOLIS_FIRE_ESCAPE_SCENE_VIEW;
    _gAcropolisFireEscapeRepeatScene.capSlot         = ACROPOLIS_FIRE_ESCAPE_SCENE_CAP_SLOT;
    _gAcropolisFireEscapeRepeatScene.capFile         = ACROPOLIS_FIRE_ESCAPE_SCENE_CAP_FILE;
    _gAcropolisFireEscapeRepeatScene.skipScene       = ACROPOLIS_FIRE_ESCAPE_SCENE_PLAY;
    _gAcropolisFireEscapeRepeatScene.startSound      = ACROPOLIS_FIRE_ESCAPE_SCENE_START_SOUND;
    _gAcropolisFireEscapeRepeatScene.endSound        = ACROPOLIS_FIRE_ESCAPE_SCENE_END_SOUND;
    _gAcropolisFireEscapeRepeatScene.sceneSound      = ACROPOLIS_FIRE_ESCAPE_SCENE_PLAYBACK_SOUND;
    _gAcropolisFireEscapeRepeatScene.afterSceneSound = ACROPOLIS_FIRE_ESCAPE_SCENE_AFTER_SOUND;
    taskSpawnFromTable(_gRoomCutsceneTaskDescs, ACROPOLIS_FIRE_ESCAPE_SCENE_TASK, ACROPOLIS_FIRE_ESCAPE_SCENE_FOLLOW_UP_COMMAND, &_gAcropolisFireEscapeRepeatScene);
}

/// Handles the fire escape's scene, story-progress and placed-actor dialogue commands.
///
/// `ROOM_MESSAGE_COMMAND` supplies integer command 4 for the first/repeat scene,
/// 3 for dialogue progress 6 and objective 7, or 1 for actor-dependent dialogue.
/// Other commands do nothing. Requires loaded room/CAP resources and live save
/// state; repeated scenes borrow the persistent record through completion.
/// Receiver, message ID and second payload are ignored; always returns zero.
static s32 _acropolisFireEscapeHandleRoomCommand(Task* unusedTask, s32 messageId, s32 command, s32 unusedArg)
{
    enum {
        ACROPOLIS_FIRE_ESCAPE_COMMAND_SCENE             = 4,
        ACROPOLIS_FIRE_ESCAPE_COMMAND_ADVANCE_DIALOGUE  = 3,
        ACROPOLIS_FIRE_ESCAPE_COMMAND_ACTOR_DIALOGUE    = 1,
        ACROPOLIS_FIRE_ESCAPE_SCENE_UNSEEN              = 0,
        ACROPOLIS_FIRE_ESCAPE_SCENE_SEEN                = 1,
        ACROPOLIS_FIRE_ESCAPE_FIRST_SCENE_CAP_COMMAND   = 11,
        ACROPOLIS_FIRE_ESCAPE_DIALOGUE_CAP_COMMAND      = 3,
        ACROPOLIS_FIRE_ESCAPE_DIALOGUE_OBJECTIVE        = 7,
        ACROPOLIS_FIRE_ESCAPE_DIALOGUE_ACTOR_PLACEMENT  = 0,
        ACROPOLIS_FIRE_ESCAPE_ACTOR_ABSENT_CAP_COMMAND  = 1,
        ACROPOLIS_FIRE_ESCAPE_ACTOR_PRESENT_CAP_COMMAND = 9,
    };
    Task* actorTask;
    s32   capCommand;
    s32   actorPresent;

    if (command == ACROPOLIS_FIRE_ESCAPE_COMMAND_SCENE) {
        if (gameFlagGetNibble(GAME_FLAG_FIRE_ESCAPE_FIRST_SCENE) == ACROPOLIS_FIRE_ESCAPE_SCENE_UNSEEN) {
            gameFlagSetNibble(GAME_FLAG_FIRE_ESCAPE_FIRST_SCENE, ACROPOLIS_FIRE_ESCAPE_SCENE_SEEN);
            capRunCommandWithTransition(ACROPOLIS_FIRE_ESCAPE_FIRST_SCENE_CAP_COMMAND);
            return 0;
        }
        _acropolisFireEscapeStartRepeatScene();
    }
    if (command == ACROPOLIS_FIRE_ESCAPE_COMMAND_ADVANCE_DIALOGUE) {
        // Advancing dialogue rearms its follow-up without lowering later progress.
        if (gameFlagGetNibble(EVENT_FLAG_STORY_DIALOGUE_INDEX) < AKROPOLIS_STORY_DIALOGUE_HELICOPTER) {
            gameFlagSetNibble(EVENT_FLAG_TELEPHONE_DIALOGUE_COUNTER, EVENT_FLAG_TELEPHONE_DIALOGUE_UNHEARD);
            gameFlagSetNibble(EVENT_FLAG_STORY_DIALOGUE_INDEX, AKROPOLIS_STORY_DIALOGUE_HELICOPTER);
        }
        capSpawnEventIfIdle(ACROPOLIS_FIRE_ESCAPE_DIALOGUE_CAP_COMMAND, CAP_EVENT_PAUSE_ACTORS);
        gameFlagSetPackedByte(GAME_FLAG_CURRENT_OBJECTIVE, ACROPOLIS_FIRE_ESCAPE_DIALOGUE_OBJECTIVE);
    }
    if (command == ACROPOLIS_FIRE_ESCAPE_COMMAND_ACTOR_DIALOGUE) {
        actorTask  = sceneFindPlacedActor(ACROPOLIS_FIRE_ESCAPE_DIALOGUE_ACTOR_PLACEMENT);
        capCommand = ACROPOLIS_FIRE_ESCAPE_ACTOR_ABSENT_CAP_COMMAND;
        if (actorTask != NULL) {
            // Retain the original zero output payload; this actor also writes through it.
            actorPresent = taskMessageDispatch(actorTask, ACTOR_MESSAGE_IS_PRESENT, 0, 0);
            capCommand   = ACROPOLIS_FIRE_ESCAPE_ACTOR_PRESENT_CAP_COMMAND;
            if (actorPresent == 0) {
                capCommand = ACROPOLIS_FIRE_ESCAPE_ACTOR_ABSENT_CAP_COMMAND;
            }
        }
        capSpawnEventIfIdle(capCommand, CAP_EVENT_PAUSE_ACTORS);
    }
    return 0;
}

/// Requests view-dependent ambience and the view-8 story music transition.
///
/// State 0 resets the room's requested-level latch; state 1 updates it each
/// tick. Logical views 2/3, 4/5 and 8 request levels 30%, 15% and 100%; others
/// request silence. Changes start, mix or fade the sound over 30 audio updates.
/// The latch records requests even if the sound queue rejects them. In view 8,
/// music event 5 becomes 7 once, queuing ordinary area music and clearing the
/// session's flow options. Requires live session/save and loaded room sound data.
static void _acropolisFireEscapeAmbienceTask(Task* task)
{
    enum {
        ACROPOLIS_FIRE_ESCAPE_AMBIENCE_INIT            = 0,
        ACROPOLIS_FIRE_ESCAPE_AMBIENCE_UPDATE          = 1,
        ACROPOLIS_FIRE_ESCAPE_AMBIENCE_SILENT          = 0,
        ACROPOLIS_FIRE_ESCAPE_AMBIENCE_FULL_PERCENT    = 100,
        ACROPOLIS_FIRE_ESCAPE_AMBIENCE_VIEWS23_PERCENT = 30,
        ACROPOLIS_FIRE_ESCAPE_AMBIENCE_VIEWS45_PERCENT = 15,
        ACROPOLIS_FIRE_ESCAPE_AMBIENCE_ATTENUATION_MAX = 127,
        ACROPOLIS_FIRE_ESCAPE_AMBIENCE_FADE_TICKS      = 30,
        ACROPOLIS_FIRE_ESCAPE_ENTRY_MUSIC_EVENT        = 5,
        ACROPOLIS_FIRE_ESCAPE_VIEW8_MUSIC_EVENT        = 7,
    };
    s32 levelPercent;
    s32 previousLevelPercent;

    switch (task->state) {
        case ACROPOLIS_FIRE_ESCAPE_AMBIENCE_INIT:
            _gAcropolisFireEscapeRequestedAmbiencePercent = ACROPOLIS_FIRE_ESCAPE_AMBIENCE_SILENT;
            task->state                                   = task->state + 1;
            return;
        case ACROPOLIS_FIRE_ESCAPE_AMBIENCE_UPDATE:
            break;
        default:
            return;
    }

    switch (gGameSession->location.view) {
        case 8:
            levelPercent = ACROPOLIS_FIRE_ESCAPE_AMBIENCE_FULL_PERCENT;
            // Commit the music column before queuing its asynchronous loader.
            if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent == ACROPOLIS_FIRE_ESCAPE_ENTRY_MUSIC_EVENT) {
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = ACROPOLIS_FIRE_ESCAPE_VIEW8_MUSIC_EVENT;
                gStageMusicParams.fadeOutTicks                      = 1;
                gStageMusicParams.field_2                           = 1;
                taskSpawnFromTable(&gStageMusicTaskDesc, 0, STAGE_MUSIC_REQUEST_ORDINARY, 0);
                gGameSession->flowFlags = 0;
            }
            break;
        case 2:
        case 3:
            levelPercent = ACROPOLIS_FIRE_ESCAPE_AMBIENCE_VIEWS23_PERCENT;
            break;
        case 4:
        case 5:
            levelPercent = ACROPOLIS_FIRE_ESCAPE_AMBIENCE_VIEWS45_PERCENT;
            break;
        default:
            levelPercent = ACROPOLIS_FIRE_ESCAPE_AMBIENCE_SILENT;
            break;
    }

    previousLevelPercent = _gAcropolisFireEscapeRequestedAmbiencePercent;
    if (levelPercent == previousLevelPercent) {
        return;
    }
    if (previousLevelPercent == ACROPOLIS_FIRE_ESCAPE_AMBIENCE_SILENT) {
        sndEvtRequestScriptStart(SOUND_ACROPOLIS_FIRE_ESCAPE_AMBIENCE, 0, (s8)(((ACROPOLIS_FIRE_ESCAPE_AMBIENCE_FULL_PERCENT - levelPercent) * ACROPOLIS_FIRE_ESCAPE_AMBIENCE_ATTENUATION_MAX) / ACROPOLIS_FIRE_ESCAPE_AMBIENCE_FULL_PERCENT));
    } else if (levelPercent == ACROPOLIS_FIRE_ESCAPE_AMBIENCE_SILENT) {
        sndEvtRequestScriptStop(SOUND_ACROPOLIS_FIRE_ESCAPE_AMBIENCE, ACROPOLIS_FIRE_ESCAPE_AMBIENCE_FADE_TICKS);
    } else {
        sndEvtRequestScriptMix(SOUND_ACROPOLIS_FIRE_ESCAPE_AMBIENCE, 0, (s8)(((ACROPOLIS_FIRE_ESCAPE_AMBIENCE_FULL_PERCENT - levelPercent) * ACROPOLIS_FIRE_ESCAPE_AMBIENCE_ATTENUATION_MAX) / ACROPOLIS_FIRE_ESCAPE_AMBIENCE_FULL_PERCENT));
    }
    _gAcropolisFireEscapeRequestedAmbiencePercent = levelPercent;
}

#include "../../shared/room/room_cutscene_sound_task.inc.c"

/// Resolves departure from the fire escape and fades its ambience on execution.
///
/// Borrows complete request/reply records, which may be the same object.
/// Queries only copy the request. Execution fades ambience over 15 audio
/// updates and selects bridge room 2 at progress 3, room 1 otherwise.
/// Always returns 1 to permit the resolved transition; receiver and ID are unused.
static s32 _acropolisFireEscapeResolveRoomTransition(Task* unusedTask, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    enum {
        ACROPOLIS_FIRE_ESCAPE_DEPARTURE_FADE_TICKS    = 15,
        ACROPOLIS_FIRE_ESCAPE_BRIDGE_PROGRESS_CHANGED = 3,
        ACROPOLIS_FIRE_ESCAPE_BRIDGE_ROOM_INITIAL     = 1,
        ACROPOLIS_FIRE_ESCAPE_BRIDGE_ROOM_CHANGED     = 2,
        ACROPOLIS_FIRE_ESCAPE_TRANSITION_ALLOWED      = 1
    };

    *reply = *request;
    if (request->queryOnly == ROOM_EVENT_EXECUTE) {
        sndEvtRequestScriptStop(SOUND_ACROPOLIS_FIRE_ESCAPE_AMBIENCE, ACROPOLIS_FIRE_ESCAPE_DEPARTURE_FADE_TICKS);
    }
    if (request->areaId == GAME_AREA_AKROPOLIS_BRIDGE && request->queryOnly == ROOM_EVENT_EXECUTE) {
        if (gameFlagGetNibble(GAME_FLAG_AKROPOLIS_BRIDGE_PROGRESS) == ACROPOLIS_FIRE_ESCAPE_BRIDGE_PROGRESS_CHANGED) {
            reply->roomState = ACROPOLIS_FIRE_ESCAPE_BRIDGE_ROOM_CHANGED;
        } else {
            reply->roomState = ACROPOLIS_FIRE_ESCAPE_BRIDGE_ROOM_INITIAL;
        }
    }
    return ACROPOLIS_FIRE_ESCAPE_TRANSITION_ALLOWED;
}

/// Refuses every key-item use in this room, returning zero without consuming it.
///
/// The receiver, message ID, item ID and unused second argument are ignored.
static s32 _acropolisFireEscapeRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedArg)
{
    enum { ACROPOLIS_FIRE_ESCAPE_KEY_ITEM_USE_REFUSED = 0 };

    return ACROPOLIS_FIRE_ESCAPE_KEY_ITEM_USE_REFUSED;
}

/// Ignores room sound commands and returns zero without starting a sound.
static s32 _acropolisFireEscapeIgnoreSoundMessage(Task* task, s32 messageId, s32 soundCommand, s32 unusedArg)
{
    return 0;
}

/// Publishes the room receiver and starts its view-dependent ambience.
///
/// State 0 requires a live bodyless task and loaded room/gameplay resources.
/// Saved scene event 5 selects load-only mode for area music. Advances
/// to the interaction-check state even if the ambience task fails to spawn.
static void _acropolisFireEscapeInitializeRoomTask(Task* task)
{
    enum {
        ACROPOLIS_FIRE_ESCAPE_INITIAL_AMBIENCE_TASK = 0,
        ACROPOLIS_FIRE_ESCAPE_ENTRY_MUSIC_EVENT     = 5,
    };

    task->msgTable = _gAcropolisFireEscapeRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    taskSpawnFromTable(_gAcropolisFireEscapeAmbienceTaskDescriptors, ACROPOLIS_FIRE_ESCAPE_INITIAL_AMBIENCE_TASK, 0, 0);
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent == ACROPOLIS_FIRE_ESCAPE_ENTRY_MUSIC_EVENT) {
        gGameSession->flowFlags = GAME_SESSION_FLOW_LOAD_AREA_MUSIC_ONLY;
    }
    task->state = task->state + 1;
}

/// Disables the placed actor's interaction when the actor is absent.
///
/// Idle room-state callback; the receiver is unused. Queries placement 0 with
/// `ACTOR_MESSAGE_IS_PRESENT` and clears the trigger's ENABLED bit on a zero
/// reply or missing actor. Never re-enables it. The presence query retains the
/// original zero second payload, although this room's actor writes through it.
static void _acropolisFireEscapeDisableAbsentActorInteraction(Task* unusedTask)
{
    enum { ACROPOLIS_FIRE_ESCAPE_ACTOR_INTERACTION_TRIGGER = 5 };
    Task* actorTask;

    actorTask = sceneFindPlacedActor(0);
    if (actorTask == NULL || taskMessageDispatch(actorTask, ACTOR_MESSAGE_IS_PRESENT, 0, 0) == 0) {
        WorldCollisionTrigger* interaction = &_gAcropolisFireEscapeActionTriggers[ACROPOLIS_FIRE_ESCAPE_ACTOR_INTERACTION_TRIGGER];
        interaction->flags                &= (0xFF ^ WORLD_COLLISION_TRIGGER_ENABLED);
    }
}

void acropolisFireEscapeRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers = _gAcropolisFireEscapeRoomStateHandlers;
    stateHandlers.funcs[task->state](task);
}

/// Spawns one view-selected flare at an offset from the light emitter.
///
/// XYZ are signed-halfword units in `emitterCoord`'s local frame; `options`
/// packs pulse rate, radius and the fire-escape flare flags. Requires live
/// emitter work and coordinates. Spawning snapshots XYZ before the reusable
/// vector changes; retained effect work also borrows its address, but this
/// one-frame drawer never reads it. Allocation failure is left to `effectSpawn`.
static inline void _acropolisFireEscapeSpawnFlare(EffectWork* emitterWork, GfxCoord* emitterCoord,
                                                  s16 offsetX, s16 offsetY, s16 offsetZ, s32 options)
{
    emitterWork->move.vx = offsetX;
    emitterWork->move.vy = offsetY;
    emitterWork->move.vz = offsetZ;
    effectSpawn(EFFECT_ACROPOLIS_FIRE_ESCAPE_FLARE, emitterCoord, options, &emitterWork->move);
}

void acropolisFireEscapeLightEmitterTask(Task* task)
{
    enum {
        ACROPOLIS_FIRE_ESCAPE_LIGHT_EMITTER_INITIALIZE = 0,
        ACROPOLIS_FIRE_ESCAPE_LIGHT_EMITTER_EMIT       = 1,
        // Radius byte 32; the lamp does not read the retained upper bits.
        ACROPOLIS_FIRE_ESCAPE_FLICKER_SPAWN_OPTIONS = 0x42000,
        ACROPOLIS_FIRE_ESCAPE_RED_DIAMOND_VIEW      = 3,
        ACROPOLIS_FIRE_ESCAPE_RED_RADIAL_VIEW       = 8,
        ACROPOLIS_FIRE_ESCAPE_CYAN_DIAMOND_VIEW     = 6,
        ACROPOLIS_FIRE_ESCAPE_CYAN_RADIAL_VIEW      = 9,
        ACROPOLIS_FIRE_ESCAPE_RED_PULSE_RATE        = 14,
        ACROPOLIS_FIRE_ESCAPE_CYAN_PULSE_RATE       = 8,
        ACROPOLIS_FIRE_ESCAPE_RED_DIAMOND_RADIUS    = 6,
        ACROPOLIS_FIRE_ESCAPE_RED_RADIAL_RADIUS     = 3,
        ACROPOLIS_FIRE_ESCAPE_CYAN_DIAMOND_RADIUS   = 4,
        ACROPOLIS_FIRE_ESCAPE_CYAN_RADIAL_RADIUS    = 2,
    };
    EffectWork* emitterWork;
    GfxCoord*   emitterCoord;

    emitterWork  = task->spawnArg2.pointer;
    emitterCoord = task->extra.coordBody->coord;
    switch (task->state) {
        case ACROPOLIS_FIRE_ESCAPE_LIGHT_EMITTER_INITIALIZE:
            // The persistent lamp snapshots this local placement once.
            emitterWork->move.vx = 2904;
            emitterWork->move.vy = -2082;
            emitterWork->move.vz = -229;
            effectSpawn(EFFECT_ACROPOLIS_FIRE_ESCAPE_FLICKER_LIGHT, emitterCoord, ACROPOLIS_FIRE_ESCAPE_FLICKER_SPAWN_OPTIONS, &emitterWork->move);
            task->state = task->state + 1;
            break;
        case ACROPOLIS_FIRE_ESCAPE_LIGHT_EMITTER_EMIT:
            // Actor pause/hide modes still emit; a cancellation update suppresses emission.
            if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
                if (gGameSession->location.view == ACROPOLIS_FIRE_ESCAPE_RED_DIAMOND_VIEW) {
                    _acropolisFireEscapeSpawnFlare(emitterWork, emitterCoord, 1167, -913, 1670,
                                                   (ACROPOLIS_FIRE_ESCAPE_RED_DIAMOND_RADIUS << ACROPOLIS_FIRE_ESCAPE_FLARE_RADIUS_SHIFT) | ACROPOLIS_FIRE_ESCAPE_RED_PULSE_RATE);
                }
                if (gGameSession->location.view == ACROPOLIS_FIRE_ESCAPE_RED_RADIAL_VIEW) {
                    _acropolisFireEscapeSpawnFlare(emitterWork, emitterCoord, 1167, -913, 1670,
                                                   ACROPOLIS_FIRE_ESCAPE_FLARE_RADIAL | (ACROPOLIS_FIRE_ESCAPE_RED_RADIAL_RADIUS << ACROPOLIS_FIRE_ESCAPE_FLARE_RADIUS_SHIFT) | ACROPOLIS_FIRE_ESCAPE_RED_PULSE_RATE);
                }
                if (gGameSession->location.view == ACROPOLIS_FIRE_ESCAPE_CYAN_DIAMOND_VIEW) {
                    _acropolisFireEscapeSpawnFlare(emitterWork, emitterCoord, -3103, -3344, 2272,
                                                   ACROPOLIS_FIRE_ESCAPE_FLARE_CYAN | (ACROPOLIS_FIRE_ESCAPE_CYAN_DIAMOND_RADIUS << ACROPOLIS_FIRE_ESCAPE_FLARE_RADIUS_SHIFT) | ACROPOLIS_FIRE_ESCAPE_CYAN_PULSE_RATE);
                }
                if (gGameSession->location.view == ACROPOLIS_FIRE_ESCAPE_CYAN_RADIAL_VIEW) {
                    _acropolisFireEscapeSpawnFlare(emitterWork, emitterCoord, -3103, -3344, 2272,
                                                   ACROPOLIS_FIRE_ESCAPE_FLARE_RADIAL | ACROPOLIS_FIRE_ESCAPE_FLARE_CYAN | (ACROPOLIS_FIRE_ESCAPE_CYAN_RADIAL_RADIUS << ACROPOLIS_FIRE_ESCAPE_FLARE_RADIUS_SHIFT) | ACROPOLIS_FIRE_ESCAPE_CYAN_PULSE_RATE);
                }
            }
            break;
    }
}

/// Queues an initialized fire-escape glow quad with additive blending.
///
/// `quad` is a word-aligned frame-arena packet; its DMA link and
/// semitransparency bit are updated. `sortingDepth` points to a stable signed
/// camera-Z/4 word, after any bias, already clipped to at least `GLOW_MIN_DEPTH`.
/// Unsigned depth scaling by `gDisplayState.otDepthShift` (0..3) selects a
/// wrapped tag in 0..1023. The current table must contain all 1024 depth tags,
/// and the frame arena must have `sizeof(DR_TPAGE)` bytes available.
/// The depth pointer is not retained. The quad and the added draw-mode packet
/// must remain live until GPU completion; the draw mode persists until replaced.
static inline void _acropolisFireEscapeQueueGlow(POLY_G4* quad, const s32* sortingDepth)
{
    enum { ACROPOLIS_FIRE_ESCAPE_GLOW_DEPTH_TO_TAG_INDEX_SHIFT = 4 };

    addPrim(&gGpuCurrentOt[((u32)*sortingDepth << gDisplayState.otDepthShift) >> ACROPOLIS_FIRE_ESCAPE_GLOW_DEPTH_TO_TAG_INDEX_SHIFT & (GPU_ORDERING_TABLE_DEPTH_BYTE_MASK / sizeof(*gGpuCurrentOt))],
            quad);
    // Prepending the draw mode after the quad makes the GPU apply it first.
    gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, *sortingDepth);
}

void acropolisFireEscapeFlickerLightTask(Task* task)
{
    enum {
        ACROPOLIS_FIRE_ESCAPE_FLICKER_VIEW_MASK           = (1 << (2 - 1)) | (1 << (3 - 1)) | (1 << (7 - 1)),
        ACROPOLIS_FIRE_ESCAPE_FLICKER_DEPTH_BIAS          = 32,
        ACROPOLIS_FIRE_ESCAPE_FLICKER_MODE_FRAME_MASK     = 31,
        ACROPOLIS_FIRE_ESCAPE_FLICKER_MODE_MASK           = 3,
        ACROPOLIS_FIRE_ESCAPE_FLICKER_RADIUS_SHIFT        = 8,
        ACROPOLIS_FIRE_ESCAPE_FLICKER_RADIUS_MASK         = 0xFF,
        ACROPOLIS_FIRE_ESCAPE_FLICKER_DIM_MAX             = 16,
        ACROPOLIS_FIRE_ESCAPE_FLICKER_SOUND_MIN           = 32,
        ACROPOLIS_FIRE_ESCAPE_FLICKER_BRIGHTNESS_MASK     = 0x30,
        ACROPOLIS_FIRE_ESCAPE_FLICKER_LOW_BRIGHTNESS_MASK = 0x10,
        ACROPOLIS_FIRE_ESCAPE_FLICKER_RANDOM              = 0,
        ACROPOLIS_FIRE_ESCAPE_FLICKER_RANDOM_ODD_FRAMES   = 1,
        ACROPOLIS_FIRE_ESCAPE_FLICKER_STEADY              = 2,
        ACROPOLIS_FIRE_ESCAPE_FLICKER_DIM_RANDOM          = 3,
        ACROPOLIS_FIRE_ESCAPE_FLICKER_RADIUS_SCALE        = 0x600,
        ACROPOLIS_FIRE_ESCAPE_FLICKER_INNER_RADIUS_SCALE  = 0xC0,
        ACROPOLIS_FIRE_ESCAPE_GLOW_CIRCLE_STEPS           = 16,
    };

    EffectWork*           effectWork;
    GfxCoord*             coord;
    RoomGlowRadiiScratch* projection;
    POLY_G4*              quad;
    s32                   wasDim;
    s32                   cornerIndex;
    u16                   brightness;

    effectWork = task->spawnArg2.pointer;
    coord      = task->extra.coordBody->coord;
    wasDim     = 0;
    // The installed room has 1-based view IDs; only views 2, 3 and 7 show the lamp.
    if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN && ((ACROPOLIS_FIRE_ESCAPE_FLICKER_VIEW_MASK >> (gGameSession->location.view - 1)) & 1)) {
        // Project the lamp and pull its sorting depth forward before sizing it.
        actorRenderComposeCoord(coord);
        projection              = SCRATCH_STACK_RESERVE_BLOCK(RoomGlowRadiiScratch);
        projection->worldPos.vx = coord->workm.t[0];
        projection->worldPos.vy = coord->workm.t[1];
        projection->worldPos.vz = coord->workm.t[2];
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&projection->worldPos);
        gte_rtps();
        gte_stsxy(&projection->screenPos);
        gte_stszotz(&projection->otz);
        projection->otz -= ACROPOLIS_FIRE_ESCAPE_FLICKER_DEPTH_BIAS;
        if (projection->otz >= GLOW_MIN_DEPTH) {
            if (!(gDisplayState.animFrame & ACROPOLIS_FIRE_ESCAPE_FLICKER_MODE_FRAME_MASK)) {
                gRandomLcgState   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                effectWork->index = (gRandomLcgState >> 16) & ACROPOLIS_FIRE_ESCAPE_FLICKER_MODE_MASK;
            }
            // Choose flicker brightness, then sound only a dim-to-bright transition.
            brightness = effectWork->scale;
            if (effectWork->scale <= ACROPOLIS_FIRE_ESCAPE_FLICKER_DIM_MAX) {
                wasDim = 1;
            }
            switch (effectWork->index) {
                case ACROPOLIS_FIRE_ESCAPE_FLICKER_RANDOM:
                    gRandomLcgState   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    effectWork->scale = (gRandomLcgState >> 16) & ACROPOLIS_FIRE_ESCAPE_FLICKER_BRIGHTNESS_MASK;
                    break;
                case ACROPOLIS_FIRE_ESCAPE_FLICKER_RANDOM_ODD_FRAMES:
                    if (gDisplayState.animFrame & 1) {
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        brightness      = (gRandomLcgState >> 16) & ACROPOLIS_FIRE_ESCAPE_FLICKER_BRIGHTNESS_MASK;
                    }
                    effectWork->scale = brightness;
                    break;
                case ACROPOLIS_FIRE_ESCAPE_FLICKER_STEADY:
                    effectWork->scale = ACROPOLIS_FIRE_ESCAPE_FLICKER_BRIGHTNESS_MASK;
                    break;
                case ACROPOLIS_FIRE_ESCAPE_FLICKER_DIM_RANDOM:
                    gRandomLcgState   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    effectWork->scale = (gRandomLcgState >> 16) & ACROPOLIS_FIRE_ESCAPE_FLICKER_LOW_BRIGHTNESS_MASK;
                    break;
            }
            if (wasDim && effectWork->scale >= ACROPOLIS_FIRE_ESCAPE_FLICKER_SOUND_MIN) {
                sndEvtRequestScriptStart(SOUND_ACROPOLIS_FIRE_ESCAPE_LIGHT_FLICKER, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            }
            projection->outerRadius = (((task->spawnArg1.value >> ACROPOLIS_FIRE_ESCAPE_FLICKER_RADIUS_SHIFT) & ACROPOLIS_FIRE_ESCAPE_FLICKER_RADIUS_MASK) * ACROPOLIS_FIRE_ESCAPE_FLICKER_RADIUS_SCALE) / projection->otz;
            projection->innerRadius = (((task->spawnArg1.value >> ACROPOLIS_FIRE_ESCAPE_FLICKER_RADIUS_SHIFT) & ACROPOLIS_FIRE_ESCAPE_FLICKER_RADIUS_MASK) * ACROPOLIS_FIRE_ESCAPE_FLICKER_INNER_RADIUS_SCALE) / projection->otz;
            // Eight wedges in each of three concentric intensity layers.
            for (cornerIndex = 0; cornerIndex < ACROPOLIS_FIRE_ESCAPE_GLOW_CIRCLE_STEPS; cornerIndex += 2) {
                quad           = gGpuPrimCursor;
                gGpuPrimCursor = quad + 1;
                ACROPOLIS_FIRE_ESCAPE_INIT_GLOW_QUAD(quad, effectWork->scale >> 1, effectWork->scale >> 1, effectWork->scale >> 1);
                quad->x0 = projection->screenPos.vx + ((projection->outerRadius * _gAcropolisFireEscapeGlowCircleSamples[cornerIndex + 4]) >> GLOW_TRIG_SHIFT);
                quad->y0 = projection->screenPos.vy + ((projection->outerRadius * _gAcropolisFireEscapeGlowCircleSamples[cornerIndex]) >> GLOW_TRIG_SHIFT);
                quad->x1 = projection->screenPos.vx + ((projection->outerRadius * _gAcropolisFireEscapeGlowCircleSamples[cornerIndex + 5]) >> GLOW_TRIG_SHIFT);
                quad->y1 = projection->screenPos.vy + ((projection->outerRadius * _gAcropolisFireEscapeGlowCircleSamples[cornerIndex + 1]) >> GLOW_TRIG_SHIFT);
                quad->x2 = projection->screenPos.vx;
                quad->y2 = projection->screenPos.vy;
                quad->x3 = projection->screenPos.vx + ((projection->outerRadius * _gAcropolisFireEscapeGlowCircleSamples[cornerIndex + 6]) >> GLOW_TRIG_SHIFT);
                quad->y3 = projection->screenPos.vy + ((projection->outerRadius * _gAcropolisFireEscapeGlowCircleSamples[cornerIndex + 2]) >> GLOW_TRIG_SHIFT);
                _acropolisFireEscapeQueueGlow(quad, &projection->otz);

                quad           = gGpuPrimCursor;
                gGpuPrimCursor = quad + 1;
                ACROPOLIS_FIRE_ESCAPE_INIT_GLOW_QUAD(quad, effectWork->scale, effectWork->scale, effectWork->scale);
                quad->x0 = projection->screenPos.vx + ((projection->outerRadius * _gAcropolisFireEscapeGlowCircleSamples[cornerIndex + 4]) >> (GLOW_TRIG_SHIFT + 1));
                quad->y0 = projection->screenPos.vy + ((projection->outerRadius * _gAcropolisFireEscapeGlowCircleSamples[cornerIndex]) >> (GLOW_TRIG_SHIFT + 1));
                quad->x1 = projection->screenPos.vx + ((projection->outerRadius * _gAcropolisFireEscapeGlowCircleSamples[cornerIndex + 5]) >> (GLOW_TRIG_SHIFT + 1));
                quad->y1 = projection->screenPos.vy + ((projection->outerRadius * _gAcropolisFireEscapeGlowCircleSamples[cornerIndex + 1]) >> (GLOW_TRIG_SHIFT + 1));
                quad->x2 = projection->screenPos.vx;
                quad->y2 = projection->screenPos.vy;
                quad->x3 = projection->screenPos.vx + ((projection->outerRadius * _gAcropolisFireEscapeGlowCircleSamples[cornerIndex + 6]) >> (GLOW_TRIG_SHIFT + 1));
                quad->y3 = projection->screenPos.vy + ((projection->outerRadius * _gAcropolisFireEscapeGlowCircleSamples[cornerIndex + 2]) >> (GLOW_TRIG_SHIFT + 1));
                _acropolisFireEscapeQueueGlow(quad, &projection->otz);

                quad           = gGpuPrimCursor;
                gGpuPrimCursor = quad + 1;
                ACROPOLIS_FIRE_ESCAPE_INIT_GLOW_QUAD(quad, (u8)effectWork->scale * 4, (u8)effectWork->scale * 4, (u8)effectWork->scale * 4);
                quad->x0 = projection->screenPos.vx + ((projection->outerRadius * _gAcropolisFireEscapeGlowCircleSamples[cornerIndex + 4]) >> (GLOW_TRIG_SHIFT + 3));
                quad->y0 = projection->screenPos.vy + ((projection->outerRadius * _gAcropolisFireEscapeGlowCircleSamples[cornerIndex]) >> (GLOW_TRIG_SHIFT + 3));
                quad->x1 = projection->screenPos.vx + ((projection->outerRadius * _gAcropolisFireEscapeGlowCircleSamples[cornerIndex + 5]) >> (GLOW_TRIG_SHIFT + 3));
                quad->y1 = projection->screenPos.vy + ((projection->outerRadius * _gAcropolisFireEscapeGlowCircleSamples[cornerIndex + 1]) >> (GLOW_TRIG_SHIFT + 3));
                quad->x2 = projection->screenPos.vx;
                quad->y2 = projection->screenPos.vy;
                quad->x3 = projection->screenPos.vx + ((projection->outerRadius * _gAcropolisFireEscapeGlowCircleSamples[cornerIndex + 6]) >> (GLOW_TRIG_SHIFT + 3));
                quad->y3 = projection->screenPos.vy + ((projection->outerRadius * _gAcropolisFireEscapeGlowCircleSamples[cornerIndex + 2]) >> (GLOW_TRIG_SHIFT + 3));
                _acropolisFireEscapeQueueGlow(quad, &projection->otz);
            }
        }
        SCRATCH_STACK_RELEASE_BLOCK(RoomGlowRadiiScratch);
    }
}

void acropolisFireEscapeFlareTask(Task* task)
{
    enum {
        ACROPOLIS_FIRE_ESCAPE_FLARE_PULSE_FALLING_BIT    = 0x80,
        ACROPOLIS_FIRE_ESCAPE_FLARE_PULSE_LEVEL_MASK     = 0x7F,
        ACROPOLIS_FIRE_ESCAPE_FLARE_RADIAL_RADIUS_SHIFT  = 10,
        ACROPOLIS_FIRE_ESCAPE_FLARE_INNER_RADIUS_SHIFT   = 7,
        ACROPOLIS_FIRE_ESCAPE_FLARE_DIAMOND_RADIUS_SHIFT = 9,
        ACROPOLIS_FIRE_ESCAPE_GLOW_CIRCLE_STEPS          = 16,
    };

    RoomGlowRadiiScratch* projection;
    POLY_G4*              quad;
    LINE_G3*              streak;
    GfxCoord*             coord;
    EffectWork*           effectWork;
    s32                   partIndex;
    s32                   pulsePhase;
    s32                   pulseOrOptions; // Folded pulse level, then the packed spawn options
    s32                   radiusScale;
    s16                   intensity;
    s16                   cyan;
    s16                   halfIntensity;

    coord      = task->extra.coordBody->coord;
    effectWork = task->spawnArg2.pointer;
    // Each invocation projects and draws one frame before retiring its counted work.
    actorRenderComposeCoord(coord);
    projection              = SCRATCH_STACK_RESERVE_BLOCK(RoomGlowRadiiScratch);
    projection->worldPos.vx = (u16)coord->workm.t[0];
    projection->worldPos.vy = (u16)coord->workm.t[1];
    projection->worldPos.vz = (u16)coord->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&projection->worldPos);
    gte_rtps();
    gte_stsxy(&projection->screenPos);
    gte_stszotz(&projection->otz);
    if (projection->otz >= GLOW_MIN_DEPTH) {
        // Fold the low-byte phase into a 0..127 triangle wave and select red or cyan.
        pulsePhase  = gDisplayState.animFrame;
        pulsePhase *= task->spawnArg1.value & ACROPOLIS_FIRE_ESCAPE_FLARE_PULSE_RATE_MASK;
        cyan        = (task->spawnArg1.value >> ACROPOLIS_FIRE_ESCAPE_FLARE_CYAN_SHIFT) & 1;
        if (pulsePhase & ACROPOLIS_FIRE_ESCAPE_FLARE_PULSE_FALLING_BIT) {
            pulseOrOptions = ~pulsePhase & ACROPOLIS_FIRE_ESCAPE_FLARE_PULSE_LEVEL_MASK;
        } else {
            pulseOrOptions = pulsePhase & ACROPOLIS_FIRE_ESCAPE_FLARE_PULSE_LEVEL_MASK;
        }
        intensity      = pulseOrOptions * 2;
        pulseOrOptions = task->spawnArg1.value;
        if (pulseOrOptions < 0) {
            radiusScale             = (pulseOrOptions >> ACROPOLIS_FIRE_ESCAPE_FLARE_RADIUS_SHIFT) & ACROPOLIS_FIRE_ESCAPE_FLARE_RADIUS_MASK;
            projection->outerRadius = (radiusScale << ACROPOLIS_FIRE_ESCAPE_FLARE_RADIAL_RADIUS_SHIFT) / projection->otz;
            projection->innerRadius = (((task->spawnArg1.value >> ACROPOLIS_FIRE_ESCAPE_FLARE_RADIUS_SHIFT) & ACROPOLIS_FIRE_ESCAPE_FLARE_RADIUS_MASK) << ACROPOLIS_FIRE_ESCAPE_FLARE_INNER_RADIUS_SHIFT) / projection->otz;
            for (partIndex = 0; partIndex < ACROPOLIS_FIRE_ESCAPE_GLOW_CIRCLE_STEPS; partIndex += 2) {
                quad           = gGpuPrimCursor;
                gGpuPrimCursor = quad + 1;
                ACROPOLIS_FIRE_ESCAPE_INIT_GLOW_QUAD(quad, (intensity * (cyan ^ 1)) >> 1, (cyan * intensity) >> 1, (cyan * intensity) >> 1);
                quad->x0 = projection->screenPos.vx + ((projection->outerRadius * _gAcropolisFireEscapeGlowCircleSamples[partIndex + 4]) >> GLOW_TRIG_SHIFT);
                quad->y0 = projection->screenPos.vy + ((projection->outerRadius * _gAcropolisFireEscapeGlowCircleSamples[partIndex]) >> GLOW_TRIG_SHIFT);
                quad->x1 = projection->screenPos.vx + ((projection->outerRadius * _gAcropolisFireEscapeGlowCircleSamples[partIndex + 5]) >> GLOW_TRIG_SHIFT);
                quad->y1 = projection->screenPos.vy + ((projection->outerRadius * _gAcropolisFireEscapeGlowCircleSamples[partIndex + 1]) >> GLOW_TRIG_SHIFT);
                quad->x2 = projection->screenPos.vx;
                quad->y2 = projection->screenPos.vy;
                quad->x3 = projection->screenPos.vx + ((projection->outerRadius * _gAcropolisFireEscapeGlowCircleSamples[partIndex + 6]) >> GLOW_TRIG_SHIFT);
                quad->y3 = projection->screenPos.vy + ((projection->outerRadius * _gAcropolisFireEscapeGlowCircleSamples[partIndex + 2]) >> GLOW_TRIG_SHIFT);
                _acropolisFireEscapeQueueGlow(quad, &projection->otz);

                quad           = gGpuPrimCursor;
                gGpuPrimCursor = quad + 1;
                ACROPOLIS_FIRE_ESCAPE_INIT_GLOW_QUAD(quad, intensity * (cyan ^ 1), cyan * intensity, cyan * intensity);
                quad->x0 = projection->screenPos.vx + ((projection->outerRadius * _gAcropolisFireEscapeGlowCircleSamples[partIndex + 4]) >> (GLOW_TRIG_SHIFT + 1));
                quad->y0 = projection->screenPos.vy + ((projection->outerRadius * _gAcropolisFireEscapeGlowCircleSamples[partIndex]) >> (GLOW_TRIG_SHIFT + 1));
                quad->x1 = projection->screenPos.vx + ((projection->outerRadius * _gAcropolisFireEscapeGlowCircleSamples[partIndex + 5]) >> (GLOW_TRIG_SHIFT + 1));
                quad->y1 = projection->screenPos.vy + ((projection->outerRadius * _gAcropolisFireEscapeGlowCircleSamples[partIndex + 1]) >> (GLOW_TRIG_SHIFT + 1));
                quad->x2 = projection->screenPos.vx;
                quad->y2 = projection->screenPos.vy;
                quad->x3 = projection->screenPos.vx + ((projection->outerRadius * _gAcropolisFireEscapeGlowCircleSamples[partIndex + 6]) >> (GLOW_TRIG_SHIFT + 1));
                quad->y3 = projection->screenPos.vy + ((projection->outerRadius * _gAcropolisFireEscapeGlowCircleSamples[partIndex + 2]) >> (GLOW_TRIG_SHIFT + 1));
                _acropolisFireEscapeQueueGlow(quad, &projection->otz);
            }
            // Overlay four half-bright rays on the concentric disc.
            // Retain the first ray's pre-array read of the preceding descriptor's zero halfword.
            halfIntensity = intensity >> 1;
            for (partIndex = 2; partIndex < ACROPOLIS_FIRE_ESCAPE_GLOW_CIRCLE_STEPS; partIndex += 8) {
                quad           = gGpuPrimCursor;
                gGpuPrimCursor = quad + 1;
                ACROPOLIS_FIRE_ESCAPE_INIT_GLOW_QUAD(quad, halfIntensity * (cyan ^ 1), cyan * halfIntensity, cyan * halfIntensity);
                quad->x0 = projection->screenPos.vx + ((projection->innerRadius * _gAcropolisFireEscapeGlowCircleSamples[partIndex]) >> GLOW_TRIG_SHIFT);
                quad->y0 = projection->screenPos.vy + ((projection->innerRadius * _gAcropolisFireEscapeGlowCircleSamples[partIndex - 4]) >> GLOW_TRIG_SHIFT);
                quad->x1 = projection->screenPos.vx + ((projection->outerRadius * _gAcropolisFireEscapeGlowCircleSamples[partIndex + 4]) >> (GLOW_TRIG_SHIFT - 1));
                quad->y1 = projection->screenPos.vy + ((projection->outerRadius * _gAcropolisFireEscapeGlowCircleSamples[partIndex]) >> (GLOW_TRIG_SHIFT - 1));
                quad->x2 = projection->screenPos.vx;
                quad->y2 = projection->screenPos.vy;
                quad->x3 = projection->screenPos.vx + ((projection->innerRadius * _gAcropolisFireEscapeGlowCircleSamples[partIndex + 8]) >> GLOW_TRIG_SHIFT);
                quad->y3 = projection->screenPos.vy + ((projection->innerRadius * _gAcropolisFireEscapeGlowCircleSamples[partIndex + 4]) >> GLOW_TRIG_SHIFT);
                _acropolisFireEscapeQueueGlow(quad, &projection->otz);

                quad           = gGpuPrimCursor;
                gGpuPrimCursor = quad + 1;
                ACROPOLIS_FIRE_ESCAPE_INIT_GLOW_QUAD(quad, halfIntensity * (cyan ^ 1), cyan * halfIntensity, cyan * halfIntensity);
                quad->x0 = projection->screenPos.vx + ((projection->innerRadius * _gAcropolisFireEscapeGlowCircleSamples[partIndex + 4]) >> (GLOW_TRIG_SHIFT + 1));
                quad->y0 = projection->screenPos.vy + ((projection->innerRadius * _gAcropolisFireEscapeGlowCircleSamples[partIndex]) >> (GLOW_TRIG_SHIFT + 1));
                quad->x1 = projection->screenPos.vx + ((projection->outerRadius * _gAcropolisFireEscapeGlowCircleSamples[partIndex + 8]) >> GLOW_TRIG_SHIFT);
                quad->y1 = projection->screenPos.vy + ((projection->outerRadius * _gAcropolisFireEscapeGlowCircleSamples[partIndex + 4]) >> GLOW_TRIG_SHIFT);
                quad->x2 = projection->screenPos.vx;
                quad->y2 = projection->screenPos.vy;
                quad->x3 = projection->screenPos.vx + ((projection->innerRadius * _gAcropolisFireEscapeGlowCircleSamples[partIndex + 0xC]) >> (GLOW_TRIG_SHIFT + 1));
                quad->y3 = projection->screenPos.vy + ((projection->innerRadius * _gAcropolisFireEscapeGlowCircleSamples[partIndex + 8]) >> (GLOW_TRIG_SHIFT + 1));
                _acropolisFireEscapeQueueGlow(quad, &projection->otz);
            }
        } else {
            projection->outerRadius = (((pulseOrOptions >> ACROPOLIS_FIRE_ESCAPE_FLARE_RADIUS_SHIFT) & ACROPOLIS_FIRE_ESCAPE_FLARE_RADIUS_MASK) << ACROPOLIS_FIRE_ESCAPE_FLARE_DIAMOND_RADIUS_SHIFT) / projection->otz;
            projection->innerRadius = (((task->spawnArg1.value >> ACROPOLIS_FIRE_ESCAPE_FLARE_RADIUS_SHIFT) & ACROPOLIS_FIRE_ESCAPE_FLARE_RADIUS_MASK) << ACROPOLIS_FIRE_ESCAPE_FLARE_DIAMOND_RADIUS_SHIFT) / projection->otz;
            for (partIndex = 0; partIndex < 2; partIndex++) {
                quad           = gGpuPrimCursor;
                gGpuPrimCursor = quad + 1;
                ACROPOLIS_FIRE_ESCAPE_INIT_GLOW_QUAD(quad, intensity * (cyan ^ 1), cyan * intensity, cyan * intensity);
                quad->x0 = projection->screenPos.vx - projection->outerRadius;
                quad->x1 = quad->x2 = projection->screenPos.vx;
                quad->x3            = projection->screenPos.vx + projection->outerRadius;
                quad->y0 = quad->y2 = quad->y3 = projection->screenPos.vy;
                quad->y1                       = (projection->screenPos.vy - projection->innerRadius) + projection->innerRadius * (partIndex + partIndex);
                _acropolisFireEscapeQueueGlow(quad, &projection->otz);
            }
            // Retained packet reuse: the lines are initialized but the last quad is linked again.
            if (task->spawnArg1.value & ACROPOLIS_FIRE_ESCAPE_FLARE_STREAK_PACKETS) {
                for (partIndex = 0; partIndex < 2; partIndex++) {
                    streak         = gGpuPrimCursor;
                    gGpuPrimCursor = streak + 1;
                    setLineG3(streak);
                    setRGB0(streak, 0, 0, 0);
                    setRGB1(streak, intensity * (cyan ^ 1), cyan * intensity, cyan * intensity);
                    setRGB2(streak, 0, 0, 0);
                    streak->x0 = projection->screenPos.vx + projection->outerRadius * (partIndex * 3 - 1);
                    streak->y0 = projection->screenPos.vy - projection->innerRadius * (partIndex + 1);
                    streak->x1 = projection->screenPos.vx;
                    streak->y1 = projection->screenPos.vy;
                    streak->x2 = projection->screenPos.vx - projection->outerRadius * (partIndex * 3 - 1);
                    streak->y2 = projection->screenPos.vy + projection->innerRadius * (partIndex + 1);
                    _acropolisFireEscapeQueueGlow(quad, &projection->otz);
                }
            }
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(RoomGlowRadiiScratch);
    effectKillTask(effectWork, task);
}

#undef ACROPOLIS_FIRE_ESCAPE_INIT_GLOW_QUAD
