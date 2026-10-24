#include "rooms/akropolis_fire_escape.h"

#include "config.h"
#include "../../event_flags.h"
#include "../../room_states.h"
#include "../../event_flags.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "gameplay/actor_presentation.h"
#include "gameplay/actor_render.h"
#include "gameplay/area_transitions.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction_input.h"
#include "gameplay/display.h"
#include "gameplay/effects.h"
#include "gameplay/gameflag.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"
#include "gameplay/message.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/event_flags.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/random.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/stage_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/text.h"
#include "main/ui.h"
#include "main/ui_types.h"

#include "rooms/akropolis_square.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../../../shared/room/room_cutscene.h"
#include "../../../../shared/effects/glow_draw.h"

#include "assets/rooms/akropolis/fire_escape/declarations.inc"

static void _roomCutsceneTask(Task* task);

static void _roomCutsceneSoundTask(Task* task);

/// The save's `companionType` byte under a symbol of its own; the cutscene's end reads it through this name rather
/// than through `gMcSaveData`.

/// View saved when the cutscene starts and restored when it ends.

/// Object whose `field_4A` bit 0x40 the message task clears each frame while the slot-4 task does not answer message
/// 0x7D6.

/// Last requested fire-escape ambience level, in whole percent (0, 15, 30 or 100).
static s32 _gAkropolisFireEscapeRequestedAmbiencePercent;

/// The cutscene's sound task, killed when the scene is skipped.
extern Task* gRoomCutsceneSoundTask;

/// Persistent playback request for the repeat fire-escape scene in view 9.
RoomCutsceneRec _gAkropolisFireEscapeRepeatScene;

#include "../../../../shared/room/telephone.h"

/// Initializes a glow quad with RGB at vertex 2 and a black rim elsewhere.
#define AKROPOLIS_FIRE_ESCAPE_INIT_GLOW_QUAD(quad, red, green, blue) \
    ((void)(setPolyG4((quad)),                                       \
            setRGB0((quad), 0, 0, 0),                                \
            setRGB1((quad), 0, 0, 0),                                \
            setRGB2((quad), (red), (green), (blue)),                 \
            setRGB3((quad), 0, 0, 0)))

static void _akropolisFireEscapeInitializeRoomTask(Task* task);
static void _akropolisFireEscapeDisableAbsentActorInteraction(Task* unusedTask);

static s32 _akropolisFireEscapeHandleRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 command, s32 unusedSecondArg);
static s32 _akropolisFireEscapeResolveRoomTransition(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32 _akropolisFireEscapeRejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedSecondArg);
static s32 _akropolisFireEscapeIgnoreSoundCue(Task* unusedTask, s32 unusedMessageId, s32 unusedCueIndex, s32 unusedSecondArg);

static void _akropolisFireEscapeAmbienceTask(Task* task);

#include "../../../../shared/room/telephone_data.inc.c"

/// This carrier's bodyless scene runner and timed sound task, followed by END.
static TaskDesc _gRoomCutsceneTaskDescs[ROOM_CUTSCENE_TASK_DESCRIPTOR_COUNT] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _roomCutsceneTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _roomCutsceneSoundTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

/// Messages of the room task.
static TaskMessageEntry _gAkropolisFireEscapeRoomMessages[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _akropolisFireEscapeResolveRoomTransition },
    { ROOM_MESSAGE_USE_KEY_ITEM, _akropolisFireEscapeRejectKeyItemUse },
    { ROOM_MESSAGE_COMMAND, _akropolisFireEscapeHandleRoomCommand },
    { ROOM_MESSAGE_SOUND, _akropolisFireEscapeIgnoreSoundCue },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Bodyless view-dependent ambience recipe at index 0, followed by END.
