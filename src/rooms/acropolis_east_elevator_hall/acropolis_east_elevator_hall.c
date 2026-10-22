#include "rooms/acropolis_east_elevator_hall.h"
#include "../../stages/akropolis/event_flags.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/gtemac.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "common.h"

#include "actors/task_tables.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/gameflag.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effects.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/model_objects.h"
#include "gameplay/player_actor.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/areas.h"
#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/gfxgte.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "mapui/map_akropolis.h"

#include "rooms/room.h"

#include "rooms/room_common.h"

#include "assets/rooms/akropolis/east_elevator_hall/declarations.inc"

extern void func_807245E4(void*);
extern void func_80724608(void*, s32, s32, void*);

static const char _gAcropolisEastElevatorHallPlayerDebugLabel[];

/// Keeps the reflection scale at this room's earlier rodata position.
///
/// 0 requires `planar_reflection_rodata.inc.c` before the shared implementation.
#define PLANAR_REFLECTION_DEFINE_SCALE_WITH_IMPLEMENTATION 0
#include "../../shared/room/planar_reflection.h"
/// Binds the shared beacon body to this room's public `void (Task*)` callback.
///
/// Required before `red_beacon.h` and its task fragment; gameplay imports this instance.
#define RED_BEACON_TASK acropolisEastElevatorHallRedBeaconTask
#include "../../shared/effects/red_beacon.h"

static void _acropolisEastElevatorHallInitializeRoomTask(Task* task);
static void _acropolisEastElevatorHallUpdatePlayerDebugDisplay(Task* unusedTask);

/// Scale handed to `ScaleMatrix` to flip the reflection across X.
#include "../../shared/room/planar_reflection_rodata.inc.c"

/// Room-task states 0..2: initialize, update the player debug display, release.
///
/// Copied by value before unchecked dispatch; keep the state within this range.
static const TaskFuncTable3 _gAcropolisEastElevatorHallRoomTaskStates = {
    { _acropolisEastElevatorHallInitializeRoomTask, _acropolisEastElevatorHallUpdatePlayerDebugDisplay, taskKill },
};

static AnimationSet _gAkropolisEastElevatorHallAcropolisEastElevatorHallOpeningPlayerLeadInAnim01Clip;
static AnimationSet _gAkropolisEastElevatorHallAcropolisEastElevatorHallOpeningPlayerMainAnim01Clip;
static AnimationSet _gAkropolisEastElevatorHallAcropolisEastElevatorHallOpeningPlayerExitAnim01Clip;

