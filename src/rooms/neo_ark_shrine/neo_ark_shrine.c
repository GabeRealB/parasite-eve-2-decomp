#include "rooms/neo_ark_shrine.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>

#include "common.h"

#include "neo_ark_shrine_private.h"

#include "gameplay/action_prompt.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/player_actor.h"
#include "gameplay/direction.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"
#include "gameplay/message.h"
#include "gameplay/pad_script.h"

#include "main/display.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_neo_ark.h"

#include "overlay.h"

#include "rooms/room_common.h"
#include "../../shared/room/action_prompt.h"

/// Edge of one square tile of the sliding-tile puzzle, in screen pixels and in
/// texels alike: the tile sheet is drawn unscaled.
#define NEO_ARK_SHRINE_TILE_SIZE 32

/// Tile zero is the gap; the remaining tile numbers select texture-sheet squares.
enum { NEO_ARK_SHRINE_PUZZLE_GAP_TILE = 0 };

/// Arrangement-check replies selecting the puzzle script's next action.
///
/// Replies select puzzle completion, enemy release or a room layout transition.
enum {
    NEO_ARK_SHRINE_PUZZLE_MATCH_NONE            = 0,
    NEO_ARK_SHRINE_PUZZLE_MATCH_SOLVED          = 1,
    NEO_ARK_SHRINE_PUZZLE_MATCH_RELEASE_ENEMIES = 2,
    NEO_ARK_SHRINE_PUZZLE_MATCH_ACTIVATE_LAYOUT = 3,
    NEO_ARK_SHRINE_PUZZLE_MATCH_RESTORE_LAYOUT  = 4,
};

enum {
    NEO_ARK_SHRINE_OBJECT_EVENT_FIRST_KEY  = 2,
    NEO_ARK_SHRINE_NEIGHBOUR_END           = 255,
    NEO_ARK_SHRINE_NEIGHBOUR_LIST_CAPACITY = 5,
};

static s16  _neoArkShrineCheckPuzzleArrangement(void);
static void _neoArkShrineRoomIdle(Task* task);

static s32  _neoArkShrineRefuseKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedArg);
static s32  _neoArkShrineResolveRoomEvent(Task* unusedTask, s32 unusedMessageId, RoomEventMsg* request, RoomEventMsg* reply);
static s32  _neoArkShrineHandleRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 commandId, s32 unusedSecondArg);
static s32  _neoArkShrineHandleRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg);
static void _neoArkShrinePuzzleSessionTask(Task* task);

