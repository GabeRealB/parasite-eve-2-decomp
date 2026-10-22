#include "rooms/acropolis_promenade.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "actors/task_tables.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/gameflag.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effects.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/pad_input.h"
#include "gameplay/pad_script.h"
#include "gameplay/player_actor.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflag.h"
#include "main/gameflow.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
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

#include "mapui/map_akropolis.h"

#include "overlay.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/effects/glow_draw.h"
#include "../../shared/room/bridge_model.h"
#include "../../shared/effects/acropolis_glows.h"

#include "assets/rooms/akropolis/promenade/declarations.inc"

static void _acropolisPromenadeInitializeRoomTask(Task* task);
static void _acropolisPromenadeUpdateRoomProgress(Task* unusedTask);
static void _acropolisPromenadeUpdateBridgeVisibility(Task* task);

static void _acropolisPromenadeMoviePathTask(Task* task);
static void _acropolisPromenadeMovieSkipFadeOutTask(Task* task);
static void _acropolisPromenadeSceneFadeInTask(Task* task);

static s32  _acropolisPromenadeResolveRoomTransition(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32  _acropolisPromenadeRefuseKeyItem(Task* task, s32 messageId, s32 itemId, s32 secondArg);
static s32  _acropolisPromenadeHandleRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 command, s32 unusedSecondArg);
static s32  _acropolisPromenadeHandleSoundCue(Task* unusedTask, s32 messageId, s32 soundCue, s32 unusedArg);
static s32  _acropolisPromenadeIgnoreRoomAction(Task* task, s32 messageId, s32 firstArg, s32 secondArg);
static void _acropolisPromenadeTranslucentPlaneTask(Task* task);

/// Room-local binding of the key-item-use request; the first payload is an item ID.
enum { ACROPOLIS_PROMENADE_MESSAGE_USE_KEY_ITEM = 0x13F1 };

/// Overlay-lifetime arrival latch and battle follow-up states, stored as s32.
enum {
    ACROPOLIS_PROMENADE_ARRIVAL_MOVIE_NOT_STARTED = 0,
    ACROPOLIS_PROMENADE_ARRIVAL_MOVIE_STARTED     = 1,
    ACROPOLIS_PROMENADE_BATTLE_FOLLOWUP_INACTIVE  = 0,
    ACROPOLIS_PROMENADE_BATTLE_FOLLOWUP_PENDING   = 2,
};

/// CAP command and signed pan offset used by the variant-1 battle follow-up.
enum {
    ACROPOLIS_PROMENADE_BATTLE_FOLLOWUP_CAP_COMMAND = 10,
    ACROPOLIS_PROMENADE_BATTLE_SOUND_PAN_OFFSET     = 13,
};

/// Fixed plane scheduling priority and roles of the glow group's position slots.
enum {
    ACROPOLIS_PROMENADE_PLANE_TASK_PRIORITY = 192,
    ACROPOLIS_PROMENADE_GLOW_DISC_POSITION  = 0,
    ACROPOLIS_PROMENADE_GLOW_FIRST_STAR     = 1,
    ACROPOLIS_PROMENADE_GLOW_SECOND_STAR    = 2,
};

/// Mapped camera/image view bits used by the room effect selectors.
enum {
    ACROPOLIS_PROMENADE_EFFECT_VIEW_1  = 1 << 0,
    ACROPOLIS_PROMENADE_EFFECT_VIEW_2  = 1 << 1,
    ACROPOLIS_PROMENADE_EFFECT_VIEW_3  = 1 << 2,
    ACROPOLIS_PROMENADE_EFFECT_VIEW_4  = 1 << 3,
    ACROPOLIS_PROMENADE_EFFECT_VIEW_5  = 1 << 4,
    ACROPOLIS_PROMENADE_EFFECT_VIEW_6  = 1 << 5,
    ACROPOLIS_PROMENADE_EFFECT_VIEW_7  = 1 << 6,
    ACROPOLIS_PROMENADE_EFFECT_VIEW_8  = 1 << 7,
    ACROPOLIS_PROMENADE_EFFECT_VIEW_9  = 1 << 8,
    ACROPOLIS_PROMENADE_EFFECT_VIEW_10 = 1 << 9,
};

/// Live indices in the movie task descriptor table; slots 0 and 1 are unused.
enum {
    ACROPOLIS_PROMENADE_MOVIE_PATH_TASK       = 2,
    ACROPOLIS_PROMENADE_MOVIE_SKIP_FADE_TASK  = 3,
    ACROPOLIS_PROMENADE_MOVIE_SCENE_FADE_TASK = 4,
};

#include "assets/rooms/akropolis/bridge/acropolis_bridge_model_0AD9C.inc"

/// Room-message handlers for exits, CAP commands, actions, key items and sounds.
///
/// Borrowed by the room receiver until it is released; this overlay and its
/// callbacks must remain loaded. TASK_MESSAGE_TABLE_END makes unknown IDs
/// return zero. The room-action entry ignores its payload and leaves the
/// result unset, so its sender must discard that result.
static TaskMessageEntry _gAcropolisPromenadeRoomMessages[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _acropolisPromenadeResolveRoomTransition },
    { ROOM_MESSAGE_COMMAND, _acropolisPromenadeHandleRoomCommand },
    { DIRECTION_MESSAGE_ROOM_ACTION, _acropolisPromenadeIgnoreRoomAction },
    { ACROPOLIS_PROMENADE_MESSAGE_USE_KEY_ITEM, _acropolisPromenadeRefuseKeyItem },
    { ROOM_MESSAGE_SOUND, _acropolisPromenadeHandleSoundCue },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Spawn recipe for the fixed translucent plane, followed by TASK_DESC_END.