static s32  _acropolisEastElevatorHallResolveTransitionMessage(Task* task, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32  _acropolisEastElevatorHallRejectKeyItemMessage(Task* task, s32 messageId, s32 itemId, s32 unusedArg);
static s32  _acropolisEastElevatorHallStartOpeningEvent(Task* unusedTask, s32 messageId, const DirectionActionRequest* actionRequest, s32 unusedSecondArg);
static s32  _acropolisEastElevatorHallHandleCommandMessage(Task* unusedTask, s32 messageId, s32 commandId, s32 unusedExecutionMode);
static void _acropolisEastElevatorHallStartSkipCapSequence(void);

/// Key-item use request sent to the room task by the inventory menu.
enum { ACROPOLIS_EAST_ELEVATOR_HALL_MESSAGE_USE_KEY_ITEM = 0x13F1 };

#include "../../shared/room/planar_reflection_data.inc.c"

/// Execution priority shared by the player and attachment reflection tasks.
enum { ACROPOLIS_EAST_ELEVATOR_HALL_REFLECTION_PRIORITY = 112 };

/// Task recipes for the hall's player reflection (slot 0) and held models (slot 1).
///
/// Both start bodyless at the same priority and attach their cloned models during
/// initialization. Slot 1 is used by the shared reflection implementation; the
/// hall starts player reflections through gameplay's task bank. There is no
/// terminator. Descriptors are borrowed during spawning; callbacks and source
/// geometry must stay loaded for the spawned tasks' lifetimes.
static TaskDesc _gAcropolisEastElevatorHallReflectionTaskDescriptors[2] = {
    { { { TASK_BODY_NONE, ACROPOLIS_EAST_ELEVATOR_HALL_REFLECTION_PRIORITY } }, acropolisEastElevatorHallPlayerReflectionTask, { .value = 0 } },
    { { { TASK_BODY_NONE, ACROPOLIS_EAST_ELEVATOR_HALL_REFLECTION_PRIORITY } }, _planarReflectionAttachmentTask, { .value = 0 } },
};

/// Borrows this overlay's two reflection task descriptors.
///
/// Slot 0 spawns the player reflection; slot 1 spawns an attachment or equipment
/// reflection. There is no terminator. The table and its callbacks remain valid
/// while the overlay is loaded; the caller neither owns nor copies the table.
static inline TaskDesc* _planarReflectionGetTaskTable(void)
{
    return _gAcropolisEastElevatorHallReflectionTaskDescriptors;
}

#include "assets/rooms/akropolis/east_elevator_hall/clips.inc"

/// Set-table slots used by the opening's player requests and view-figure clips.
enum {
    ACROPOLIS_EAST_ELEVATOR_HALL_OPENING_PLAYER_ANIMATION        = 1,
    ACROPOLIS_EAST_ELEVATOR_HALL_WEAPON_ANIMATION                = 1,
    ACROPOLIS_EAST_ELEVATOR_HALL_VIEW_FIGURE_LEAD_IN_ANIMATION   = 1,
    ACROPOLIS_EAST_ELEVATOR_HALL_VIEW_FIGURE_MAIN_ANIMATION      = 2,
    ACROPOLIS_EAST_ELEVATOR_HALL_VIEW_FIGURE_EXIT_ANIMATION      = 3,
    ACROPOLIS_EAST_ELEVATOR_HALL_VIEW_FIGURE_COMMAND_2_ANIMATION = 5,
};

/// Player set table for the opening's main phase after the second CAP cue.
///
/// Slot 0 is absent; slot 1 is the only playable set. No terminator is read.
/// The player borrows this table and its clip until playback is replaced;
/// the room overlay must remain loaded throughout that interval.
#include "assets/rooms/akropolis/east_elevator_hall/acropolis_east_elevator_hall_opening_player_main_bank.inc"

/// Player set table for the opening's lead-in phase after the first CAP cue.
///
/// Slot 0 is absent and slot 1 is the only playable set, with no terminator.
/// Borrowed by player playback until replacement; keep the room overlay loaded.
#include "assets/rooms/akropolis/east_elevator_hall/acropolis_east_elevator_hall_opening_player_lead_in_bank.inc"

/// Player set table for the opening's final 80-frame phase.
///
/// Slot 0 is absent and slot 1 is the only playable set, with no terminator.
/// Borrowed by player playback until replacement; keep the room overlay loaded.
#include "assets/rooms/akropolis/east_elevator_hall/acropolis_east_elevator_hall_opening_player_exit_bank.inc"

/// Body clip restored on entry and after the hall's scripted figure sequences.
enum { ACROPOLIS_EAST_ELEVATOR_HALL_VIEW_FIGURE_DEFAULT_ANIMATION = 4 };

/// Event-script CAP slots, placed figure selector and sound cue.
enum {
    ACROPOLIS_EAST_ELEVATOR_HALL_OPENING_CAP_SEQUENCE      = 1,
    ACROPOLIS_EAST_ELEVATOR_HALL_COMMAND_2_CAP_SEQUENCE    = 2,
    ACROPOLIS_EAST_ELEVATOR_HALL_EVENT_FIGURE_INDEX        = 0,
    ACROPOLIS_EAST_ELEVATOR_HALL_EVENT_FIGURE_DRAW_ENABLED = 1,
    ACROPOLIS_EAST_ELEVATOR_HALL_OPENING_SOUND             = 0x51020003,
    ACROPOLIS_EAST_ELEVATOR_HALL_SKIP_END_VIEW             = 5,
};

#include "assets/rooms/akropolis/east_elevator_hall/events.inc"
/// Room receiver's transition, command, key-item and action message handlers.
///
/// Four entries followed by TASK_MESSAGE_TABLE_END; unsupported IDs return zero.
/// Borrowed by the room task until teardown. Transition payloads are request/reply
/// pointers, commands and key items use integer words, and actions borrow a request.
static TaskMessageEntry _gAcropolisEastElevatorHallRoomMessageTable[] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _acropolisEastElevatorHallResolveTransitionMessage },
    { ROOM_MESSAGE_COMMAND, _acropolisEastElevatorHallHandleCommandMessage },
    { ACROPOLIS_EAST_ELEVATOR_HALL_MESSAGE_USE_KEY_ITEM, _acropolisEastElevatorHallRejectKeyItemMessage },
    { DIRECTION_MESSAGE_ROOM_ACTION, _acropolisEastElevatorHallStartOpeningEvent },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Opening-event latch for this loaded overlay visit (0 not started, 1 started).