/// Shrine handlers for exits, key-item refusal, puzzle actions and CAP commands.
///
/// Four handlers and a `TASK_MESSAGE_TABLE_END` record; the room task borrows
/// this read-only table and its callbacks while the overlay remains loaded.
static const TaskMessageEntry _gNeoArkShrineRoomMessages[5] SECTION(".data") = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _neoArkShrineResolveRoomEvent },
    { ROOM_MESSAGE_USE_KEY_ITEM, _neoArkShrineRefuseKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _neoArkShrineHandleRoomAction },
    { ROOM_MESSAGE_COMMAND, _neoArkShrineHandleRoomCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Bodyless puzzle-session recipe at entry 0, followed by `TASK_DESC_END`.
///
/// Priority 32 and zero metadata/payloads. The session holds the player and
/// owns the puzzle-controller handle until completion. Spawning reads the
/// recipe synchronously; callback code must remain loaded for the session.
static TaskDesc _gNeoArkShrinePuzzleSessionTaskDescs[2] = {
    { .header = { .fields = { .flags = TASK_BODY_NONE, .priority = TASK_PRIORITY_SCENE } }, .callback = _neoArkShrinePuzzleSessionTask, .data = { .value = 0 } },
    { .header = { .fields = { .flags = TASK_DESC_END } } },
};

/// Variant key for first/repeat playback of the ready shrine object's event.
///
/// Starts at 2 and becomes 1 after the first object-7 state-1 dispatch.
/// Other object states play key 0 without changing it. Retained for this
/// overlay load; word comparisons/stores precede signed-halfword CAP transport.
static s32 _gNeoArkShrineObjectEventVariantKey = NEO_ARK_SHRINE_OBJECT_EVENT_FIRST_KEY;

static TmdBone _gNeoArkShrineModel049E8Skeleton[1] = {
#include "assets/neo_ark_shrine_model_049E8_skeleton.inc"
};

static u32 _gNeoArkShrineModel049E8PartVerts[1] = {
#include "assets/neo_ark_shrine_model_049E8_partVerts.inc"
};

static SVECTOR _gNeoArkShrineModel049E8Verts[33] = {
#include "assets/neo_ark_shrine_model_049E8_verts.inc"
};

static u32 _gNeoArkShrineModel049E8Stream[106] = {
#include "assets/neo_ark_shrine_model_049E8_stream.inc"
};

static TmdSource _gNeoArkShrineModel049E8 = {
    0,
    800,
    0,
    1,
    _gNeoArkShrineModel049E8PartVerts,
    _gNeoArkShrineModel049E8Verts,
    &_gNeoArkShrineModel049E8Verts[33],
    _gNeoArkShrineModel049E8Skeleton,
    _gNeoArkShrineModel049E8Stream,
};

static TmdBone _gNeoArkShrineModel04C6CSkeleton[1] = {
#include "assets/neo_ark_shrine_model_04C6C_skeleton.inc"
};

static u32 _gNeoArkShrineModel04C6CPartVerts[1] = {
#include "assets/neo_ark_shrine_model_04C6C_partVerts.inc"
};

static SVECTOR _gNeoArkShrineModel04C6CVerts[18] = {
#include "assets/neo_ark_shrine_model_04C6C_verts.inc"
};

static u32 _gNeoArkShrineModel04C6CStream[109] = {
#include "assets/neo_ark_shrine_model_04C6C_stream.inc"
};

static TmdSource _gNeoArkShrineModel04C6C = {
    0,
    672,
    0,
    1,
    _gNeoArkShrineModel04C6CPartVerts,
    _gNeoArkShrineModel04C6CVerts,
    &_gNeoArkShrineModel04C6CVerts[18],
    _gNeoArkShrineModel04C6CSkeleton,
    _gNeoArkShrineModel04C6CStream,
};

TaskDesc gNeoArkShrinePuzzleCursorTaskDescs[1] = {
    {
        .header   = { .fields = { .flags = TASK_BODY_NONE, .priority = TASK_PRIORITY_CONTROL } },
        .callback = neoArkShrinePuzzleCursorTask,
        .data     = { .value = 0 },
    },
};

u16 gNeoArkShrinePuzzleInitialCellTiles[NEO_ARK_SHRINE_PUZZLE_TILE_COUNT] = {
    1,
    2,
    3,
    4,
    5,
    6,
    7,
    8,
    9,
    10,
    11,
    12,
    13,
    14,
    15,
    0,
};

enum { NEO_ARK_SHRINE_PUZZLE_PROMPT_EXAMINE = 0 };

ActionPromptHotspot gNeoArkShrinePuzzleHotspots[NEO_ARK_SHRINE_PUZZLE_HOTSPOT_COUNT] = {
    { -64, -64, NEO_ARK_SHRINE_TILE_SIZE, NEO_ARK_SHRINE_TILE_SIZE, 0, NEO_ARK_SHRINE_PUZZLE_PROMPT_EXAMINE, false },
    { -32, -64, NEO_ARK_SHRINE_TILE_SIZE, NEO_ARK_SHRINE_TILE_SIZE, 1, NEO_ARK_SHRINE_PUZZLE_PROMPT_EXAMINE, false },
    { 0, -64, NEO_ARK_SHRINE_TILE_SIZE, NEO_ARK_SHRINE_TILE_SIZE, 2, NEO_ARK_SHRINE_PUZZLE_PROMPT_EXAMINE, false },
    { 32, -64, NEO_ARK_SHRINE_TILE_SIZE, NEO_ARK_SHRINE_TILE_SIZE, 3, NEO_ARK_SHRINE_PUZZLE_PROMPT_EXAMINE, false },
    { -64, -32, NEO_ARK_SHRINE_TILE_SIZE, NEO_ARK_SHRINE_TILE_SIZE, 4, NEO_ARK_SHRINE_PUZZLE_PROMPT_EXAMINE, false },
    { -32, -32, NEO_ARK_SHRINE_TILE_SIZE, NEO_ARK_SHRINE_TILE_SIZE, 5, NEO_ARK_SHRINE_PUZZLE_PROMPT_EXAMINE, false },
    { 0, -32, NEO_ARK_SHRINE_TILE_SIZE, NEO_ARK_SHRINE_TILE_SIZE, 6, NEO_ARK_SHRINE_PUZZLE_PROMPT_EXAMINE, false },
    { 32, -32, NEO_ARK_SHRINE_TILE_SIZE, NEO_ARK_SHRINE_TILE_SIZE, 7, NEO_ARK_SHRINE_PUZZLE_PROMPT_EXAMINE, false },
    { -64, 0, NEO_ARK_SHRINE_TILE_SIZE, NEO_ARK_SHRINE_TILE_SIZE, 8, NEO_ARK_SHRINE_PUZZLE_PROMPT_EXAMINE, false },
    { -32, 0, NEO_ARK_SHRINE_TILE_SIZE, NEO_ARK_SHRINE_TILE_SIZE, 9, NEO_ARK_SHRINE_PUZZLE_PROMPT_EXAMINE, false },
    { 0, 0, NEO_ARK_SHRINE_TILE_SIZE, NEO_ARK_SHRINE_TILE_SIZE, 10, NEO_ARK_SHRINE_PUZZLE_PROMPT_EXAMINE, false },
    { 32, 0, NEO_ARK_SHRINE_TILE_SIZE, NEO_ARK_SHRINE_TILE_SIZE, 11, NEO_ARK_SHRINE_PUZZLE_PROMPT_EXAMINE, false },
    { -64, 32, NEO_ARK_SHRINE_TILE_SIZE, NEO_ARK_SHRINE_TILE_SIZE, 12, NEO_ARK_SHRINE_PUZZLE_PROMPT_EXAMINE, false },
    { -32, 32, NEO_ARK_SHRINE_TILE_SIZE, NEO_ARK_SHRINE_TILE_SIZE, 13, NEO_ARK_SHRINE_PUZZLE_PROMPT_EXAMINE, false },
    { 0, 32, NEO_ARK_SHRINE_TILE_SIZE, NEO_ARK_SHRINE_TILE_SIZE, 14, NEO_ARK_SHRINE_PUZZLE_PROMPT_EXAMINE, false },
    { 32, 32, NEO_ARK_SHRINE_TILE_SIZE, NEO_ARK_SHRINE_TILE_SIZE, 15, NEO_ARK_SHRINE_PUZZLE_PROMPT_EXAMINE, false },
    { -160, -120, 320, 240, NEO_ARK_SHRINE_HOTSPOT_OFF_BOARD, NEO_ARK_SHRINE_PUZZLE_PROMPT_EXAMINE, false },
    { 0, 0, 0, 0, ACTION_PROMPT_HOTSPOT_END, 0, 0 },
};

TaskDesc gNeoArkShrinePuzzleTaskDescs[3] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, neoArkShrinePuzzleTask, { .value = 0 } },
    { { { TASK_BODY_TMD, TASK_PRIORITY_CONTROL } }, neoArkShrineFirstFallingPropTask, { .model = &_gNeoArkShrineModel049E8 } },
    { { { TASK_BODY_TMD, TASK_PRIORITY_CONTROL } }, neoArkShrineSecondFallingPropTask, { .model = &_gNeoArkShrineModel04C6C } },
};

