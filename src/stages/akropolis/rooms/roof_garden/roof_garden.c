#include "stages/akropolis/rooms/roof_garden.h"

#include "config.h"
#include "../../event_flags.h"
#include "../../event_flags.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>
#include <psyq/abs.h>

#include "common.h"
#include "gte.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/display.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/gameflag.h"
#include "gameplay/items.h"
#include "gameplay/message.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/view.h"
#include "gameplay/world_collision.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/event_flags.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "overlay.h"

#include "rooms/room_common.h"
#include "../../../../shared/effects/falling_leaves.h"
#include "../../../../shared/actor/actor_contacts.h"
#include "../../../../shared/effects/glow_draw.h"

#include "assets/rooms/akropolis/roof_garden/declarations.inc"

static Task* _gAkropolisRoofGardenCutsceneSoundTask;

static s32 _gAkropolisRoofGardenRequestedAmbiencePercent;

/// Whole-unit X/Y/Z displacement left by `_actorContactApplyGridPushback`.
extern SVECTOR ActorContact_ScratchPosition;

/// Returns this carrier's persistent last contact-push correction.
static inline SVECTOR* _actorContactGetLastPushStep(void)
{
    return &ActorContact_ScratchPosition;
}

/// Initializes a flare quad's command and colors, with vertex 2 lit and a black rim.
static void _akropolisRoofGardenInitializeRoomTask(Task* task);
static void _akropolisRoofGardenStartArrivalScene(Task* task);

/// Room task states: initialize, start the arrival scene, release.
static const TaskFuncTable3 _gAkropolisRoofGardenRoomTaskStates = {
    { _akropolisRoofGardenInitializeRoomTask, _akropolisRoofGardenStartArrivalScene, taskKill },
};

static void _akropolisRoofGardenReleaseMaggotCaterpillarEntrance(void);

/// Sound-task actions passed by the arrival scene and its skip script.
enum {
    CUTSCENE_SOUND_OPENING = 0,
    CUTSCENE_SOUND_CLOSING = 1,
    CUTSCENE_SOUND_CANCEL  = 2,
};
static void _akropolisRoofGardenControlCutsceneSound(s32 soundAction);

static AnimationSet _gAkropolisRoofGardenArrivalPlayerAnim00Clip;
static AnimationSet _gAkropolisRoofGardenArrivalPlayerAnim01Clip;
static AnimationSet _gAkropolisRoofGardenArrivalPlayerAnim02Clip;

static s32  _akropolisRoofGardenResolveRoomTransition(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32  _akropolisRoofGardenRejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedSecondArg);
static s32  _akropolisRoofGardenHandleRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg);
static s32  _akropolisRoofGardenHandleSoundCue(Task* unusedTask, s32 unusedMessageId, s32 cueIndex, s32 unusedSecondArg);
static s32  _akropolisRoofGardenHandleRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 command, s32 unusedSecondArg);
static void _akropolisRoofGardenAmbienceTask(Task* task);
static void _akropolisRoofGardenCutsceneOpeningSoundTask(Task* task);
static void _akropolisRoofGardenCutsceneClosingSoundTask(Task* task);

#include "assets/rooms/akropolis/roof_garden/clips_1.inc"

/// Messages of the room task.
static TaskMessageEntry _gAkropolisRoofGardenRoomMessages[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _akropolisRoofGardenResolveRoomTransition },
    { DIRECTION_MESSAGE_ROOM_ACTION, _akropolisRoofGardenHandleRoomAction },
    { ROOM_MESSAGE_COMMAND, _akropolisRoofGardenHandleRoomCommand },
    { ROOM_MESSAGE_USE_KEY_ITEM, _akropolisRoofGardenRejectKeyItemUse },
    { ROOM_MESSAGE_SOUND, _akropolisRoofGardenHandleSoundCue },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Borrowed handle of the arrival scene's active opening or closing sound task.