///
/// Set when the opening starts, so skipping does not clear it. Reloading the
/// overlay restores zero; persistent story progress is stored in game flags.
static s32 _gAcropolisEastElevatorHallOpeningEventStarted = 0;
#include "assets/rooms/akropolis/east_elevator_hall/header.inc"
#include "assets/rooms/akropolis/east_elevator_hall/geometry.inc"
#include "assets/rooms/akropolis/east_elevator_hall/view_boundaries.inc"
#include "assets/rooms/akropolis/east_elevator_hall/action_triggers.inc"
#include "assets/rooms/akropolis/east_elevator_hall/placements.inc"
#include "assets/rooms/akropolis/east_elevator_hall/sprites.inc"
#include "assets/rooms/akropolis/east_elevator_hall/lights.inc"
#include "assets/rooms/akropolis/east_elevator_hall/cameras.inc"
#include "assets/rooms/akropolis/east_elevator_hall/surfaces.inc"

/// Sound-script and view/map selectors for the hall's only warp endpoint.
enum {
    ACROPOLIS_EAST_ELEVATOR_HALL_WARP_ARRIVAL_SOUND   = 0x51020002,
    ACROPOLIS_EAST_ELEVATOR_HALL_WARP_DEPARTURE_SOUND = 0x51020001,
    ACROPOLIS_EAST_ELEVATOR_HALL_WARP_ARRIVAL_VIEW    = 4,
};

/// One-based endpoint slots packed into the departure action's two nibbles.
enum {
    ACROPOLIS_EAST_ELEVATOR_HALL_DEPARTURE_WARP_SLOT = 1,
    ACROPOLIS_EAST_ELEVATOR_HALL_SQUARE_ARRIVAL_SLOT = 2,
};

/// Source-element boundary between view 2's two authored sprite batches.
enum { ACROPOLIS_EAST_ELEVATOR_HALL_VIEW_2_FIRST_BATCH_COUNT = 22 };

/// Source-element boundary between view 3's two authored sprite batches.
enum { ACROPOLIS_EAST_ELEVATOR_HALL_VIEW_3_FIRST_BATCH_COUNT = 3 };

/// Loaded-bank script bases for the hall's paired walking and running cues.
enum {
    ACROPOLIS_EAST_ELEVATOR_HALL_FOOTSTEP_WALK = SOUND_SCRIPT_REQUEST_TYPE_1 | 17,
    ACROPOLIS_EAST_ELEVATOR_HALL_FOOTSTEP_RUN  = SOUND_SCRIPT_REQUEST_TYPE_1 | 19,
};

#include "../../shared/room/planar_reflection.inc.c"

void acropolisEastElevatorHallPlayerReflectionTask(Task* reflectionTask)
{
    _planarReflectionPlayerTask(reflectionTask);
}

#undef PLANAR_REFLECTION_DEFINE_SCALE_WITH_IMPLEMENTATION

/// Allows a room transition with the requested destination unchanged.
///
/// The resolve message borrows a readable eight-byte request and a writable
/// reply for this call; they may be the same record. Copies the entire record
/// for both queries and execution requests and returns 1 (transition allowed).
/// The receiving task and message ID are unused; no payload storage is retained.
static s32 _acropolisEastElevatorHallResolveTransitionMessage(Task* task, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    enum { ROOM_EVENT_RESULT_ALLOWED = 1 };

    *reply = *request;
    return ROOM_EVENT_RESULT_ALLOWED;
}

/// Refuses every key-item use request in this room.
///
/// `itemId` is the collected inventory item's ID and `unusedArg` is the unused
/// second message word. All arguments are ignored. Returns 0, which tells the
/// inventory menu that the item cannot be used here; no item is consumed.
static s32 _acropolisEastElevatorHallRejectKeyItemMessage(Task* task, s32 messageId, s32 itemId, s32 unusedArg)
{
    enum { ROOM_KEY_ITEM_RESULT_REFUSED = 0 };

    return ROOM_KEY_ITEM_RESULT_REFUSED;
}

