#include "stages/akropolis/rooms/promenade.h"

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

#include "gameplay/actor_render.h"
#include "gameplay/captions.h"
#include "gameplay/direction.h"
#include "gameplay/display.h"
#include "gameplay/effects.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/gameflag.h"
#include "gameplay/items.h"
#include "gameplay/message.h"
#include "gameplay/pad_input.h"
#include "gameplay/pad_script.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/event_flags.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflag.h"
#include "main/gameflow.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/random.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stream.h"
#include "main/stream_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../../../shared/effects/glow_draw.h"
#include "../../../../shared/room/bridge_model.h"
#include "../../../../shared/effects/acropolis_glows.h"

#include "assets/rooms/akropolis/promenade/declarations.inc"
#include "assets/engine/animation/player_schema.h"

static void _akropolisPromenadeInitializeRoomTask(Task* task);
static void _akropolisPromenadeUpdateRoomProgress(Task* unusedTask);
static void _akropolisPromenadeUpdateBridgeVisibility(Task* task);

static void _akropolisPromenadeMoviePathTask(Task* task);
static void _akropolisPromenadeMovieSkipFadeOutTask(Task* task);
static void _akropolisPromenadeSceneFadeInTask(Task* task);

static s32  _akropolisPromenadeResolveRoomTransition(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32  _akropolisPromenadeRejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedSecondArg);
static s32  _akropolisPromenadeHandleRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 command, s32 unusedSecondArg);
static s32  _akropolisPromenadeHandleSoundCue(Task* unusedTask, s32 unusedMessageId, s32 cueIndex, s32 unusedSecondArg);
static s32  _akropolisPromenadeIgnoreRoomAction(Task* unusedTask, s32 unusedMessageId, s32 unusedRequest, s32 unusedSecondArg);
static void _akropolisPromenadeTranslucentPlaneTask(Task* task);

/// Overlay-lifetime arrival latch and battle follow-up states, stored as s32.
enum {
    ARRIVAL_MOVIE_NOT_STARTED = 0,
    ARRIVAL_MOVIE_STARTED     = 1,
    BATTLE_FOLLOWUP_INACTIVE  = 0,
    BATTLE_FOLLOWUP_PENDING   = 2,
};

/// Roles of the glow group's position slots.
enum {
    GLOW_DISC_POSITION = 0,
    GLOW_FIRST_STAR    = 1,
    GLOW_SECOND_STAR   = 2,
};

/// Live indices in the movie task descriptor table; slots 0 and 1 are unused.
enum {
    MOVIE_PATH_TASK       = 2,
    MOVIE_SKIP_FADE_TASK  = 3,
    MOVIE_SCENE_FADE_TASK = 4,
};

#include "assets/rooms/akropolis/bridge/water_surface.inc"

/// Room-message handlers for exits, CAP commands, actions, key items and sounds.
static TaskMessageEntry _gAkropolisPromenadeRoomMessages[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _akropolisPromenadeResolveRoomTransition },
    { ROOM_MESSAGE_COMMAND, _akropolisPromenadeHandleRoomCommand },
    { DIRECTION_MESSAGE_ROOM_ACTION, _akropolisPromenadeIgnoreRoomAction },
    { ROOM_MESSAGE_USE_KEY_ITEM, _akropolisPromenadeRejectKeyItemUse },
    { ROOM_MESSAGE_SOUND, _akropolisPromenadeHandleSoundCue },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Spawn recipe for the fixed translucent plane, followed by TASK_DESC_END.
static TaskDesc _gAkropolisPromenadeTranslucentPlaneTaskDescriptors[2] = {
    { { { TASK_BODY_TMD, TASK_PRIORITY_CONTROL } }, _akropolisPromenadeTranslucentPlaneTask, { .model = &_gAkropolisBridgeWaterSurfaceModel } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

#include "assets/rooms/akropolis/promenade/events.inc"
/// Warp-4 arrival movie latch (0 not started, 1 spawn already attempted).
static s32 _gAkropolisPromenadeArrivalMovieStarted = ARRIVAL_MOVIE_NOT_STARTED;

/// Battle follow-up phase (0 inactive, 1 engaged observed, 2 awaiting reset).
static s32 _gAkropolisPromenadeBattleFollowupState = BATTLE_FOLLOWUP_INACTIVE;

/// Bodyless movie-path and skip-fade spawn recipes, indexed by MOVIE_*_TASK.
static TaskDesc _gAkropolisPromenadeMovieTaskDescriptors[5] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, NULL, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, NULL, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _akropolisPromenadeMoviePathTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _akropolisPromenadeMovieSkipFadeOutTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _akropolisPromenadeSceneFadeInTask, { .value = 0 } },
};