static Task* _gAkropolisRoofGardenCutsceneSoundTask = NULL;

/// Bodyless sound-task recipes: 0 ambience, 1 arrival opening, 2 arrival closing.
static TaskDesc _gAkropolisRoofGardenSoundTaskDescriptors[4] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _akropolisRoofGardenAmbienceTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _akropolisRoofGardenCutsceneOpeningSoundTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _akropolisRoofGardenCutsceneClosingSoundTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

/// Borrowed player clips for arrival opening, closing and dialogue, in that order.

#define ROOM_PART _gAkropolisRoofGardenUnusedActorCommand
#include "assets/rooms/akropolis/roof_garden/events.inc"
#include "assets/rooms/akropolis/roof_garden/arrival_player_bank.inc"

// Arrival waits count interpreter ticks; sound cues use type-3 bank 0x108.

#define ROOM_PART _gAkropolisRoofGardenArrivalPlayerBankCopy
#include "assets/rooms/akropolis/roof_garden/events.inc"

/// Arrival-scene latch (0 unstarted, 1 attempted), reset by loading the overlay.
static s32 _gAkropolisRoofGardenArrivalSceneStarted = 0;

#include "assets/rooms/akropolis/roof_garden/clips_2.inc"

/// Player extension slots for the Maggot/Caterpillar entrance: empty, start, end.
#include "assets/rooms/akropolis/roof_garden/entrance_player_bank.inc"

#define ROOM_PART _gAkropolisRoofGardenEntrancePlayerBankCopy
#include "assets/rooms/akropolis/roof_garden/events.inc"
#include "assets/rooms/akropolis/roof_garden/markers.inc"
#include "assets/rooms/akropolis/roof_garden/view_masks.inc"
#include "assets/rooms/akropolis/roof_garden/samples.inc"
#include "assets/rooms/akropolis/roof_garden/header.inc"
#include "assets/rooms/akropolis/roof_garden/geometry.inc"
#include "assets/rooms/akropolis/roof_garden/view_boundaries.inc"
#include "assets/rooms/akropolis/roof_garden/action_triggers.inc"
#include "assets/rooms/akropolis/roof_garden/placements.inc"
#include "assets/rooms/akropolis/roof_garden/sprites.inc"
#include "assets/rooms/akropolis/roof_garden/lights.inc"
#include "assets/rooms/akropolis/roof_garden/cameras.inc"
#include "assets/rooms/akropolis/roof_garden/occluders.inc"
#include "assets/rooms/akropolis/roof_garden/surfaces.inc"
#include "assets/rooms/akropolis/roof_garden/armory_cardkey_pickup.inc"

/// Last ambience volume percentage requested by the view controller (0, 30 or 100).
static s32 _gAkropolisRoofGardenRequestedAmbiencePercent = 0;

SVECTOR ActorContact_ScratchPosition;

/// Adjusts the roof-garden ambience to the current camera view.
static void _akropolisRoofGardenAmbienceTask(Task* task)
{
    enum {
        INITIALIZE      = 0,
        UPDATE          = 1,
        SILENT          = 0,
        VIEW5_PERCENT   = 30,
        FULL_PERCENT    = 100,
        MAX_ATTENUATION = 127,
        FADE_UPDATES    = 30,
    };
    s32 volumePercent;
    u8  view;
    s32 previousPercent;

    switch (task->state) {
        case INITIALIZE:
            _gAkropolisRoofGardenRequestedAmbiencePercent = SILENT;
            task->state                                   = task->state + 1;
            return;
        case UPDATE:
            break;
        default:
            return;
    }

    view = gGameSession->location.view;
    if (view != 5) {
        volumePercent = SILENT;
        if (view == 7) {
            volumePercent = FULL_PERCENT;
        }
    } else {
        volumePercent = VIEW5_PERCENT;
    }

    previousPercent = _gAkropolisRoofGardenRequestedAmbiencePercent;
    if (volumePercent == previousPercent) {
        return;
    }
    // Convert the requested percentage to the driver's signed attenuation byte.
    if (previousPercent == SILENT) {
        sndEvtRequestScriptStart(SOUND_AKROPOLIS_ROOF_GARDEN_AMBIENCE, 0, (s8)(((FULL_PERCENT - volumePercent) * MAX_ATTENUATION) / FULL_PERCENT));
    } else if (volumePercent == SILENT) {
        sndEvtRequestScriptStop(SOUND_AKROPOLIS_ROOF_GARDEN_AMBIENCE, FADE_UPDATES);
    } else {
        sndEvtRequestScriptMix(SOUND_AKROPOLIS_ROOF_GARDEN_AMBIENCE, 0, (s8)(((FULL_PERCENT - volumePercent) * MAX_ATTENUATION) / FULL_PERCENT));
    }
    _gAkropolisRoofGardenRequestedAmbiencePercent = volumePercent;
}