/// Records the opening event's start and enables its subsequent story interactions.
///
/// Requires the live hall visit and game-flag bank. Latches this visit, selects
/// Acropolis progress 1 and objective 2, resets the follow-up dialogue state,
/// selects dialogue index 3 and unlocks the patio's cafeteria door. Records
/// progress at scene start, so skipping the scene retains the same story state.
static inline void _acropolisEastElevatorHallRecordOpeningProgress(void)
{
    enum {
        ACROPOLIS_EAST_ELEVATOR_HALL_OPENING_STARTED         = 1,
        ACROPOLIS_EAST_ELEVATOR_HALL_CAFETERIA_DOOR_UNLOCKED = 2,
        ACROPOLIS_EAST_ELEVATOR_HALL_OPENING_OBJECTIVE       = 2
    };

    _gAcropolisEastElevatorHallOpeningEventStarted = ACROPOLIS_EAST_ELEVATOR_HALL_OPENING_STARTED;
    gameFlagSetNibble(EVENT_FLAG_AKROPOLIS_CAFETERIA_ROUTE_PROGRESS, ACROPOLIS_EAST_ELEVATOR_HALL_OPENING_STARTED);
    gameFlagSetNibble(EVENT_FLAG_TELEPHONE_DIALOGUE_COUNTER, EVENT_FLAG_TELEPHONE_DIALOGUE_UNHEARD);
    gameFlagSetNibble(EVENT_FLAG_STORY_DIALOGUE_INDEX, AKROPOLIS_STORY_DIALOGUE_CIVILIAN_EVAC);
    gameFlagSetNibble(GAME_FLAG_PATIO_CAFETERIA_DOOR_STATE, ACROPOLIS_EAST_ELEVATOR_HALL_CAFETERIA_DOOR_UNLOCKED);
    gameFlagSetPackedByte(GAME_FLAG_CURRENT_OBJECTIVE, ACROPOLIS_EAST_ELEVATOR_HALL_OPENING_OBJECTIVE);
}

/// Starts the east elevator hall's opening scene once from room action zero.
///
/// Borrows a readable four-byte action request only during dispatch. Starts the
/// skippable scene and records story progress only while Acropolis progress and
/// the visit latch are both zero. Receiver, message ID and second word are unused;
/// no request storage is retained. Always returns zero; the sender ignores it.
static s32 _acropolisEastElevatorHallStartOpeningEvent(Task* unusedTask, s32 messageId, const DirectionActionRequest* actionRequest, s32 unusedSecondArg)
{
    enum {
        ACROPOLIS_EAST_ELEVATOR_HALL_OPENING_ACTION_ID   = 0,
        ACROPOLIS_EAST_ELEVATOR_HALL_OPENING_NOT_STARTED = 0
    };

    if (actionRequest->actionId == ACROPOLIS_EAST_ELEVATOR_HALL_OPENING_ACTION_ID &&
        gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_CAFETERIA_ROUTE_PROGRESS) == ACROPOLIS_EAST_ELEVATOR_HALL_OPENING_NOT_STARTED &&
        _gAcropolisEastElevatorHallOpeningEventStarted == ACROPOLIS_EAST_ELEVATOR_HALL_OPENING_NOT_STARTED) {
        evsStartScriptWithSkip(_gAcropolisEastElevatorHallOpeningScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gAcropolisEastElevatorHallOpeningSkipScript);
        _acropolisEastElevatorHallRecordOpeningProgress();
    }
    return 0;
}

/// Runs CAP sequence 2 with the room's scripted actor animation for command 2.
///
/// Other commands do nothing. Receiver, message ID and execution mode are unused;
/// always returns zero. The room and its script resources must remain loaded.
static s32 _acropolisEastElevatorHallHandleCommandMessage(Task* unusedTask, s32 messageId, s32 commandId, s32 unusedExecutionMode)
{
    enum { ACROPOLIS_EAST_ELEVATOR_HALL_COMMAND_CAP_SEQUENCE_2 = 2 };

    if (commandId == ACROPOLIS_EAST_ELEVATOR_HALL_COMMAND_CAP_SEQUENCE_2) {
        evsStartScript(_gAcropolisEastElevatorHallCommand2Script, EVENT_SCRIPT_HUD_HIDE_RESTORE);
    }
    return 0;
}