///
/// Index 0 creates a priority-192 TMD task borrowing this overlay's bridge
/// model. Spawning copies the recipe; its callback and model must stay loaded
/// through teardown. The room initializer retains the result, including NULL.
static TaskDesc _gAcropolisPromenadeTranslucentPlaneTaskDescriptors[2] = {
    { { { TASK_BODY_TMD, ACROPOLIS_PROMENADE_PLANE_TASK_PRIORITY } }, _acropolisPromenadeTranslucentPlaneTask, { .model = &_gAkropolisBridgeAcropolisBridgeModel0AD9C } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

#include "assets/rooms/akropolis/promenade/events.inc"
/// Warp-4 arrival movie latch (0 not started, 1 spawn already attempted).
///
/// Reset by loading this overlay; set before spawning, even if spawning fails.
static s32 _gAcropolisPromenadeArrivalMovieStarted = ACROPOLIS_PROMENADE_ARRIVAL_MOVIE_NOT_STARTED;

/// Battle follow-up phase (0 inactive, 1 engaged observed, 2 awaiting reset).
///
/// Variant 1 mirrors SCENE_COMBAT_BATTLE_ENGAGED, then waits for battle reset
/// after that phase changes. Reset to inactive before starting the scene.
/// Stored as a signed word and initialized again when this overlay is loaded.
static s32 _gAcropolisPromenadeBattleFollowupState = ACROPOLIS_PROMENADE_BATTLE_FOLLOWUP_INACTIVE;

/// Bodyless movie-path and skip-fade spawn recipes, indexed by MOVIE_*_TASK.
///
/// Slots 0 and 1 retain unused null callbacks; spawn only slots 2..4. All use
/// priority 192. This fixed-size indexed table has no TASK_DESC_END record.
/// Spawning copies a recipe synchronously; its callback requires this overlay
/// to remain loaded through task teardown.
static TaskDesc _gAcropolisPromenadeMovieTaskDescriptors[5] = {
    { { { TASK_BODY_NONE, 192 } }, NULL, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, NULL, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _acropolisPromenadeMoviePathTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _acropolisPromenadeMovieSkipFadeOutTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _acropolisPromenadeSceneFadeInTask, { .value = 0 } },
};

/// Player-root positions for the warp-4 arrival movie, in whole room units.
///
/// Index by 69 - movieFrame and subtract 200 from Z. The consumer requires
/// movieFrame in 1..69 (sample indices 0..68) and hands off to scripted walking
/// at frame 64 or later. All 300 authored samples remain stored; the movie's
/// frame limit is 300, so its limit alone does not guarantee safe indexing.
/// Borrowed only during the movie-path task; keep this overlay loaded.
static SVECTOR _gAcropolisPromenadeArrivalMoviePlayerPositions[300] = {
#include "assets/acropolis_promenade_path_03BC4.inc"
};

/// X/Z unit signs of the ground-glow square's four corners in GPU strip order.
///
/// Axis 0 is local X and axis 1 is local Z. The drawer scales each sign by its
/// 768-game-unit half-side before applying the body's composed transform.
static EffectUnitQuadCorner _gAcropolisPromenadeGroundGlowUnitCorners[4] = {
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

/// Map markers shared with the destination endpoints of the two marked exits.
enum {
    ACROPOLIS_PROMENADE_SANCTUARY_EXIT_MAP_FLAG = 493,
    ACROPOLIS_PROMENADE_BRIDGE_EXIT_MAP_FLAG    = 491,
};

/// Texture-page encodings, palette rows and colour flags for mapped views 7..9.
///
/// Sources use 8-bit indexed textures and average blending when semitransparent.
enum {
    ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_832 = getTPage(1, GPU_BLEND_AVERAGE, 832, 0),
    ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896 = getTPage(1, GPU_BLEND_AVERAGE, 896, 0),
    ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_960 = getTPage(1, GPU_BLEND_AVERAGE, 960, 0),
    ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255     = getClut(0, 255),
    ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_256     = getClut(0, 256),
    ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB      = 128,
    ACROPOLIS_PROMENADE_SPRITE_OPAQUE           = 0,
};

/// Last executed sanctuary-exit reply before bridge progress, with warp 3.
///
/// Stores the complete eight-byte resolved message. No reader is established;
/// this storage does not own or defer a transition. Reloading clears it.
static RoomEventMsg _gAcropolisPromenadeSanctuaryExitReply;

/// Saved result of the room initializer's translucent-plane spawn, or NULL.
///
/// Borrowed task pointer, written once during setup and never read here.
/// It neither owns teardown nor tracks whether that task is still live.
static Task* _gAcropolisPromenadeTranslucentPlaneTask;

/// Starts the warp-4 movie once and schedules the variant-1 battle follow-up.
///
/// Runs in the room task's active state without advancing it. Normalizes saved
/// scene event 6 to 5, holds area-music loading during variant 1, and fades MIDI
/// over 60 audio updates when an observed engaged battle ends. Starts the skippable
/// follow-up only after battle reset is pending. The receiver is unused.
static void _acropolisPromenadeUpdateRoomProgress(Task* unusedTask)
{
    enum {
        ACROPOLIS_PROMENADE_MOVIE_ARRIVAL_WARP     = 4,
        ACROPOLIS_PROMENADE_SCENE_EVENT_SANCTUARY  = 6,
        ACROPOLIS_PROMENADE_SCENE_EVENT_PROMENADE  = 5,
        ACROPOLIS_PROMENADE_BATTLE_VARIANT         = 1,
        ACROPOLIS_PROMENADE_ALL_MIDI_SEQUENCES     = 0,
        ACROPOLIS_PROMENADE_BATTLE_MIDI_FADE_TICKS = 60,
    };
    u8 placementVariant;
    u8 battlePhase;

    if (_gAcropolisPromenadeArrivalMovieStarted == ACROPOLIS_PROMENADE_ARRIVAL_MOVIE_NOT_STARTED) {
        if (gGameSession->location.warp == ACROPOLIS_PROMENADE_MOVIE_ARRIVAL_WARP) {
            _gAcropolisPromenadeArrivalMovieStarted = ACROPOLIS_PROMENADE_ARRIVAL_MOVIE_STARTED;
            taskSpawnFromTable(_gAcropolisPromenadeMovieTaskDescriptors, ACROPOLIS_PROMENADE_MOVIE_PATH_TASK, 0, 0);
        }
    }
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent == ACROPOLIS_PROMENADE_SCENE_EVENT_SANCTUARY) {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = ACROPOLIS_PROMENADE_SCENE_EVENT_PROMENADE;
    }
    placementVariant = gGameSession->location.variant;
    if (placementVariant == ACROPOLIS_PROMENADE_BATTLE_VARIANT) {
        gGameSession->flowFlags = (GAME_SESSION_FLOW_SKIP_AREA_MUSIC | GAME_SESSION_FLOW_LOAD_AREA_MUSIC_ONLY);
        battlePhase             = gSceneCombatState.signals.bytes.battlePhase;
        if (battlePhase == placementVariant) {
            _gAcropolisPromenadeBattleFollowupState = battlePhase;
        }
        if ((_gAcropolisPromenadeBattleFollowupState == placementVariant) && (battlePhase != _gAcropolisPromenadeBattleFollowupState)) {
            _gAcropolisPromenadeBattleFollowupState = ACROPOLIS_PROMENADE_BATTLE_FOLLOWUP_PENDING;
            sndEvtRequestMidiStop(ACROPOLIS_PROMENADE_ALL_MIDI_SEQUENCES, ACROPOLIS_PROMENADE_BATTLE_MIDI_FADE_TICKS);
        }
        if ((_gAcropolisPromenadeBattleFollowupState == ACROPOLIS_PROMENADE_BATTLE_FOLLOWUP_PENDING) && (gGameSession->battleResetPending != 0)) {
            _gAcropolisPromenadeBattleFollowupState = ACROPOLIS_PROMENADE_BATTLE_FOLLOWUP_INACTIVE;
            evsStartScriptWithSkip(_gAcropolisPromenadeBattleFollowupScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gAcropolisPromenadeBattleFollowupSkipScript);
        }
    }
}

/// Resolves promenade exits from movie availability and route progress.
///
/// Receives `ROOM_EVENT_MESSAGE_RESOLVE` with an eight-byte borrowed request and
/// writable reply, which may alias. Copies the complete record. Execution
/// selects observatory arrival 1 when the movie or route is unavailable; it
/// stores sanctuary arrival 3 before bridge progress and selects scene event 4.
/// The bridge exit returns 0 and starts CAP command 2 before progress, then
/// returns 1 and selects room 1 or 2. Queries suppress these side effects.
static s32 _acropolisPromenadeResolveRoomTransition(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    enum {
        ACROPOLIS_PROMENADE_ROUTE_FROM_OBSERVATORY         = 4,
        ACROPOLIS_PROMENADE_ROUTE_AT_SANCTUARY             = 5,
        ACROPOLIS_PROMENADE_OBSERVATORY_BLOCKED_ARRIVAL    = 1,
        ACROPOLIS_PROMENADE_SANCTUARY_SCENE_ARRIVAL        = 3,
        ACROPOLIS_PROMENADE_SCENE_EVENT_SANCTUARY_APPROACH = 4,
        ACROPOLIS_PROMENADE_BRIDGE_LOCKED_CAP_COMMAND      = 2,
        ACROPOLIS_PROMENADE_EXIT_MAP_MARKED                = 2,
        ACROPOLIS_PROMENADE_BRIDGE_SCENE_COMPLETE          = 3,
        ACROPOLIS_PROMENADE_BRIDGE_ROOM_AFTER_SCENE        = 2,
        ACROPOLIS_PROMENADE_BRIDGE_ROOM_BEFORE_SCENE       = 1,
    };
    // The unused record retains the original stack reservation.
    RoomEventMsg retainedEventSpace;
    u16          destinationArea;

    *reply = *request;
    if (request->areaId == GAME_AREA_AKROPOLIS_OBSERVATORY && request->queryOnly == ROOM_EVENT_EXECUTE) {
        if (gDisplayState.debugMode < 0 || gStreamInterFile.startSector == 0) {
            reply->warp = ACROPOLIS_PROMENADE_OBSERVATORY_BLOCKED_ARRIVAL;
        }
        if (gameFlagGetNibble(GAME_FLAG_OBSERVATORY_ROUTE_PROGRESS) == ACROPOLIS_PROMENADE_ROUTE_FROM_OBSERVATORY) {
            gameFlagSetNibble(GAME_FLAG_OBSERVATORY_ROUTE_PROGRESS, ACROPOLIS_PROMENADE_ROUTE_AT_SANCTUARY);
        } else {
            reply->warp = ACROPOLIS_PROMENADE_OBSERVATORY_BLOCKED_ARRIVAL;
        }
    }
    if (request->areaId == GAME_AREA_AKROPOLIS_SANCTUARY && gameFlagGetNibble(GAME_FLAG_AKROPOLIS_BRIDGE_PROGRESS) == 0) {
        if (request->queryOnly == ROOM_EVENT_EXECUTE) {
            reply->warp                                         = ACROPOLIS_PROMENADE_SANCTUARY_SCENE_ARRIVAL;
            _gAcropolisPromenadeSanctuaryExitReply              = *reply;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = ACROPOLIS_PROMENADE_SCENE_EVENT_SANCTUARY_APPROACH;
        }
        return 1;
    }
    destinationArea = request->areaId;
    if (destinationArea == GAME_AREA_AKROPOLIS_BRIDGE) {
        if (gameFlagGetNibble(GAME_FLAG_AKROPOLIS_BRIDGE_PROGRESS) == 0) {
            if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                capSpawnEventIfIdle(ACROPOLIS_PROMENADE_BRIDGE_LOCKED_CAP_COMMAND, CAP_EVENT_PAUSE_ACTORS);
                gameFlagSetNibbleIfPresent(request->flagId, ACROPOLIS_PROMENADE_EXIT_MAP_MARKED);
            }
            return 0;
        }
        if (request->areaId == destinationArea) {
            if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                if (gameFlagGetNibble(GAME_FLAG_AKROPOLIS_BRIDGE_PROGRESS) == ACROPOLIS_PROMENADE_BRIDGE_SCENE_COMPLETE) {
                    reply->roomState = ACROPOLIS_PROMENADE_BRIDGE_ROOM_AFTER_SCENE;
                } else {
                    reply->roomState = ACROPOLIS_PROMENADE_BRIDGE_ROOM_BEFORE_SCENE;
                }
            }
        }
    }
    return 1;
}