/// Screen origins of the sixteen puzzle cells, in row-major cell order.
///
/// Coordinates are display-centred pixels on a 4x4 grid of 32-pixel cells.
/// The cell-to-tile board selects each tile's animation target from this table.
static NeoArkShrineTileOrigin _gNeoArkShrinePuzzleCellOrigins[16] = {
    { -64, -64 },
    { -32, -64 },
    { 0, -64 },
    { 32, -64 },
    { -64, -32 },
    { -32, -32 },
    { 0, -32 },
    { 32, -32 },
    { -64, 0 },
    { -32, 0 },
    { 0, 0 },
    { 32, 0 },
    { -64, 32 },
    { -32, 32 },
    { 0, 32 },
    { 32, 32 },
};

NeoArkShrineTileOrigin gNeoArkShrinePuzzleInitialTileOrigins[NEO_ARK_SHRINE_PUZZLE_TILE_COUNT] = {
    { 32, 32 },
    { -64, -64 },
    { -32, -64 },
    { 0, -64 },
    { 32, -64 },
    { -64, -32 },
    { -32, -32 },
    { 0, -32 },
    { 32, -32 },
    { -64, 0 },
    { -32, 0 },
    { 0, 0 },
    { 32, 0 },
    { -64, 32 },
    { -32, 32 },
    { 0, 32 },
};

/// Texture-sheet origins of the puzzle tiles, indexed by tile number 0..15.
///
/// Coordinates are texels in the 32x32 tile sheet. Tile 0 is the undrawn gap;
/// its retained (96, 96) origin is not sampled. The table has no terminator.
static NeoArkShrineTileOrigin _gNeoArkShrinePuzzleTileTextureOrigins[16] = {
    { 96, 96 },
    { 0, 0 },
    { 32, 0 },
    { 64, 0 },
    { 96, 0 },
    { 0, 32 },
    { 32, 32 },
    { 64, 32 },
    { 96, 32 },
    { 0, 64 },
    { 32, 64 },
    { 64, 64 },
    { 96, 64 },
    { 0, 96 },
    { 32, 96 },
    { 64, 96 },
};

/// Orthogonal neighbours of each row-major cell of the 4x4 sliding-tile board.
///
/// Cell indices are 0..15; each signed-halfword row holds up to four neighbours
/// followed by `NEO_ARK_SHRINE_NEIGHBOUR_END` (255). Remaining entries also hold
/// END. Selection must be a board cell, excluding the off-board hotspot.
static const s16 _gNeoArkShrinePuzzleCellNeighbours[NEO_ARK_SHRINE_PUZZLE_CELL_COUNT][NEO_ARK_SHRINE_NEIGHBOUR_LIST_CAPACITY] SECTION(".data") = {
    { 1, 4, NEO_ARK_SHRINE_NEIGHBOUR_END, NEO_ARK_SHRINE_NEIGHBOUR_END, NEO_ARK_SHRINE_NEIGHBOUR_END },
    { 0, 2, 5, NEO_ARK_SHRINE_NEIGHBOUR_END, NEO_ARK_SHRINE_NEIGHBOUR_END },
    { 1, 3, 6, NEO_ARK_SHRINE_NEIGHBOUR_END, NEO_ARK_SHRINE_NEIGHBOUR_END },
    { 2, 7, NEO_ARK_SHRINE_NEIGHBOUR_END, NEO_ARK_SHRINE_NEIGHBOUR_END, NEO_ARK_SHRINE_NEIGHBOUR_END },
    { 0, 5, 8, NEO_ARK_SHRINE_NEIGHBOUR_END, NEO_ARK_SHRINE_NEIGHBOUR_END },
    { 1, 4, 6, 9, NEO_ARK_SHRINE_NEIGHBOUR_END },
    { 2, 5, 7, 10, NEO_ARK_SHRINE_NEIGHBOUR_END },
    { 3, 6, 11, NEO_ARK_SHRINE_NEIGHBOUR_END, NEO_ARK_SHRINE_NEIGHBOUR_END },
    { 4, 9, 12, NEO_ARK_SHRINE_NEIGHBOUR_END, NEO_ARK_SHRINE_NEIGHBOUR_END },
    { 5, 8, 10, 13, NEO_ARK_SHRINE_NEIGHBOUR_END },
    { 6, 9, 11, 14, NEO_ARK_SHRINE_NEIGHBOUR_END },
    { 7, 10, 15, NEO_ARK_SHRINE_NEIGHBOUR_END, NEO_ARK_SHRINE_NEIGHBOUR_END },
    { 8, 13, NEO_ARK_SHRINE_NEIGHBOUR_END, NEO_ARK_SHRINE_NEIGHBOUR_END, NEO_ARK_SHRINE_NEIGHBOUR_END },
    { 9, 12, 14, NEO_ARK_SHRINE_NEIGHBOUR_END, NEO_ARK_SHRINE_NEIGHBOUR_END },
    { 10, 13, 15, NEO_ARK_SHRINE_NEIGHBOUR_END, NEO_ARK_SHRINE_NEIGHBOUR_END },
    { 11, 14, NEO_ARK_SHRINE_NEIGHBOUR_END, NEO_ARK_SHRINE_NEIGHBOUR_END, NEO_ARK_SHRINE_NEIGHBOUR_END },
};