/// Starts CAP sequence 16 at the end of the opening scene's skip script.
///
/// Requires the hall's CAP file loaded; uses transition playback and variant 0.
static void _acropolisEastElevatorHallStartSkipCapSequence(void)
{
    enum { ACROPOLIS_EAST_ELEVATOR_HALL_SKIP_CAP_SEQUENCE = 16 };
    capStartSequenceSlot(ACROPOLIS_EAST_ELEVATOR_HALL_SKIP_CAP_SEQUENCE, CAP_PLAYBACK_DISPLAY_TRANSITION, 0);
}

/// Registers the hall's room receiver and starts placed actor 0's initial animation.
///
/// Requires a live bodyless task in state 0, initialized placements and loaded
/// animation resources. Publishes `GAME_TASK_SLOT_ROOM`, enables actor drawing
/// and borrows its animation request synchronously, then enters state 1.
static void _acropolisEastElevatorHallInitializeRoomTask(Task* task)
{
    enum {
        ACROPOLIS_EAST_ELEVATOR_HALL_INITIAL_PLACED_ACTOR      = 0,
        ACROPOLIS_EAST_ELEVATOR_HALL_PLACED_ACTOR_DRAW_ENABLED = 1
    };
    task->msgTable = _gAcropolisEastElevatorHallRoomMessageTable;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    sceneSetPlacedActorDrawMode(ACROPOLIS_EAST_ELEVATOR_HALL_INITIAL_PLACED_ACTOR, ACROPOLIS_EAST_ELEVATOR_HALL_PLACED_ACTOR_DRAW_ENABLED);
    TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(ACROPOLIS_EAST_ELEVATOR_HALL_INITIAL_PLACED_ACTOR), ACTOR_MESSAGE_PLAY_ANIMATION, &_gAcropolisEastElevatorHallViewFigureDefaultAnimationRequest, 0);
    task->state++;
}

/// Player diagnostic label, with its eight-byte stored representation retained.
///
/// The visible string ends after "Player". The final byte has no established
/// display meaning; keep it and the rodata position intact.
static const char _gAcropolisEastElevatorHallPlayerDebugLabel[8] = "Player\0\x0F";
#include "assets/rooms/akropolis/east_elevator_hall/markers.inc"

/// Updates the player's diagnostic display during the room task's idle state.
///
/// With a nonzero debug mode, calls the player diagnostic hook, then checks the
/// mode again before passing the player label and display position (-140, -50).
/// The receiver is unused; the player slot and diagnostic hooks must be live.
static void _acropolisEastElevatorHallUpdatePlayerDebugDisplay(Task* unusedTask)
{
    enum {
        ACROPOLIS_EAST_ELEVATOR_HALL_PLAYER_DEBUG_X = -140,
        ACROPOLIS_EAST_ELEVATOR_HALL_PLAYER_DEBUG_Y = -50
    };

    if (gDisplayState.debugMode != 0) {
        func_807245E4(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER));
        if (gDisplayState.debugMode != 0) {
            func_80724608(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ACROPOLIS_EAST_ELEVATOR_HALL_PLAYER_DEBUG_X,
                          ACROPOLIS_EAST_ELEVATOR_HALL_PLAYER_DEBUG_Y, _gAcropolisEastElevatorHallPlayerDebugLabel);
        }
    }
}

void acropolisEastElevatorHallRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers = _gAcropolisEastElevatorHallRoomTaskStates;
    stateHandlers.funcs[task->state](task);
}