/// Refuses every key-item-use request in the promenade.
///
/// The first payload is the selected inventory item ID; the second is unused.
/// Ignores all arguments and returns zero, selecting the item's refusal text.
static s32 _acropolisPromenadeRefuseKeyItem(Task* task, s32 messageId, s32 itemId, s32 secondArg)
{
    enum { ACROPOLIS_PROMENADE_KEY_ITEM_REFUSED = 0 };
    return ACROPOLIS_PROMENADE_KEY_ITEM_REFUSED;
}

/// Selects CAP playback for promenade room command 5.
///
/// The first payload is the integer command; other arguments are unused.
/// Object state 2 for index 21 selects CAP command 9 with a display transition;
/// otherwise starts CAP slot 5 in transition mode. Always returns 0.
static s32 _acropolisPromenadeHandleRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 command, s32 unusedSecondArg)
{
    enum {
        ACROPOLIS_PROMENADE_ROOM_COMMAND_CAP_5    = 5,
        ACROPOLIS_PROMENADE_CAP_OBJECT_INDEX      = 21,
        ACROPOLIS_PROMENADE_CAP_OBJECT_COMPLETE   = 2,
        ACROPOLIS_PROMENADE_CAP_SLOT              = 5,
        ACROPOLIS_PROMENADE_CAP_COMPLETED_COMMAND = 9,
    };
    if (command == ACROPOLIS_PROMENADE_ROOM_COMMAND_CAP_5) {
        if (areaGetCurrentObjectState(ACROPOLIS_PROMENADE_CAP_OBJECT_INDEX) != ACROPOLIS_PROMENADE_CAP_OBJECT_COMPLETE) {
            capStartSequenceSlot(ACROPOLIS_PROMENADE_CAP_SLOT, CAP_PLAYBACK_DISPLAY_TRANSITION, 0);
        } else {
            capRunCommandWithTransition(ACROPOLIS_PROMENADE_CAP_COMPLETED_COMMAND);
        }
    }
    return 0;
}