/// Player-root positions for the warp-4 arrival movie, in whole room units.
static SVECTOR _gAkropolisPromenadeArrivalMoviePlayerPositions[300] = {
#include "assets/rooms/akropolis/promenade/arrival_player_path.inc"
};

/// X/Z unit signs of the ground-glow square's four corners in GPU strip order.
static EffectUnitQuadCorner _gAkropolisPromenadeGroundGlowUnitCorners[4] = {
    { -1, 1 },
    { 1, 1 },
    { -1, -1 },
    { 1, -1 },
};
#include "assets/rooms/akropolis/promenade/markers.inc"
#include "assets/rooms/akropolis/promenade/view_masks.inc"
#include "assets/rooms/akropolis/promenade/header.inc"
#include "assets/rooms/akropolis/promenade/geometry.inc"
#include "assets/rooms/akropolis/promenade/view_boundaries.inc"
#include "assets/rooms/akropolis/promenade/action_triggers.inc"
#include "assets/rooms/akropolis/promenade/placements.inc"
#include "assets/rooms/akropolis/promenade/lights.inc"
#include "assets/rooms/akropolis/promenade/sprites.inc"
#include "assets/rooms/akropolis/promenade/cameras.inc"
#include "assets/rooms/akropolis/promenade/vibrations.inc"
#include "assets/rooms/akropolis/promenade/surfaces.inc"

/// Last executed sanctuary-exit reply before bridge progress, with warp 3.
static RoomEventMsg _gAkropolisPromenadeSanctuaryExitReply;

/// Saved result of the room initializer's translucent-plane spawn, or NULL.
static Task* _gAkropolisPromenadeTranslucentPlaneTask;

/// Starts the warp-4 movie once and schedules the variant-1 battle follow-up.
static void _akropolisPromenadeUpdateRoomProgress(Task* unusedTask)
{
    enum {
        SCENE_EVENT_SANCTUARY  = 6,
        SCENE_EVENT_PROMENADE  = 5,
        BATTLE_VARIANT         = 1,
        ALL_MIDI_SEQUENCES     = 0,
        BATTLE_MIDI_FADE_TICKS = 60,
    };
    u8 placementVariant;
    u8 battlePhase;

    if (_gAkropolisPromenadeArrivalMovieStarted == ARRIVAL_MOVIE_NOT_STARTED) {
        if (gGameSession->location.warp == AKROPOLIS_PROMENADE_RIDE_ARRIVAL_WARP) {
            _gAkropolisPromenadeArrivalMovieStarted = ARRIVAL_MOVIE_STARTED;
            taskSpawnFromTable(_gAkropolisPromenadeMovieTaskDescriptors, MOVIE_PATH_TASK, 0, 0);
        }
    }
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent == SCENE_EVENT_SANCTUARY) {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = SCENE_EVENT_PROMENADE;
    }
    placementVariant = gGameSession->location.variant;
    if (placementVariant == BATTLE_VARIANT) {
        gGameSession->flowFlags = (GAME_SESSION_FLOW_SKIP_AREA_MUSIC | GAME_SESSION_FLOW_LOAD_AREA_MUSIC_ONLY);
        battlePhase             = gSceneCombatState.signals.bytes.battlePhase;
        if (battlePhase == placementVariant) {
            _gAkropolisPromenadeBattleFollowupState = battlePhase;
        }
        if ((_gAkropolisPromenadeBattleFollowupState == placementVariant) && (battlePhase != _gAkropolisPromenadeBattleFollowupState)) {
            _gAkropolisPromenadeBattleFollowupState = BATTLE_FOLLOWUP_PENDING;
            sndEvtRequestMidiStop(ALL_MIDI_SEQUENCES, BATTLE_MIDI_FADE_TICKS);
        }
        if ((_gAkropolisPromenadeBattleFollowupState == BATTLE_FOLLOWUP_PENDING) && (gGameSession->battleResetPending != 0)) {
            _gAkropolisPromenadeBattleFollowupState = BATTLE_FOLLOWUP_INACTIVE;
            evsStartScriptWithSkip(_gAkropolisPromenadeBattleFollowupScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gAkropolisPromenadeBattleFollowupSkipScript);
        }
    }
}

