#include "rooms/akropolis_square.h"

#include "config.h"
#include "../../event_flags.h"
#include "../../room_ids.h"
#include "../../room_states.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/gtemac.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "actors/task_tables.h"

#include "gameplay/direction_input.h"
#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/area_transitions.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/player_actor.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effects.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/model_objects.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
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
#include "main/gfx.h"
#include "main/gfxgte.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/text.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/ui.h"
#include "main/ui_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "mapui/map_akropolis.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../../../shared/room/room_cutscene.h"

#include "assets/rooms/akropolis/square/declarations.inc"

static void _roomCutsceneTask(Task* task);

static void _roomCutsceneSoundTask(Task* task);

/// The cutscene's sound task while it runs, NULL otherwise.
extern Task* gRoomCutsceneSoundTask;

/// `planar_reflection.inc.c` defines its scale where it is included.
#define PLANAR_REFLECTION_DEFINE_SCALE_WITH_IMPLEMENTATION 1
#include "../../../../shared/room/planar_reflection.h"

#include "../../../../shared/room/telephone.h"

static void _akropolisSquareInitializeRoomTask(Task* task);
static void _akropolisSquareIdleRoomTask(Task* unusedTask);

static s32  _akropolisSquareResolveRoomTransition(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32  _akropolisSquareHandleRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 command, s32 unusedSecondArg);
static s32  _akropolisSquareHandleRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg);
static s32  _akropolisSquareRejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedSecondArg);
static s32  _akropolisSquareHandleSoundCue(Task* unusedTask, s32 unusedMessageId, s32 cueIndex, s32 unusedSecondArg);
static s32  _akropolisSquareSetTelephoneLightMode(Task* unusedTask, s32 unusedMessageId, s32 glowMode, s32 unusedSecondArg);
static void _akropolisSquareTelephoneRingTask(Task* task);
static void _akropolisSquareScrollingBackdropTask(Task* task);
static void _akropolisSquareCapViewSequenceTask(Task* task);
static void _akropolisSquareControlScrollingBackdrop(s32 action);

#include "../../../../shared/room/planar_reflection_data.inc.c"

/// Reflection tasks: the player (0) and their attachment (1).
static TaskDesc _gAkropolisSquareReflectionTaskDescs[2] = {
    { { { TASK_BODY_NONE, AKROPOLIS_SQUARE_REFLECTION_TASK_PRIORITY } }, akropolisSquarePlayerReflectionTask, { .value = 0 } },
    { { { TASK_BODY_NONE, AKROPOLIS_SQUARE_REFLECTION_TASK_PRIORITY } }, _planarReflectionAttachmentTask, { .value = 0 } },
};

/// The reflection task table, for `planar_reflection.inc.c`.
static inline TaskDesc* _planarReflectionGetTaskTable(void)
{
    return _gAkropolisSquareReflectionTaskDescs;
}

#include "../../../../shared/room/telephone_data.inc.c"