/// Ignores the promenade's room-action request.
///
/// The first payload transports a borrowed `DirectionActionRequest` address;
/// the second is zero. Neither is read, so the unused payloads remain words.
/// Leaves the result unset; callers must discard the dispatch result.
static s32 _acropolisPromenadeIgnoreRoomAction(Task* task, s32 messageId, s32 firstArg, s32 secondArg)
{
}

/// Queues promenade sound-bank entries for direct and completed CAP sound cues.
///
/// Cue 10 starts entry 9; cue 103 (CAP command 3 completed) starts entry 10.
/// Uses each script's base mix and ignores other cues and queue failures.
/// Receiver, ID and second payload are unused; always returns zero.
static s32 _acropolisPromenadeHandleSoundCue(Task* unusedTask, s32 messageId, s32 soundCue, s32 unusedArg)
{
    enum {
        ACROPOLIS_PROMENADE_SOUND_CUE_ENTRY_9            = 10,
        ACROPOLIS_PROMENADE_SOUND_CUE_CAP_COMMAND_3_DONE = 103
    };

    switch (soundCue) {
        case ACROPOLIS_PROMENADE_SOUND_CUE_ENTRY_9:
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_PROMENADE, 9), 0, 0);
            break;
        case ACROPOLIS_PROMENADE_SOUND_CUE_CAP_COMMAND_3_DONE:
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_PROMENADE, 0x0A), 0, 0);
            break;
    }
    return 0;
}

/// Room-task handlers indexed by state: initialize, update progress, release.
///
/// States must stay in 0..2. Dispatch copies all three callbacks to the stack,
/// then indexes without bounds checking or a sentinel. Keep this overlay
/// loaded through the selected callback, which may release the task.
static const TaskFuncTable3 _gAcropolisPromenadeRoomStates = {
    { _acropolisPromenadeInitializeRoomTask, _acropolisPromenadeUpdateRoomProgress, taskKill },
};

/// Translucent-plane handlers indexed by task state: setup, visibility, teardown.
///
/// States 0..2 respectively allocate/place the TMD model, update its visibility
/// and release its task/work. Dispatch copies the three handlers to the stack
/// before indexing; there is no sentinel or bounds check. Keep the overlay
/// loaded through the selected handler.
static const TaskFuncTable3 _gAcropolisPromenadeTranslucentPlaneStates = {
    { _bridgeModelSetup, _acropolisPromenadeUpdateBridgeVisibility, taskKill },
};

/// Runs the promenade's translucent plane model task.
///
/// Requires a live TMD task with state 0 (allocate work and place the model),
/// 1 (hide in mapped view 5, show in other views), or 2 (release the task and its work).
/// The state is an unchecked table index. Keep the room and model resources
/// loaded until teardown; allocation failure also tears down the task.
/// The plane's visual purpose is unproven.
static void _acropolisPromenadeTranslucentPlaneTask(Task* task)
{
    TaskFuncTable3 stateHandlers = _gAcropolisPromenadeTranslucentPlaneStates;
    stateHandlers.funcs[task->state](task);
}

/// Registers the promenade room receiver and spawns its translucent plane.
///
/// Called in state 0 with this overlay's resources loaded. Publishes the room
/// task, saves the spawned plane task, advances to the active state and enables
/// CAP-completion sound forwarding. Spawn failure leaves the saved pointer NULL.
static void _acropolisPromenadeInitializeRoomTask(Task* task)
{
    enum { ACROPOLIS_PROMENADE_CAP_COMPLETION_SOUNDS_ENABLED = 1 };
    task->msgTable = _gAcropolisPromenadeRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    _gAcropolisPromenadeTranslucentPlaneTask = taskSpawnFromTable(_gAcropolisPromenadeTranslucentPlaneTaskDescriptors, 0, 0, 0);
    task->state                              = task->state + 1;
    gCapRoomCompletionSoundsEnabled          = ACROPOLIS_PROMENADE_CAP_COMPLETION_SOUNDS_ENABLED;
}

void acropolisPromenadeRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers = _gAcropolisPromenadeRoomStates;
    stateHandlers.funcs[task->state](task);
}

#include "../../shared/room/bridge_model_setup.inc.c"