/// The doors: the observatory's plays the ride up when the route reaches it; the sanctuary's, before the bridge has
/// any progress, walks in with its scene; the bridge's is refused until then, and leads to the bridge's state.
static s32 _akropolisPromenadeResolveRoomTransition(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    // Removing this unused reservation changes the original call frame.
    RoomEventMsg retainedEventSpace;
    u16          destinationArea;

    *reply = *request;
    if (request->areaId == AKROPOLIS_ROOM_ID_OBSERVATORY && request->queryOnly == ROOM_EVENT_EXECUTE) {
        if (gDisplayState.debugMode < 0 || gStreamInterFile.startSector == 0) {
            reply->warp = AKROPOLIS_PROMENADE_OBSERVATORY_STAIRS_WARP;
        }
        if (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_SANCTUARY_ROUTE_PROGRESS) == AKROPOLIS_SANCTUARY_ROUTE_PROMENADE_ENTERED) {
            gameFlagSetNibble(EVENT_FLAG_AKROPOLIS_SANCTUARY_ROUTE_PROGRESS, AKROPOLIS_SANCTUARY_ROUTE_SANCTUARY_REACHED);
        } else {
            reply->warp = AKROPOLIS_PROMENADE_OBSERVATORY_STAIRS_WARP;
        }
    }
    if (request->areaId == AKROPOLIS_ROOM_ID_SANCTUARY && gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_BRIDGE_PROGRESS) == AKROPOLIS_BRIDGE_NOT_STARTED) {
        if (request->queryOnly == ROOM_EVENT_EXECUTE) {
            reply->warp                                         = AKROPOLIS_PROMENADE_SANCTUARY_SCENE_WARP;
            _gAkropolisPromenadeSanctuaryExitReply              = *reply;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = AKROPOLIS_PROMENADE_SCENE_EVENT_SANCTUARY_APPROACH;
        }
        return ROOM_EVENT_DEPART;
    }
    destinationArea = request->areaId;
    if (destinationArea == AKROPOLIS_ROOM_ID_BRIDGE) {
        if (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_BRIDGE_PROGRESS) == AKROPOLIS_BRIDGE_NOT_STARTED) {
            if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                capSpawnEventIfIdle(AKROPOLIS_PROMENADE_BRIDGE_REFUSED_CAP_COMMAND, CAP_EVENT_PAUSE_ACTORS);
                gameFlagSetNibbleIfPresent(request->flagId, EVENT_FLAG_MAP_DOOR_LOCKED);
            }
            return ROOM_EVENT_STAY;
        }
        if (request->areaId == destinationArea) {
            if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                if (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_BRIDGE_PROGRESS) == AKROPOLIS_BRIDGE_RAISED) {
                    reply->roomState = AKROPOLIS_ROOM_STATE_BRIDGE_RAISED;
                } else {
                    reply->roomState = AKROPOLIS_ROOM_STATE_BRIDGE_LOWERED;
                }
            }
        }
    }
    return ROOM_EVENT_DEPART;
}

/// No key item can be used here.
static s32 _akropolisPromenadeRejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Room command 5, the dead SWAT member: "His body armor is shredded..." (CAP 5) until his armor is taken, then "A red
/// leaf on his body armor..." (CAP 9).
static s32 _akropolisPromenadeHandleRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 command, s32 unusedSecondArg)
{
    enum {
        ROOM_COMMAND_DEAD_SWAT = 5,
        DEAD_SWAT_OBJECT       = 21,
        DEAD_SWAT_ARMOR_TAKEN  = 2,
        DEAD_SWAT_CAP          = 5,
        DEAD_SWAT_AFTER_CAP    = 9,
    };
    if (command == ROOM_COMMAND_DEAD_SWAT) {
        if (areaGetCurrentObjectState(DEAD_SWAT_OBJECT) != DEAD_SWAT_ARMOR_TAKEN) {
            capStartSequenceSlot(DEAD_SWAT_CAP, CAP_PLAYBACK_DISPLAY_TRANSITION, 0);
        } else {
            capRunCommandWithTransition(DEAD_SWAT_AFTER_CAP);
        }
    }
    return 0;
}

/// Room actions do nothing here, and the reply is left unset.
static s32 _akropolisPromenadeIgnoreRoomAction(Task* unusedTask, s32 unusedMessageId, s32 unusedRequest, s32 unusedSecondArg)
{
}

/// Plays the instrument case opening for room sound cue 10, and closing for cue 103.
static s32 _akropolisPromenadeHandleSoundCue(Task* unusedTask, s32 unusedMessageId, s32 cueIndex, s32 unusedSecondArg)
{
    switch (cueIndex) {
        case AKROPOLIS_PROMENADE_SOUND_CUE_INSTRUMENT_CASE_OPEN:
            sndEvtRequestScriptStart(AKROPOLIS_PROMENADE_SOUND(AKROPOLIS_PROMENADE_SOUND_INSTRUMENT_CASE_OPEN), 0, 0);
            break;
        case AKROPOLIS_PROMENADE_SOUND_CUE_INSTRUMENT_CASE_CLOSE:
            sndEvtRequestScriptStart(AKROPOLIS_PROMENADE_SOUND(AKROPOLIS_PROMENADE_SOUND_INSTRUMENT_CASE_CLOSE), 0, 0);
            break;
    }
    return 0;
}