/// The sanctuary's door: leaving that way passes the bomb scene by if it has not played, and hides the pickup.
static s32 _akropolisRoofGardenResolveRoomTransition(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    *reply = *request;
    if (request->areaId == AKROPOLIS_ROOM_ID_SANCTUARY && request->queryOnly == ROOM_EVENT_EXECUTE && gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_SANCTUARY_BOMB_SCENE_STATE) == 0) {
        gameFlagSetNibble(EVENT_FLAG_AKROPOLIS_SANCTUARY_BOMB_SCENE_STATE, AKROPOLIS_SANCTUARY_BOMB_SCENE_PASSED_BY);
        areaSetCurrentObjectState(AKROPOLIS_ROOF_GARDEN_PICKUP_OBJECT, AKROPOLIS_ROOF_GARDEN_PICKUP_STATE_HIDDEN);
    }
    return ROOM_EVENT_DEPART;
}

/// No key item can be used here.
static s32 _akropolisRoofGardenRejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Room actions 1 and 2, in placement variants 1 and 7: the first arms the enemy drop, the second starts it.
static s32 _akropolisRoofGardenHandleRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    enum {
        ACTION_ARM_ENTRANCE       = 1,
        ACTION_START_ENTRANCE     = 2,
        ENTRANCE_PROGRESS_CLEAR   = 0,
        ENTRANCE_PROGRESS_ARMED   = 1,
        ENTRANCE_PROGRESS_STARTED = 2,
        ACTION_REPLY              = 1,
    };
    switch (request->actionId) {
        case ACTION_ARM_ENTRANCE:
            if (((gGameSession->location.variant == 1) || (gGameSession->location.variant == 7)) && (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_ROOF_GARDEN_ENEMY_DROP_STATE) == ENTRANCE_PROGRESS_CLEAR)) {
                gameFlagSetNibble(EVENT_FLAG_AKROPOLIS_ROOF_GARDEN_ENEMY_DROP_STATE, ENTRANCE_PROGRESS_ARMED);
            }
            break;
        case ACTION_START_ENTRANCE:
            if (((gGameSession->location.variant == 1) || (gGameSession->location.variant == 7)) && (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_ROOF_GARDEN_ENEMY_DROP_STATE) == ENTRANCE_PROGRESS_ARMED)) {
                evsStartScript(_gAkropolisRoofGardenMaggotCaterpillarEntranceScript, EVENT_SCRIPT_HUD_KEEP);
                gameFlagSetNibble(EVENT_FLAG_AKROPOLIS_ROOF_GARDEN_ENEMY_DROP_STATE, ENTRANCE_PROGRESS_STARTED);
            }
            break;
    }
    return ACTION_REPLY;
}