/// Hides the bridge model in promenade view 5 and enables it in the other views.
///
/// Requires the model task's initialized TMD body. Replaces its draw flags and
/// marks the first coordinate dirty so the next draw recomposes its transform.
static void _acropolisPromenadeUpdateBridgeVisibility(Task* task)
{
    enum { ACROPOLIS_PROMENADE_BRIDGE_HIDDEN_VIEW = 5 };
    TmdObject* bridge;
    GfxCoord*  coord;

    bridge = task->extra.tmd;
    coord  = bridge->coords;
    if (viewGetMappedIndex() == ACROPOLIS_PROMENADE_BRIDGE_HIDDEN_VIEW) {
        bridge->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else {
        bridge->flags = 0;
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Copies a movie-path sample into the player root, with the promenade Z offset.
///
/// Requires live borrowed work/root and path index 0..299. Positions use whole
/// room units; rotation and coordinate cache stamps are unchanged.
/// Work and index are evaluated three times, once per axis, and must be
/// stable and side-effect-free; zOffset is evaluated once. Captures the room's
/// path array and expands to a braced statement block inside a braced scope.
#define ACROPOLIS_PROMENADE_APPLY_MOVIE_PATH_POSITION(movieWork, pathIndex, zOffset)                                \
    {                                                                                                               \
        (movieWork)->playerMtx->t[0] = _gAcropolisPromenadeArrivalMoviePlayerPositions[(pathIndex)].vx;             \
        (movieWork)->playerMtx->t[1] = _gAcropolisPromenadeArrivalMoviePlayerPositions[(pathIndex)].vy;             \
        (movieWork)->playerMtx->t[2] = _gAcropolisPromenadeArrivalMoviePlayerPositions[(pathIndex)].vz - (zOffset); \
    }

/// Moves the player along the promenade movie path and restores player control.
///
/// Starts bodyless in state 0 with a live player and loaded movie resources.
/// Owns `RoomMoviePathWork`; allocation failure kills the task. Waits for movie
/// readiness, adopts its vibration task, then copies the player root position
/// from path index 69 - movieFrame, subtracting 200 whole room units from Z.
/// Every access requires movieFrame <= 69. Start initiates a polled skip fade;
/// skip places the player with yaw 3072, while the final six samples start a
/// scripted walk using position only. Waits for that motion before restoring
/// HUD/input/actors and saved view 2. Resources must remain live through exit.
static void _acropolisPromenadeMoviePathTask(Task* task)
{
    enum {
        ACROPOLIS_PROMENADE_MOVIE_INITIAL              = 0,
        ACROPOLIS_PROMENADE_MOVIE_WAIT_READY           = 1,
        ACROPOLIS_PROMENADE_MOVIE_FOLLOW_PATH          = 2,
        ACROPOLIS_PROMENADE_MOVIE_WAIT_PLAYER          = 3,
        ACROPOLIS_PROMENADE_MOVIE_FRAME_ORIGIN         = 69,
        ACROPOLIS_PROMENADE_MOVIE_Z_OFFSET             = 200,
        ACROPOLIS_PROMENADE_MOVIE_EXIT_X               = 642,
        ACROPOLIS_PROMENADE_MOVIE_EXIT_Y               = 41,
        ACROPOLIS_PROMENADE_MOVIE_EXIT_YAW             = 3072,
        ACROPOLIS_PROMENADE_MOVIE_EXIT_VIEW            = 2,
        ACROPOLIS_PROMENADE_MOVIE_WALK_HANDOFF_SAMPLES = 6,
        ACROPOLIS_PROMENADE_PRIMARY_WEAPON_BANK_BASE   = 1,
        ACROPOLIS_PROMENADE_ALTERNATE_WEAPON_BANK_BASE = 34,
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
        case ACROPOLIS_PROMENADE_MOVIE_INITIAL:
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
            animationRequest.source.index                = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) ? equippedWeapon + ACROPOLIS_PROMENADE_PRIMARY_WEAPON_BANK_BASE : equippedWeapon + ACROPOLIS_PROMENADE_ALTERNATE_WEAPON_BANK_BASE;
            animationRequest.animationId                 = 1;
            animationRequest.blend                       = ANIMATION_BLEND_RESET;
            animationRequest.blendFrames                 = 0;
            animationRequest.enableWorldCollision        = ANIMATION_WORLD_COLLISION_DISABLE;
            TASK_MESSAGE_DISPATCH_POINTER(((RoomMoviePathWork*)task->work)->playerTask, ANIMATION_MESSAGE_PLAY, &animationRequest, 0);
            padInputChangeSuppression(PAD_INPUT_SUPPRESSION_SET_AND_HOLD, PAD_INPUT_SUPPRESS_ACTIONS_AND_MENU);
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
            task->state                    = task->state + 1;
            break;

        case ACROPOLIS_PROMENADE_MOVIE_WAIT_READY:
            if (cdQueue->movieReady != 0) {
                work->padScriptTask           = padScriptSpawn(_gAcropolisPromenadeArrivalMovieVibrationCommands,
                                                               _gAcropolisPromenadeArrivalMovieVibrationSegments);
                gGameSession->padScriptFlags |= GAME_SESSION_PAD_SCRIPT_DURING_BATTLE_FREEZE;
                taskReparent(task, work->padScriptTask);
                task->state = task->state + 1;
            }
            break;

        case ACROPOLIS_PROMENADE_MOVIE_FOLLOW_PATH:
            // Follow decoded movie frames, giving a completed skip fade priority over walking.
            ACROPOLIS_PROMENADE_APPLY_MOVIE_PATH_POSITION(work, ACROPOLIS_PROMENADE_MOVIE_FRAME_ORIGIN - cdQueue->movieFrame, ACROPOLIS_PROMENADE_MOVIE_Z_OFFSET);
            if (work->skipFadeStarted != 0) {
                if (taskPollKill(work->skipFadeTask, &skipFadeResult) != 0) {
                    playerPlacement.pos.vx = ACROPOLIS_PROMENADE_MOVIE_EXIT_X;
                    playerPlacement.pos.vy = ACROPOLIS_PROMENADE_MOVIE_EXIT_Y;
                    playerPlacement.pos.vz = _gAcropolisPromenadeArrivalMoviePlayerPositions[ACROPOLIS_PROMENADE_MOVIE_FRAME_ORIGIN - cdQueue->movieFrame].vz - ACROPOLIS_PROMENADE_MOVIE_Z_OFFSET;
                    playerPlacement.rot.vz = 0;
                    playerPlacement.rot.vx = 0;
                    playerPlacement.rot.vy = ACROPOLIS_PROMENADE_MOVIE_EXIT_YAW;
                    TASK_MESSAGE_DISPATCH_POINTER(((RoomMoviePathWork*)task->work)->playerTask, GAME_ACTOR_MESSAGE_PLACE, &playerPlacement, 0);
                    taskSpawnFromTable(_gAcropolisPromenadeMovieTaskDescriptors, ACROPOLIS_PROMENADE_MOVIE_SCENE_FADE_TASK, 0, 0);
                    task->state = task->state + 1;
                    break;
                }
            } else if (padIsStartPressed() != 0) {
                work->skipFadeTask    = taskSpawnFromTable(_gAcropolisPromenadeMovieTaskDescriptors, ACROPOLIS_PROMENADE_MOVIE_SKIP_FADE_TASK, 0, 0);
                work->skipFadeStarted = 1;
            }
            // The walk handler reads XYZ only; rotation need not be initialized here.
            if ((ACROPOLIS_PROMENADE_MOVIE_FRAME_ORIGIN - cdQueue->movieFrame) < ACROPOLIS_PROMENADE_MOVIE_WALK_HANDOFF_SAMPLES) {
                playerPlacement.pos.vx = ACROPOLIS_PROMENADE_MOVIE_EXIT_X;
                playerPlacement.pos.vy = ACROPOLIS_PROMENADE_MOVIE_EXIT_Y;
                playerPlacement.pos.vz = _gAcropolisPromenadeArrivalMoviePlayerPositions[ACROPOLIS_PROMENADE_MOVIE_FRAME_ORIGIN - cdQueue->movieFrame].vz - ACROPOLIS_PROMENADE_MOVIE_Z_OFFSET;
                TASK_MESSAGE_DISPATCH_POINTER(((RoomMoviePathWork*)task->work)->playerTask, GAME_ACTOR_MESSAGE_MOVE_TO, &playerPlacement, 0);
                task->state = task->state + 1;
            }
            break;

        case ACROPOLIS_PROMENADE_MOVIE_WAIT_PLAYER:
            // Release scene holds only after the scripted player motion finishes.
            if (taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) == 0) {
                taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), CAP_CONTROL_MESSAGE_SHOW_HUD, 0, 0);
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = ACROPOLIS_PROMENADE_MOVIE_EXIT_VIEW;
                padInputChangeSuppression(PAD_INPUT_SUPPRESSION_CLEAR_ALIAS, PAD_INPUT_SUPPRESS_ACTIONS_AND_MENU);
                gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
                gGameSession->padScriptFlags  &= (0xFF ^ GAME_SESSION_PAD_SCRIPT_DURING_BATTLE_FREEZE);
                taskKill(task);
            }
            break;
    }
}