/// Room task states: initialize, update, release.
static const TaskFuncTable3 _gAkropolisPromenadeRoomStates = {
    { _akropolisPromenadeInitializeRoomTask, _akropolisPromenadeUpdateRoomProgress, taskKill },
};

/// Translucent-plane handlers indexed by task state: setup, visibility, teardown.
static const TaskFuncTable3 _gAkropolisPromenadeTranslucentPlaneStates = {
    { _bridgeModelSetup, _akropolisPromenadeUpdateBridgeVisibility, taskKill },
};

/// Runs the promenade's translucent plane model task.
static void _akropolisPromenadeTranslucentPlaneTask(Task* task)
{
    TaskFuncTable3 stateHandlers = _gAkropolisPromenadeTranslucentPlaneStates;
    stateHandlers.funcs[task->state](task);
}

/// Registers the promenade room receiver and spawns its translucent plane.
static void _akropolisPromenadeInitializeRoomTask(Task* task)
{
    enum { CAP_COMPLETION_SOUNDS_ENABLED = 1 };
    task->msgTable = _gAkropolisPromenadeRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    _gAkropolisPromenadeTranslucentPlaneTask = taskSpawnFromTable(_gAkropolisPromenadeTranslucentPlaneTaskDescriptors, 0, 0, 0);
    task->state                              = task->state + 1;
    gCapRoomCompletionSoundsEnabled          = CAP_COMPLETION_SOUNDS_ENABLED;
}

void akropolisPromenadeRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers = _gAkropolisPromenadeRoomStates;
    stateHandlers.funcs[task->state](task);
}

#include "../../../../shared/room/bridge_model_setup.inc.c"