static void _neoArkShrineInitializeRoom(Task* task);

/// Refuses every collected key item offered to the shrine room.
///
/// Handles `ROOM_MESSAGE_USE_KEY_ITEM`; the item menu supplies `itemId` and a
/// zero second payload. All arguments are ignored and no room state changes.
static s32 _neoArkShrineRefuseKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Resolves a shrine exit and blocks Power Plant 1 until the puzzle is solved.
///
/// Handles `ROOM_EVENT_MESSAGE_RESOLVE`. Copies the eight-byte borrowed request
/// to the writable reply before map resolution; the pointers may alias.
/// Returns 1 for allowed exits and 0 for the blocked exit, including queries.
/// Executing a blocked exit marks its optional flag with 2 and starts CAP
/// command 4. Requires the Neo Ark map and room CAP resources to be loaded.
static s32 _neoArkShrineResolveRoomEvent(Task* unusedTask, s32 unusedMessageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    enum { NEO_ARK_SHRINE_EXIT_ALLOWED          = 1,
           NEO_ARK_SHRINE_EXIT_BLOCKED          = 0,
           NEO_ARK_SHRINE_BLOCKED_EXIT_MAP_MARK = 2,
           NEO_ARK_SHRINE_BLOCKED_EXIT_COMMAND  = 4 };

    *reply = *request;
    mapNeoArkResolveRoomVariant(request, reply);
    if (request->areaId != GAME_AREA_NEO_ARK_POWER_PLANT_1) {
        return NEO_ARK_SHRINE_EXIT_ALLOWED;
    }
    if (gameFlagGetNibble(GAME_FLAG_NEO_ARK_SHRINE_PUZZLE_SOLVED) != 0) {
        return NEO_ARK_SHRINE_EXIT_ALLOWED;
    }
    if (request->queryOnly != ROOM_EVENT_EXECUTE) {
        return NEO_ARK_SHRINE_EXIT_BLOCKED;
    }
    gameFlagSetNibbleIfPresent(request->flagId, NEO_ARK_SHRINE_BLOCKED_EXIT_MAP_MARK);
    capRunCommandWithTransition(NEO_ARK_SHRINE_BLOCKED_EXIT_COMMAND);
    return NEO_ARK_SHRINE_EXIT_BLOCKED;
}

/// Selects shrine CAP playback from object state and Power Plant 1 progress.
///
/// Handles `ROOM_MESSAGE_COMMAND` with an integer command and ignored second
/// word. Command 7 runs sequence 7: object 7 in state 1 uses the latched event
/// key (initially 2, then 1), other states use key 0. Command 5 runs CAP command
/// 5 before Power Plant 1 is cleared and 12 afterwards. Always returns zero.
static s32 _neoArkShrineHandleRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 commandId, s32 unusedSecondArg)
{
    enum { NEO_ARK_SHRINE_COMMAND_OBJECT_EVENT            = 7,
           NEO_ARK_SHRINE_EVENT_OBJECT                    = 7,
           NEO_ARK_SHRINE_EVENT_OBJECT_READY              = 1,
           NEO_ARK_SHRINE_OBJECT_EVENT_SEQUENCE           = 7,
           NEO_ARK_SHRINE_COMMAND_POWER_PLANT_1           = 5,
           NEO_ARK_SHRINE_POWER_PLANT_1_UNCLEARED_COMMAND = 5,
           NEO_ARK_SHRINE_POWER_PLANT_1_CLEARED_COMMAND   = 12 };
    s32 objectState;

    if (commandId == NEO_ARK_SHRINE_COMMAND_OBJECT_EVENT) {
        objectState = areaGetCurrentObjectState(NEO_ARK_SHRINE_EVENT_OBJECT);
        if (objectState == NEO_ARK_SHRINE_EVENT_OBJECT_READY) {
            capStartSequenceSlot(NEO_ARK_SHRINE_OBJECT_EVENT_SEQUENCE, CAP_PLAYBACK_DISPLAY_TRANSITION, (s16)_gNeoArkShrineObjectEventVariantKey);
            if (_gNeoArkShrineObjectEventVariantKey == NEO_ARK_SHRINE_OBJECT_EVENT_FIRST_KEY) {
                _gNeoArkShrineObjectEventVariantKey = objectState;
            }
        } else {
            capStartSequenceSlot(NEO_ARK_SHRINE_OBJECT_EVENT_SEQUENCE, CAP_PLAYBACK_DISPLAY_TRANSITION, 0);
        }
    }
    if (commandId == NEO_ARK_SHRINE_COMMAND_POWER_PLANT_1) {
        capRunCommandWithTransition(gameFlagGetNibble(GAME_FLAG_NEO_ARK_POWER_PLANT_1_CLEARED) == 0 ? NEO_ARK_SHRINE_POWER_PLANT_1_UNCLEARED_COMMAND : NEO_ARK_SHRINE_POWER_PLANT_1_CLEARED_COMMAND);
    }
    return 0;
}

