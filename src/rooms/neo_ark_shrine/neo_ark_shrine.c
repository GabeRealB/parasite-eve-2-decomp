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
#include "../../shared/action_prompt.h"

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

/// Message table installed at `Task::msgTable` by the room task's state 0.
extern TaskMessageEntry D_neo_ark_shrine_80181E34[];

extern TaskDesc D_neo_ark_shrine_80181E5C[];

/// Cap event key (`capStartSequence`'s third argument) handed to the slot-7 event
/// this room starts, so the event's exit can tell which one it was.
extern s32 D_neo_ark_shrine_80181E74;

/// Per-slot group tables for the shrine's arrangement puzzle: five `s16` order
/// indices per slot, `0xFF` terminated, into `D_neo_ark_shrine_8018686C`.
extern s16 D_neo_ark_shrine_801825EC[][5];

extern NeoArkShrineTileOrigin D_neo_ark_shrine_8018252C[16];
extern NeoArkShrineTileOrigin D_neo_ark_shrine_801825AC[16];

static s16  _neoArkShrineCheckPuzzleArrangement(void);
static void _neoArkShrineRoomIdle(Task* task);

static s32 _neoArkShrineRefuseKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedArg);
s32        func_neo_ark_shrine_8017D6AC(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32        func_neo_ark_shrine_8017D740(Task*, s32, s32, s32);
s32        func_neo_ark_shrine_8017D7F0(Task* task, s32 msgId, const void* firstArg, s32 arg3);
void       func_neo_ark_shrine_8017D84C(Task*);

TaskMessageEntry D_neo_ark_shrine_80181E34[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_neo_ark_shrine_8017D6AC },
    { ROOM_MESSAGE_USE_KEY_ITEM, _neoArkShrineRefuseKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_neo_ark_shrine_8017D7F0 },
    { ROOM_MESSAGE_COMMAND, func_neo_ark_shrine_8017D740 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_neo_ark_shrine_80181E5C[2] = {
    { { { TASK_BODY_NONE, 32 } }, func_neo_ark_shrine_8017D84C, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

s32 D_neo_ark_shrine_80181E74 = 2;

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

TaskDesc D_neo_ark_shrine_80182404[1] = {
    { { { TASK_BODY_NONE, 192 } }, neoArkShrinePuzzleCursorTask, { .value = 0 } },
};

u16 D_neo_ark_shrine_80182410[16] = {
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

ActionPromptHotspot D_neo_ark_shrine_80182430[18] = {
    { -64, -64, 32, 32, 0, 0, 0 },
    { -32, -64, 32, 32, 1, 0, 0 },
    { 0, -64, 32, 32, 2, 0, 0 },
    { 32, -64, 32, 32, 3, 0, 0 },
    { -64, -32, 32, 32, 4, 0, 0 },
    { -32, -32, 32, 32, 5, 0, 0 },
    { 0, -32, 32, 32, 6, 0, 0 },
    { 32, -32, 32, 32, 7, 0, 0 },
    { -64, 0, 32, 32, 8, 0, 0 },
    { -32, 0, 32, 32, 9, 0, 0 },
    { 0, 0, 32, 32, 10, 0, 0 },
    { 32, 0, 32, 32, 11, 0, 0 },
    { -64, 32, 32, 32, 12, 0, 0 },
    { -32, 32, 32, 32, 13, 0, 0 },
    { 0, 32, 32, 32, 14, 0, 0 },
    { 32, 32, 32, 32, 15, 0, 0 },
    { -160, -120, 320, 240, NEO_ARK_SHRINE_HOTSPOT_OFF_BOARD, 0, 0 },
    { 0, 0, 0, 0, ACTION_PROMPT_HOTSPOT_END, 0, 0 },
};

TaskDesc D_neo_ark_shrine_80182508[3] = {
    { { { TASK_BODY_NONE, 192 } }, neoArkShrinePuzzleTask, { .value = 0 } },
    { { { TASK_BODY_TMD, 192 } }, neoArkShrineFirstFallingPropTask, { .model = &_gNeoArkShrineModel049E8 } },
    { { { TASK_BODY_TMD, 192 } }, neoArkShrineSecondFallingPropTask, { .model = &_gNeoArkShrineModel04C6C } },
};

NeoArkShrineTileOrigin D_neo_ark_shrine_8018252C[16] = {
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

NeoArkShrineTileOrigin D_neo_ark_shrine_8018256C[16] = {
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

NeoArkShrineTileOrigin D_neo_ark_shrine_801825AC[16] = {
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

s16 D_neo_ark_shrine_801825EC[16][5] = {
    { 1, 4, 255, 255, 255 },
    { 0, 2, 5, 255, 255 },
    { 1, 3, 6, 255, 255 },
    { 2, 7, 255, 255, 255 },
    { 0, 5, 8, 255, 255 },
    { 1, 4, 6, 9, 255 },
    { 2, 5, 7, 10, 255 },
    { 3, 6, 11, 255, 255 },
    { 4, 9, 12, 255, 255 },
    { 5, 8, 10, 13, 255 },
    { 6, 9, 11, 14, 255 },
    { 7, 10, 15, 255, 255 },
    { 8, 13, 255, 255, 255 },
    { 9, 12, 14, 255, 255 },
    { 10, 13, 15, 255, 255 },
    { 11, 14, 255, 255, 255 },
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

s32 func_neo_ark_shrine_8017D6AC(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    mapNeoArkResolveRoomVariant(in, out);
    if (in->areaId != GAME_AREA_NEO_ARK_POWER_PLANT_1) {
        return 1;
    }
    if (gameFlagGetNibble(GAME_FLAG_NEO_ARK_SHRINE_PUZZLE_SOLVED) != 0) {
        return 1;
    }
    if (in->queryOnly != ROOM_EVENT_EXECUTE) {
        return 0;
    }
    gameFlagSetNibbleIfPresent(in->flagId, 2);
    capRunCommandWithTransition(4);
    return 0;
}

s32 func_neo_ark_shrine_8017D740(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    s32 bit2;

    if (arg2 == 7) {
        bit2 = areaGetCurrentObjectState(7);
        if (bit2 == 1) {
            capStartSequenceSlot(7, 1, (s16)D_neo_ark_shrine_80181E74);
            if (D_neo_ark_shrine_80181E74 == 2) {
                D_neo_ark_shrine_80181E74 = bit2;
            }
        } else {
            capStartSequenceSlot(7, 1, 0);
        }
    }
    if (arg2 == 5) {
        capRunCommandWithTransition(gameFlagGetNibble(GAME_FLAG_NEO_ARK_POWER_PLANT_1_CLEARED) == 0 ? 5 : 0xC);
    }
    return 0;
}

s32 func_neo_ark_shrine_8017D7F0(Task* task, s32 msgId, const void* firstArg, s32 arg3)
{
    const DirectionActionRequest* request = firstArg;

    if (request->actionId == 1) {
        if (gameFlagGetNibble(GAME_FLAG_NEO_ARK_POWER_PLANT_2_CLEARED) == 0) {
            taskSpawnFromTable(D_neo_ark_shrine_80181E5C, 0, 0, 0);
        } else {
            capRunCommandWithTransition(9);
        }
    }
    return 0;
}

void func_neo_ark_shrine_8017D84C(Task* task)
{
    s32 sp10;

    switch (task->state) {
        case 0:
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE);
            D_neo_ark_shrine_80186864 = taskSpawnFromTable(D_neo_ark_shrine_80182508, 0, 0, 0);
            task->state++;
            return;
        case 1:
            if (taskPollKill(D_neo_ark_shrine_80186864, &sp10) != 0) {
                D_neo_ark_shrine_80186864 = NULL;
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
    task->msgTable = D_neo_ark_shrine_80181E34;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    neoArkShrineResetPuzzle();
    task->state++;
}

/// Keeps the initialized room task available for messages without per-frame work.
static void _neoArkShrineRoomIdle(Task* task)
{
}

/// State handlers of the room task `neoArkShrineRoomTask`, indexed by
/// `Task::state`: the set-up tick, the idle tick, and `taskKill`.
static const TaskFuncTable3 D_neo_ark_shrine_8017D5C4 = {
    { _neoArkShrineInitializeRoom, _neoArkShrineRoomIdle, taskKill },
};

void neoArkShrineRoomTask(Task* task)
{
    TaskFuncTable3 states;

    states = D_neo_ark_shrine_8017D5C4;
    states.funcs[task->state](task);
}

/// Routes the first marked hotspot and returns from the caller when confirmed.
///
/// Invoke in a braced statement context inside a void function, after hit
/// testing and a confirm press. `task`, `prompt` and `work` must be stable
/// live pointers; they are evaluated only for a hit, possibly repeatedly.
/// `hotspot` must be a modifiable, stable `ActionPromptHotspot*` lvalue into
/// a marked, terminated table; it advances while scanning. No argument may
/// name the local `hotspotId`. Captures no caller identifiers. A hit latches
/// its id and prompt kind and changes state; command mode also hides/stops
/// the cursor. The return skips the caller's remaining input processing.
#define NEO_ARK_SHRINE_ROUTE_PUZZLE_CONFIRMATION(task, prompt, work, hotspot)                          \
    {                                                                                                  \
        enum { NEO_ARK_SHRINE_PUZZLE_STATE_COMMANDS = 3,                                               \
               NEO_ARK_SHRINE_PUZZLE_STATE_SLIDE    = 6 };                                                \
        s16 hotspotId;                                                                                 \
                                                                                                       \
        hotspotId = (hotspot)->id;                                                                     \
        if ((hotspot)->id != ACTION_PROMPT_HOTSPOT_END) {                                              \
            do {                                                                                       \
                if ((hotspot)->hit != 0) {                                                             \
                    if (hotspotId == NEO_ARK_SHRINE_HOTSPOT_OFF_BOARD || (work)->boardExamined == 0) { \
                        (prompt)->mode        = ACTION_PROMPT_MODE_HIDDEN;                             \
                        (prompt)->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;                           \
                        (work)->selection     = (hotspot)->id;                                         \
                        (work)->promptKind    = (hotspot)->promptKind;                                 \
                        (task)->state         = NEO_ARK_SHRINE_PUZZLE_STATE_COMMANDS;                  \
                        return;                                                                        \
                    }                                                                                  \
                    (work)->selection  = hotspotId;                                                    \
                    (work)->promptKind = (hotspot)->promptKind;                                        \
                    (task)->state      = NEO_ARK_SHRINE_PUZZLE_STATE_SLIDE;                            \
                    return;                                                                            \
                }                                                                                      \
                (hotspot)++;                                                                           \
                hotspotId = (hotspot)->id;                                                             \
            } while ((hotspot)->id != ACTION_PROMPT_HOTSPOT_END);                                      \
        }                                                                                              \
    }

void neoArkShrinePuzzleIdle(Task* task)
{
    enum { NEO_ARK_SHRINE_PUZZLE_STATE_CLOSE  = 5,
           NEO_ARK_SHRINE_PUZZLE_CONFIRM_SLOT = 0,
           NEO_ARK_SHRINE_PUZZLE_CANCEL_SLOT  = 1 };

    ActionPromptHotspot*    hotspot = D_neo_ark_shrine_80182430;
    ActionPrompt*           prompt  = D_80114D28;
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
        prompt->mode = ACTION_PROMPT_MODE_HOTSPOT;
        if (prompt->buttons.slots[NEO_ARK_SHRINE_PUZZLE_CONFIRM_SLOT].state == ACTION_PROMPT_BUTTON_PRESSED) {
            NEO_ARK_SHRINE_ROUTE_PUZZLE_CONFIRMATION(task, prompt, work, hotspot);
        }
    } else {
        prompt->mode = ACTION_PROMPT_MODE_IDLE;
    }
    if (prompt->buttons.slots[NEO_ARK_SHRINE_PUZZLE_CANCEL_SLOT].state == ACTION_PROMPT_BUTTON_PRESSED) {
        task->state = NEO_ARK_SHRINE_PUZZLE_STATE_CLOSE;
    }
}

#undef NEO_ARK_SHRINE_ROUTE_PUZZLE_CONFIRMATION

/// Runs one step of the shrine's arrangement puzzle: for each of the five
/// entries of the group the current slot selects, it rotates the entry's index
/// to the front of `D_neo_ark_shrine_8018686C` when that index is still unused,
/// and plays a click for each move. The task then advances to state 2 and, if
/// anything moved, hands the step the group's helper reports to the cap
/// script - state 9 for a completed set, 7 / 0xE for the two sound-only steps,
/// and the flag-0xDB branch that starts the cap slot for the last one.
///
/// Three shapes here are load-bearing, not style:
///
/// - The walk is a `for` loop rather than the `do { } while` splat's `goto`
///   form compiles to. Only the front end's `NOTE_INSN_LOOP_BEG` marks make
///   loop.c run, and it is what hoists the two table addresses into the
///   preheader; the `goto` form leaves both `lui/addiu` pairs re-materialized
///   inside the loop.
/// - The tables are indexed as arrays (`D_...[i]`), which is what puts the
///   scaled index on the left of the address sum and adds the base last.
///   Reaching them through a pointer local instead flips both adds.
/// - `swapped` is a `u8`, and it is tested as an assignment inside the
///   condition. Narrow, its 0/1 stores are recorded by reload's CSE in QImode,
///   so they are not substituted for the `SImode` constant 0 of `i = 0` or the
///   shift amount of `state * 2`; and the `u8` store and the `zero_extend` the
///   test needs sit in one statement, which is close enough for combine to fold
///   the pair into a plain copy of the flag. Widen `swapped` or split the test
///   from the assignment and one of those three spots stops matching.
void func_neo_ark_shrine_8017DB10(Task* arg0)
{
    s16                     arrangementResult;
    s16                     state;
    s16                     slot;
    s32                     i;
    u8                      swapped;
    u8                      moved;
    u16*                    ord;
    u16                     prev;
    NeoArkShrinePuzzleWork* work;

    work    = arg0->work;
    swapped = 0;
    neoArkShrineDrawPuzzleFrame();
    for (i = 0; i < 5; i++) {
        state = D_neo_ark_shrine_801825EC[work->selection][i];
        if (state == 0xFF) {
            break;
        }
        if (D_neo_ark_shrine_8018686C[state] == 0) {
            sndEvtRequestScriptStart(SOUND_NEO_ARK_SHRINE_TILE_SLIDE, 0, 0);
            slot                                                                     = work->selection;
            ord                                                                      = (u16*)&D_neo_ark_shrine_8018686C[slot];
            prev                                                                     = *ord;
            *ord                                                                     = D_neo_ark_shrine_8018686C[D_neo_ark_shrine_801825EC[slot][i]];
            swapped                                                                  = 1;
            D_neo_ark_shrine_8018686C[D_neo_ark_shrine_801825EC[work->selection][i]] = prev;
        }
    }
    arg0->state = 2;
    if ((moved = swapped != 0)) {
        arrangementResult = _neoArkShrineCheckPuzzleArrangement();
        switch (arrangementResult) {
            case NEO_ARK_SHRINE_PUZZLE_MATCH_SOLVED:
                if (gameFlagGetNibble(GAME_FLAG_NEO_ARK_SHRINE_PUZZLE_SOLVED) == 0) {
                    sndEvtRequestScriptStart(SOUND_NEO_ARK_SHRINE_PUZZLE_SOLVED, 0, 0);
                    gameFlagSetNibble(GAME_FLAG_NEO_ARK_SHRINE_PUZZLE_SOLVED, 1);
                    gameFlagSetNibble(GAME_FLAG_MAP_MARK_SHRINE, 0);
                    capStartSequenceSlot(3, 0, 0);
                    return;
                }
                break;
            case NEO_ARK_SHRINE_PUZZLE_MATCH_RELEASE_ENEMIES:
                arg0->state = 9;
                break;
            case NEO_ARK_SHRINE_PUZZLE_MATCH_ACTIVATE_LAYOUT:
                sndEvtRequestScriptStart(SOUND_NEO_ARK_SHRINE_MECHANISM_ACTIVATE, 0, 0);
                arg0->state = 7;
                break;
            case NEO_ARK_SHRINE_PUZZLE_MATCH_RESTORE_LAYOUT:
                sndEvtRequestScriptStart(SOUND_NEO_ARK_SHRINE_MECHANISM_ACTIVATE, 0, 0);
                arg0->state = 0xE;
                break;
        }
    }
}

#include "../../shared/action_prompt_outline_rect.inc.c"

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

    D_neo_ark_shrine_8018688C[tileNumber].x +=
        (D_neo_ark_shrine_801868CC[tileNumber].x - D_neo_ark_shrine_8018688C[tileNumber].x) >> 1;
    D_neo_ark_shrine_8018688C[tileNumber].y +=
        (D_neo_ark_shrine_801868CC[tileNumber].y - D_neo_ark_shrine_8018688C[tileNumber].y) >> 1;
    if (ABS(D_neo_ark_shrine_8018688C[tileNumber].x - D_neo_ark_shrine_801868CC[tileNumber].x) < NEO_ARK_SHRINE_PUZZLE_SNAP_PIXELS &&
        ABS(D_neo_ark_shrine_8018688C[tileNumber].y - D_neo_ark_shrine_801868CC[tileNumber].y) < NEO_ARK_SHRINE_PUZZLE_SNAP_PIXELS) {
        D_neo_ark_shrine_8018688C[tileNumber].x = D_neo_ark_shrine_801868CC[tileNumber].x;
        D_neo_ark_shrine_8018688C[tileNumber].y = D_neo_ark_shrine_801868CC[tileNumber].y;
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
    for (cellIndex = 0; cellIndex < (s32)ARRAY_SIZE(D_neo_ark_shrine_8018686C); cellIndex++) {
        tileNumber                              = D_neo_ark_shrine_8018686C[cellIndex];
        D_neo_ark_shrine_801868CC[tileNumber].x = D_neo_ark_shrine_8018252C[cellIndex].x;
        D_neo_ark_shrine_801868CC[tileNumber].y = D_neo_ark_shrine_8018252C[cellIndex].y;
    }

    // Animate the gap too, but reserve packets only for visible tiles.
    for (cellIndex = 0; cellIndex < (s32)ARRAY_SIZE(D_neo_ark_shrine_8018686C); cellIndex++) {
        tileNumber = D_neo_ark_shrine_8018686C[cellIndex];
        _neoArkShrineEasePuzzleTile(tileNumber);
        if (tileNumber != NEO_ARK_SHRINE_PUZZLE_GAP_TILE) {
            tileQuad       = gGpuPrimCursor;
            gGpuPrimCursor = tileQuad + 1;
            setPolyFT4(tileQuad);
            setUVWH(tileQuad, D_neo_ark_shrine_801825AC[tileNumber].x, D_neo_ark_shrine_801825AC[tileNumber].y,
                    NEO_ARK_SHRINE_TILE_SIZE, NEO_ARK_SHRINE_TILE_SIZE);
            tileQuad->tpage = NEO_ARK_SHRINE_PUZZLE_TPAGE;
            tileQuad->clut  = NEO_ARK_SHRINE_PUZZLE_CLUT;
            setShadeTex(tileQuad, true);
            setXYWH(tileQuad, D_neo_ark_shrine_8018688C[tileNumber].x, D_neo_ark_shrine_8018688C[tileNumber].y,
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

    D_neo_ark_shrine_8018686A = false;
    if (gameFlagGetNibble(GAME_FLAG_0E9) == 0) {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = NEO_ARK_SHRINE_LAYOUT_BASE;
        gGameSession->location.loc.room                            = NEO_ARK_SHRINE_LAYOUT_BASE;
    } else {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = NEO_ARK_SHRINE_LAYOUT_BASE_AFTER_REVEAL;
        gGameSession->location.loc.room                            = NEO_ARK_SHRINE_LAYOUT_BASE_AFTER_REVEAL;
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

    if (D_neo_ark_shrine_8018686C[0] == 9 && D_neo_ark_shrine_8018686C[1] == 10 &&
        D_neo_ark_shrine_8018686C[2] == 11 && D_neo_ark_shrine_8018686C[3] == 12 &&
        D_neo_ark_shrine_8018686C[15] == NEO_ARK_SHRINE_PUZZLE_GAP_TILE) {
        return NEO_ARK_SHRINE_PUZZLE_MATCH_ACTIVATE_LAYOUT;
    }
    if (D_neo_ark_shrine_8018686C[0] == 12 && D_neo_ark_shrine_8018686C[1] == 11 &&
        D_neo_ark_shrine_8018686C[2] == 10 && D_neo_ark_shrine_8018686C[3] == 9 &&
        D_neo_ark_shrine_8018686C[15] == NEO_ARK_SHRINE_PUZZLE_GAP_TILE) {
        return NEO_ARK_SHRINE_PUZZLE_MATCH_ACTIVATE_LAYOUT;
    }
    if (D_neo_ark_shrine_80186868 == true) {
        return NEO_ARK_SHRINE_PUZZLE_MATCH_RESTORE_LAYOUT;
    }
    if (D_neo_ark_shrine_8018686C[3] == 1 && D_neo_ark_shrine_8018686C[6] == 2 &&
        D_neo_ark_shrine_8018686C[9] == 3 && D_neo_ark_shrine_8018686C[12] == 4 &&
        D_neo_ark_shrine_8018686C[15] == NEO_ARK_SHRINE_PUZZLE_GAP_TILE) {
        return NEO_ARK_SHRINE_PUZZLE_MATCH_SOLVED;
    }
    if (D_neo_ark_shrine_8018686C[3] == 4 && D_neo_ark_shrine_8018686C[6] == 3 &&
        D_neo_ark_shrine_8018686C[9] == 2 && D_neo_ark_shrine_8018686C[12] == 1 &&
        D_neo_ark_shrine_8018686C[15] == NEO_ARK_SHRINE_PUZZLE_GAP_TILE) {
        return NEO_ARK_SHRINE_PUZZLE_MATCH_SOLVED;
    }
    // Consume the pending restore only after the higher-priority pattern checks.
    restoreRequest = D_neo_ark_shrine_8018686A;
    if (restoreRequest == true) {
        _neoArkShrineRestorePuzzleLayout();
    }
    if (D_neo_ark_shrine_8018686C[0] == 5 && D_neo_ark_shrine_8018686C[4] == 6 &&
        D_neo_ark_shrine_8018686C[8] == 7 && D_neo_ark_shrine_8018686C[12] == 8 &&
        D_neo_ark_shrine_8018686C[15] == NEO_ARK_SHRINE_PUZZLE_GAP_TILE) {
        return NEO_ARK_SHRINE_PUZZLE_MATCH_RELEASE_ENEMIES;
    }
    if (D_neo_ark_shrine_8018686C[0] == 8 && D_neo_ark_shrine_8018686C[4] == 7 &&
        D_neo_ark_shrine_8018686C[8] == 6 && D_neo_ark_shrine_8018686C[12] == 5 &&
        D_neo_ark_shrine_8018686C[15] == NEO_ARK_SHRINE_PUZZLE_GAP_TILE) {
        return NEO_ARK_SHRINE_PUZZLE_MATCH_RELEASE_ENEMIES;
    }
    return NEO_ARK_SHRINE_PUZZLE_MATCH_NONE;
}