/// Hides the bridge model in promenade view 5 and enables it in the other views.
static void _akropolisPromenadeUpdateBridgeVisibility(Task* task)
{
    enum { BRIDGE_HIDDEN_VIEW = 5 };
    TmdObject* bridge;
    GfxCoord*  coord;

    bridge = task->extra.tmd;
    coord  = bridge->coords;
    if (viewGetMappedIndex() == BRIDGE_HIDDEN_VIEW) {
        bridge->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else {
        bridge->flags = 0;
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Copies a movie-path sample into the player root, with the promenade Z offset.
#define APPLY_MOVIE_PATH_POSITION(movieWork, pathIndex, zOffset)                                                    \
    {                                                                                                               \
        (movieWork)->playerMtx->t[0] = _gAkropolisPromenadeArrivalMoviePlayerPositions[(pathIndex)].vx;             \
        (movieWork)->playerMtx->t[1] = _gAkropolisPromenadeArrivalMoviePlayerPositions[(pathIndex)].vy;             \
        (movieWork)->playerMtx->t[2] = _gAkropolisPromenadeArrivalMoviePlayerPositions[(pathIndex)].vz - (zOffset); \
    }

/// Moves the player along the promenade movie path and restores player control.
static void _akropolisPromenadeMoviePathTask(Task* task)
{
    enum {
        INITIAL              = 0,
        WAIT_READY           = 1,
        FOLLOW_PATH          = 2,
        WAIT_PLAYER          = 3,
        FRAME_ORIGIN         = 69,
        Z_OFFSET             = 200,
        EXIT_X               = 642,
        EXIT_Y               = 41,
        EXIT_YAW             = 3072,
        EXIT_VIEW            = 2,
        WALK_HANDOFF_SAMPLES = 6,
    };
    AnimationPlayRequest animationRequest;
    ActorTransform       playerPlacement;
    s32                  skipFadeResult;
    RoomMoviePathWork*   work;
    CdCmdQueue*          cdQueue;
    s32                  equippedWeapon;

    cdQueue = &gCdCmdQueue;
    work    = task->work;
    switch (task->state) {
        case INITIAL:
            // Capture the new work through task->work; the early local predates allocation.
            task->work = memCalloc(sizeof(RoomMoviePathWork), 0);
            if (task->work == NULL) {
                taskKill(task);
                break;
            }
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), CAP_CONTROL_MESSAGE_HIDE_HUD, 0, 0);
            ((RoomMoviePathWork*)task->work)->playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
            ((RoomMoviePathWork*)task->work)->playerMtx  = gPlayerStatus.coordMtx;
            equippedWeapon                               = gPlayerStatus.weapon;
            animationRequest.source.index                = PLAYER_ACTOR_SCENE_WEAPON_BANK(equippedWeapon);
            animationRequest.animationId                 = PLAYER_ANIMATION_IDLE;
            animationRequest.blend                       = ANIMATION_BLEND_RESET;
            animationRequest.blendFrames                 = 0;
            animationRequest.enableWorldCollision        = ANIMATION_WORLD_COLLISION_DISABLE;
            TASK_MESSAGE_DISPATCH_POINTER(((RoomMoviePathWork*)task->work)->playerTask, ANIMATION_MESSAGE_PLAY, &animationRequest, 0);
            padInputChangeSuppression(PAD_INPUT_SUPPRESSION_SET_AND_HOLD, PAD_INPUT_SUPPRESS_ACTIONS_AND_MENU);
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
            task->state                    = task->state + 1;
            break;

        case WAIT_READY:
            if (cdQueue->movieReady != 0) {
                work->padScriptTask           = padScriptSpawn(_gAkropolisPromenadeArrivalMovieVibrationCommands,
                                                               _gAkropolisPromenadeArrivalMovieVibrationSegments);
                gGameSession->padScriptFlags |= GAME_SESSION_PAD_SCRIPT_DURING_BATTLE_FREEZE;
                taskReparent(task, work->padScriptTask);
                task->state = task->state + 1;
            }
            break;

        case FOLLOW_PATH:
            // Follow decoded movie frames, giving a completed skip fade priority over walking.
            APPLY_MOVIE_PATH_POSITION(work, FRAME_ORIGIN - cdQueue->movieFrame, Z_OFFSET);
            if (work->skipFadeStarted != 0) {
                if (taskPollKill(work->skipFadeTask, &skipFadeResult) != 0) {
                    playerPlacement.pos.vx = EXIT_X;
                    playerPlacement.pos.vy = EXIT_Y;
                    playerPlacement.pos.vz = _gAkropolisPromenadeArrivalMoviePlayerPositions[FRAME_ORIGIN - cdQueue->movieFrame].vz - Z_OFFSET;
                    playerPlacement.rot.vz = 0;
                    playerPlacement.rot.vx = 0;
                    playerPlacement.rot.vy = EXIT_YAW;
                    TASK_MESSAGE_DISPATCH_POINTER(((RoomMoviePathWork*)task->work)->playerTask, GAME_ACTOR_MESSAGE_PLACE, &playerPlacement, 0);
                    taskSpawnFromTable(_gAkropolisPromenadeMovieTaskDescriptors, MOVIE_SCENE_FADE_TASK, 0, 0);
                    task->state = task->state + 1;
                    break;
                }
            } else if (padIsStartPressed() != 0) {
                work->skipFadeTask    = taskSpawnFromTable(_gAkropolisPromenadeMovieTaskDescriptors, MOVIE_SKIP_FADE_TASK, 0, 0);
                work->skipFadeStarted = 1;
            }
            // The walk handler reads XYZ only; rotation need not be initialized here.
            if ((FRAME_ORIGIN - cdQueue->movieFrame) < WALK_HANDOFF_SAMPLES) {
                playerPlacement.pos.vx = EXIT_X;
                playerPlacement.pos.vy = EXIT_Y;
                playerPlacement.pos.vz = _gAkropolisPromenadeArrivalMoviePlayerPositions[FRAME_ORIGIN - cdQueue->movieFrame].vz - Z_OFFSET;
                TASK_MESSAGE_DISPATCH_POINTER(((RoomMoviePathWork*)task->work)->playerTask, GAME_ACTOR_MESSAGE_MOVE_TO, &playerPlacement, 0);
                task->state = task->state + 1;
            }
            break;

        case WAIT_PLAYER:
            // Release scene holds only after the scripted player motion finishes.
            if (taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) == 0) {
                taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), CAP_CONTROL_MESSAGE_SHOW_HUD, 0, 0);
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = EXIT_VIEW;
                padInputChangeSuppression(PAD_INPUT_SUPPRESSION_CLEAR_ALIAS, PAD_INPUT_SUPPRESS_ACTIONS_AND_MENU);
                gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
                gGameSession->padScriptFlags  &= (0xFF ^ GAME_SESSION_PAD_SCRIPT_DURING_BATTLE_FREEZE);
                taskKill(task);
            }
            break;
    }
}

#undef APPLY_MOVIE_PATH_POSITION

/// Darkens the promenade movie before its skip transition.
static void _akropolisPromenadeMovieSkipFadeOutTask(Task* task)
{
    enum {
        FADE_STEP = 32,
        FADE_END  = 256
    };
    u8  darkness;
    s16 nextFadeCounter;

    darkness = (u8)task->killCountdown;
    fadeDrawOverlay(darkness, darkness, darkness, GPU_BLEND_SUBTRACT);
    nextFadeCounter     = (u16)task->killCountdown + FADE_STEP;
    task->killCountdown = nextFadeCounter;
    if (nextFadeCounter >= FADE_END) {
        taskRequestKill(task, 0);
    }
}