/// Opens the sliding-tile puzzle while Power Plant 2 remains uncleared.
///
/// Handles `DIRECTION_MESSAGE_ROOM_ACTION` with a borrowed four-byte request
/// and ignored second word. Action 1 spawns the puzzle session, or runs CAP
/// command 9 once Power Plant 2 is cleared. Other actions do nothing; returns zero.
static s32 _neoArkShrineHandleRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    enum { NEO_ARK_SHRINE_ACTION_OPEN_PUZZLE            = 1,
           NEO_ARK_SHRINE_POWER_PLANT_2_CLEARED_COMMAND = 9 };

    if (request->actionId == NEO_ARK_SHRINE_ACTION_OPEN_PUZZLE) {
        if (gameFlagGetNibble(GAME_FLAG_NEO_ARK_POWER_PLANT_2_CLEARED) == 0) {
            taskSpawnFromTable(_gNeoArkShrinePuzzleSessionTaskDescs, 0, 0, 0);
        } else {
            capRunCommandWithTransition(NEO_ARK_SHRINE_POWER_PLANT_2_CLEARED_COMMAND);
        }
    }
    return 0;
}

/// Holds the player while the puzzle runs, then polls and releases its task.
///
/// State 0 requires a bodyless task and a live player; holds and hides the player
/// before spawning the puzzle controller. State 1 polls its stop request, freeing
/// that task and its owned work before clearing the handle and killing this task.
/// The puzzle restores room control before requesting its stop. No work is
/// allocated here; requires successful puzzle spawning and the room overlay
/// to stay loaded through completion.
static void _neoArkShrinePuzzleSessionTask(Task* task)
{
    enum { NEO_ARK_SHRINE_PUZZLE_SESSION_START,
           NEO_ARK_SHRINE_PUZZLE_SESSION_WAIT,
           NEO_ARK_SHRINE_PUZZLE_CONTROLLER_ENTRY = 0 };
    s32 puzzleResult;

    switch (task->state) {
        case NEO_ARK_SHRINE_PUZZLE_SESSION_START:
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE);
            gNeoArkShrinePuzzleControllerTask = taskSpawnFromTable(gNeoArkShrinePuzzleTaskDescs, NEO_ARK_SHRINE_PUZZLE_CONTROLLER_ENTRY, 0, 0);
            task->state++;
            return;
        case NEO_ARK_SHRINE_PUZZLE_SESSION_WAIT:
            if (taskPollKill(gNeoArkShrinePuzzleControllerTask, &puzzleResult) != 0) {
                gNeoArkShrinePuzzleControllerTask = NULL;
                taskKill(task);
            }
            return;
    }
}

/// Installs the room's message handlers and initializes the sliding-tile board.
///
/// State 0 requires the room's writable board and initial tables. Publishes
/// the live task in the room slot and advances to the message-only idle state.
/// No work block is allocated.
static void _neoArkShrineInitializeRoom(Task* task)
{
    task->msgTable = _gNeoArkShrineRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    neoArkShrineResetPuzzle();
    task->state++;
}

/// Keeps the initialized room task available for messages without per-frame work.
static void _neoArkShrineRoomIdle(Task* task)
{
}

/// Shrine room states: 0 initialize messages/board, 1 idle, 2 release.
///
/// The dispatcher copies all three callbacks before indexing by `Task::state`.
/// There is no terminator or bounds check; keep the room loaded through dispatch.
static const TaskFuncTable3 _gNeoArkShrineRoomStateHandlers = {
    { _neoArkShrineInitializeRoom, _neoArkShrineRoomIdle, taskKill },
};

void neoArkShrineRoomTask(Task* task)
{
    TaskFuncTable3 states;

    states = _gNeoArkShrineRoomStateHandlers;
    states.funcs[task->state](task);
}