/// Plays room sound cue 3, the door locked; cues 5 and 9 do nothing.
static s32 _akropolisRoofGardenHandleSoundCue(Task* unusedTask, s32 unusedMessageId, s32 cueIndex, s32 unusedSecondArg)
{
    switch (cueIndex) {
        case AKROPOLIS_ROOF_GARDEN_SOUND_DOOR_LOCKED:
            sndEvtRequestScriptStart(AKROPOLIS_ROOF_GARDEN_SOUND(AKROPOLIS_ROOF_GARDEN_SOUND_DOOR_LOCKED), 0, 0);
            break;
        case 5:
            break;
        case 9:
            break;
    }
    return 0;
}

/// Room command 2 is the locked door, with a pickup on the ground before it: "There's something on the ground!" (CAP 5)
/// while the pickup is there, "It won't open..." (CAP 2) after; 4 is the bomb ("Someone planted a bomb!", CAP 4), which
/// moves the story to the helicopter.
static s32 _akropolisRoofGardenHandleRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 command, s32 unusedSecondArg)
{
    enum {
        COMMAND_LOCKED_DOOR    = 2,
        COMMAND_BOMB           = 4,
        LOCKED_DOOR_PICKUP_CAP = 5,
        LOCKED_DOOR_CAP        = 2,
        LOCKED_DOOR_CAP_KEY    = 0,
    };
    if (command == COMMAND_LOCKED_DOOR) {
        // Retain the separate reads of the saved two-bit object state.
        if ((areaGetCurrentObjectState(AKROPOLIS_ROOF_GARDEN_PICKUP_OBJECT) == AKROPOLIS_ROOF_GARDEN_PICKUP_STATE_UNSET) || (areaGetCurrentObjectState(AKROPOLIS_ROOF_GARDEN_PICKUP_OBJECT) == AKROPOLIS_ROOF_GARDEN_PICKUP_STATE_PRESENT)) {
            capRunCommandWithTransition(LOCKED_DOOR_PICKUP_CAP);
        } else {
            capStartSequenceSlot(LOCKED_DOOR_CAP, CAP_PLAYBACK_DISPLAY_TRANSITION, LOCKED_DOOR_CAP_KEY);
        }
    }
    if (command == COMMAND_BOMB) {
        if (gameFlagGetNibble(EVENT_FLAG_STORY_DIALOGUE_INDEX) < AKROPOLIS_STORY_DIALOGUE_HELICOPTER) {
            gameFlagSetNibble(EVENT_FLAG_TELEPHONE_DIALOGUE_COUNTER, EVENT_FLAG_TELEPHONE_DIALOGUE_UNHEARD);
            gameFlagSetNibble(EVENT_FLAG_STORY_DIALOGUE_INDEX, AKROPOLIS_STORY_DIALOGUE_HELICOPTER);
        }
        gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
        capRunCommand(COMMAND_BOMB, CAP_PLAYBACK_IN_PLACE);
        gameFlagSetPackedByte(EVENT_FLAG_CURRENT_OBJECTIVE, EVENT_FLAG_OBJECTIVE_AKROPOLIS_ROOF_GARDEN_BOMB_SEEN);
    }
    return 0;
}