/// Reveals the scene after a movie skip by reducing a subtractive black overlay.
static void _akropolisPromenadeSceneFadeInTask(Task* task)
{
    enum {
        FADE_STEP = 32,
        FADE_END  = 256
    };
    u8  darkness;
    s16 nextFadeCounter;

    darkness = ~(u8)task->killCountdown;
    fadeDrawOverlay(darkness, darkness, darkness, GPU_BLEND_SUBTRACT);
    nextFadeCounter     = (u16)task->killCountdown + FADE_STEP;
    task->killCountdown = nextFadeCounter;
    if (nextFadeCounter >= FADE_END) {
        taskKill(task);
    }
}

void akropolisPromenadeRoomEffectTask(Task* task)
{
    enum {
        EFFECTS_SUPPRESSED_VIEW = 7,
        LAMP_SINGLE_END         = 3,
        LAMP_GROUP_FIRST        = 3,
        LAMP_GROUP_END          = 5,
        DRIP_VIEW_ENTRY_COUNT   = 40,
        DISC_RADIUS_SCALE       = 256,
        DISC_PACKED_COLOR       = 0x5C40,
    };
    GfxCoord*   roomCoord;
    EffectWork* work;
    u8          mappedView;
    s32         placementIndex;
    s32         viewMask;
    s16         previousView;

    roomCoord  = task->extra.coordBody->coord;
    work       = task->spawnArg2.pointer;
    mappedView = viewGetMappedIndex();
    if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        return;
    }
    work->age++;
    if (mappedView == EFFECTS_SUPPRESSED_VIEW) {
        return;
    }
    viewMask = 1 << (mappedView - 1);
    if (_gAkropolisPromenadeGlowViewMask & viewMask) {
        effectSpawn((EFFECT_AKROPOLIS_PROMENADE_GLOW_STAR | EFFECT_SPAWN_UNLIMITED), roomCoord, (s32)(work->age), &_gAkropolisPromenadeGlowPositions[GLOW_FIRST_STAR]);
        effectSpawn((EFFECT_AKROPOLIS_PROMENADE_GLOW_STAR | EFFECT_SPAWN_UNLIMITED), roomCoord, (s32)(work->age), &_gAkropolisPromenadeGlowPositions[GLOW_SECOND_STAR]);
        effectSpawn(EFFECT_AKROPOLIS_PROMENADE_GROUND_GLOW, roomCoord, (s32)(work->age), &_gAkropolisPromenadeGroundGlowPosition);
        glowDrawTintedDiscNoBias(&_gAkropolisPromenadeGlowPositions[GLOW_DISC_POSITION], DISC_RADIUS_SCALE, DISC_PACKED_COLOR);
    }
    // The grouped pairs cover lamp placements 3..10; placement 11 follows separately.
    for (placementIndex = 0; placementIndex < LAMP_SINGLE_END; placementIndex++) {
        if (_gAkropolisPromenadeLampGlowViewMasks[placementIndex] & viewMask) {
            effectSpawn(EFFECT_AKROPOLIS_PROMENADE_LAMP_GLOW, roomCoord, 0, &_gAkropolisPromenadeLampGlowPositions[placementIndex]);
        }
    }
    for (placementIndex = LAMP_GROUP_FIRST; placementIndex < LAMP_GROUP_END; placementIndex++) {
        if (_gAkropolisPromenadeLampGlowViewMasks[placementIndex] & viewMask) {
            effectSpawn(EFFECT_AKROPOLIS_PROMENADE_LAMP_GLOW, roomCoord, 1, &_gAkropolisPromenadeLampGlowPositions[placementIndex]);
        }
        if (_gAkropolisPromenadeLampGlowViewMasks[placementIndex + 2] & viewMask) {
            effectSpawn(EFFECT_AKROPOLIS_PROMENADE_LAMP_GLOW, roomCoord, 2, &_gAkropolisPromenadeLampGlowPositions[placementIndex + 2]);
        }
        if (_gAkropolisPromenadeLampGlowViewMasks[placementIndex + 4] & viewMask) {
            effectSpawn(EFFECT_AKROPOLIS_PROMENADE_LAMP_GLOW, roomCoord, 1, &_gAkropolisPromenadeLampGlowPositions[placementIndex + 4]);
        }
        if (_gAkropolisPromenadeLampGlowViewMasks[placementIndex + 6] & viewMask) {
            effectSpawn(EFFECT_AKROPOLIS_PROMENADE_LAMP_GLOW, roomCoord, 2, &_gAkropolisPromenadeLampGlowPositions[placementIndex + 6]);
        }
    }
    if (_gAkropolisPromenadeLampGlowViewMasks[11] & viewMask) {
        effectSpawn(EFFECT_AKROPOLIS_PROMENADE_LAMP_GLOW, roomCoord, 1, &_gAkropolisPromenadeLampGlowPositions[11]);
    }
    // Seed a drip field on entry; subsequent ticks add two replacements.
    if (_gAkropolisPromenadeDripViewMask & viewMask) {
        previousView = work->scale;
        if (previousView != mappedView) {
            for (placementIndex = 0; placementIndex < DRIP_VIEW_ENTRY_COUNT; placementIndex++) {
                effectSpawn(EFFECT_AKROPOLIS_PROMENADE_SCREEN_DRIP, roomCoord, (s32)(mappedView), NULL);
            }
        } else {
            effectSpawn(EFFECT_AKROPOLIS_PROMENADE_SCREEN_DRIP, roomCoord, (s32)(previousView), NULL);
            effectSpawn(EFFECT_AKROPOLIS_PROMENADE_SCREEN_DRIP, roomCoord, (s32)(previousView), NULL);
        }
    }
    work->scale = mappedView;
}