static TaskDesc _gAkropolisFireEscapeAmbienceTaskDescriptors[2] = {
    {
        .header   = { .fields = { TASK_BODY_NONE, TASK_PRIORITY_SCENE } },
        .callback = _akropolisFireEscapeAmbienceTask,
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

Task* gRoomCutsceneSoundTask;

/// Retained bytes following this room's telephone menu title terminator.
#define TELEPHONE_MENU_TITLE_TRAILING_BYTES "\x01\x00"
#include "../../../../shared/room/telephone.inc.c"
#undef TELEPHONE_MENU_TITLE_TRAILING_BYTES

void akropolisFireEscapeTelephoneMenuTask(Task* task)
{
    _telephoneMenuTask(task);
}

#include "../../../../shared/room/telephone_panels.inc.c"

#include "../../../../shared/room/room_cutscene_task.inc.c"

/// Room-task handlers for state 0 initialization, state 1 interaction checks and state 2 teardown.
static const TaskFuncTable3 _gAkropolisFireEscapeRoomStateHandlers = {
    .funcs = {
        _akropolisFireEscapeInitializeRoomTask,
        _akropolisFireEscapeDisableAbsentActorInteraction,
        taskKill,
    },
};

/// Starts the repeat fire-escape scene using the room's persistent playback record.
static inline void _akropolisFireEscapeStartRepeatScene(void)
{
    enum {
        AKROPOLIS_FIRE_ESCAPE_SCENE_VIEW              = 9,
        AKROPOLIS_FIRE_ESCAPE_SCENE_CAP_SLOT          = 1,
        AKROPOLIS_FIRE_ESCAPE_SCENE_CAP_FILE          = 1,
        AKROPOLIS_FIRE_ESCAPE_SCENE_PLAY              = 0,
        AKROPOLIS_FIRE_ESCAPE_SCENE_TASK              = 0,
        AKROPOLIS_FIRE_ESCAPE_SCENE_FOLLOW_UP_COMMAND = 3,
        AKROPOLIS_FIRE_ESCAPE_SCENE_START_SOUND       = AKROPOLIS_FIRE_ESCAPE_SOUND(AKROPOLIS_FIRE_ESCAPE_SOUND_SCENE_START),
        AKROPOLIS_FIRE_ESCAPE_SCENE_END_SOUND         = AKROPOLIS_FIRE_ESCAPE_SOUND(AKROPOLIS_FIRE_ESCAPE_SOUND_SCENE_END),
        AKROPOLIS_FIRE_ESCAPE_SCENE_PLAYBACK_SOUND    = AKROPOLIS_FIRE_ESCAPE_SOUND(AKROPOLIS_FIRE_ESCAPE_SOUND_SCENE_PLAYBACK),
        AKROPOLIS_FIRE_ESCAPE_SCENE_AFTER_SOUND       = AKROPOLIS_FIRE_ESCAPE_SOUND(AKROPOLIS_FIRE_ESCAPE_SOUND_SCENE_AFTER),
    };

    _gAkropolisFireEscapeRepeatScene.view            = AKROPOLIS_FIRE_ESCAPE_SCENE_VIEW;
    _gAkropolisFireEscapeRepeatScene.capSlot         = AKROPOLIS_FIRE_ESCAPE_SCENE_CAP_SLOT;
    _gAkropolisFireEscapeRepeatScene.capFile         = AKROPOLIS_FIRE_ESCAPE_SCENE_CAP_FILE;
    _gAkropolisFireEscapeRepeatScene.skipScene       = AKROPOLIS_FIRE_ESCAPE_SCENE_PLAY;
    _gAkropolisFireEscapeRepeatScene.startSound      = AKROPOLIS_FIRE_ESCAPE_SCENE_START_SOUND;
    _gAkropolisFireEscapeRepeatScene.endSound        = AKROPOLIS_FIRE_ESCAPE_SCENE_END_SOUND;
    _gAkropolisFireEscapeRepeatScene.sceneSound      = AKROPOLIS_FIRE_ESCAPE_SCENE_PLAYBACK_SOUND;
    _gAkropolisFireEscapeRepeatScene.afterSceneSound = AKROPOLIS_FIRE_ESCAPE_SCENE_AFTER_SOUND;
    taskSpawnFromTable(_gRoomCutsceneTaskDescs, AKROPOLIS_FIRE_ESCAPE_SCENE_TASK, AKROPOLIS_FIRE_ESCAPE_SCENE_FOLLOW_UP_COMMAND, &_gAkropolisFireEscapeRepeatScene);
}

/// Room commands: 4 the telephone, 3 the dialogue moving the story on, 1 the dialogue by whether its actor is here.
static s32 _akropolisFireEscapeHandleRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 command, s32 unusedSecondArg)
{
    enum {
        AKROPOLIS_FIRE_ESCAPE_FIRST_SCENE_CAP_COMMAND   = 11,
        AKROPOLIS_FIRE_ESCAPE_DIALOGUE_CAP_COMMAND      = 3,
        AKROPOLIS_FIRE_ESCAPE_DIALOGUE_OBJECTIVE        = 7,
        AKROPOLIS_FIRE_ESCAPE_DIALOGUE_ACTOR_PLACEMENT  = 0,
        AKROPOLIS_FIRE_ESCAPE_ACTOR_ABSENT_CAP_COMMAND  = 1,
        AKROPOLIS_FIRE_ESCAPE_ACTOR_PRESENT_CAP_COMMAND = 9,
    };
    Task* actorTask;
    s32   capCommand;
    s32   actorPresent;

    if (command == AKROPOLIS_FIRE_ESCAPE_ROOM_COMMAND_TELEPHONE) {
        if (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_FIRE_ESCAPE_TELEPHONE_INSPECTED) == 0) {
            gameFlagSetNibble(EVENT_FLAG_AKROPOLIS_FIRE_ESCAPE_TELEPHONE_INSPECTED, 1);
            capRunCommandWithTransition(AKROPOLIS_FIRE_ESCAPE_FIRST_SCENE_CAP_COMMAND);
            return 0;
        }
        _akropolisFireEscapeStartRepeatScene();
    }
    if (command == AKROPOLIS_FIRE_ESCAPE_ROOM_COMMAND_ADVANCE_DIALOGUE) {
        // Advancing dialogue rearms its follow-up without lowering later progress.
        if (gameFlagGetNibble(EVENT_FLAG_STORY_DIALOGUE_INDEX) < AKROPOLIS_STORY_DIALOGUE_HELICOPTER) {
            gameFlagSetNibble(EVENT_FLAG_TELEPHONE_DIALOGUE_COUNTER, EVENT_FLAG_TELEPHONE_DIALOGUE_UNHEARD);
            gameFlagSetNibble(EVENT_FLAG_STORY_DIALOGUE_INDEX, AKROPOLIS_STORY_DIALOGUE_HELICOPTER);
        }
        capSpawnEventIfIdle(AKROPOLIS_FIRE_ESCAPE_DIALOGUE_CAP_COMMAND, CAP_EVENT_PAUSE_ACTORS);
        gameFlagSetPackedByte(EVENT_FLAG_CURRENT_OBJECTIVE, AKROPOLIS_FIRE_ESCAPE_DIALOGUE_OBJECTIVE);
    }
    if (command == AKROPOLIS_FIRE_ESCAPE_ROOM_COMMAND_ACTOR_DIALOGUE) {
        actorTask  = sceneFindPlacedActor(AKROPOLIS_FIRE_ESCAPE_DIALOGUE_ACTOR_PLACEMENT);
        capCommand = AKROPOLIS_FIRE_ESCAPE_ACTOR_ABSENT_CAP_COMMAND;
        if (actorTask != NULL) {
            // Retain the original zero output payload; this actor also writes through it.
            actorPresent = taskMessageDispatch(actorTask, ACTOR_MESSAGE_IS_PRESENT, 0, 0);
            capCommand   = AKROPOLIS_FIRE_ESCAPE_ACTOR_PRESENT_CAP_COMMAND;
            if (actorPresent == 0) {
                capCommand = AKROPOLIS_FIRE_ESCAPE_ACTOR_ABSENT_CAP_COMMAND;
            }
        }
        capSpawnEventIfIdle(capCommand, CAP_EVENT_PAUSE_ACTORS);
    }
    return 0;
}