/// Plays the arrival cutscene's opening sound sequence and retires its task.
static void _akropolisRoofGardenCutsceneOpeningSoundTask(Task* task)
{
    enum {
        CUE6_TICK  = 20,
        CUE7_TICK  = 39,
        CUE8_TICK  = 57,
        CUE9_TICK  = 99,
        FINAL_TICK = 114,
    };
    switch (task->state) {
        case CUE6_TICK:
            sndEvtRequestScriptStart(AKROPOLIS_ROOF_GARDEN_SOUND(AKROPOLIS_ROOF_GARDEN_SOUND_ARRIVAL_MOVEMENT_1), 0, 0);
            break;
        case CUE7_TICK:
            sndEvtRequestScriptStart(AKROPOLIS_ROOF_GARDEN_SOUND(AKROPOLIS_ROOF_GARDEN_SOUND_ARRIVAL_MOVEMENT_2), 0, 0);
            break;
        case CUE8_TICK:
            sndEvtRequestScriptStart(AKROPOLIS_ROOF_GARDEN_SOUND(AKROPOLIS_ROOF_GARDEN_SOUND_ARRIVAL_MOVEMENT_3), 0, 0);
            break;
        case CUE9_TICK:
            sndEvtRequestScriptStart(AKROPOLIS_ROOF_GARDEN_SOUND(AKROPOLIS_ROOF_GARDEN_SOUND_ARRIVAL_MOVEMENT_4), 0, 0);
            break;
        case FINAL_TICK:
            sndEvtRequestScriptStart(AKROPOLIS_ROOF_GARDEN_SOUND(AKROPOLIS_ROOF_GARDEN_SOUND_ARRIVAL_MOVEMENT_5), 0, 0);
            _gAkropolisRoofGardenCutsceneSoundTask = NULL;
            taskKill(task);
            break;
    }
    task->state += 1;
}

/// Plays the arrival cutscene's closing sound sequence and retires its task.
static void _akropolisRoofGardenCutsceneClosingSoundTask(Task* task)
{
    enum {
        CUE15_TICK = 76,
        FINAL_TICK = 100,
    };
    switch (task->state) {
        case CUE15_TICK:
            sndEvtRequestScriptStart(AKROPOLIS_ROOF_GARDEN_SOUND(AKROPOLIS_ROOF_GARDEN_SOUND_STEP_1), 0, 0);
            break;
        case FINAL_TICK:
            sndEvtRequestScriptStart(AKROPOLIS_ROOF_GARDEN_SOUND(AKROPOLIS_ROOF_GARDEN_SOUND_STEP_2), 0, 0);
            _gAkropolisRoofGardenCutsceneSoundTask = NULL;
            taskKill(task);
            break;
    }
    task->state += 1;
}

/// Starts or cancels the arrival cutscene's singleton timed-sound task.
static void _akropolisRoofGardenControlCutsceneSound(s32 soundAction)
{
    enum {
        OPENING_SOUND_TASK_INDEX = 1,
        CLOSING_SOUND_TASK_INDEX = 2,
    };
    switch (soundAction) {
        case CUTSCENE_SOUND_OPENING:
            _gAkropolisRoofGardenCutsceneSoundTask = taskSpawnFromTable(_gAkropolisRoofGardenSoundTaskDescriptors, OPENING_SOUND_TASK_INDEX, 0, 0);
            break;
        case CUTSCENE_SOUND_CLOSING:
            _gAkropolisRoofGardenCutsceneSoundTask = taskSpawnFromTable(_gAkropolisRoofGardenSoundTaskDescriptors, CLOSING_SOUND_TASK_INDEX, 0, 0);
            break;
        case CUTSCENE_SOUND_CANCEL:
            if (_gAkropolisRoofGardenCutsceneSoundTask != NULL) {
                taskKill(_gAkropolisRoofGardenCutsceneSoundTask);
                _gAkropolisRoofGardenCutsceneSoundTask = NULL;
            }
            break;
    }
}

/// Installs the room's messages, moves the music on from the arrival's event and starts the ambience.
static void _akropolisRoofGardenInitializeRoomTask(Task* task)
{
    enum {
        ARRIVAL_MUSIC_EVENT = 6,
        ROOM_MUSIC_EVENT    = 5,
        AMBIENCE_TASK_INDEX = 0,
    };
    task->msgTable = _gAkropolisRoofGardenRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent == ARRIVAL_MUSIC_EVENT) {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = ROOM_MUSIC_EVENT;
    }
    taskSpawnFromTable(_gAkropolisRoofGardenSoundTaskDescriptors, AMBIENCE_TASK_INDEX, 0, 0);
    task->state += 1;
}