/// The room cutscene task and its sound task.
static TaskDesc _gRoomCutsceneTaskDescs[ROOM_CUTSCENE_TASK_DESCRIPTOR_COUNT] = {
    { { { TASK_BODY_NONE, ROOM_CUTSCENE_TASK_PRIORITY } }, _roomCutsceneTask, { .value = 0 } },
    { { { TASK_BODY_NONE, ROOM_CUTSCENE_TASK_PRIORITY } }, _roomCutsceneSoundTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

/// Messages of the room task.
static TaskMessageEntry _gAkropolisSquareRoomMessages[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _akropolisSquareResolveRoomTransition },
    { DIRECTION_MESSAGE_ROOM_ACTION, _akropolisSquareHandleRoomAction },
    { AKROPOLIS_SQUARE_MESSAGE_USE_KEY_ITEM, _akropolisSquareRejectKeyItemUse },
    { ROOM_MESSAGE_COMMAND, _akropolisSquareHandleRoomCommand },
    { ROOM_MESSAGE_SOUND, _akropolisSquareHandleSoundCue },
    { TASK_MESSAGE_TABLE_END, NULL },
};

#define ROOM_PART _gAkropolisSquareArrivalWeaponAnimation
#include "assets/rooms/akropolis/square/events.inc"
/// Room tasks: the telephone ring (0), the CAP view sequence (1, never spawned)
/// and the scrolling backdrop (2).
static TaskDesc _gAkropolisSquareSequenceTaskDescs[3] = {
    { { { TASK_BODY_NONE, AKROPOLIS_SQUARE_SEQUENCE_TASK_PRIORITY } }, _akropolisSquareTelephoneRingTask, { .value = 0 } },
    { { { TASK_BODY_NONE, AKROPOLIS_SQUARE_SEQUENCE_TASK_PRIORITY } }, _akropolisSquareCapViewSequenceTask, { .value = 0 } },
    { { { TASK_BODY_NONE, AKROPOLIS_SQUARE_BACKDROP_TASK_PRIORITY } }, _akropolisSquareScrollingBackdropTask, { .value = 0 } },
};

/// 1 while the phone rings and the call has not been answered.
static s32 _gAkropolisSquareTelephoneCallPending = 0;

/// 1 once the arrival scene has started.
static s32 _gAkropolisSquareArrivalEventStarted = 0;

/// Actions of the scrolling backdrop, sent by the arrival scene's script.
enum {
    AKROPOLIS_SQUARE_BACKDROP_ACTION_START = 0,
    AKROPOLIS_SQUARE_BACKDROP_ACTION_STOP  = 1,
};

#define ROOM_PART _gAkropolisSquareTelephoneSceneScript
#include "assets/rooms/akropolis/square/events.inc"
/// Sound scripts by room sound cue; cues 0 and 2..4 play nothing.
static s32 _gAkropolisSquareSoundCueScripts[9] = {
    SOUND_SCRIPT_REQUEST_NO_OP,
    AKROPOLIS_SQUARE_SOUND(AKROPOLIS_SQUARE_SOUND_CALL_START),
    SOUND_SCRIPT_REQUEST_NO_OP,
    SOUND_SCRIPT_REQUEST_NO_OP,
    SOUND_SCRIPT_REQUEST_NO_OP,
    AKROPOLIS_SQUARE_SOUND(AKROPOLIS_SQUARE_SOUND_SCRIPT_CUE_5),
    AKROPOLIS_SQUARE_SOUND(AKROPOLIS_SQUARE_SOUND_CALL_SCENE),
    AKROPOLIS_SQUARE_SOUND(AKROPOLIS_SQUARE_SOUND_CALL_END),
    AKROPOLIS_SQUARE_SOUND(AKROPOLIS_SQUARE_SOUND_SCRIPT_CUE_8),
};

/// Messages of the room effect task. The first glow ray reads this table's
/// terminator as a sample, so it stays right before `_gAkropolisSquareGlowSineSamples`.
static TaskMessageEntry _gAkropolisSquareRoomEffectMessages[2] = {
    { AKROPOLIS_SQUARE_MESSAGE_SET_TELEPHONE_LIGHT_MODE, _akropolisSquareSetTelephoneLightMode },
    { TASK_MESSAGE_TABLE_END, NULL },
};

#include "assets/rooms/akropolis/square/samples.inc"

/// The telephone light's mode: 0 idle, 1 ringing.
static s32 _gAkropolisSquareTelephoneLightMode = AKROPOLIS_SQUARE_TELEPHONE_LIGHT_IDLE;

#include "assets/rooms/akropolis/square/header.inc"
#include "assets/rooms/akropolis/square/geometry.inc"
#include "assets/rooms/akropolis/square/view_boundaries.inc"
#include "assets/rooms/akropolis/square/action_triggers.inc"
#include "assets/rooms/akropolis/square/placements.inc"
#include "assets/rooms/akropolis/square/lights.inc"
#include "assets/rooms/akropolis/square/ambient.inc"
#include "assets/rooms/akropolis/square/sprites.inc"
#include "assets/rooms/akropolis/square/cameras.inc"
#include "assets/rooms/akropolis/square/surfaces.inc"
#include "assets/rooms/akropolis/square/updates.inc"

/// Frames since the last ring.
static s32 _gAkropolisSquareTelephoneRingFrames = 0;

/// The scrolling backdrop task.
static Task* _gAkropolisSquareBackdropTask = NULL;

/// The backdrop's scroll position, from -320 to 0.
static s32 _gAkropolisSquareBackdropScrollX = 0;

/// Frames into the backdrop's delay or hold.
static s32 _gAkropolisSquareBackdropPhaseFrames = 0;

Task* gRoomCutsceneSoundTask = NULL;

/// The call cutscene's record.
static RoomCutsceneRecStorage _gAkropolisSquareCutsceneRec = { 0 };

/// The telephone's position, for the ring's pan and depth.
static GfxCoord _gAkropolisSquareTelephoneCoord = { 0 };

#include "../../../../shared/room/planar_reflection.inc.c"

void akropolisSquarePlayerReflectionTask(Task* reflectionTask)
{
    _planarReflectionPlayerTask(reflectionTask);
}

#undef PLANAR_REFLECTION_DEFINE_SCALE_WITH_IMPLEMENTATION

/// Bytes after the telephone menu title's terminator.
#define TELEPHONE_MENU_TITLE_TRAILING_BYTES "\xDC\xDD"
#include "../../../../shared/room/telephone.inc.c"
#undef TELEPHONE_MENU_TITLE_TRAILING_BYTES

void akropolisSquareTelephoneMenuTask(Task* task)
{
    _telephoneMenuTask(task);
}

#include "../../../../shared/room/telephone_panels.inc.c"

#include "../../../../shared/room/room_cutscene_task.inc.c"

/// Room task states: initialize, idle, tear down.
static const TaskFuncTable3 _gAkropolisSquareRoomStates = {
    {
        _akropolisSquareInitializeRoomTask,
        _akropolisSquareIdleRoomTask,
        taskKill,
    },
};

/// Resolves the square's exits. Leaving by the forked road or the east elevator
/// while the phone rings misses the call; the elevators are closed once the
/// cafeteria boss is dead; the forked road and the patio pick their room state.
static s32 _akropolisSquareResolveRoomTransition(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    // These unused aggregate stores are present in the original instruction stream.
    GameLocationKey retainedLocation;

    retainedLocation.stage = GAME_STAGE_AKROPOLIS;
    retainedLocation.area  = AKROPOLIS_ROOM_ID_CAFETERIA;
    *reply                 = *request;

    if (request->areaId == AKROPOLIS_ROOM_ID_FORKED_ROAD) {
        if (_gAkropolisSquareTelephoneCallPending && !request->queryOnly) {
            gameFlagSetNibble(EVENT_FLAG_TELEPHONE_DIALOGUE_COUNTER, EVENT_FLAG_TELEPHONE_DIALOGUE_UNHEARD);
            gameFlagSetNibble(EVENT_FLAG_STORY_DIALOGUE_INDEX, AKROPOLIS_STORY_DIALOGUE_LATE);
        }
        if (request->areaId == AKROPOLIS_ROOM_ID_FORKED_ROAD) {
            if (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_SECURITY_ROOM_PANEL_KEYS) & AKROPOLIS_SECURITY_ROOM_PANEL_KEY_BLUE_INSERTED) {
                reply->roomState = AKROPOLIS_ROOM_STATE_FORKED_ROAD_ESCALATOR_OPERATIONAL;
            }
        }
        return ROOM_EVENT_DEPART;
    }

    if (request->areaId == AKROPOLIS_ROOM_ID_EAST_ELEVATOR_HALL) {
        if (_gAkropolisSquareTelephoneCallPending && !request->queryOnly) {
            gameFlagSetNibble(EVENT_FLAG_TELEPHONE_DIALOGUE_COUNTER, EVENT_FLAG_TELEPHONE_DIALOGUE_UNHEARD);
            gameFlagSetNibble(EVENT_FLAG_STORY_DIALOGUE_INDEX, AKROPOLIS_STORY_DIALOGUE_LATE);
        }
        if (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_CAFETERIA_ROUTE_PROGRESS) < AKROPOLIS_CAFETERIA_ROUTE_BOSS_DEFEATED) {
            return ROOM_EVENT_DEPART;
        }
        if ((gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_CAFETERIA_ROUTE_PROGRESS) == AKROPOLIS_CAFETERIA_ROUTE_BOSS_DEFEATED) || (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_CAFETERIA_ROUTE_PROGRESS) >= AKROPOLIS_CAFETERIA_ROUTE_MIST_CONTACTED)) {
            if (!request->queryOnly) {
                // Mark the map and show the blocked-exit caption, retaining the exit.
                do {
                    gameFlagSetNibbleIfPresent(request->flagId, EVENT_FLAG_MAP_DOOR_LOCKED);
                    capRunCommandWithTransition(AKROPOLIS_SQUARE_ELEVATOR_BLOCKED_CAP_COMMAND);
                } while (0);
            }
            return ROOM_EVENT_STAY;
        }
    }

    if (request->areaId == AKROPOLIS_ROOM_ID_WEST_ELEVATOR_HALL) {
        if (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_CAFETERIA_ROUTE_PROGRESS) < AKROPOLIS_CAFETERIA_ROUTE_BOSS_DEFEATED) {
            return ROOM_EVENT_DEPART;
        }
        if ((gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_CAFETERIA_ROUTE_PROGRESS) == AKROPOLIS_CAFETERIA_ROUTE_BOSS_DEFEATED) || (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_CAFETERIA_ROUTE_PROGRESS) >= AKROPOLIS_CAFETERIA_ROUTE_MIST_CONTACTED)) {
            if (!request->queryOnly) {
                // Mark the map and show the blocked-exit caption, retaining the exit.
                do {
                    gameFlagSetNibbleIfPresent(request->flagId, EVENT_FLAG_MAP_DOOR_LOCKED);
                    capRunCommandWithTransition(AKROPOLIS_SQUARE_ELEVATOR_BLOCKED_CAP_COMMAND);
                } while (0);
            }
            return ROOM_EVENT_STAY;
        }
    }

    if (request->areaId == AKROPOLIS_ROOM_ID_PATIO) {
        if (!request->queryOnly) {
            if (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_CAFETERIA_ROUTE_PROGRESS) < AKROPOLIS_CAFETERIA_ROUTE_BOSS_DEFEATED) {
                if (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_PATIO_CUTSCENE_PROGRESS) <= AKROPOLIS_PATIO_CUTSCENE_START) {
                    reply->roomState = AKROPOLIS_ROOM_STATE_PATIO_PRE_SWAT_CUTSCENE;
                } else {
                    reply->roomState = AKROPOLIS_ROOM_STATE_PATIO_POST_SWAT_CUTSCENE;
                }
            } else {
                reply->roomState = AKROPOLIS_ROOM_STATE_PATIO_POST_CAFETERIA_BOSS;
            }
        }
    }
    return ROOM_EVENT_DEPART;
}