void acropolisEastElevatorHallReflectionsAndBeaconsTask(Task* task)
{
    enum {
        ACROPOLIS_EAST_ELEVATOR_HALL_EFFECTS_INITIALIZE   = 0,
        ACROPOLIS_EAST_ELEVATOR_HALL_EFFECTS_UPDATE       = 1,
        ACROPOLIS_EAST_ELEVATOR_HALL_REFLECTION_TASK_BANK = 1,
        ACROPOLIS_EAST_ELEVATOR_HALL_REFLECTION_TASK_SLOT = 0x25,
        ACROPOLIS_EAST_ELEVATOR_HALL_REFLECTION_FLOOR     = 0,
        ACROPOLIS_EAST_ELEVATOR_HALL_REFLECTION_MIRROR    = 1,
        ACROPOLIS_EAST_ELEVATOR_HALL_BEACON_VIEW          = 2,
        ACROPOLIS_EAST_ELEVATOR_HALL_BEACON_SMALL_RATE    = 3,
        ACROPOLIS_EAST_ELEVATOR_HALL_BEACON_SMALL_SIZE    = 12,
        ACROPOLIS_EAST_ELEVATOR_HALL_BEACON_LARGE_RATE    = 4,
        ACROPOLIS_EAST_ELEVATOR_HALL_BEACON_LARGE_SIZE    = 18
    };
    GfxCoord* parentCoord;

    parentCoord = task->extra.coordBody->coord;
    switch (task->state) {
        case ACROPOLIS_EAST_ELEVATOR_HALL_EFFECTS_INITIALIZE:
            taskSpawn(ACROPOLIS_EAST_ELEVATOR_HALL_REFLECTION_TASK_BANK, ACROPOLIS_EAST_ELEVATOR_HALL_REFLECTION_TASK_SLOT, ACROPOLIS_EAST_ELEVATOR_HALL_REFLECTION_FLOOR, 0);
            taskSpawn(ACROPOLIS_EAST_ELEVATOR_HALL_REFLECTION_TASK_BANK, ACROPOLIS_EAST_ELEVATOR_HALL_REFLECTION_TASK_SLOT, ACROPOLIS_EAST_ELEVATOR_HALL_REFLECTION_MIRROR, 0);
            task->state++;
            /* fallthrough */
        case ACROPOLIS_EAST_ELEVATOR_HALL_EFFECTS_UPDATE:
            // Each beacon lasts one frame, so refresh all six while this view is active.
            if (gGameSession->location.view == ACROPOLIS_EAST_ELEVATOR_HALL_BEACON_VIEW) {
                SVECTOR spawnOffset = _gAcropolisEastElevatorHallFirstBeaconOffset;

                effectSpawn(EFFECT_ACROPOLIS_EAST_ELEVATOR_HALL_RED_BEACON, parentCoord, RED_BEACON_ARG(ACROPOLIS_EAST_ELEVATOR_HALL_BEACON_SMALL_RATE, ACROPOLIS_EAST_ELEVATOR_HALL_BEACON_SMALL_SIZE), &spawnOffset);
                spawnOffset.vx = 0x1600;
                spawnOffset.vy = -0x985;
                spawnOffset.vz = 0x55;
                effectSpawn(EFFECT_ACROPOLIS_EAST_ELEVATOR_HALL_RED_BEACON, parentCoord, RED_BEACON_ARG(ACROPOLIS_EAST_ELEVATOR_HALL_BEACON_SMALL_RATE, ACROPOLIS_EAST_ELEVATOR_HALL_BEACON_SMALL_SIZE), &spawnOffset);
                spawnOffset.vx = 0x1600;
                spawnOffset.vy = -0xA81;
                spawnOffset.vz = -0x1CA;
                effectSpawn(EFFECT_ACROPOLIS_EAST_ELEVATOR_HALL_RED_BEACON, parentCoord, RED_BEACON_ARG(ACROPOLIS_EAST_ELEVATOR_HALL_BEACON_LARGE_RATE, ACROPOLIS_EAST_ELEVATOR_HALL_BEACON_LARGE_SIZE), &spawnOffset);
                spawnOffset.vx = 0x1600;
                spawnOffset.vy = -0xA93;
                spawnOffset.vz = -0x61C;
                effectSpawn(EFFECT_ACROPOLIS_EAST_ELEVATOR_HALL_RED_BEACON, parentCoord, RED_BEACON_ARG(ACROPOLIS_EAST_ELEVATOR_HALL_BEACON_LARGE_RATE, ACROPOLIS_EAST_ELEVATOR_HALL_BEACON_LARGE_SIZE), &spawnOffset);
                spawnOffset.vx = 0x1600;
                spawnOffset.vy = -0x460;
                spawnOffset.vz = -0x1A1;
                effectSpawn(EFFECT_ACROPOLIS_EAST_ELEVATOR_HALL_RED_BEACON, parentCoord, RED_BEACON_ARG(ACROPOLIS_EAST_ELEVATOR_HALL_BEACON_LARGE_RATE, ACROPOLIS_EAST_ELEVATOR_HALL_BEACON_LARGE_SIZE), &spawnOffset);
                spawnOffset.vx = 0x1600;
                spawnOffset.vy = -0x449;
                spawnOffset.vz = -0x635;
                effectSpawn(EFFECT_ACROPOLIS_EAST_ELEVATOR_HALL_RED_BEACON, parentCoord, RED_BEACON_ARG(ACROPOLIS_EAST_ELEVATOR_HALL_BEACON_LARGE_RATE, ACROPOLIS_EAST_ELEVATOR_HALL_BEACON_LARGE_SIZE), &spawnOffset);
            }
            break;
    }
}