#undef ACROPOLIS_PROMENADE_APPLY_MOVIE_PATH_POSITION

/// Darkens the promenade movie before its skip transition.
///
/// Start this bodyless task with `killCountdown` zero. Draws subtractive grey
/// levels 0, 32, ... 224, advancing the signed 16-bit counter after each draw.
/// At 256 requests deferred teardown with result zero; the spawning movie
/// task polls that completion before placing the player and starting fade-in.
/// Requires a live overlay and the current frame's primitive arena.
static void _acropolisPromenadeMovieSkipFadeOutTask(Task* task)
{
    enum {
        ACROPOLIS_PROMENADE_MOVIE_SKIP_FADE_STEP = 32,
        ACROPOLIS_PROMENADE_MOVIE_SKIP_FADE_END  = 256
    };
    u8  darkness;
    s16 nextFadeCounter;

    darkness = (u8)task->killCountdown;
    fadeDrawOverlay(darkness, darkness, darkness, GPU_BLEND_SUBTRACT);
    nextFadeCounter     = (u16)task->killCountdown + ACROPOLIS_PROMENADE_MOVIE_SKIP_FADE_STEP;
    task->killCountdown = nextFadeCounter;
    if (nextFadeCounter >= ACROPOLIS_PROMENADE_MOVIE_SKIP_FADE_END) {
        taskRequestKill(task, 0);
    }
}

/// Reveals the scene after a movie skip by reducing a subtractive black overlay.
///
/// Requires a bodyless task with `killCountdown` initially zero. Draws grey
/// levels 255, 223, ... 31 on successive frames, advancing the signed counter
/// by 32 after each draw and releasing the task when it reaches 256.
static void _acropolisPromenadeSceneFadeInTask(Task* task)
{
    enum {
        ACROPOLIS_PROMENADE_FADE_STEP = 32,
        ACROPOLIS_PROMENADE_FADE_END  = 256
    };
    u8  darkness;
    s16 nextFadeCounter;

    darkness = ~(u8)task->killCountdown;
    fadeDrawOverlay(darkness, darkness, darkness, GPU_BLEND_SUBTRACT);
    nextFadeCounter     = (u16)task->killCountdown + ACROPOLIS_PROMENADE_FADE_STEP;
    task->killCountdown = nextFadeCounter;
    if (nextFadeCounter >= ACROPOLIS_PROMENADE_FADE_END) {
        taskKill(task);
    }
}