/// Requests view-dependent ambience and the view-8 story music transition.
static void _akropolisFireEscapeAmbienceTask(Task* task)
{
    enum {
        AKROPOLIS_FIRE_ESCAPE_AMBIENCE_INIT            = 0,
        AKROPOLIS_FIRE_ESCAPE_AMBIENCE_UPDATE          = 1,
        AKROPOLIS_FIRE_ESCAPE_AMBIENCE_SILENT          = 0,
        AKROPOLIS_FIRE_ESCAPE_AMBIENCE_FULL_PERCENT    = 100,
        AKROPOLIS_FIRE_ESCAPE_AMBIENCE_VIEWS23_PERCENT = 30,
        AKROPOLIS_FIRE_ESCAPE_AMBIENCE_VIEWS45_PERCENT = 15,
        AKROPOLIS_FIRE_ESCAPE_AMBIENCE_ATTENUATION_MAX = 127,
        AKROPOLIS_FIRE_ESCAPE_AMBIENCE_FADE_TICKS      = 30,
        AKROPOLIS_FIRE_ESCAPE_ENTRY_MUSIC_EVENT        = 5,
        AKROPOLIS_FIRE_ESCAPE_VIEW8_MUSIC_EVENT        = 7,
    };
    s32 levelPercent;
    s32 previousLevelPercent;

    switch (task->state) {
        case AKROPOLIS_FIRE_ESCAPE_AMBIENCE_INIT:
            _gAkropolisFireEscapeRequestedAmbiencePercent = AKROPOLIS_FIRE_ESCAPE_AMBIENCE_SILENT;
            task->state                                   = task->state + 1;
            return;
        case AKROPOLIS_FIRE_ESCAPE_AMBIENCE_UPDATE:
            break;
        default:
            return;
    }

    switch (gGameSession->location.view) {
        case 8:
            levelPercent = AKROPOLIS_FIRE_ESCAPE_AMBIENCE_FULL_PERCENT;
            // Commit the music column before queuing its asynchronous loader.
            if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent == AKROPOLIS_FIRE_ESCAPE_ENTRY_MUSIC_EVENT) {
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = AKROPOLIS_FIRE_ESCAPE_VIEW8_MUSIC_EVENT;
                gStageMusicParams.fadeOutTicks                      = 1;
                gStageMusicParams.field_2                           = 1;
                taskSpawnFromTable(&gStageMusicTaskDesc, 0, STAGE_MUSIC_REQUEST_ORDINARY, 0);
                gGameSession->flowFlags = 0;
            }
            break;
        case 2:
        case 3:
            levelPercent = AKROPOLIS_FIRE_ESCAPE_AMBIENCE_VIEWS23_PERCENT;
            break;
        case 4:
        case 5:
            levelPercent = AKROPOLIS_FIRE_ESCAPE_AMBIENCE_VIEWS45_PERCENT;
            break;
        default:
            levelPercent = AKROPOLIS_FIRE_ESCAPE_AMBIENCE_SILENT;
            break;
    }

    previousLevelPercent = _gAkropolisFireEscapeRequestedAmbiencePercent;
    if (levelPercent == previousLevelPercent) {
        return;
    }
    if (previousLevelPercent == AKROPOLIS_FIRE_ESCAPE_AMBIENCE_SILENT) {
        sndEvtRequestScriptStart(SOUND_AKROPOLIS_FIRE_ESCAPE_AMBIENCE, 0, (s8)(((AKROPOLIS_FIRE_ESCAPE_AMBIENCE_FULL_PERCENT - levelPercent) * AKROPOLIS_FIRE_ESCAPE_AMBIENCE_ATTENUATION_MAX) / AKROPOLIS_FIRE_ESCAPE_AMBIENCE_FULL_PERCENT));
    } else if (levelPercent == AKROPOLIS_FIRE_ESCAPE_AMBIENCE_SILENT) {
        sndEvtRequestScriptStop(SOUND_AKROPOLIS_FIRE_ESCAPE_AMBIENCE, AKROPOLIS_FIRE_ESCAPE_AMBIENCE_FADE_TICKS);
    } else {
        sndEvtRequestScriptMix(SOUND_AKROPOLIS_FIRE_ESCAPE_AMBIENCE, 0, (s8)(((AKROPOLIS_FIRE_ESCAPE_AMBIENCE_FULL_PERCENT - levelPercent) * AKROPOLIS_FIRE_ESCAPE_AMBIENCE_ATTENUATION_MAX) / AKROPOLIS_FIRE_ESCAPE_AMBIENCE_FULL_PERCENT));
    }
    _gAkropolisFireEscapeRequestedAmbiencePercent = levelPercent;
}

#include "../../../../shared/room/room_cutscene_sound_task.inc.c"

/// Leaving fades the ambience out; the bridge's door leads to the bridge's state.
static s32 _akropolisFireEscapeResolveRoomTransition(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    enum { AKROPOLIS_FIRE_ESCAPE_DEPARTURE_FADE_TICKS = 15 };

    *reply = *request;
    if (request->queryOnly == ROOM_EVENT_EXECUTE) {
        sndEvtRequestScriptStop(SOUND_AKROPOLIS_FIRE_ESCAPE_AMBIENCE, AKROPOLIS_FIRE_ESCAPE_DEPARTURE_FADE_TICKS);
    }
    if (request->areaId == AKROPOLIS_ROOM_ID_BRIDGE && request->queryOnly == ROOM_EVENT_EXECUTE) {
        if (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_BRIDGE_PROGRESS) == AKROPOLIS_BRIDGE_RAISED) {
            reply->roomState = AKROPOLIS_ROOM_STATE_BRIDGE_RAISED;
        } else {
            reply->roomState = AKROPOLIS_ROOM_STATE_BRIDGE_LOWERED;
        }
    }
    return ROOM_EVENT_DEPART;
}

/// No key item can be used here.
static s32 _akropolisFireEscapeRejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Room sound cues do nothing here.
static s32 _akropolisFireEscapeIgnoreSoundCue(Task* unusedTask, s32 unusedMessageId, s32 unusedCueIndex, s32 unusedSecondArg)
{
    return 0;
}