/// Starts the call cutscene, skipping its scene when the phone was ringing.
static inline void _akropolisSquareStartRoomCutscene(void)
{
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp == AKROPOLIS_SQUARE_CUTSCENE_ARRIVAL_WARP) {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp = AKROPOLIS_SQUARE_CUTSCENE_RESUME_WARP;
    }
    _gAkropolisSquareCutsceneRec.rec.view            = AKROPOLIS_SQUARE_CUTSCENE_VIEW;
    _gAkropolisSquareCutsceneRec.rec.capSlot         = AKROPOLIS_SQUARE_CUTSCENE_CAP_SLOT;
    _gAkropolisSquareCutsceneRec.rec.capFile         = AKROPOLIS_SQUARE_CUTSCENE_CAP_FILE;
    _gAkropolisSquareCutsceneRec.rec.startSound      = AKROPOLIS_SQUARE_SOUND(AKROPOLIS_SQUARE_SOUND_CALL_START);
    _gAkropolisSquareCutsceneRec.rec.endSound        = AKROPOLIS_SQUARE_SOUND(AKROPOLIS_SQUARE_SOUND_CALL_END);
    _gAkropolisSquareCutsceneRec.rec.sceneSound      = AKROPOLIS_SQUARE_SOUND(AKROPOLIS_SQUARE_SOUND_CALL_SCENE);
    _gAkropolisSquareCutsceneRec.rec.afterSceneSound = AKROPOLIS_SQUARE_SOUND(AKROPOLIS_SQUARE_SOUND_CALL_AFTER);
    _gAkropolisSquareCutsceneRec.rec.skipScene       = _gAkropolisSquareTelephoneCallPending;
    _gAkropolisSquareTelephoneCallPending            = 0;
    taskSpawnFromTable(_gRoomCutsceneTaskDescs, AKROPOLIS_SQUARE_CUTSCENE_TASK_ENTRY, AKROPOLIS_SQUARE_CUTSCENE_FOLLOWUP, &_gAkropolisSquareCutsceneRec.rec);
}

/// Room commands: 2 answers the telephone, 14 and 16 show one-time hints.
static s32 _akropolisSquareHandleRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 command, s32 unusedSecondArg)
{
    s32 hintCommand;

    if (command == AKROPOLIS_SQUARE_ROOM_COMMAND_CUTSCENE) {
        _akropolisSquareStartRoomCutscene();
    }
    if ((command == AKROPOLIS_SQUARE_ROOM_COMMAND_HINT_14) && (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_SQUARE_AMMO_BOX_HINT_SEEN) == 0)) {
        gameFlagSetNibble(EVENT_FLAG_AKROPOLIS_SQUARE_AMMO_BOX_HINT_SEEN, 1);
        capSpawnEventIfIdle(AKROPOLIS_SQUARE_ROOM_COMMAND_HINT_14, CAP_EVENT_PAUSE_ACTORS);
    }
    if ((command == AKROPOLIS_SQUARE_ROOM_COMMAND_CONTROLS_HINT) && (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_SQUARE_STAIRS_HINT_SEEN) == 0)) {
        gameFlagSetNibble(EVENT_FLAG_AKROPOLIS_SQUARE_STAIRS_HINT_SEEN, 1);
        hintCommand = AKROPOLIS_SQUARE_CONTROLS_HINT_ALTERNATE_COMMAND;
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.buttonLayout != 1) {
            hintCommand = AKROPOLIS_SQUARE_ROOM_COMMAND_CONTROLS_HINT;
        }
        capSpawnEventIfIdle(hintCommand, CAP_EVENT_PAUSE_ACTORS);
    }
    return 0;
}
/// Rings the telephone once, from its position.
static inline void _akropolisSquareRingTelephone(s32 resetRepeatFrames)
{
    s32 soundPan;

    _gAkropolisSquareTelephoneCoord.coord.t[0] = AKROPOLIS_SQUARE_TELEPHONE_X;
    _gAkropolisSquareTelephoneCoord.coord.t[1] = AKROPOLIS_SQUARE_TELEPHONE_Y;
    _gAkropolisSquareTelephoneCoord.coord.t[2] = AKROPOLIS_SQUARE_TELEPHONE_Z;
    if (resetRepeatFrames) {
        _gAkropolisSquareTelephoneRingFrames = 0;
    }
    _gAkropolisSquareTelephoneCoord.parent = &gGfxViewCoord;
    actorRenderComposeCoord(&_gAkropolisSquareTelephoneCoord);
    soundPan = worldCoordGetOriginAudioPan(&_gAkropolisSquareTelephoneCoord);
    sndEvtRequestScriptStart(AKROPOLIS_SQUARE_SOUND(AKROPOLIS_SQUARE_SOUND_TELEPHONE_RING), (s8)soundPan,
                             (s8)worldCoordGetOriginAudioDepth(&_gAkropolisSquareTelephoneCoord));
}