/// Starts the arrival scene once, on arrival through its warp; Rupert has then left the sanctuary.
static void _akropolisRoofGardenStartArrivalScene(Task* task)
{
    enum {
        NOT_STARTED = 0,
        STARTED     = 1,
    };
    GameLocationKey sanctuary;

    if ((gGameSession->location.warp == AKROPOLIS_ROOF_GARDEN_ARRIVAL_SCENE_WARP) && (_gAkropolisRoofGardenArrivalSceneStarted == NOT_STARTED)) {
        _gAkropolisRoofGardenArrivalSceneStarted = STARTED;
        evsStartScriptWithSkip(_gAkropolisRoofGardenArrivalScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gAkropolisRoofGardenArrivalSkipScript);
        gameFlagSetNibble(EVENT_FLAG_AKROPOLIS_SANCTUARY_RUPERT_LEFT, 1);
        sanctuary.stage = GAME_STAGE_AKROPOLIS;
        sanctuary.area  = AKROPOLIS_ROOM_ID_SANCTUARY;
        areaSetPlacementVariant(&sanctuary, AKROPOLIS_ROOF_GARDEN_SANCTUARY_RUPERT_LEFT_VARIANT, AREA_VARIANT_RESET_ALWAYS);
    }
}

void akropolisRoofGardenRoomTask(Task* task)
{
    TaskFuncTable3 states;

    states = _gAkropolisRoofGardenRoomTaskStates;
    states.funcs[task->state](task);
}

/// Releases the Maggot/Caterpillar scripted entrance for the roof garden encounter.
static void _akropolisRoofGardenReleaseMaggotCaterpillarEntrance(void)
{
    enum { ENTRANCE_RELEASED = 1 };

    gSceneCombatState.maggotCaterpillarEntranceReady = ENTRANCE_RELEASED;
}

/// Spawns a one-frame flare at (-4770, -220, -3865) in the emitter's local space.
static inline void _akropolisRoofGardenEmitFlare(EffectWork* emitterWork, GfxCoord* emitterCoord, s32 packedFlareOptions)
{
    emitterWork->move.vx = -4770;
    emitterWork->move.vy = -220;
    emitterWork->move.vz = -3865;
    effectSpawn(EFFECT_AKROPOLIS_ROOF_GARDEN_FLARE, emitterCoord, packedFlareOptions, &emitterWork->move);
}