#include "../../shared/effects/red_beacon_task.inc.c"
#undef RED_BEACON_TASK

/// Projects a view-space translation and queues a single additive gray pixel.
///
/// `composedCoord` must have a current view-space composition. Only its cached
/// x/y/z translation is read, in signed 32-bit game-coordinate units, and each
/// component narrows to signed 16 bits before projection through `GsWSMATRIX`.
/// The GTE projection geometry must already be configured; FLAG is not tested.
/// `tileScratch` borrows one complete, word-aligned `EffectPointTileScratch`
/// block for this call. Neither input nor scratch storage is retained.
///
/// The word-aligned frame arena needs one `TILE_1` even at rejected depths and
/// one additional `DR_TPAGE` when SZ3 / 4 is at least `EFFECT_POINT_TILE_MIN_DEPTH`.
/// The current ordering table must provide 1024 depth tags; scaled depths wrap
/// into those tags. Queued packets remain live until GPU drawing completes.
/// Changes GTE state and leaves additive GPU draw mode active after the
/// accepted pixel.
static inline void _acropolisEastElevatorHallDrawPointTile(const GfxCoord* composedCoord, EffectPointTileScratch* tileScratch)
{
    enum {
        ACROPOLIS_EAST_ELEVATOR_HALL_POINT_TILE_COLOR                   = 0x80,
        ACROPOLIS_EAST_ELEVATOR_HALL_POINT_TILE_DEPTH_TO_OT_BYTES_SHIFT = 2,
    };

    TILE_1* tile;

    // Narrow the composed view position before perspective projection.
    tileScratch->viewPoint.vx = composedCoord->workm.t[0];
    tileScratch->viewPoint.vy = composedCoord->workm.t[1];
    tileScratch->viewPoint.vz = composedCoord->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&tileScratch->viewPoint);
    gte_rtps();
    // Reserve the tile even when its depth will reject it.
    tile           = gGpuPrimCursor;
    gGpuPrimCursor = tile + 1;
    setTile1(tile);
    gte_stsxy(&tile->x0);
    gte_stszotz(&tileScratch->depth);
    if (tileScratch->depth >= EFFECT_POINT_TILE_MIN_DEPTH) {
        setRGB0(tile, ACROPOLIS_EAST_ELEVATOR_HALL_POINT_TILE_COLOR, ACROPOLIS_EAST_ELEVATOR_HALL_POINT_TILE_COLOR, ACROPOLIS_EAST_ELEVATOR_HALL_POINT_TILE_COLOR);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(
                    (((u32)tileScratch->depth << gDisplayState.otDepthShift) >> ACROPOLIS_EAST_ELEVATOR_HALL_POINT_TILE_DEPTH_TO_OT_BYTES_SHIFT) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK),
                tile);
        // Prepend the draw-mode command at the same depth so blending is set before the tile.
        gpuSetPrimitiveBlendMode(tile, GPU_BLEND_ADD, tileScratch->depth);
    }
}

void acropolisEastElevatorHallPointTileTask(Task* task)
{
    EffectPointTileScratch* tileScratch;
    GfxCoord*               effectCoord;
    void*                   effectWork;

    effectCoord = task->extra.coordBody->coord;
    effectWork  = task->spawnArg2.pointer;
    actorRenderComposeCoord(effectCoord);
    tileScratch = SCRATCH_STACK_RESERVE_BLOCK(EffectPointTileScratch);
    _acropolisEastElevatorHallDrawPointTile(effectCoord, tileScratch);
    SCRATCH_STACK_RELEASE_BLOCK(EffectPointTileScratch);
    effectKillTask(effectWork, task);
}