void acropolisPromenadeRoomEffectTask(Task* task)
{
    enum {
        ACROPOLIS_PROMENADE_EFFECTS_SUPPRESSED_VIEW = 7,
        ACROPOLIS_PROMENADE_LAMP_SINGLE_END         = 3,
        ACROPOLIS_PROMENADE_LAMP_GROUP_FIRST        = 3,
        ACROPOLIS_PROMENADE_LAMP_GROUP_END          = 5,
        ACROPOLIS_PROMENADE_DRIP_VIEW_ENTRY_COUNT   = 40,
        ACROPOLIS_PROMENADE_DISC_RADIUS_SCALE       = 256,
        ACROPOLIS_PROMENADE_DISC_PACKED_COLOR       = 0x5C40,
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
    if (mappedView == ACROPOLIS_PROMENADE_EFFECTS_SUPPRESSED_VIEW) {
        return;
    }
    viewMask = 1 << (mappedView - 1);
    if (_gAcropolisPromenadeGlowViewMask & viewMask) {
        effectSpawn((EFFECT_ACROPOLIS_PROMENADE_GLOW_STAR | EFFECT_SPAWN_UNLIMITED), roomCoord, (s32)(work->age), &_gAcropolisPromenadeGlowPositions[ACROPOLIS_PROMENADE_GLOW_FIRST_STAR]);
        effectSpawn((EFFECT_ACROPOLIS_PROMENADE_GLOW_STAR | EFFECT_SPAWN_UNLIMITED), roomCoord, (s32)(work->age), &_gAcropolisPromenadeGlowPositions[ACROPOLIS_PROMENADE_GLOW_SECOND_STAR]);
        effectSpawn(EFFECT_ACROPOLIS_PROMENADE_GROUND_GLOW, roomCoord, (s32)(work->age), &_gAcropolisPromenadeGroundGlowPosition);
        glowDrawTintedDiscNoBias(&_gAcropolisPromenadeGlowPositions[ACROPOLIS_PROMENADE_GLOW_DISC_POSITION], ACROPOLIS_PROMENADE_DISC_RADIUS_SCALE, ACROPOLIS_PROMENADE_DISC_PACKED_COLOR);
    }
    // The grouped pairs cover lamp placements 3..10; placement 11 follows separately.
    for (placementIndex = 0; placementIndex < ACROPOLIS_PROMENADE_LAMP_SINGLE_END; placementIndex++) {
        if (_gAcropolisPromenadeLampGlowViewMasks[placementIndex] & viewMask) {
            effectSpawn(EFFECT_ACROPOLIS_PROMENADE_LAMP_GLOW, roomCoord, 0, &_gAcropolisPromenadeLampGlowPositions[placementIndex]);
        }
    }
    for (placementIndex = ACROPOLIS_PROMENADE_LAMP_GROUP_FIRST; placementIndex < ACROPOLIS_PROMENADE_LAMP_GROUP_END; placementIndex++) {
        if (_gAcropolisPromenadeLampGlowViewMasks[placementIndex] & viewMask) {
            effectSpawn(EFFECT_ACROPOLIS_PROMENADE_LAMP_GLOW, roomCoord, 1, &_gAcropolisPromenadeLampGlowPositions[placementIndex]);
        }
        if (_gAcropolisPromenadeLampGlowViewMasks[placementIndex + 2] & viewMask) {
            effectSpawn(EFFECT_ACROPOLIS_PROMENADE_LAMP_GLOW, roomCoord, 2, &_gAcropolisPromenadeLampGlowPositions[placementIndex + 2]);
        }
        if (_gAcropolisPromenadeLampGlowViewMasks[placementIndex + 4] & viewMask) {
            effectSpawn(EFFECT_ACROPOLIS_PROMENADE_LAMP_GLOW, roomCoord, 1, &_gAcropolisPromenadeLampGlowPositions[placementIndex + 4]);
        }
        if (_gAcropolisPromenadeLampGlowViewMasks[placementIndex + 6] & viewMask) {
            effectSpawn(EFFECT_ACROPOLIS_PROMENADE_LAMP_GLOW, roomCoord, 2, &_gAcropolisPromenadeLampGlowPositions[placementIndex + 6]);
        }
    }
    if (_gAcropolisPromenadeLampGlowViewMasks[11] & viewMask) {
        effectSpawn(EFFECT_ACROPOLIS_PROMENADE_LAMP_GLOW, roomCoord, 1, &_gAcropolisPromenadeLampGlowPositions[11]);
    }
    // Seed a drip field on entry; subsequent ticks add two replacements.
    if (_gAcropolisPromenadeDripViewMask & viewMask) {
        previousView = work->scale;
        if (previousView != mappedView) {
            for (placementIndex = 0; placementIndex < ACROPOLIS_PROMENADE_DRIP_VIEW_ENTRY_COUNT; placementIndex++) {
                effectSpawn(EFFECT_ACROPOLIS_PROMENADE_SCREEN_DRIP, roomCoord, (s32)(mappedView), NULL);
            }
        } else {
            effectSpawn(EFFECT_ACROPOLIS_PROMENADE_SCREEN_DRIP, roomCoord, (s32)(previousView), NULL);
            effectSpawn(EFFECT_ACROPOLIS_PROMENADE_SCREEN_DRIP, roomCoord, (s32)(previousView), NULL);
        }
    }
    work->scale = mappedView;
}

/// Seeds one screen drip's pixel position, lifetime, width and descent period.
///
/// Requires a live writable task and its zero-age effect work. Sets `move.vx`
/// to X in 0..239 pixels, `move.vy` to Y in 176..239 pixels, `scale` to the
/// lifetime cutoff in 30..119 frames, `angle` to width in 16..79 pixels and
/// `period` to 1..4 frames per row. Advances task state without changing age.
/// Consumes five shared LCG draws in X/Y/lifetime/width/period order, using
/// unsigned lifetime reduction. Borrows both objects and retains no pointer.
static inline void _acropolisPromenadeInitializeScreenDrip(Task* task, EffectWork* work)
{
    enum {
        ACROPOLIS_PROMENADE_DRIP_COLUMN_COUNT          = 240,
        ACROPOLIS_PROMENADE_DRIP_FIRST_ROW             = 176,
        ACROPOLIS_PROMENADE_DRIP_ROW_RANGE             = 64,
        ACROPOLIS_PROMENADE_DRIP_MIN_LIFETIME_FRAMES   = 30,
        ACROPOLIS_PROMENADE_DRIP_LIFETIME_RANGE_FRAMES = 90,
        ACROPOLIS_PROMENADE_DRIP_MIN_WIDTH_PIXELS      = 16,
        ACROPOLIS_PROMENADE_DRIP_WIDTH_RANGE_PIXELS    = 64,
        ACROPOLIS_PROMENADE_DRIP_MAX_FRAMES_PER_ROW    = 4
    };
    u16 randomBits;

    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->move.vx   = (gRandomLcgState >> 16) % ACROPOLIS_PROMENADE_DRIP_COLUMN_COUNT;
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->move.vy   = ((gRandomLcgState >> 16) & (ACROPOLIS_PROMENADE_DRIP_ROW_RANGE - 1)) + ACROPOLIS_PROMENADE_DRIP_FIRST_ROW;
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    randomBits      = gRandomLcgState >> 16;
    work->scale     = (u32)randomBits % ACROPOLIS_PROMENADE_DRIP_LIFETIME_RANGE_FRAMES + ACROPOLIS_PROMENADE_DRIP_MIN_LIFETIME_FRAMES;
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->angle     = ((gRandomLcgState >> 16) & (ACROPOLIS_PROMENADE_DRIP_WIDTH_RANGE_PIXELS - 1)) + ACROPOLIS_PROMENADE_DRIP_MIN_WIDTH_PIXELS;
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->period    = ((gRandomLcgState >> 16) & (ACROPOLIS_PROMENADE_DRIP_MAX_FRAMES_PER_ROW - 1)) + 1;
    task->state++;
}

void acropolisPromenadeScreenDripTask(Task* task)
{
    enum {
        ACROPOLIS_PROMENADE_DRAW_BUFFER_STRIDE_PIXELS = 272,
        ACROPOLIS_PROMENADE_DRIP_DEPTH_FIRST_ROW      = 176,
        ACROPOLIS_PROMENADE_DRIP_LAST_SOURCE_ROW      = 239,
        ACROPOLIS_PROMENADE_DRIP_START_DEPTH          = 0x500,
        ACROPOLIS_PROMENADE_DRIP_DEPTH_PER_ROW        = 10,
        ACROPOLIS_PROMENADE_DRIP_OT_DEPTH_SHIFT       = 4
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
    bufferY = gDisplayState.drawBuffer * ACROPOLIS_PROMENADE_DRAW_BUFFER_STRIDE_PIXELS;
    if ((u8)viewGetMappedIndex() == task->spawnArg1.value) {
        // The work fields hold pixels and frame counts for this screen-space effect.
        if (work->age == 0) {
            _acropolisPromenadeInitializeScreenDrip(task, work);
        }
        y        = work->move.vy + work->age / work->period;
        x        = work->move.vx;
        depth    = ACROPOLIS_PROMENADE_DRIP_START_DEPTH - (y - ACROPOLIS_PROMENADE_DRIP_DEPTH_FIRST_ROW) * ACROPOLIS_PROMENADE_DRIP_DEPTH_PER_ROW;
        onScreen = y < ACROPOLIS_PROMENADE_DRIP_LAST_SOURCE_ROW;
        // Copy one row down within the current buffer, sorting by its screen row.
        if (onScreen) {
            sourceStrip.x  = x;
            sourceStrip.y  = y + bufferY;
            sourceStrip.w  = work->angle;
            sourceStrip.h  = 1;
            movePacket     = gGpuPrimCursor;
            gGpuPrimCursor = movePacket + 1;
            SetDrawMove(movePacket, &sourceStrip, x, y + bufferY + 1);
            addPrim(gGpuCurrentOt + (depth >> ACROPOLIS_PROMENADE_DRIP_OT_DEPTH_SHIFT), movePacket);
        }
        work->age++;
        if (work->age <= work->scale && onScreen) {
            return;
        }
    }
    effectKillTask(work, task);
}

/// Binds the shared star glow to the promenade's exported `void (Task*)` callback.
///
/// Define an unqualified function identifier before including the star fragment;
/// the fragment defines that function and undefines this binding afterwards.
#define GLOW_STAR_TASK acropolisPromenadeGlowStarTask
/// Stores the star core's pixel half-height separately from its equal half-width.
///
/// Numeric configuration for `acropolis_glows_star.inc.c`: nonzero stores
/// `0x1680 / otz` in `OverlaySpriteScratch::cornerDy` and uses it for the core's
/// Y extent; zero or omission reuses `cornerDx` for both axes. The Promenade enables
/// the separate storage; the Bridge omits it. The fragment undefines the
/// switch after compiling this instance.
#define GLOW_STAR_STORE_HALF_HEIGHT 1
#include "../../shared/effects/acropolis_glows_star.inc.c"

/// Places one corner of the ground glow in the task's composed coordinate space.
///
/// `cornerIndex` is 0..3 in GPU strip order; the local XZ half-side is 768
/// game-coordinate units. Requires a live word-aligned scratch block and a
/// coordinate whose `workm` is composed, with 12-fractional-bit rotation.
/// Rotates the local corner through GTE IR, then adds the cached translation,
/// retaining the low 16 bits of each component. Leaves `depth` and the vector's
/// fourth halfword untouched; changes GTE rotation, V0 and result registers.
/// Borrows both objects and retains no pointer.
static inline void _acropolisPromenadeTransformGroundGlowCorner(EffectQuadCornersScratch* quadScratch, s32 cornerIndex, const GfxCoord* coord)
{
    enum { ACROPOLIS_PROMENADE_GROUND_GLOW_HALF_SIDE = 768 };
    quadScratch->vertices[cornerIndex].vx     = _gAcropolisPromenadeGroundGlowUnitCorners[cornerIndex].axis0Sign * ACROPOLIS_PROMENADE_GROUND_GLOW_HALF_SIDE;
    (&quadScratch->vertices[cornerIndex])->vy = 0;
    (&quadScratch->vertices[cornerIndex])->vz = _gAcropolisPromenadeGroundGlowUnitCorners[cornerIndex].axis1Sign * ACROPOLIS_PROMENADE_GROUND_GLOW_HALF_SIDE;
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
///
/// Requires aligned space for a `POLY_FT4` at `gGpuPrimCursor`. Advances the
/// cursor and returns the packet with its DMA length and textured-quad code
/// set; coordinates, colour and texture fields remain uninitialized. Does not
/// check capacity. The caller fills and links it; storage is borrowed from the
/// current frame arena and must live until GPU completion.
static inline POLY_FT4* _acropolisPromenadeReserveGroundGlowQuad(void)
{
    POLY_FT4* quad;

    quad           = gGpuPrimCursor;
    gGpuPrimCursor = quad + 1;
    setPolyFT4(quad);
    return quad;
}

void acropolisPromenadeGroundGlowTask(Task* task)
{
    enum {
        ACROPOLIS_PROMENADE_GROUND_GLOW_DEPTH_BIAS       = 32,
        ACROPOLIS_PROMENADE_GROUND_GLOW_MIN_DEPTH        = 17,
        ACROPOLIS_PROMENADE_GROUND_GLOW_TEXTURE_V        = 16,
        ACROPOLIS_PROMENADE_GROUND_GLOW_LAST_TEXEL       = 39,
        ACROPOLIS_PROMENADE_GROUND_GLOW_TEXTURE_PAGE     = getTPage(0, GPU_BLEND_ADD, 704, 0),
        ACROPOLIS_PROMENADE_GROUND_GLOW_TEXTURE_CLUT     = getClut(16, 270),
        ACROPOLIS_PROMENADE_GROUND_GLOW_GREY_LEVEL_COUNT = 16
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
    for (cornerIndex = 0; cornerIndex < ARRAY_SIZE(_gAcropolisPromenadeGroundGlowUnitCorners); cornerIndex++) {
        _acropolisPromenadeTransformGroundGlowCorner(quadScratch, cornerIndex, coord);
    }
    // Project into the packet; a rejected quad still consumes packet space.
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&quadScratch->vertices[0]);
    gte_rtps();
    quad = _acropolisPromenadeReserveGroundGlowQuad();
    gte_stsxy(&quad->x0);
    gte_ldv3(&quadScratch->vertices[1], &quadScratch->vertices[2], &quadScratch->vertices[3]);
    gte_rtpt();
    quad->u0 = 0;
    quad->v0 = ACROPOLIS_PROMENADE_GROUND_GLOW_TEXTURE_V;
    quad->u1 = ACROPOLIS_PROMENADE_GROUND_GLOW_LAST_TEXEL;
    quad->v1 = ACROPOLIS_PROMENADE_GROUND_GLOW_TEXTURE_V;
    quad->u2 = 0;
    quad->v2 = ACROPOLIS_PROMENADE_GROUND_GLOW_TEXTURE_V + ACROPOLIS_PROMENADE_GROUND_GLOW_LAST_TEXEL;
    quad->u3 = ACROPOLIS_PROMENADE_GROUND_GLOW_LAST_TEXEL;
    quad->v3 = ACROPOLIS_PROMENADE_GROUND_GLOW_TEXTURE_V + ACROPOLIS_PROMENADE_GROUND_GLOW_LAST_TEXEL;
    gte_stsxy3(&quad->x1, &quad->x2, &quad->x3);
    gte_stszotz(&quadScratch->depth);
    quadScratch->depth += ACROPOLIS_PROMENADE_GROUND_GLOW_DEPTH_BIAS;
    if (quadScratch->depth >= ACROPOLIS_PROMENADE_GROUND_GLOW_MIN_DEPTH) {
        quad->tpage     = ACROPOLIS_PROMENADE_GROUND_GLOW_TEXTURE_PAGE;
        quad->clut      = ACROPOLIS_PROMENADE_GROUND_GLOW_TEXTURE_CLUT;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        grey            = (gRandomLcgState >> 16) & (ACROPOLIS_PROMENADE_GROUND_GLOW_GREY_LEVEL_COUNT - 1);
        quad->r0        = grey;
        quad->g0        = grey;
        quad->b0        = grey;
        setSemiTrans(quad, 1);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)quadScratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectQuadCornersScratch);
    effectKillTask(work, task);
}

/// Binds the included lamp glow to the promenade's exported `void (Task*)` callback.
///
/// Define this function identifier immediately before the lamp fragment, which
/// undefines it. No arguments, conditional alternatives or tokens are constructed.
#define GLOW_LAMP_TASK acropolisPromenadeGlowLampTask
#include "../../shared/effects/acropolis_glows_lamp.inc.c"

#include "../../shared/effects/glow_draw_tinted_disc_no_bias.inc.c"