void akropolisRoofGardenAmbientEffectsTask(Task* task)
{
    enum {
        EFFECT_INITIALIZE   = 0,
        GLOW_SCALE_SHIFT    = 16,
        GLOW_CELL_SHIFT     = 8,
        LARGE_GLOW_COUNT    = 2,
        UPPER_GLOW_INDEX    = 2,
        SMALL_GLOW_FIRST    = 3,
        LARGE_GLOW_SCALE    = 512,
        UPPER_GLOW_SCALE    = 1024,
        UPPER_GLOW_CELL     = 1,
        SMALL_GLOW_CELL     = 2,
        FLARE_DIAMOND_VIEWS = (1 << (5 - 1)) | (1 << (6 - 1)),
        FLARE_PULSE_STEPS   = 14,
        FLARE_RADIUS_SHIFT  = 8,
        FLARE_DIAMOND_SCALE = 6,
        FLARE_DISC_SCALE    = 3,
        FLARE_DISC_SHAPE    = 0x80000000,
    };
    EffectWork* work;
    GfxCoord*   coord;
    SVECTOR*    lightOffsets;
    s32         lightIndex;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (task->state == EFFECT_INITIALIZE) {
        // Place ten persistent glows, keeping one index across both table walks.
        for (lightIndex = 0; lightIndex < LARGE_GLOW_COUNT; lightIndex++) {
            effectSpawn(EFFECT_AKROPOLIS_ROOF_GARDEN_LIGHT_GLOW, coord, lightIndex + (LARGE_GLOW_SCALE << GLOW_SCALE_SHIFT), &_gAkropolisRoofGardenLightOffsets[lightIndex]);
        }
        lightOffsets = _gAkropolisRoofGardenLightOffsets;
        effectSpawn(EFFECT_AKROPOLIS_ROOF_GARDEN_LIGHT_GLOW, coord, (UPPER_GLOW_SCALE << GLOW_SCALE_SHIFT) | (UPPER_GLOW_CELL << GLOW_CELL_SHIFT) | UPPER_GLOW_INDEX, &lightOffsets[UPPER_GLOW_INDEX]);
        for (lightIndex = SMALL_GLOW_FIRST; lightIndex < ARRAY_SIZE(_gAkropolisRoofGardenLightOffsets); lightIndex++) {
            effectSpawn(EFFECT_AKROPOLIS_ROOF_GARDEN_LIGHT_GLOW, coord, lightIndex + (SMALL_GLOW_CELL << GLOW_CELL_SHIFT), &lightOffsets[lightIndex]);
        }
        task->state = task->state + 1;
    }
    // Emit one-frame flares only while room effects accept new spawns.
    if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        if ((FLARE_DIAMOND_VIEWS >> (gGameSession->location.view - 1)) & 1) {
            _akropolisRoofGardenEmitFlare(work, coord, (FLARE_DIAMOND_SCALE << FLARE_RADIUS_SHIFT) | FLARE_PULSE_STEPS);
        }
        if (gGameSession->location.view == 7) {
            _akropolisRoofGardenEmitFlare(work, coord, FLARE_DISC_SHAPE | (FLARE_DISC_SCALE << FLARE_RADIUS_SHIFT) | FLARE_PULSE_STEPS);
        }
    }
}

#include "../../../../shared/effects/acropolis_glow_sprite_bounds.inc.c"

void akropolisRoofGardenLightGlowTask(Task* task)
{
    enum {
        INITIALIZE    = 0,
        INDEX_MASK    = 0xF,
        CELL_SHIFT    = 8,
        CELL_MASK     = 3,
        CELL_COUNT    = 3,
        SCALE_SHIFT   = 16,
        SCALE_MASK    = 0xFFF,
        DEFAULT_SCALE = 640,
        FLICKER_STEP  = 16,
        CLUT_X_STRIDE = 16,
        CLUT_Y        = 270,
    };
    EffectWork*            work;
    GfxCoord*              coord;
    RoomGlowSpriteScratch* projection;
    POLY_FT4*              quad;
    s32                    greyLevel;
    s32                    flickerLevel;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_HIDDEN) {
        if ((_gAkropolisRoofGardenLightViewMasks[task->spawnArg1.value & INDEX_MASK] >> (gGameSession->location.view - 1)) & 1) {
            actorRenderComposeCoord(coord);
            projection = SCRATCH_STACK_RESERVE_BLOCK(RoomGlowSpriteScratch);
            if (task->state == INITIALIZE) {
                // Resting grey of each sheet cell; the room only spawns cells 0..2.
                u8 restingLevels[CELL_COUNT] = { 0x40, 0x60, 0x10 };

                // Decode once, retaining only the view-mask index in the spawn argument.
                if (task->spawnArg1.value & (SCALE_MASK << SCALE_SHIFT)) {
                    work->scale = (task->spawnArg1.value >> SCALE_SHIFT) & SCALE_MASK;
                } else {
                    work->scale = DEFAULT_SCALE;
                }
                work->angle           = (task->spawnArg1.value >> CELL_SHIFT) & CELL_MASK;
                task->spawnArg1.value = task->spawnArg1.value & INDEX_MASK;
                work->period          = restingLevels[work->angle];
                task->state++;
            }
            // Rejected projections still consume one frame-arena quad.
            projection->worldPos.vx = coord->workm.t[0];
            projection->worldPos.vy = coord->workm.t[1];
            projection->worldPos.vz = coord->workm.t[2];
            gte_SetTransMatrix(&GsWSMATRIX);
            gte_SetRotMatrix(&GsWSMATRIX);
            gte_ldv0(&projection->worldPos);
            gte_rtps();
            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            setPolyFT4(quad);
            gte_stsxy(&projection->screenPos);
            gte_stszotz(&projection->otz);
            if (projection->otz >= GLOW_MIN_DEPTH) {
                flickerLevel = ((u8)gDisplayState.animFrame & 1) * FLICKER_STEP;
                greyLevel    = (u8)work->period + flickerLevel;
                quad->tpage  = GLOW_FLARE_TEXTURE_PAGE;
                setRGB0(quad, greyLevel, greyLevel, greyLevel);
                setSemiTrans(quad, true);
                quad->clut = getClut(work->angle * CLUT_X_STRIDE, CLUT_Y);
                quad->u0   = work->angle * GLOW_FLARE_CELL_STRIDE;
                quad->v0   = 0;
                quad->u1   = work->angle * GLOW_FLARE_CELL_STRIDE + GLOW_FLARE_CELL_LAST_TEXEL;
                quad->v1   = 0;
                quad->u2   = work->angle * GLOW_FLARE_CELL_STRIDE;
                quad->v2   = GLOW_FLARE_CELL_LAST_TEXEL;
                quad->u3   = work->angle * GLOW_FLARE_CELL_STRIDE + GLOW_FLARE_CELL_LAST_TEXEL;
                quad->v3   = GLOW_FLARE_CELL_LAST_TEXEL;

                projection->halfExtent = (work->scale * GLOW_FLARE_CELL_LAST_TEXEL) / projection->otz;
                _acropolisGlowSetSpriteBounds(quad, projection);
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(projection->otz),
                        quad);
            }
            SCRATCH_STACK_RELEASE_BLOCK(RoomGlowSpriteScratch);
        }
    }
}