/// Plays the telephone scene, then rings until the call is answered or the
/// player leaves the phone's views.
static void _akropolisSquareTelephoneRingTask(Task* task)
{
    enum {
        AKROPOLIS_SQUARE_TELEPHONE_START          = 0,
        AKROPOLIS_SQUARE_TELEPHONE_STARTUP_TICK_1 = 1,
        AKROPOLIS_SQUARE_TELEPHONE_STARTUP_TICK_2 = 2,
        AKROPOLIS_SQUARE_TELEPHONE_FIRST_RING     = 3,
        AKROPOLIS_SQUARE_TELEPHONE_WAIT_EVENT     = 4,
        AKROPOLIS_SQUARE_TELEPHONE_WAIT_HANDOFF   = 5,
    };
    s32 repeatFrames;

    switch (task->state) {
        case AKROPOLIS_SQUARE_TELEPHONE_START:
            // Hold the player while the opening camera and telephone scene script runs.
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            _gAkropolisSquareTelephoneCallPending = 1;
            _gAkropolisSquareTelephoneRingFrames  = 0;
            evsStartScriptWithSkip(_gAkropolisSquareTelephoneSceneScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gAkropolisSquareTelephoneSceneSkipScript);
            task->state += 1;
            return;

        case AKROPOLIS_SQUARE_TELEPHONE_FIRST_RING:
            _akropolisSquareRingTelephone(0);
            task->state += 1;
            return;

        case AKROPOLIS_SQUARE_TELEPHONE_WAIT_EVENT:
            repeatFrames                         = _gAkropolisSquareTelephoneRingFrames + 1;
            _gAkropolisSquareTelephoneRingFrames = repeatFrames;
            if (repeatFrames >= AKROPOLIS_SQUARE_TELEPHONE_RING_FRAMES) {
                _akropolisSquareRingTelephone(1);
            }
            if (gGameSession->eventState != 0) {
                return;
            }
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
            /* fallthrough */

        case AKROPOLIS_SQUARE_TELEPHONE_STARTUP_TICK_1:
        case AKROPOLIS_SQUARE_TELEPHONE_STARTUP_TICK_2:
            task->state += 1;
            return;

        case AKROPOLIS_SQUARE_TELEPHONE_WAIT_HANDOFF:
            // Stop ringing after leaving views 5..7/9 or once the call is answered.
            if (((u32)(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view - AKROPOLIS_SQUARE_TELEPHONE_FIRST_FOLLOWUP_VIEW) >= (u32)AKROPOLIS_SQUARE_TELEPHONE_FOLLOWUP_VIEW_COUNT &&
                 gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view != AKROPOLIS_SQUARE_TELEPHONE_SCENE_VIEW) ||
                _gAkropolisSquareTelephoneCallPending == 0) {
                if (_gAkropolisSquareTelephoneCallPending != 0) {
                    gameFlagSetNibble(EVENT_FLAG_TELEPHONE_DIALOGUE_COUNTER, EVENT_FLAG_TELEPHONE_DIALOGUE_UNHEARD);
                    gameFlagSetNibble(EVENT_FLAG_STORY_DIALOGUE_INDEX, AKROPOLIS_STORY_DIALOGUE_LATE);
                    _gAkropolisSquareTelephoneCallPending = 0;
                }
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_ROOM_EFFECT), AKROPOLIS_SQUARE_MESSAGE_SET_TELEPHONE_LIGHT_MODE, 0, 0);
                sndEvtRequestScriptStop(AKROPOLIS_SQUARE_SOUND(AKROPOLIS_SQUARE_SOUND_TELEPHONE_RING), SOUND_SCRIPT_STOP_KEEP_RELEASE);
                taskKill(task);
                return;
            }
            repeatFrames                         = _gAkropolisSquareTelephoneRingFrames + 1;
            _gAkropolisSquareTelephoneRingFrames = repeatFrames;
            if (repeatFrames >= AKROPOLIS_SQUARE_TELEPHONE_RING_FRAMES) {
                _akropolisSquareRingTelephone(1);
            }
            break;
    }
}
/// The arrival scene's backdrop: pans a 640-pixel image by 320, then cuts to view 13.
static void _akropolisSquareScrollingBackdropTask(Task* task)
{
    enum {
        AKROPOLIS_SQUARE_BACKDROP_START          = 0,
        AKROPOLIS_SQUARE_BACKDROP_DELAY          = 1,
        AKROPOLIS_SQUARE_BACKDROP_SCROLL         = 2,
        AKROPOLIS_SQUARE_BACKDROP_HOLD           = 3,
        AKROPOLIS_SQUARE_BACKDROP_FINISH         = 4,
        AKROPOLIS_SQUARE_BACKDROP_STRIP_COUNT    = 3,
        AKROPOLIS_SQUARE_BACKDROP_ORDERING_DEPTH = 4,
        AKROPOLIS_SQUARE_BACKDROP_TEXTURE_8_BIT  = 1,
    };
    SPRT*     sprite;
    DR_TPAGE* drawMode;
    s32       screenX;
    s32       stripIndex;
    s32       texturePageX;
    s32       delayFrames;
    s32       holdFrames;
    s32       scrollX;

    switch (task->state) {
        case AKROPOLIS_SQUARE_BACKDROP_START:
            _gAkropolisSquareBackdropScrollX     = -0x140;
            _gAkropolisSquareBackdropPhaseFrames = 0;
            task->state                         += 1;
            break;

        case AKROPOLIS_SQUARE_BACKDROP_DELAY:
            delayFrames                          = _gAkropolisSquareBackdropPhaseFrames + 1;
            _gAkropolisSquareBackdropPhaseFrames = delayFrames;
            if (delayFrames >= AKROPOLIS_SQUARE_BACKDROP_DELAY_FRAMES) {
                task->state += 1;
            }
            break;

        case AKROPOLIS_SQUARE_BACKDROP_SCROLL:
            scrollX                          = _gAkropolisSquareBackdropScrollX + 1;
            _gAkropolisSquareBackdropScrollX = scrollX;
            if (scrollX >= 0) {
                _gAkropolisSquareBackdropPhaseFrames = 0;
                task->state                         += 1;
            }
            break;

        case AKROPOLIS_SQUARE_BACKDROP_HOLD:
            holdFrames                           = _gAkropolisSquareBackdropPhaseFrames + 1;
            _gAkropolisSquareBackdropPhaseFrames = holdFrames;
            if (holdFrames >= AKROPOLIS_SQUARE_BACKDROP_HOLD_FRAMES) {
                task->state += 1;
            }
            break;

        case AKROPOLIS_SQUARE_BACKDROP_FINISH:
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = AKROPOLIS_SQUARE_BACKDROP_FINISH_VIEW;
            taskKill(task);
            break;
    }

    // Draw even on the final tick; prepend each texture page before its sprite.
    screenX = _gAkropolisSquareBackdropScrollX;
    for (stripIndex = 0; stripIndex < AKROPOLIS_SQUARE_BACKDROP_STRIP_COUNT; stripIndex++) {
        texturePageX   = 0x1C0 + stripIndex * 0x80;
        sprite         = gGpuPrimCursor;
        gGpuPrimCursor = sprite + 1;
        setSprt(sprite);
        setShadeTex(sprite, 1);
        sprite->x0 = screenX - 0xA0;
        sprite->y0 = -0x78;
        sprite->u0 = 0;
        sprite->v0 = 0;
        if (stripIndex == AKROPOLIS_SQUARE_BACKDROP_STRIP_COUNT - 1) {
            sprite->w = 0x80;
            sprite->h = 0xF0;
        } else {
            sprite->w = 0x100;
            sprite->h = 0xF0;
        }
        sprite->clut = GetClut(0, 0xFF);
        addPrim(&gGpuCurrentOt[AKROPOLIS_SQUARE_BACKDROP_ORDERING_DEPTH], sprite);

        drawMode       = gGpuPrimCursor;
        gGpuPrimCursor = drawMode + 1;
        setDrawTPage(drawMode, 0, 1, GetTPage(AKROPOLIS_SQUARE_BACKDROP_TEXTURE_8_BIT, GPU_BLEND_AVERAGE, texturePageX, 0x100));
        addPrim(&gGpuCurrentOt[AKROPOLIS_SQUARE_BACKDROP_ORDERING_DEPTH], drawMode);

        screenX += 0x100;
    }
}