/// Seeds one screen drip's pixel position, lifetime, width and descent period.
static inline void _akropolisPromenadeInitializeScreenDrip(Task* task, EffectWork* work)
{
    enum {
        COLUMN_COUNT          = 240,
        FIRST_ROW             = 176,
        ROW_RANGE             = 64,
        MIN_LIFETIME_FRAMES   = 30,
        LIFETIME_RANGE_FRAMES = 90,
        MIN_WIDTH_PIXELS      = 16,
        WIDTH_RANGE_PIXELS    = 64,
        MAX_FRAMES_PER_ROW    = 4
    };
    u16 randomBits;

    RANDOM_LCG_ADVANCE();
    work->move.vx = RANDOM_LCG_DRAW() % COLUMN_COUNT;
    RANDOM_LCG_ADVANCE();
    work->move.vy = (RANDOM_LCG_DRAW() & (ROW_RANGE - 1)) + FIRST_ROW;
    RANDOM_LCG_ADVANCE();
    randomBits  = RANDOM_LCG_DRAW();
    work->scale = (u32)randomBits % LIFETIME_RANGE_FRAMES + MIN_LIFETIME_FRAMES;
    RANDOM_LCG_ADVANCE();
    work->angle = (RANDOM_LCG_DRAW() & (WIDTH_RANGE_PIXELS - 1)) + MIN_WIDTH_PIXELS;
    RANDOM_LCG_ADVANCE();
    work->period = (RANDOM_LCG_DRAW() & (MAX_FRAMES_PER_ROW - 1)) + 1;
    task->state++;
}

void akropolisPromenadeScreenDripTask(Task* task)
{
    enum {
        DEPTH_FIRST_ROW = 176,
        LAST_SOURCE_ROW = 239,
        START_DEPTH     = 0x500,
        DEPTH_PER_ROW   = 10,
        OT_DEPTH_SHIFT  = 4
    };
    EffectWork* work;
    RECT        sourceStrip;
    DR_MOVE*    movePacket;
    s32         bufferY;
    s32         x;
    s32         y;
    s32         onScreen;
    s32         depth;

    work    = task->spawnArg2.pointer;
    bufferY = gDisplayState.drawBuffer * DISPLAY_BUFFER_STRIDE_ROWS;
    if ((u8)viewGetMappedIndex() == task->spawnArg1.value) {
        // The work fields hold pixels and frame counts for this screen-space effect.
        if (work->age == 0) {
            _akropolisPromenadeInitializeScreenDrip(task, work);
        }
        y        = work->move.vy + work->age / work->period;
        x        = work->move.vx;
        depth    = START_DEPTH - (y - DEPTH_FIRST_ROW) * DEPTH_PER_ROW;
        onScreen = y < LAST_SOURCE_ROW;
        // Copy one row down within the current buffer, sorting by its screen row.
        if (onScreen) {
            sourceStrip.x  = x;
            sourceStrip.y  = y + bufferY;
            sourceStrip.w  = work->angle;
            sourceStrip.h  = 1;
            movePacket     = gGpuPrimCursor;
            gGpuPrimCursor = movePacket + 1;
            SetDrawMove(movePacket, &sourceStrip, x, y + bufferY + 1);
            addPrim(gGpuCurrentOt + (depth >> OT_DEPTH_SHIFT), movePacket);
        }
        work->age++;
        if (work->age <= work->scale && onScreen) {
            return;
        }
    }
    effectKillTask(work, task);
}

/// Binds the shared star glow to the promenade's exported `void (Task*)` callback.
#define GLOW_STAR_TASK akropolisPromenadeGlowStarTask
/// Stores the star core's pixel half-height separately from its equal half-width.
#define GLOW_STAR_STORE_HALF_HEIGHT 1
#include "../../../../shared/effects/acropolis_glows_star.inc.c"