#define PULSING_FLARE_TASK                         akropolisRoofGardenFlareTask
#define PULSING_FLARE_SINE_SAMPLES                 _gAkropolisRoofGardenFlareSineSamples
#define PULSING_FLARE_BLUE(level)                  0 // The second color is green
#define PULSING_FLARE_OUTER_RADIUS(scale, options) ((scale) << 10)
#define PULSING_FLARE_INNER_RADIUS(scale, options) ((scale) << 7)
#include "../../../../shared/effects/pulsing_flare.inc.c"

#undef AKROPOLIS_ROOF_GARDEN_INIT_FLARE_QUAD

#include "../../../../shared/effects/falling_leaves_task.inc.c"

void akropolisRoofGardenLeafFallTask(Task* task)
{
    _leafFallTask(task);
}

#include "../../../../shared/effects/falling_leaves_draw.inc.c"

#include "../../../../shared/actor/actor_contacts_push_contact.inc.c"

#include "../../../../shared/actor/actor_contacts_push.inc.c"

void akropolisRoofGardenPickupModelTask(Task* task)
{
    enum {
        FIRST_VIEW = 5,
        VIEW_LIMIT = 8,
        HIDDEN     = 2,
    };
    Enemy*     enemy;
    TmdObject* tmd;
    s32        objectState;
    s32        mappedView;

    enemy = task->spawnArg2.pointer;
    tmd   = task->extra.tmd;
    // Only the low byte is the saved object ID; mapped views can differ from logical views.
    objectState = areaGetCurrentObjectState((u8)enemy->placeKey);
    mappedView  = viewGetMappedIndex();
    // Separate view tests preserve the two comparisons in the compiled draw gate.
    if (mappedView >= VIEW_LIMIT) {
        tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else if (mappedView < FIRST_VIEW) {
        tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else if (objectState == HIDDEN) {
        tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else {
        tmd->flags    = TMD_OBJECT_FLAGGED_PASS;
        tmd->otOffset = 0;
    }
}