#include "../../../../shared/room/room_cutscene_sound_task.inc.c"

/// Room action 0 plays CAP 5.
static s32 _akropolisSquareHandleRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    if (request->actionId == AKROPOLIS_SQUARE_ACTION_START_EVENT) {
        capSpawnEventIfIdle(AKROPOLIS_SQUARE_ACTION_CAP_EVENT, CAP_EVENT_NO_FLAGS);
    }
    return 0;
}

/// No key item can be used in the square.
static s32 _akropolisSquareRejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedSecondArg)
{
    return 0;
}

/// Plays a room sound cue.
static s32 _akropolisSquareHandleSoundCue(Task* unusedTask, s32 unusedMessageId, s32 cueIndex, s32 unusedSecondArg)
{
    sndEvtRequestScriptStart(_gAkropolisSquareSoundCueScripts[cueIndex], 0, 0);
    return 0;
}

/// Plays CAP 5 over views 7 and 8, then makes the Akropolis map available.
/// Never spawned.
static void _akropolisSquareCapViewSequenceTask(Task* task)
{
    enum {
        AKROPOLIS_SQUARE_CAP_VIEW_FIRST_START    = 0,
        AKROPOLIS_SQUARE_CAP_VIEW_FIRST_SELECT   = 1,
        AKROPOLIS_SQUARE_CAP_VIEW_FIRST_FINISH   = 2,
        AKROPOLIS_SQUARE_CAP_VIEW_SECOND_START   = 3,
        AKROPOLIS_SQUARE_CAP_VIEW_SECOND_DELAY_1 = 4,
        AKROPOLIS_SQUARE_CAP_VIEW_SECOND_DELAY_2 = 5,
        AKROPOLIS_SQUARE_CAP_VIEW_SECOND_SELECT  = 6,
        AKROPOLIS_SQUARE_CAP_VIEW_SECOND_FINISH  = 7,
    };
    switch (task->state) {
        case AKROPOLIS_SQUARE_CAP_VIEW_FIRST_START:
            capRunCommandWithTransition(AKROPOLIS_SQUARE_CAP_VIEW_COMMAND);
            task->state++;
            return;
        case AKROPOLIS_SQUARE_CAP_VIEW_FIRST_SELECT:
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = AKROPOLIS_SQUARE_CAP_VIEW_FIRST;
            task->state++;
            return;
        case AKROPOLIS_SQUARE_CAP_VIEW_SECOND_START:
            capRunCommandWithTransition(AKROPOLIS_SQUARE_CAP_VIEW_COMMAND);
            task->state++;
            return;
        case AKROPOLIS_SQUARE_CAP_VIEW_SECOND_DELAY_1:
        case AKROPOLIS_SQUARE_CAP_VIEW_SECOND_DELAY_2:
            task->state++;
            return;
        case AKROPOLIS_SQUARE_CAP_VIEW_SECOND_SELECT:
            capRunCommandWithTransition(AKROPOLIS_SQUARE_CAP_VIEW_COMMAND);
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = AKROPOLIS_SQUARE_CAP_VIEW_SECOND;
            task->state++;
            return;
        case AKROPOLIS_SQUARE_CAP_VIEW_FIRST_FINISH:
        case AKROPOLIS_SQUARE_CAP_VIEW_SECOND_FINISH:
            gameFlagSetNibble(EVENT_FLAG_MAP_AKROPOLIS_AVAILABLE, 1);
            taskKill(task);
            return;
    }
}

/// Starts (0) or stops (1) the scrolling backdrop.
static void _akropolisSquareControlScrollingBackdrop(s32 action)
{
    switch (action) {
        case AKROPOLIS_SQUARE_BACKDROP_ACTION_START:
            _gAkropolisSquareBackdropTask = taskSpawnFromTable(_gAkropolisSquareSequenceTaskDescs, AKROPOLIS_SQUARE_BACKDROP_TASK_INDEX, 0, 0);
            return;
        case AKROPOLIS_SQUARE_BACKDROP_ACTION_STOP:
            taskKill(_gAkropolisSquareBackdropTask);
            return;
    }
}

/// Installs the room's messages.
static void _akropolisSquareInitializeRoomTask(Task* task)
{
    task->msgTable = _gAkropolisSquareRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state += 1;
}

/// Starts the arrival scene when arriving by warp 7.
static void _akropolisSquareIdleRoomTask(Task* unusedTask)
{
    // Removing this unused reservation changes the original 40-byte call frame.
    char retainedStackSpace[0x10];

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp == AKROPOLIS_SQUARE_CUTSCENE_ARRIVAL_WARP && _gAkropolisSquareArrivalEventStarted == 0) {
        _gAkropolisSquareArrivalEventStarted                = 1;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = AKROPOLIS_SQUARE_ARRIVAL_SCENE_EVENT;
        evsStartScriptWithSkip(_gAkropolisSquareArrivalEventScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gAkropolisSquareArrivalSkipScript);
    }
}

void akropolisSquareRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gAkropolisSquareRoomStates;
    stateHandlers.funcs[task->state](task);
}

s32 akropolisSquareStartTelephoneScene(s32 unused)
{
    enum {
        AKROPOLIS_SQUARE_TELEPHONE_NEWLY_ARMED   = 0,
        AKROPOLIS_SQUARE_TELEPHONE_ALREADY_ARMED = 1,
    };

    GameLocationKey key;

    if (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_SQUARE_TELEPHONE_SCENE_SEEN) == 0) {
        gameFlagSetNibble(EVENT_FLAG_AKROPOLIS_SQUARE_TELEPHONE_SCENE_SEEN, 1);
        key.stage = GAME_STAGE_AKROPOLIS;
        key.area  = AKROPOLIS_ROOM_ID_SQUARE;
        areaSetPlacementVariant(&key, AKROPOLIS_SQUARE_TELEPHONE_PLACEMENT_VARIANT, AREA_VARIANT_RESET_ALWAYS);
        gGameSession->eventState = 1;
        taskSpawnFromTable(_gAkropolisSquareSequenceTaskDescs, AKROPOLIS_SQUARE_TELEPHONE_TASK_INDEX, 0, 0);
        return AKROPOLIS_SQUARE_TELEPHONE_NEWLY_ARMED;
    }
    return AKROPOLIS_SQUARE_TELEPHONE_ALREADY_ARMED;
}

void akropolisSquareRoomEffectTask(Task* task)
{
    enum {
        AKROPOLIS_SQUARE_ROOM_EFFECT_INITIAL = 0,
        AKROPOLIS_SQUARE_ROOM_EFFECT_ACTIVE  = 1,
    };
    EffectWork* work;
    GfxCoord*   roomCoord;

    roomCoord = task->extra.coordBody->coord;
    work      = task->spawnArg2.pointer;
    switch (task->state) {
        case AKROPOLIS_SQUARE_ROOM_EFFECT_INITIAL:
            // Install the effect receiver before starting both reflection modes.
            task->msgTable = _gAkropolisSquareRoomEffectMessages;
            gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM_EFFECT);
            _gAkropolisSquareTelephoneLightMode = AKROPOLIS_SQUARE_TELEPHONE_LIGHT_IDLE;
            taskSpawn(AKROPOLIS_SQUARE_REFLECTION_TASK_BANK, AKROPOLIS_SQUARE_REFLECTION_TASK_SLOT, AKROPOLIS_SQUARE_REFLECTION_FLOOR, 0);
            taskSpawn(AKROPOLIS_SQUARE_REFLECTION_TASK_BANK, AKROPOLIS_SQUARE_REFLECTION_TASK_SLOT, AKROPOLIS_SQUARE_REFLECTION_PLANE, 0);
            task->state++;
            return;
        case AKROPOLIS_SQUARE_ROOM_EFFECT_ACTIVE:
            // Each view of the phone uses its own packed pulse/radius/colour/shape preset.
            if ((AKROPOLIS_SQUARE_TELEPHONE_LIGHT_VIEW_MASK >> (gGameSession->location.view - 1)) & 1) {
                work->move.vx = AKROPOLIS_SQUARE_TELEPHONE_X;
                work->move.vy = AKROPOLIS_SQUARE_TELEPHONE_Y;
                work->move.vz = AKROPOLIS_SQUARE_TELEPHONE_Z;
                effectSpawn(EFFECT_AKROPOLIS_SQUARE_TELEPHONE_LIGHT, roomCoord, _gAkropolisSquareTelephoneLightMode * AKROPOLIS_SQUARE_TELEPHONE_LIGHT_FRONT_MODE_DELTA + AKROPOLIS_SQUARE_TELEPHONE_LIGHT_FRONT_BASE,
                            &work->move);
            }
            if (gGameSession->location.view == AKROPOLIS_SQUARE_TELEPHONE_LIGHT_SIDE_VIEW) {
                work->move.vx = 0x18D2;
                work->move.vy = -0x100B;
                work->move.vz = 0x8AB;
                effectSpawn(EFFECT_AKROPOLIS_SQUARE_TELEPHONE_LIGHT, roomCoord, _gAkropolisSquareTelephoneLightMode * AKROPOLIS_SQUARE_TELEPHONE_LIGHT_SIDE_MODE_DELTA + AKROPOLIS_SQUARE_TELEPHONE_LIGHT_SIDE_BASE,
                            &work->move);
            }
            if (gGameSession->location.view == AKROPOLIS_SQUARE_TELEPHONE_LIGHT_ROUND_VIEW) {
                work->move.vx = 0x19AA;
                work->move.vy = -0xF96;
                work->move.vz = 0x8E8;
                effectSpawn(EFFECT_AKROPOLIS_SQUARE_TELEPHONE_LIGHT, roomCoord, _gAkropolisSquareTelephoneLightMode * AKROPOLIS_SQUARE_TELEPHONE_LIGHT_ROUND_MODE_DELTA + AKROPOLIS_SQUARE_TELEPHONE_LIGHT_ROUND_BASE,
                            &work->move);
            }
            return;
    }
}