/// Installs the room's messages and starts the ambience.
static void _akropolisFireEscapeInitializeRoomTask(Task* task)
{
    enum {
        AKROPOLIS_FIRE_ESCAPE_INITIAL_AMBIENCE_TASK = 0,
        AKROPOLIS_FIRE_ESCAPE_ENTRY_MUSIC_EVENT     = 5,
    };

    task->msgTable = _gAkropolisFireEscapeRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    taskSpawnFromTable(_gAkropolisFireEscapeAmbienceTaskDescriptors, AKROPOLIS_FIRE_ESCAPE_INITIAL_AMBIENCE_TASK, 0, 0);
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent == AKROPOLIS_FIRE_ESCAPE_ENTRY_MUSIC_EVENT) {
        gGameSession->flowFlags = GAME_SESSION_FLOW_LOAD_AREA_MUSIC_ONLY;
    }
    task->state = task->state + 1;
}

/// Disables the placed actor's interaction when the actor is absent.
static void _akropolisFireEscapeDisableAbsentActorInteraction(Task* unusedTask)
{
    enum { AKROPOLIS_FIRE_ESCAPE_ACTOR_INTERACTION_TRIGGER = 5 };
    Task* actorTask;

    actorTask = sceneFindPlacedActor(0);
    if (actorTask == NULL || taskMessageDispatch(actorTask, ACTOR_MESSAGE_IS_PRESENT, 0, 0) == 0) {
        WorldCollisionTrigger* interaction = &_gAkropolisFireEscapeActionTriggers[AKROPOLIS_FIRE_ESCAPE_ACTOR_INTERACTION_TRIGGER];
        interaction->flags                &= (0xFF ^ WORLD_COLLISION_TRIGGER_ENABLED);
    }
}

void akropolisFireEscapeRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers = _gAkropolisFireEscapeRoomStateHandlers;
    stateHandlers.funcs[task->state](task);
}

/// Spawns one view-selected flare at an offset from the light emitter.
static inline void _akropolisFireEscapeSpawnFlare(EffectWork* emitterWork, GfxCoord* emitterCoord,
                                                  s16 offsetX, s16 offsetY, s16 offsetZ, s32 options)
{
    emitterWork->move.vx = offsetX;
    emitterWork->move.vy = offsetY;
    emitterWork->move.vz = offsetZ;
    effectSpawn(EFFECT_AKROPOLIS_FIRE_ESCAPE_FLARE, emitterCoord, options, &emitterWork->move);
}

void akropolisFireEscapeLightEmitterTask(Task* task)
{
    enum {
        AKROPOLIS_FIRE_ESCAPE_LIGHT_EMITTER_INITIALIZE = 0,
        AKROPOLIS_FIRE_ESCAPE_LIGHT_EMITTER_EMIT       = 1,
        // Radius byte 32; the lamp does not read the retained upper bits.
        AKROPOLIS_FIRE_ESCAPE_FLICKER_SPAWN_OPTIONS = 0x42000,
        AKROPOLIS_FIRE_ESCAPE_RED_DIAMOND_VIEW      = 3,
        AKROPOLIS_FIRE_ESCAPE_RED_RADIAL_VIEW       = 8,
        AKROPOLIS_FIRE_ESCAPE_CYAN_DIAMOND_VIEW     = 6,
        AKROPOLIS_FIRE_ESCAPE_CYAN_RADIAL_VIEW      = 9,
        AKROPOLIS_FIRE_ESCAPE_RED_PULSE_RATE        = 14,
        AKROPOLIS_FIRE_ESCAPE_CYAN_PULSE_RATE       = 8,
        AKROPOLIS_FIRE_ESCAPE_RED_DIAMOND_RADIUS    = 6,
        AKROPOLIS_FIRE_ESCAPE_RED_RADIAL_RADIUS     = 3,
        AKROPOLIS_FIRE_ESCAPE_CYAN_DIAMOND_RADIUS   = 4,
        AKROPOLIS_FIRE_ESCAPE_CYAN_RADIAL_RADIUS    = 2,
    };
    EffectWork* emitterWork;
    GfxCoord*   emitterCoord;

    emitterWork  = task->spawnArg2.pointer;
    emitterCoord = task->extra.coordBody->coord;
    switch (task->state) {
        case AKROPOLIS_FIRE_ESCAPE_LIGHT_EMITTER_INITIALIZE:
            // The persistent lamp snapshots this local placement once.
            emitterWork->move.vx = 2904;
            emitterWork->move.vy = -2082;
            emitterWork->move.vz = -229;
            effectSpawn(EFFECT_AKROPOLIS_FIRE_ESCAPE_FLICKER_LIGHT, emitterCoord, AKROPOLIS_FIRE_ESCAPE_FLICKER_SPAWN_OPTIONS, &emitterWork->move);
            task->state = task->state + 1;
            break;
        case AKROPOLIS_FIRE_ESCAPE_LIGHT_EMITTER_EMIT:
            // Actor pause/hide modes still emit; a cancellation update suppresses emission.
            if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
                if (gGameSession->location.view == AKROPOLIS_FIRE_ESCAPE_RED_DIAMOND_VIEW) {
                    _akropolisFireEscapeSpawnFlare(emitterWork, emitterCoord, 1167, -913, 1670,
                                                   (AKROPOLIS_FIRE_ESCAPE_RED_DIAMOND_RADIUS << AKROPOLIS_FIRE_ESCAPE_FLARE_RADIUS_SHIFT) | AKROPOLIS_FIRE_ESCAPE_RED_PULSE_RATE);
                }
                if (gGameSession->location.view == AKROPOLIS_FIRE_ESCAPE_RED_RADIAL_VIEW) {
                    _akropolisFireEscapeSpawnFlare(emitterWork, emitterCoord, 1167, -913, 1670,
                                                   AKROPOLIS_FIRE_ESCAPE_FLARE_RADIAL | (AKROPOLIS_FIRE_ESCAPE_RED_RADIAL_RADIUS << AKROPOLIS_FIRE_ESCAPE_FLARE_RADIUS_SHIFT) | AKROPOLIS_FIRE_ESCAPE_RED_PULSE_RATE);
                }
                if (gGameSession->location.view == AKROPOLIS_FIRE_ESCAPE_CYAN_DIAMOND_VIEW) {
                    _akropolisFireEscapeSpawnFlare(emitterWork, emitterCoord, -3103, -3344, 2272,
                                                   AKROPOLIS_FIRE_ESCAPE_FLARE_CYAN | (AKROPOLIS_FIRE_ESCAPE_CYAN_DIAMOND_RADIUS << AKROPOLIS_FIRE_ESCAPE_FLARE_RADIUS_SHIFT) | AKROPOLIS_FIRE_ESCAPE_CYAN_PULSE_RATE);
                }
                if (gGameSession->location.view == AKROPOLIS_FIRE_ESCAPE_CYAN_RADIAL_VIEW) {
                    _akropolisFireEscapeSpawnFlare(emitterWork, emitterCoord, -3103, -3344, 2272,
                                                   AKROPOLIS_FIRE_ESCAPE_FLARE_RADIAL | AKROPOLIS_FIRE_ESCAPE_FLARE_CYAN | (AKROPOLIS_FIRE_ESCAPE_CYAN_RADIAL_RADIUS << AKROPOLIS_FIRE_ESCAPE_FLARE_RADIUS_SHIFT) | AKROPOLIS_FIRE_ESCAPE_CYAN_PULSE_RATE);
                }
            }
            break;
    }
}