/// Places one corner of the ground glow in the task's composed coordinate space.
static inline void _akropolisPromenadeTransformGroundGlowCorner(EffectQuadCornersScratch* quadScratch, s32 cornerIndex, const GfxCoord* coord)
{
    enum { GROUND_GLOW_HALF_SIDE = 768 };
    quadScratch->vertices[cornerIndex].vx     = _gAkropolisPromenadeGroundGlowUnitCorners[cornerIndex].axis0Sign * GROUND_GLOW_HALF_SIDE;
    (&quadScratch->vertices[cornerIndex])->vy = 0;
    (&quadScratch->vertices[cornerIndex])->vz = _gAkropolisPromenadeGroundGlowUnitCorners[cornerIndex].axis1Sign * GROUND_GLOW_HALF_SIDE;
    // Rotate the local corner before adding the composition-root translation.
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&quadScratch->vertices[cornerIndex]);
    gte_rtv0();
    gte_stsv(&quadScratch->vertices[cornerIndex]);
    quadScratch->vertices[cornerIndex].vx     += coord->workm.t[0];
    (&quadScratch->vertices[cornerIndex])->vy += coord->workm.t[1];
    (&quadScratch->vertices[cornerIndex])->vz += coord->workm.t[2];
}

/// Reserves and initializes one ground-glow quad in the current frame's packet arena.
static inline POLY_FT4* _akropolisPromenadeReserveGroundGlowQuad(void)
{
    POLY_FT4* quad;

    quad           = gGpuPrimCursor;
    gGpuPrimCursor = quad + 1;
    setPolyFT4(quad);
    return quad;
}

void akropolisPromenadeGroundGlowTask(Task* task)
{
    enum {
        DEPTH_BIAS       = 32,
        TEXTURE_V        = 16,
        LAST_TEXEL       = 39,
        TEXTURE_PAGE     = getTPage(0, GPU_BLEND_ADD, 704, 0),
        TEXTURE_CLUT     = getClut(16, 270),
        GREY_LEVEL_COUNT = 16
    };
    GfxCoord*                 coord;
    EffectWork*               work;
    EffectQuadCornersScratch* quadScratch;
    POLY_FT4*                 quad;
    s32                       cornerIndex;
    s32                       grey;

    coord = task->extra.coordBody->coord;
    work  = task->spawnArg2.pointer;
    actorRenderComposeCoord(coord);
    work->age   = task->spawnArg1.value;
    quadScratch = SCRATCH_STACK_RESERVE_BLOCK(EffectQuadCornersScratch);
    // Rotate the local XZ square, then translate its signed 16-bit corners.
    for (cornerIndex = 0; cornerIndex < ARRAY_SIZE(_gAkropolisPromenadeGroundGlowUnitCorners); cornerIndex++) {
        _akropolisPromenadeTransformGroundGlowCorner(quadScratch, cornerIndex, coord);
    }
    // Project into the packet; a rejected quad still consumes packet space.
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&quadScratch->vertices[0]);
    gte_rtps();
    quad = _akropolisPromenadeReserveGroundGlowQuad();
    gte_stsxy(&quad->x0);
    gte_ldv3(&quadScratch->vertices[1], &quadScratch->vertices[2], &quadScratch->vertices[3]);
    gte_rtpt();
    quad->u0 = 0;
    quad->v0 = TEXTURE_V;
    quad->u1 = LAST_TEXEL;
    quad->v1 = TEXTURE_V;
    quad->u2 = 0;
    quad->v2 = TEXTURE_V + LAST_TEXEL;
    quad->u3 = LAST_TEXEL;
    quad->v3 = TEXTURE_V + LAST_TEXEL;
    gte_stsxy3(&quad->x1, &quad->x2, &quad->x3);
    gte_stszotz(&quadScratch->depth);
    quadScratch->depth += DEPTH_BIAS;
    if (quadScratch->depth >= GLOW_MIN_DEPTH) {
        quad->tpage = TEXTURE_PAGE;
        quad->clut  = TEXTURE_CLUT;
        RANDOM_LCG_ADVANCE();
        grey     = RANDOM_LCG_DRAW() & (GREY_LEVEL_COUNT - 1);
        quad->r0 = grey;
        quad->g0 = grey;
        quad->b0 = grey;
        setSemiTrans(quad, 1);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(quadScratch->depth),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectQuadCornersScratch);
    effectKillTask(work, task);
}

/// Binds the included lamp glow to the promenade's exported `void (Task*)` callback.
#define GLOW_LAMP_TASK akropolisPromenadeGlowLampTask
#include "../../../../shared/effects/acropolis_glows_lamp.inc.c"

#include "../../../../shared/effects/glow_draw_tinted_disc_no_bias.inc.c"