/// Queues one primitive of the telephone light, blended additively.
static inline void _akropolisSquareQueueTelephoneLight(void* primitive, const s32* sortingDepth)
{
    // Sixteen depth units per tag, expressed as four-byte table offsets.
    enum { AKROPOLIS_SQUARE_GLOW_DEPTH_TO_BYTE_OFFSET_SHIFT = 2 };

    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(
                ((((u32)*sortingDepth << gDisplayState.otDepthShift) >> AKROPOLIS_SQUARE_GLOW_DEPTH_TO_BYTE_OFFSET_SHIFT) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
            primitive);
    // Prepending the draw mode after the primitive makes the GPU apply it first.
    gpuSetPrimitiveBlendMode(primitive, GPU_BLEND_ADD, *sortingDepth);
}

void akropolisSquareTelephoneLightTask(Task* task)
{
    enum {
        AKROPOLIS_SQUARE_GLOW_MIN_DEPTH            = 17,
        AKROPOLIS_SQUARE_GLOW_PULSE_RATE_MASK      = 0xFF,
        AKROPOLIS_SQUARE_GLOW_RADIUS_SHIFT         = 8,
        AKROPOLIS_SQUARE_GLOW_RADIUS_MASK          = 0xFF,
        AKROPOLIS_SQUARE_GLOW_CYAN_SHIFT           = 16,
        AKROPOLIS_SQUARE_GLOW_STREAKS              = 0x10000000,
        AKROPOLIS_SQUARE_GLOW_PULSE_FALLING        = 0x80,
        AKROPOLIS_SQUARE_GLOW_PULSE_RAMP_MASK      = 0x7F,
        AKROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS   = 12,
        AKROPOLIS_SQUARE_GLOW_FAN_RADIUS_FACTOR    = 0x600,
        AKROPOLIS_SQUARE_GLOW_RAY_RADIUS_FACTOR    = 0xC0,
        AKROPOLIS_SQUARE_GLOW_DIAMOND_RADIUS_SHIFT = 9
    };
    RoomGlowRadiiScratch* projection;
    POLY_G4*              quad;
    LINE_G3*              streak;
    GfxCoord*             effectCoord;
    void*                 effectWork;
    s32                   segmentIndex;
    s32                   pulsePhase;
    s32                   glowValue;
    s32                   radiusScale;
    s16                   intensity;
    s16                   cyan;
    s16                   halfIntensity;

    // Project the composed effect center after narrowing to game coordinates.
    effectCoord = task->extra.coordBody->coord;
    effectWork  = task->spawnArg2.pointer;
    actorRenderComposeCoord(effectCoord);
    projection              = SCRATCH_STACK_RESERVE_BLOCK(RoomGlowRadiiScratch);
    projection->worldPos.vx = (u16)effectCoord->workm.t[0];
    projection->worldPos.vy = (u16)effectCoord->workm.t[1];
    projection->worldPos.vz = (u16)effectCoord->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&projection->worldPos);
    gte_rtps();
    gte_stsxy(&projection->screenPos);
    gte_stszotz(&projection->otz);
    if (projection->otz >= AKROPOLIS_SQUARE_GLOW_MIN_DEPTH) {
        // Fold the frame phase into a triangular red or cyan brightness ramp.
        pulsePhase  = gDisplayState.animFrame;
        pulsePhase *= task->spawnArg1.value & AKROPOLIS_SQUARE_GLOW_PULSE_RATE_MASK;
        cyan        = (task->spawnArg1.value >> AKROPOLIS_SQUARE_GLOW_CYAN_SHIFT) & 1;
        if (pulsePhase & AKROPOLIS_SQUARE_GLOW_PULSE_FALLING) {
            glowValue = ~pulsePhase & AKROPOLIS_SQUARE_GLOW_PULSE_RAMP_MASK;
        } else {
            glowValue = pulsePhase & AKROPOLIS_SQUARE_GLOW_PULSE_RAMP_MASK;
        }
        intensity = glowValue * 2;
        glowValue = task->spawnArg1.value;
        // The sign bit selects the round fan; the other form is a flat diamond.
        if (glowValue < 0) {
            radiusScale             = (glowValue >> AKROPOLIS_SQUARE_GLOW_RADIUS_SHIFT) & AKROPOLIS_SQUARE_GLOW_RADIUS_MASK;
            projection->outerRadius = (radiusScale * AKROPOLIS_SQUARE_GLOW_FAN_RADIUS_FACTOR) / projection->otz;
            projection->innerRadius = (radiusScale * AKROPOLIS_SQUARE_GLOW_RAY_RADIUS_FACTOR) / projection->otz;
            for (segmentIndex = 0; segmentIndex < 0x10; segmentIndex += 2) {
                quad           = gGpuPrimCursor;
                gGpuPrimCursor = quad + 1;
                setPolyG4(quad);
                setRGB0(quad, 0, 0, 0);
                setRGB1(quad, 0, 0, 0);
                setRGB2(quad, (intensity * (cyan ^ 1)) >> 1, (cyan * intensity) >> 1, (cyan * intensity) >> 1);
                setRGB3(quad, 0, 0, 0);
                quad->x0 = projection->screenPos.vx + ((projection->outerRadius * _gAkropolisSquareGlowSineSamples[segmentIndex + 4]) >> AKROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS);
                quad->y0 = projection->screenPos.vy + ((projection->outerRadius * _gAkropolisSquareGlowSineSamples[segmentIndex]) >> AKROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS);
                quad->x1 = projection->screenPos.vx + ((projection->outerRadius * _gAkropolisSquareGlowSineSamples[segmentIndex + 5]) >> AKROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS);
                quad->y1 = projection->screenPos.vy + ((projection->outerRadius * _gAkropolisSquareGlowSineSamples[segmentIndex + 1]) >> AKROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS);
                quad->x2 = projection->screenPos.vx;
                quad->y2 = projection->screenPos.vy;
                quad->x3 = projection->screenPos.vx + ((projection->outerRadius * _gAkropolisSquareGlowSineSamples[segmentIndex + 6]) >> AKROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS);
                quad->y3 = projection->screenPos.vy + ((projection->outerRadius * _gAkropolisSquareGlowSineSamples[segmentIndex + 2]) >> AKROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS);
                _akropolisSquareQueueTelephoneLight(quad, &projection->otz);

                quad           = gGpuPrimCursor;
                gGpuPrimCursor = quad + 1;
                setPolyG4(quad);
                setRGB0(quad, 0, 0, 0);
                setRGB1(quad, 0, 0, 0);
                setRGB2(quad, intensity * (cyan ^ 1), cyan * intensity, cyan * intensity);
                setRGB3(quad, 0, 0, 0);
                quad->x0 = projection->screenPos.vx + ((projection->outerRadius * _gAkropolisSquareGlowSineSamples[segmentIndex + 4]) >> (AKROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS + 1));
                quad->y0 = projection->screenPos.vy + ((projection->outerRadius * _gAkropolisSquareGlowSineSamples[segmentIndex]) >> (AKROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS + 1));
                quad->x1 = projection->screenPos.vx + ((projection->outerRadius * _gAkropolisSquareGlowSineSamples[segmentIndex + 5]) >> (AKROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS + 1));
                quad->y1 = projection->screenPos.vy + ((projection->outerRadius * _gAkropolisSquareGlowSineSamples[segmentIndex + 1]) >> (AKROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS + 1));
                quad->x2 = projection->screenPos.vx;
                quad->y2 = projection->screenPos.vy;
                quad->x3 = projection->screenPos.vx + ((projection->outerRadius * _gAkropolisSquareGlowSineSamples[segmentIndex + 6]) >> (AKROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS + 1));
                quad->y3 = projection->screenPos.vy + ((projection->outerRadius * _gAkropolisSquareGlowSineSamples[segmentIndex + 2]) >> (AKROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS + 1));
                _akropolisSquareQueueTelephoneLight(quad, &projection->otz);
            }
            // Add two opposing pairs of long rays over the circular fan.
            halfIntensity = intensity >> 1;
            for (segmentIndex = 2; segmentIndex < 0x10; segmentIndex += 8) {
                quad           = gGpuPrimCursor;
                gGpuPrimCursor = quad + 1;
                setPolyG4(quad);
                setRGB0(quad, 0, 0, 0);
                setRGB1(quad, 0, 0, 0);
                setRGB2(quad, halfIntensity * (cyan ^ 1), cyan * halfIntensity, cyan * halfIntensity);
                setRGB3(quad, 0, 0, 0);
                quad->x0 = projection->screenPos.vx + ((projection->innerRadius * _gAkropolisSquareGlowSineSamples[segmentIndex]) >> AKROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS);
                // The first ray samples the preceding message terminator's zero callback halfword.
                quad->y0 = projection->screenPos.vy + ((projection->innerRadius * _gAkropolisSquareGlowSineSamples[segmentIndex - 4]) >> AKROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS);
                quad->x1 = projection->screenPos.vx + ((projection->outerRadius * _gAkropolisSquareGlowSineSamples[segmentIndex + 4]) >> (AKROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS - 1));
                quad->y1 = projection->screenPos.vy + ((projection->outerRadius * _gAkropolisSquareGlowSineSamples[segmentIndex]) >> (AKROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS - 1));
                quad->x2 = projection->screenPos.vx;
                quad->y2 = projection->screenPos.vy;
                quad->x3 = projection->screenPos.vx + ((projection->innerRadius * _gAkropolisSquareGlowSineSamples[segmentIndex + 8]) >> AKROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS);
                quad->y3 = projection->screenPos.vy + ((projection->innerRadius * _gAkropolisSquareGlowSineSamples[segmentIndex + 4]) >> AKROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS);
                _akropolisSquareQueueTelephoneLight(quad, &projection->otz);

                quad           = gGpuPrimCursor;
                gGpuPrimCursor = quad + 1;
                setPolyG4(quad);
                setRGB0(quad, 0, 0, 0);
                setRGB1(quad, 0, 0, 0);
                setRGB2(quad, halfIntensity * (cyan ^ 1), cyan * halfIntensity, cyan * halfIntensity);
                setRGB3(quad, 0, 0, 0);
                quad->x0 = projection->screenPos.vx + ((projection->innerRadius * _gAkropolisSquareGlowSineSamples[segmentIndex + 4]) >> (AKROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS + 1));
                quad->y0 = projection->screenPos.vy + ((projection->innerRadius * _gAkropolisSquareGlowSineSamples[segmentIndex]) >> (AKROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS + 1));
                quad->x1 = projection->screenPos.vx + ((projection->outerRadius * _gAkropolisSquareGlowSineSamples[segmentIndex + 8]) >> AKROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS);
                quad->y1 = projection->screenPos.vy + ((projection->outerRadius * _gAkropolisSquareGlowSineSamples[segmentIndex + 4]) >> AKROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS);
                quad->x2 = projection->screenPos.vx;
                quad->y2 = projection->screenPos.vy;
                quad->x3 = projection->screenPos.vx + ((projection->innerRadius * _gAkropolisSquareGlowSineSamples[segmentIndex + 0xC]) >> (AKROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS + 1));
                quad->y3 = projection->screenPos.vy + ((projection->innerRadius * _gAkropolisSquareGlowSineSamples[segmentIndex + 8]) >> (AKROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS + 1));
                _akropolisSquareQueueTelephoneLight(quad, &projection->otz);
            }
        } else {
            projection->outerRadius = (((glowValue >> AKROPOLIS_SQUARE_GLOW_RADIUS_SHIFT) & AKROPOLIS_SQUARE_GLOW_RADIUS_MASK) << AKROPOLIS_SQUARE_GLOW_DIAMOND_RADIUS_SHIFT) / projection->otz;
            for (segmentIndex = 0; segmentIndex < 2; segmentIndex++) {
                quad           = gGpuPrimCursor;
                gGpuPrimCursor = quad + 1;
                setPolyG4(quad);
                setRGB0(quad, 0, 0, 0);
                setRGB1(quad, 0, 0, 0);
                setRGB2(quad, intensity * (cyan ^ 1), cyan * intensity, cyan * intensity);
                setRGB3(quad, 0, 0, 0);
                quad->x0 = projection->screenPos.vx - projection->outerRadius;
                quad->x1 = quad->x2 = projection->screenPos.vx;
                quad->x3            = projection->screenPos.vx + projection->outerRadius;
                quad->y0 = quad->y2 = quad->y3 = projection->screenPos.vy;
                quad->y1                       = (projection->screenPos.vy - projection->outerRadius) + projection->outerRadius * (segmentIndex + segmentIndex);
                addPrim((&gGpuCurrentOt[((u32)projection->otz << gDisplayState.otDepthShift) >> 4 & 0x3FF]),
                        quad);
                gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, projection->otz);
            }
            if (task->spawnArg1.value & AKROPOLIS_SQUARE_GLOW_STREAKS) {
                for (segmentIndex = 0; segmentIndex < 2; segmentIndex++) {
                    streak         = gGpuPrimCursor;
                    gGpuPrimCursor = streak + 1;
                    setLineG3(streak);
                    setRGB0(streak, 0, 0, 0);
                    setRGB1(streak, intensity * (cyan ^ 1), cyan * intensity, cyan * intensity);
                    setRGB2(streak, 0, 0, 0);
                    streak->x0 = projection->screenPos.vx + projection->outerRadius * (segmentIndex * 3 - 1);
                    streak->y0 = projection->screenPos.vy - projection->outerRadius * (segmentIndex + 1);
                    streak->x1 = projection->screenPos.vx;
                    streak->y1 = projection->screenPos.vy;
                    streak->x2 = projection->screenPos.vx - projection->outerRadius * (segmentIndex * 3 - 1);
                    streak->y2 = projection->screenPos.vy + projection->outerRadius * (segmentIndex + 1);
                    _akropolisSquareQueueTelephoneLight(streak, &projection->otz);
                }
            }
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(RoomGlowRadiiScratch);
    // This is a one-frame counted effect, including when projection rejects it.
    effectKillTask(effectWork, task);
}

/// Sets the telephone light's mode.
static s32 _akropolisSquareSetTelephoneLightMode(Task* unusedTask, s32 unusedMessageId, s32 glowMode, s32 unusedSecondArg)
{
    _gAkropolisSquareTelephoneLightMode = glowMode;
    return 0;
}

/// Unused.
static void _akropolisSquareNoOp(void)
{
}