/// Queues an initialized fire-escape glow quad with additive blending.
static inline void _akropolisFireEscapeQueueGlow(POLY_G4* quad, const s32* sortingDepth)
{
    enum { AKROPOLIS_FIRE_ESCAPE_GLOW_DEPTH_TO_TAG_INDEX_SHIFT = 4 };

    addPrim(&gGpuCurrentOt[((u32)*sortingDepth << gDisplayState.otDepthShift) >> AKROPOLIS_FIRE_ESCAPE_GLOW_DEPTH_TO_TAG_INDEX_SHIFT & (GPU_ORDERING_TABLE_DEPTH_BYTE_MASK / sizeof(*gGpuCurrentOt))],
            quad);
    // Prepending the draw mode after the quad makes the GPU apply it first.
    gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, *sortingDepth);
}

void akropolisFireEscapeFlickerLightTask(Task* task)
{
    enum {
        AKROPOLIS_FIRE_ESCAPE_FLICKER_VIEW_MASK           = (1 << (2 - 1)) | (1 << (3 - 1)) | (1 << (7 - 1)),
        AKROPOLIS_FIRE_ESCAPE_FLICKER_DEPTH_BIAS          = 32,
        AKROPOLIS_FIRE_ESCAPE_FLICKER_MODE_FRAME_MASK     = 31,
        AKROPOLIS_FIRE_ESCAPE_FLICKER_MODE_MASK           = 3,
        AKROPOLIS_FIRE_ESCAPE_FLICKER_RADIUS_SHIFT        = 8,
        AKROPOLIS_FIRE_ESCAPE_FLICKER_RADIUS_MASK         = 0xFF,
        AKROPOLIS_FIRE_ESCAPE_FLICKER_DIM_MAX             = 16,
        AKROPOLIS_FIRE_ESCAPE_FLICKER_SOUND_MIN           = 32,
        AKROPOLIS_FIRE_ESCAPE_FLICKER_BRIGHTNESS_MASK     = 0x30,
        AKROPOLIS_FIRE_ESCAPE_FLICKER_LOW_BRIGHTNESS_MASK = 0x10,
        AKROPOLIS_FIRE_ESCAPE_FLICKER_RANDOM              = 0,
        AKROPOLIS_FIRE_ESCAPE_FLICKER_RANDOM_ODD_FRAMES   = 1,
        AKROPOLIS_FIRE_ESCAPE_FLICKER_STEADY              = 2,
        AKROPOLIS_FIRE_ESCAPE_FLICKER_DIM_RANDOM          = 3,
        AKROPOLIS_FIRE_ESCAPE_FLICKER_RADIUS_SCALE        = 0x600,
        AKROPOLIS_FIRE_ESCAPE_FLICKER_INNER_RADIUS_SCALE  = 0xC0,
        AKROPOLIS_FIRE_ESCAPE_GLOW_CIRCLE_STEPS           = 16,
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
    if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN && ((AKROPOLIS_FIRE_ESCAPE_FLICKER_VIEW_MASK >> (gGameSession->location.view - 1)) & 1)) {
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
        projection->otz -= AKROPOLIS_FIRE_ESCAPE_FLICKER_DEPTH_BIAS;
        if (projection->otz >= GLOW_MIN_DEPTH) {
            if (!(gDisplayState.animFrame & AKROPOLIS_FIRE_ESCAPE_FLICKER_MODE_FRAME_MASK)) {
                RANDOM_LCG_ADVANCE();
                effectWork->index = RANDOM_LCG_DRAW() & AKROPOLIS_FIRE_ESCAPE_FLICKER_MODE_MASK;
            }
            // Choose flicker brightness, then sound only a dim-to-bright transition.
            brightness = effectWork->scale;
            if (effectWork->scale <= AKROPOLIS_FIRE_ESCAPE_FLICKER_DIM_MAX) {
                wasDim = 1;
            }
            switch (effectWork->index) {
                case AKROPOLIS_FIRE_ESCAPE_FLICKER_RANDOM:
                    RANDOM_LCG_ADVANCE();
                    effectWork->scale = RANDOM_LCG_DRAW() & AKROPOLIS_FIRE_ESCAPE_FLICKER_BRIGHTNESS_MASK;
                    break;
                case AKROPOLIS_FIRE_ESCAPE_FLICKER_RANDOM_ODD_FRAMES:
                    if (gDisplayState.animFrame & 1) {
                        RANDOM_LCG_ADVANCE();
                        brightness = RANDOM_LCG_DRAW() & AKROPOLIS_FIRE_ESCAPE_FLICKER_BRIGHTNESS_MASK;
                    }
                    effectWork->scale = brightness;
                    break;
                case AKROPOLIS_FIRE_ESCAPE_FLICKER_STEADY:
                    effectWork->scale = AKROPOLIS_FIRE_ESCAPE_FLICKER_BRIGHTNESS_MASK;
                    break;
                case AKROPOLIS_FIRE_ESCAPE_FLICKER_DIM_RANDOM:
                    RANDOM_LCG_ADVANCE();
                    effectWork->scale = RANDOM_LCG_DRAW() & AKROPOLIS_FIRE_ESCAPE_FLICKER_LOW_BRIGHTNESS_MASK;
                    break;
            }
            if (wasDim && effectWork->scale >= AKROPOLIS_FIRE_ESCAPE_FLICKER_SOUND_MIN) {
                sndEvtRequestScriptStart(SOUND_AKROPOLIS_FIRE_ESCAPE_LIGHT_FLICKER, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            }
            projection->outerRadius = (((task->spawnArg1.value >> AKROPOLIS_FIRE_ESCAPE_FLICKER_RADIUS_SHIFT) & AKROPOLIS_FIRE_ESCAPE_FLICKER_RADIUS_MASK) * AKROPOLIS_FIRE_ESCAPE_FLICKER_RADIUS_SCALE) / projection->otz;
            projection->innerRadius = (((task->spawnArg1.value >> AKROPOLIS_FIRE_ESCAPE_FLICKER_RADIUS_SHIFT) & AKROPOLIS_FIRE_ESCAPE_FLICKER_RADIUS_MASK) * AKROPOLIS_FIRE_ESCAPE_FLICKER_INNER_RADIUS_SCALE) / projection->otz;
            // Eight wedges in each of three concentric intensity layers.
            for (cornerIndex = 0; cornerIndex < AKROPOLIS_FIRE_ESCAPE_GLOW_CIRCLE_STEPS; cornerIndex += 2) {
                quad           = gGpuPrimCursor;
                gGpuPrimCursor = quad + 1;
                AKROPOLIS_FIRE_ESCAPE_INIT_GLOW_QUAD(quad, effectWork->scale >> 1, effectWork->scale >> 1, effectWork->scale >> 1);
                quad->x0 = projection->screenPos.vx + ((projection->outerRadius * _gAkropolisFireEscapeGlowCircleSamples[cornerIndex + 4]) >> GLOW_TRIG_SHIFT);
                quad->y0 = projection->screenPos.vy + ((projection->outerRadius * _gAkropolisFireEscapeGlowCircleSamples[cornerIndex]) >> GLOW_TRIG_SHIFT);
                quad->x1 = projection->screenPos.vx + ((projection->outerRadius * _gAkropolisFireEscapeGlowCircleSamples[cornerIndex + 5]) >> GLOW_TRIG_SHIFT);
                quad->y1 = projection->screenPos.vy + ((projection->outerRadius * _gAkropolisFireEscapeGlowCircleSamples[cornerIndex + 1]) >> GLOW_TRIG_SHIFT);
                quad->x2 = projection->screenPos.vx;
                quad->y2 = projection->screenPos.vy;
                quad->x3 = projection->screenPos.vx + ((projection->outerRadius * _gAkropolisFireEscapeGlowCircleSamples[cornerIndex + 6]) >> GLOW_TRIG_SHIFT);
                quad->y3 = projection->screenPos.vy + ((projection->outerRadius * _gAkropolisFireEscapeGlowCircleSamples[cornerIndex + 2]) >> GLOW_TRIG_SHIFT);
                _akropolisFireEscapeQueueGlow(quad, &projection->otz);

                quad           = gGpuPrimCursor;
                gGpuPrimCursor = quad + 1;
                AKROPOLIS_FIRE_ESCAPE_INIT_GLOW_QUAD(quad, effectWork->scale, effectWork->scale, effectWork->scale);
                quad->x0 = projection->screenPos.vx + ((projection->outerRadius * _gAkropolisFireEscapeGlowCircleSamples[cornerIndex + 4]) >> (GLOW_TRIG_SHIFT + 1));
                quad->y0 = projection->screenPos.vy + ((projection->outerRadius * _gAkropolisFireEscapeGlowCircleSamples[cornerIndex]) >> (GLOW_TRIG_SHIFT + 1));
                quad->x1 = projection->screenPos.vx + ((projection->outerRadius * _gAkropolisFireEscapeGlowCircleSamples[cornerIndex + 5]) >> (GLOW_TRIG_SHIFT + 1));
                quad->y1 = projection->screenPos.vy + ((projection->outerRadius * _gAkropolisFireEscapeGlowCircleSamples[cornerIndex + 1]) >> (GLOW_TRIG_SHIFT + 1));
                quad->x2 = projection->screenPos.vx;
                quad->y2 = projection->screenPos.vy;
                quad->x3 = projection->screenPos.vx + ((projection->outerRadius * _gAkropolisFireEscapeGlowCircleSamples[cornerIndex + 6]) >> (GLOW_TRIG_SHIFT + 1));
                quad->y3 = projection->screenPos.vy + ((projection->outerRadius * _gAkropolisFireEscapeGlowCircleSamples[cornerIndex + 2]) >> (GLOW_TRIG_SHIFT + 1));
                _akropolisFireEscapeQueueGlow(quad, &projection->otz);

                quad           = gGpuPrimCursor;
                gGpuPrimCursor = quad + 1;
                AKROPOLIS_FIRE_ESCAPE_INIT_GLOW_QUAD(quad, (u8)effectWork->scale * 4, (u8)effectWork->scale * 4, (u8)effectWork->scale * 4);
                quad->x0 = projection->screenPos.vx + ((projection->outerRadius * _gAkropolisFireEscapeGlowCircleSamples[cornerIndex + 4]) >> (GLOW_TRIG_SHIFT + 3));
                quad->y0 = projection->screenPos.vy + ((projection->outerRadius * _gAkropolisFireEscapeGlowCircleSamples[cornerIndex]) >> (GLOW_TRIG_SHIFT + 3));
                quad->x1 = projection->screenPos.vx + ((projection->outerRadius * _gAkropolisFireEscapeGlowCircleSamples[cornerIndex + 5]) >> (GLOW_TRIG_SHIFT + 3));
                quad->y1 = projection->screenPos.vy + ((projection->outerRadius * _gAkropolisFireEscapeGlowCircleSamples[cornerIndex + 1]) >> (GLOW_TRIG_SHIFT + 3));
                quad->x2 = projection->screenPos.vx;
                quad->y2 = projection->screenPos.vy;
                quad->x3 = projection->screenPos.vx + ((projection->outerRadius * _gAkropolisFireEscapeGlowCircleSamples[cornerIndex + 6]) >> (GLOW_TRIG_SHIFT + 3));
                quad->y3 = projection->screenPos.vy + ((projection->outerRadius * _gAkropolisFireEscapeGlowCircleSamples[cornerIndex + 2]) >> (GLOW_TRIG_SHIFT + 3));
                _akropolisFireEscapeQueueGlow(quad, &projection->otz);
            }
        }
        SCRATCH_STACK_RELEASE_BLOCK(RoomGlowRadiiScratch);
    }
}

void akropolisFireEscapeFlareTask(Task* task)
{
    enum {
        AKROPOLIS_FIRE_ESCAPE_FLARE_PULSE_FALLING_BIT    = 0x80,
        AKROPOLIS_FIRE_ESCAPE_FLARE_PULSE_LEVEL_MASK     = 0x7F,
        AKROPOLIS_FIRE_ESCAPE_FLARE_RADIAL_RADIUS_SHIFT  = 10,
        AKROPOLIS_FIRE_ESCAPE_FLARE_INNER_RADIUS_SHIFT   = 7,
        AKROPOLIS_FIRE_ESCAPE_FLARE_DIAMOND_RADIUS_SHIFT = 9,
        AKROPOLIS_FIRE_ESCAPE_GLOW_CIRCLE_STEPS          = 16,
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
        pulsePhase *= task->spawnArg1.value & AKROPOLIS_FIRE_ESCAPE_FLARE_PULSE_RATE_MASK;
        cyan        = (task->spawnArg1.value >> AKROPOLIS_FIRE_ESCAPE_FLARE_CYAN_SHIFT) & 1;
        if (pulsePhase & AKROPOLIS_FIRE_ESCAPE_FLARE_PULSE_FALLING_BIT) {
            pulseOrOptions = ~pulsePhase & AKROPOLIS_FIRE_ESCAPE_FLARE_PULSE_LEVEL_MASK;
        } else {
            pulseOrOptions = pulsePhase & AKROPOLIS_FIRE_ESCAPE_FLARE_PULSE_LEVEL_MASK;
        }
        intensity      = pulseOrOptions * 2;
        pulseOrOptions = task->spawnArg1.value;
        if (pulseOrOptions < 0) {
            radiusScale             = (pulseOrOptions >> AKROPOLIS_FIRE_ESCAPE_FLARE_RADIUS_SHIFT) & AKROPOLIS_FIRE_ESCAPE_FLARE_RADIUS_MASK;
            projection->outerRadius = (radiusScale << AKROPOLIS_FIRE_ESCAPE_FLARE_RADIAL_RADIUS_SHIFT) / projection->otz;
            projection->innerRadius = (((task->spawnArg1.value >> AKROPOLIS_FIRE_ESCAPE_FLARE_RADIUS_SHIFT) & AKROPOLIS_FIRE_ESCAPE_FLARE_RADIUS_MASK) << AKROPOLIS_FIRE_ESCAPE_FLARE_INNER_RADIUS_SHIFT) / projection->otz;
            for (partIndex = 0; partIndex < AKROPOLIS_FIRE_ESCAPE_GLOW_CIRCLE_STEPS; partIndex += 2) {
                quad           = gGpuPrimCursor;
                gGpuPrimCursor = quad + 1;
                AKROPOLIS_FIRE_ESCAPE_INIT_GLOW_QUAD(quad, (intensity * (cyan ^ 1)) >> 1, (cyan * intensity) >> 1, (cyan * intensity) >> 1);
                quad->x0 = projection->screenPos.vx + ((projection->outerRadius * _gAkropolisFireEscapeGlowCircleSamples[partIndex + 4]) >> GLOW_TRIG_SHIFT);
                quad->y0 = projection->screenPos.vy + ((projection->outerRadius * _gAkropolisFireEscapeGlowCircleSamples[partIndex]) >> GLOW_TRIG_SHIFT);
                quad->x1 = projection->screenPos.vx + ((projection->outerRadius * _gAkropolisFireEscapeGlowCircleSamples[partIndex + 5]) >> GLOW_TRIG_SHIFT);
                quad->y1 = projection->screenPos.vy + ((projection->outerRadius * _gAkropolisFireEscapeGlowCircleSamples[partIndex + 1]) >> GLOW_TRIG_SHIFT);
                quad->x2 = projection->screenPos.vx;
                quad->y2 = projection->screenPos.vy;
                quad->x3 = projection->screenPos.vx + ((projection->outerRadius * _gAkropolisFireEscapeGlowCircleSamples[partIndex + 6]) >> GLOW_TRIG_SHIFT);
                quad->y3 = projection->screenPos.vy + ((projection->outerRadius * _gAkropolisFireEscapeGlowCircleSamples[partIndex + 2]) >> GLOW_TRIG_SHIFT);
                _akropolisFireEscapeQueueGlow(quad, &projection->otz);

                quad           = gGpuPrimCursor;
                gGpuPrimCursor = quad + 1;
                AKROPOLIS_FIRE_ESCAPE_INIT_GLOW_QUAD(quad, intensity * (cyan ^ 1), cyan * intensity, cyan * intensity);
                quad->x0 = projection->screenPos.vx + ((projection->outerRadius * _gAkropolisFireEscapeGlowCircleSamples[partIndex + 4]) >> (GLOW_TRIG_SHIFT + 1));
                quad->y0 = projection->screenPos.vy + ((projection->outerRadius * _gAkropolisFireEscapeGlowCircleSamples[partIndex]) >> (GLOW_TRIG_SHIFT + 1));
                quad->x1 = projection->screenPos.vx + ((projection->outerRadius * _gAkropolisFireEscapeGlowCircleSamples[partIndex + 5]) >> (GLOW_TRIG_SHIFT + 1));
                quad->y1 = projection->screenPos.vy + ((projection->outerRadius * _gAkropolisFireEscapeGlowCircleSamples[partIndex + 1]) >> (GLOW_TRIG_SHIFT + 1));
                quad->x2 = projection->screenPos.vx;
                quad->y2 = projection->screenPos.vy;
                quad->x3 = projection->screenPos.vx + ((projection->outerRadius * _gAkropolisFireEscapeGlowCircleSamples[partIndex + 6]) >> (GLOW_TRIG_SHIFT + 1));
                quad->y3 = projection->screenPos.vy + ((projection->outerRadius * _gAkropolisFireEscapeGlowCircleSamples[partIndex + 2]) >> (GLOW_TRIG_SHIFT + 1));
                _akropolisFireEscapeQueueGlow(quad, &projection->otz);
            }
            // Overlay four half-bright rays on the concentric disc.
            // Retain the first ray's pre-array read of the preceding descriptor's zero halfword.
            halfIntensity = intensity >> 1;
            for (partIndex = 2; partIndex < AKROPOLIS_FIRE_ESCAPE_GLOW_CIRCLE_STEPS; partIndex += 8) {
                quad           = gGpuPrimCursor;
                gGpuPrimCursor = quad + 1;
                AKROPOLIS_FIRE_ESCAPE_INIT_GLOW_QUAD(quad, halfIntensity * (cyan ^ 1), cyan * halfIntensity, cyan * halfIntensity);
                quad->x0 = projection->screenPos.vx + ((projection->innerRadius * _gAkropolisFireEscapeGlowCircleSamples[partIndex]) >> GLOW_TRIG_SHIFT);
                quad->y0 = projection->screenPos.vy + ((projection->innerRadius * _gAkropolisFireEscapeGlowCircleSamples[partIndex - 4]) >> GLOW_TRIG_SHIFT);
                quad->x1 = projection->screenPos.vx + ((projection->outerRadius * _gAkropolisFireEscapeGlowCircleSamples[partIndex + 4]) >> (GLOW_TRIG_SHIFT - 1));
                quad->y1 = projection->screenPos.vy + ((projection->outerRadius * _gAkropolisFireEscapeGlowCircleSamples[partIndex]) >> (GLOW_TRIG_SHIFT - 1));
                quad->x2 = projection->screenPos.vx;
                quad->y2 = projection->screenPos.vy;
                quad->x3 = projection->screenPos.vx + ((projection->innerRadius * _gAkropolisFireEscapeGlowCircleSamples[partIndex + 8]) >> GLOW_TRIG_SHIFT);
                quad->y3 = projection->screenPos.vy + ((projection->innerRadius * _gAkropolisFireEscapeGlowCircleSamples[partIndex + 4]) >> GLOW_TRIG_SHIFT);
                _akropolisFireEscapeQueueGlow(quad, &projection->otz);

                quad           = gGpuPrimCursor;
                gGpuPrimCursor = quad + 1;
                AKROPOLIS_FIRE_ESCAPE_INIT_GLOW_QUAD(quad, halfIntensity * (cyan ^ 1), cyan * halfIntensity, cyan * halfIntensity);
                quad->x0 = projection->screenPos.vx + ((projection->innerRadius * _gAkropolisFireEscapeGlowCircleSamples[partIndex + 4]) >> (GLOW_TRIG_SHIFT + 1));
                quad->y0 = projection->screenPos.vy + ((projection->innerRadius * _gAkropolisFireEscapeGlowCircleSamples[partIndex]) >> (GLOW_TRIG_SHIFT + 1));
                quad->x1 = projection->screenPos.vx + ((projection->outerRadius * _gAkropolisFireEscapeGlowCircleSamples[partIndex + 8]) >> GLOW_TRIG_SHIFT);
                quad->y1 = projection->screenPos.vy + ((projection->outerRadius * _gAkropolisFireEscapeGlowCircleSamples[partIndex + 4]) >> GLOW_TRIG_SHIFT);
                quad->x2 = projection->screenPos.vx;
                quad->y2 = projection->screenPos.vy;
                quad->x3 = projection->screenPos.vx + ((projection->innerRadius * _gAkropolisFireEscapeGlowCircleSamples[partIndex + 0xC]) >> (GLOW_TRIG_SHIFT + 1));
                quad->y3 = projection->screenPos.vy + ((projection->innerRadius * _gAkropolisFireEscapeGlowCircleSamples[partIndex + 8]) >> (GLOW_TRIG_SHIFT + 1));
                _akropolisFireEscapeQueueGlow(quad, &projection->otz);
            }
        } else {
            projection->outerRadius = (((pulseOrOptions >> AKROPOLIS_FIRE_ESCAPE_FLARE_RADIUS_SHIFT) & AKROPOLIS_FIRE_ESCAPE_FLARE_RADIUS_MASK) << AKROPOLIS_FIRE_ESCAPE_FLARE_DIAMOND_RADIUS_SHIFT) / projection->otz;
            projection->innerRadius = (((task->spawnArg1.value >> AKROPOLIS_FIRE_ESCAPE_FLARE_RADIUS_SHIFT) & AKROPOLIS_FIRE_ESCAPE_FLARE_RADIUS_MASK) << AKROPOLIS_FIRE_ESCAPE_FLARE_DIAMOND_RADIUS_SHIFT) / projection->otz;
            for (partIndex = 0; partIndex < 2; partIndex++) {
                quad           = gGpuPrimCursor;
                gGpuPrimCursor = quad + 1;
                AKROPOLIS_FIRE_ESCAPE_INIT_GLOW_QUAD(quad, intensity * (cyan ^ 1), cyan * intensity, cyan * intensity);
                quad->x0 = projection->screenPos.vx - projection->outerRadius;
                quad->x1 = quad->x2 = projection->screenPos.vx;
                quad->x3            = projection->screenPos.vx + projection->outerRadius;
                quad->y0 = quad->y2 = quad->y3 = projection->screenPos.vy;
                quad->y1                       = (projection->screenPos.vy - projection->innerRadius) + projection->innerRadius * (partIndex + partIndex);
                _akropolisFireEscapeQueueGlow(quad, &projection->otz);
            }
            // Retained packet reuse: the lines are initialized but the last quad is linked again.
            if (task->spawnArg1.value & AKROPOLIS_FIRE_ESCAPE_FLARE_STREAK_PACKETS) {
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
                    _akropolisFireEscapeQueueGlow(quad, &projection->otz);
                }
            }
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(RoomGlowRadiiScratch);
    effectKillTask(effectWork, task);
}

#undef AKROPOLIS_FIRE_ESCAPE_INIT_GLOW_QUAD