void neoArkShrinePuzzleIdle(Task* task)
{
    enum { NEO_ARK_SHRINE_PUZZLE_STATE_CLOSE  = 5,
           NEO_ARK_SHRINE_PUZZLE_CONFIRM_SLOT = 0,
           NEO_ARK_SHRINE_PUZZLE_CANCEL_SLOT  = 1 };

    ActionPromptHotspot*    hotspot = gNeoArkShrinePuzzleHotspots;
    ActionPrompt*           prompt  = gActionPromptSlots;
    NeoArkShrinePuzzleWork* work    = task->work;

    neoArkShrineDrawPuzzleFrame(task);
    gGameSession->hideHud = true;
    if (capIsBusy() != 0) {
        prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
        prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
        return;
    }
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_AIM;
    // Confirmation takes precedence over cancel and selects the first marked hit.
    if (actionPromptHitTest(hotspot, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
        prompt->mode = ACTION_PROMPT_MODE_HIGHLIGHTED;
        if (prompt->buttons.slots[NEO_ARK_SHRINE_PUZZLE_CONFIRM_SLOT].state == ACTION_PROMPT_BUTTON_PRESSED) {
            // Route the first marked hit; confirmation returns before cancel.
            {
                enum { NEO_ARK_SHRINE_PUZZLE_STATE_COMMANDS = 3,
                       NEO_ARK_SHRINE_PUZZLE_STATE_SLIDE    = 6 };
                s16 hotspotId;

                hotspotId = (hotspot)->id;
                if ((hotspot)->id != ACTION_PROMPT_HOTSPOT_END) {
                    do {
                        if ((hotspot)->hit != 0) {
                            if (hotspotId == NEO_ARK_SHRINE_HOTSPOT_OFF_BOARD || (work)->boardExamined == 0) {
                                (prompt)->mode        = ACTION_PROMPT_MODE_HIDDEN;
                                (prompt)->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
                                (work)->selection     = (hotspot)->id;
                                (work)->promptKind    = (hotspot)->promptKind;
                                (task)->state         = NEO_ARK_SHRINE_PUZZLE_STATE_COMMANDS;
                                return;
                            }
                            (work)->selection  = hotspotId;
                            (work)->promptKind = (hotspot)->promptKind;
                            (task)->state      = NEO_ARK_SHRINE_PUZZLE_STATE_SLIDE;
                            return;
                        }
                        (hotspot)++;
                        hotspotId = (hotspot)->id;
                    } while ((hotspot)->id != ACTION_PROMPT_HOTSPOT_END);
                }
            }
        }
    } else {
        prompt->mode = ACTION_PROMPT_MODE_NORMAL;
    }
    if (prompt->buttons.slots[NEO_ARK_SHRINE_PUZZLE_CANCEL_SLOT].state == ACTION_PROMPT_BUTTON_PRESSED) {
        task->state = NEO_ARK_SHRINE_PUZZLE_STATE_CLOSE;
    }
}

/// Records the solved shrine puzzle and starts its completion presentation.
///
/// The caller has matched the solved board and checked that its flag is clear.
/// Starts the solved sound, records completion, clears the map marker and plays
/// CAP sequence 3 in place, in that order. Requires the live flag bank and room
/// CAP/sound resources through playback; no board storage or task is retained.
static inline void _neoArkShrineRecordPuzzleCompletion(void)
{
    enum { NEO_ARK_SHRINE_PUZZLE_SOLVED_SEQUENCE = 3,
           NEO_ARK_SHRINE_PUZZLE_SOLVED_FLAG     = 1,
           NEO_ARK_SHRINE_MAP_MARK_HIDDEN        = 0 };

    sndEvtRequestScriptStart(SOUND_NEO_ARK_SHRINE_PUZZLE_SOLVED, 0, 0);
    gameFlagSetNibble(GAME_FLAG_NEO_ARK_SHRINE_PUZZLE_SOLVED, NEO_ARK_SHRINE_PUZZLE_SOLVED_FLAG);
    gameFlagSetNibble(GAME_FLAG_MAP_MARK_SHRINE, NEO_ARK_SHRINE_MAP_MARK_HIDDEN);
    capStartSequenceSlot(NEO_ARK_SHRINE_PUZZLE_SOLVED_SEQUENCE, CAP_PLAYBACK_IN_PLACE, 0);
}

void neoArkShrineSlidePuzzleTile(Task* task)
{
    enum { NEO_ARK_SHRINE_PUZZLE_STATE_IDLE            = 2,
           NEO_ARK_SHRINE_PUZZLE_STATE_RELEASE_ENEMIES = 9,
           NEO_ARK_SHRINE_PUZZLE_STATE_ACTIVATE_LAYOUT = 7,
           NEO_ARK_SHRINE_PUZZLE_STATE_RESTORE_LAYOUT  = 14 };

    s16                     arrangementResult;
    s16                     neighbourCell;
    s16                     selectedCell;
    s32                     neighbourIndex;
    u8                      tileSlid;
    u8                      moveAccepted;
    s16*                    selectedTile;
    u16                     tileNumber;
    NeoArkShrinePuzzleWork* work;

    work     = task->work;
    tileSlid = 0;
    neoArkShrineDrawPuzzleFrame();
    // A valid board contains one gap, so at most one neighbour can accept the tile.
    for (neighbourIndex = 0; neighbourIndex < (s32)ARRAY_SIZE(_gNeoArkShrinePuzzleCellNeighbours[0]); neighbourIndex++) {
        neighbourCell = _gNeoArkShrinePuzzleCellNeighbours[work->selection][neighbourIndex];
        if (neighbourCell == NEO_ARK_SHRINE_NEIGHBOUR_END) {
            break;
        }
        if (gNeoArkShrinePuzzleCellTiles[neighbourCell] == NEO_ARK_SHRINE_PUZZLE_GAP_TILE) {
            sndEvtRequestScriptStart(SOUND_NEO_ARK_SHRINE_TILE_SLIDE, 0, 0);
            selectedCell                                                                                      = work->selection;
            selectedTile                                                                                      = &gNeoArkShrinePuzzleCellTiles[selectedCell];
            tileNumber                                                                                        = *selectedTile;
            *selectedTile                                                                                     = gNeoArkShrinePuzzleCellTiles[_gNeoArkShrinePuzzleCellNeighbours[selectedCell][neighbourIndex]];
            tileSlid                                                                                          = 1;
            gNeoArkShrinePuzzleCellTiles[_gNeoArkShrinePuzzleCellNeighbours[work->selection][neighbourIndex]] = tileNumber;
        }
    }
    // Only a successful slide can trigger arrangement-dependent room changes.
    task->state = NEO_ARK_SHRINE_PUZZLE_STATE_IDLE;
    // The binary copies the byte result before testing it.
    if ((moveAccepted = tileSlid != 0)) {
        arrangementResult = _neoArkShrineCheckPuzzleArrangement();
        switch (arrangementResult) {
            case NEO_ARK_SHRINE_PUZZLE_MATCH_SOLVED:
                if (gameFlagGetNibble(GAME_FLAG_NEO_ARK_SHRINE_PUZZLE_SOLVED) == 0) {
                    _neoArkShrineRecordPuzzleCompletion();
                    return;
                }
                break;
            case NEO_ARK_SHRINE_PUZZLE_MATCH_RELEASE_ENEMIES:
                task->state = NEO_ARK_SHRINE_PUZZLE_STATE_RELEASE_ENEMIES;
                break;
            case NEO_ARK_SHRINE_PUZZLE_MATCH_ACTIVATE_LAYOUT:
                sndEvtRequestScriptStart(SOUND_NEO_ARK_SHRINE_MECHANISM_ACTIVATE, 0, 0);
                task->state = NEO_ARK_SHRINE_PUZZLE_STATE_ACTIVATE_LAYOUT;
                break;
            case NEO_ARK_SHRINE_PUZZLE_MATCH_RESTORE_LAYOUT:
                sndEvtRequestScriptStart(SOUND_NEO_ARK_SHRINE_MECHANISM_ACTIVATE, 0, 0);
                task->state = NEO_ARK_SHRINE_PUZZLE_STATE_RESTORE_LAYOUT;
                break;
        }
    }
}

#include "../../shared/room/action_prompt_outline_rect.inc.c"

/// Moves one tile halfway towards its target and snaps small residuals.
///
/// Requires a tile number in 0..15 and initialized screen origins in pixels.
/// Includes tile zero, the gap. Each component uses a signed right shift
/// (rounding negative half-steps down), then narrows to s16. Both components
/// snap together only when their post-step residuals are strictly below four
/// pixels. Mutates only that tile's drawn origin; its target remains unchanged.
static inline void _neoArkShrineEasePuzzleTile(s32 tileNumber)
{
    enum { NEO_ARK_SHRINE_PUZZLE_SNAP_PIXELS = 4 };

    gNeoArkShrinePuzzleDrawnOrigins[tileNumber].x +=
        (gNeoArkShrinePuzzleTargetOrigins[tileNumber].x - gNeoArkShrinePuzzleDrawnOrigins[tileNumber].x) >> 1;
    gNeoArkShrinePuzzleDrawnOrigins[tileNumber].y +=
        (gNeoArkShrinePuzzleTargetOrigins[tileNumber].y - gNeoArkShrinePuzzleDrawnOrigins[tileNumber].y) >> 1;
    if (ABS(gNeoArkShrinePuzzleDrawnOrigins[tileNumber].x - gNeoArkShrinePuzzleTargetOrigins[tileNumber].x) < NEO_ARK_SHRINE_PUZZLE_SNAP_PIXELS &&
        ABS(gNeoArkShrinePuzzleDrawnOrigins[tileNumber].y - gNeoArkShrinePuzzleTargetOrigins[tileNumber].y) < NEO_ARK_SHRINE_PUZZLE_SNAP_PIXELS) {
        gNeoArkShrinePuzzleDrawnOrigins[tileNumber].x = gNeoArkShrinePuzzleTargetOrigins[tileNumber].x;
        gNeoArkShrinePuzzleDrawnOrigins[tileNumber].y = gNeoArkShrinePuzzleTargetOrigins[tileNumber].y;
    }
}

void neoArkShrineAnimateAndDrawPuzzle(void)
{
    enum {
        NEO_ARK_SHRINE_PUZZLE_TPAGE  = getTPage(1, 0, 832, 0), // 8-bit tile sheet at VRAM (832, 0)
        NEO_ARK_SHRINE_PUZZLE_CLUT   = getClut(0, 255),        // Palette at VRAM (0, 255)
        NEO_ARK_SHRINE_PUZZLE_OT_TAG = 10,
    };

    s32       cellIndex;
    s32       tileNumber;
    POLY_FT4* tileQuad;

    // Convert the cell-to-tile board into per-tile animation targets.
    for (cellIndex = 0; cellIndex < (s32)ARRAY_SIZE(gNeoArkShrinePuzzleCellTiles); cellIndex++) {
        tileNumber                                     = gNeoArkShrinePuzzleCellTiles[cellIndex];
        gNeoArkShrinePuzzleTargetOrigins[tileNumber].x = _gNeoArkShrinePuzzleCellOrigins[cellIndex].x;
        gNeoArkShrinePuzzleTargetOrigins[tileNumber].y = _gNeoArkShrinePuzzleCellOrigins[cellIndex].y;
    }

    // Animate the gap too, but reserve packets only for visible tiles.
    for (cellIndex = 0; cellIndex < (s32)ARRAY_SIZE(gNeoArkShrinePuzzleCellTiles); cellIndex++) {
        tileNumber = gNeoArkShrinePuzzleCellTiles[cellIndex];
        _neoArkShrineEasePuzzleTile(tileNumber);
        if (tileNumber != NEO_ARK_SHRINE_PUZZLE_GAP_TILE) {
            tileQuad       = gGpuPrimCursor;
            gGpuPrimCursor = tileQuad + 1;
            setPolyFT4(tileQuad);
            setUVWH(tileQuad, _gNeoArkShrinePuzzleTileTextureOrigins[tileNumber].x, _gNeoArkShrinePuzzleTileTextureOrigins[tileNumber].y,
                    NEO_ARK_SHRINE_TILE_SIZE, NEO_ARK_SHRINE_TILE_SIZE);
            tileQuad->tpage = NEO_ARK_SHRINE_PUZZLE_TPAGE;
            tileQuad->clut  = NEO_ARK_SHRINE_PUZZLE_CLUT;
            setShadeTex(tileQuad, true);
            setXYWH(tileQuad, gNeoArkShrinePuzzleDrawnOrigins[tileNumber].x, gNeoArkShrinePuzzleDrawnOrigins[tileNumber].y,
                    NEO_ARK_SHRINE_TILE_SIZE, NEO_ARK_SHRINE_TILE_SIZE);
            addPrim(&gGpuCurrentOt[NEO_ARK_SHRINE_PUZZLE_OT_TAG], tileQuad);
        }
    }
}

/// Consumes an accepted restore request and rebuilds the base shrine layout.
///
/// Called only after the pending request equals true. Saved and live room
/// selectors change together, to layout 1 before the first enemy reveal and
/// layout 4 afterwards. The port-0 motor ramp lasts 40 eligible script frames,
/// starting at intensity 48 and targeting 96.
static inline void _neoArkShrineRestorePuzzleLayout(void)
{
    enum {
        NEO_ARK_SHRINE_RESTORE_RUMBLE_FRAMES = 40,
        NEO_ARK_SHRINE_RESTORE_RUMBLE_START  = 48,
        NEO_ARK_SHRINE_RESTORE_RUMBLE_END    = 96,
    };

    gNeoArkShrinePuzzleLayoutRestorePending = false;
    if (gameFlagGetNibble(GAME_FLAG_0E9) == 0) {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = NEO_ARK_SHRINE_LAYOUT_BASE;
        gGameSession->location.roomState                            = NEO_ARK_SHRINE_LAYOUT_BASE;
    } else {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = NEO_ARK_SHRINE_LAYOUT_BASE_AFTER_REVEAL;
        gGameSession->location.roomState                            = NEO_ARK_SHRINE_LAYOUT_BASE_AFTER_REVEAL;
    }
    gGameSession->roomObjsDirty = true;
    sndEvtRequestScriptStart(SOUND_NEO_ARK_SHRINE_MECHANISM_REVERT, 0, 0);
    padScriptSpawnVariableMotorRamp(NEO_ARK_SHRINE_RESTORE_RUMBLE_FRAMES,
                                    NEO_ARK_SHRINE_RESTORE_RUMBLE_START, NEO_ARK_SHRINE_RESTORE_RUMBLE_END);
}

/// Selects the next puzzle action and restores a pending base room layout.
///
/// Called after a tile move with a valid 4x4 board. The gap must occupy the
/// bottom-right cell for any tile pattern to match. The top-row 9..12 pattern
/// takes priority over the active-layout latch, which takes priority over the
/// 1..4 diagonal; each pattern accepts either tile order. A pending restore is
/// consumed only after those early returns, before checking the 5..8 column.
/// Returns a `NEO_ARK_SHRINE_PUZZLE_MATCH_*` action, without moving any tiles.
static s16 _neoArkShrineCheckPuzzleArrangement(void)
{
    s32 restoreRequest;

    if (gNeoArkShrinePuzzleCellTiles[0] == 9 && gNeoArkShrinePuzzleCellTiles[1] == 10 &&
        gNeoArkShrinePuzzleCellTiles[2] == 11 && gNeoArkShrinePuzzleCellTiles[3] == 12 &&
        gNeoArkShrinePuzzleCellTiles[15] == NEO_ARK_SHRINE_PUZZLE_GAP_TILE) {
        return NEO_ARK_SHRINE_PUZZLE_MATCH_ACTIVATE_LAYOUT;
    }
    if (gNeoArkShrinePuzzleCellTiles[0] == 12 && gNeoArkShrinePuzzleCellTiles[1] == 11 &&
        gNeoArkShrinePuzzleCellTiles[2] == 10 && gNeoArkShrinePuzzleCellTiles[3] == 9 &&
        gNeoArkShrinePuzzleCellTiles[15] == NEO_ARK_SHRINE_PUZZLE_GAP_TILE) {
        return NEO_ARK_SHRINE_PUZZLE_MATCH_ACTIVATE_LAYOUT;
    }
    if (gNeoArkShrinePuzzleLayoutActive == true) {
        return NEO_ARK_SHRINE_PUZZLE_MATCH_RESTORE_LAYOUT;
    }
    if (gNeoArkShrinePuzzleCellTiles[3] == 1 && gNeoArkShrinePuzzleCellTiles[6] == 2 &&
        gNeoArkShrinePuzzleCellTiles[9] == 3 && gNeoArkShrinePuzzleCellTiles[12] == 4 &&
        gNeoArkShrinePuzzleCellTiles[15] == NEO_ARK_SHRINE_PUZZLE_GAP_TILE) {
        return NEO_ARK_SHRINE_PUZZLE_MATCH_SOLVED;
    }
    if (gNeoArkShrinePuzzleCellTiles[3] == 4 && gNeoArkShrinePuzzleCellTiles[6] == 3 &&
        gNeoArkShrinePuzzleCellTiles[9] == 2 && gNeoArkShrinePuzzleCellTiles[12] == 1 &&
        gNeoArkShrinePuzzleCellTiles[15] == NEO_ARK_SHRINE_PUZZLE_GAP_TILE) {
        return NEO_ARK_SHRINE_PUZZLE_MATCH_SOLVED;
    }
    // Consume the pending restore only after the higher-priority pattern checks.
    restoreRequest = gNeoArkShrinePuzzleLayoutRestorePending;
    if (restoreRequest == true) {
        _neoArkShrineRestorePuzzleLayout();
    }
    if (gNeoArkShrinePuzzleCellTiles[0] == 5 && gNeoArkShrinePuzzleCellTiles[4] == 6 &&
        gNeoArkShrinePuzzleCellTiles[8] == 7 && gNeoArkShrinePuzzleCellTiles[12] == 8 &&
        gNeoArkShrinePuzzleCellTiles[15] == NEO_ARK_SHRINE_PUZZLE_GAP_TILE) {
        return NEO_ARK_SHRINE_PUZZLE_MATCH_RELEASE_ENEMIES;
    }
    if (gNeoArkShrinePuzzleCellTiles[0] == 8 && gNeoArkShrinePuzzleCellTiles[4] == 7 &&
        gNeoArkShrinePuzzleCellTiles[8] == 6 && gNeoArkShrinePuzzleCellTiles[12] == 5 &&
        gNeoArkShrinePuzzleCellTiles[15] == NEO_ARK_SHRINE_PUZZLE_GAP_TILE) {
        return NEO_ARK_SHRINE_PUZZLE_MATCH_RELEASE_ENEMIES;
    }
    return NEO_ARK_SHRINE_PUZZLE_MATCH_NONE;
}
