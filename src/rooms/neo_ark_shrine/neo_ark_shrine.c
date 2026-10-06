#include "rooms/neo_ark_shrine.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>

#include "types.h"

#include "neo_ark_shrine_private.h"

#include "gameplay/action_prompt.h"
#include "gameplay/captions.h"
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

/// Message table installed at `Task::msgTable` by the room task's state 0.
extern TaskMessageEntry D_neo_ark_shrine_80181E34[];

extern TaskDesc D_neo_ark_shrine_80181E5C[];

/// Cap event key (`Gp_StartCap`'s third argument) handed to the slot-7 event
/// this room starts, so the event's exit can tell which one it was.
extern s32 D_neo_ark_shrine_80181E74;

/// Per-slot group tables for the shrine's arrangement puzzle: five `s16` order
/// indices per slot, `0xFF` terminated, into `D_neo_ark_shrine_8018686C`.
extern s16 D_neo_ark_shrine_801825EC[][5];

extern NeoArkShrineTileOrigin D_neo_ark_shrine_8018252C[16];
extern NeoArkShrineTileOrigin D_neo_ark_shrine_801825AC[16];

/// Steps the currently selected group and returns which kind of step it was.
static s16 func_neo_ark_shrine_8017E254(void);

s32  func_neo_ark_shrine_8017D6A4(Task*, s32, s32, s32);
s32  func_neo_ark_shrine_8017D6AC(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32  func_neo_ark_shrine_8017D740(Task*, s32, s32, s32);
s32  func_neo_ark_shrine_8017D7F0(Task* task, s32 msgId, const void* firstArg, s32 arg3);
void func_neo_ark_shrine_8017D84C(Task*);

TaskMessageEntry D_neo_ark_shrine_80181E34[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_neo_ark_shrine_8017D6AC },
    { 5105, func_neo_ark_shrine_8017D6A4 },
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
    { { { TASK_BODY_NONE, 192 } }, func_neo_ark_shrine_8017EA70, { .value = 0 } },
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
    { { { TASK_BODY_NONE, 192 } }, func_neo_ark_shrine_8017EAE0, { .value = 0 } },
    { { { TASK_BODY_TMD, 192 } }, func_neo_ark_shrine_8017EB54, { .model = &_gNeoArkShrineModel049E8 } },
    { { { TASK_BODY_TMD, 192 } }, func_neo_ark_shrine_8017EBB8, { .model = &_gNeoArkShrineModel04C6C } },
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

static void func_neo_ark_shrine_8017D8F4(Task* task);
static void func_neo_ark_shrine_8017D940(Task* task);

/// Always returns 0.
s32 func_neo_ark_shrine_8017D6A4(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

s32 func_neo_ark_shrine_8017D6AC(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_map_neo_ark_80179B14(in, out);
    if (in->areaId != GAME_AREA_NEO_ARK_POWER_PLANT_1) {
        return 1;
    }
    if (gameFlagGetNibble(GAME_FLAG_NEO_ARK_SHRINE_PUZZLE_SOLVED) != 0) {
        return 1;
    }
    if (in->queryOnly != ROOM_EVENT_EXECUTE) {
        return 0;
    }
    Gp_SetNibbleIf(in->flagId, 2);
    Gp_RunCapCmd1(4);
    return 0;
}

s32 func_neo_ark_shrine_8017D740(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    s32 bit2;

    if (arg2 == 7) {
        bit2 = Gp_GetCurBit2Flag(7);
        if (bit2 == 1) {
            Gp_StartCapSlot(7, 1, (s16)D_neo_ark_shrine_80181E74);
            if (D_neo_ark_shrine_80181E74 == 2) {
                D_neo_ark_shrine_80181E74 = bit2;
            }
        } else {
            Gp_StartCapSlot(7, 1, 0);
        }
    }
    if (arg2 == 5) {
        Gp_RunCapCmd1(gameFlagGetNibble(GAME_FLAG_NEO_ARK_POWER_PLANT_1_CLEARED) == 0 ? 5 : 0xC);
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
            Gp_RunCapCmd1(9);
        }
    }
    return 0;
}

void func_neo_ark_shrine_8017D84C(Task* task)
{
    s32 sp10;

    switch (task->state) {
        case 0:
            Gp_MsgPlayerWeapon(0);
            Gp_MsgPlayer3F3(0);
            D_neo_ark_shrine_80186864 = taskSpawnFromTable(D_neo_ark_shrine_80182508, 0, 0, 0);
            task->state++;
            return;
        case 1:
            if (Task_PollKill(D_neo_ark_shrine_80186864, &sp10) != 0) {
                D_neo_ark_shrine_80186864 = NULL;
                taskKill(task);
            }
            return;
    }
}

static void func_neo_ark_shrine_8017D8F4(Task* task)
{
    task->msgTable = D_neo_ark_shrine_80181E34;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    func_neo_ark_shrine_8017F448();
    task->state++;
}

/// Second state of the room task: nothing left to do but idle.
static void func_neo_ark_shrine_8017D940(Task* task)
{
}

/// State handlers of the room task `func_neo_ark_shrine_8017D948`, indexed by
/// `Task::state`: the set-up tick, the idle tick, and `taskKill`.
static const TaskFuncTable3 D_neo_ark_shrine_8017D5C4 = {
    { func_neo_ark_shrine_8017D8F4, func_neo_ark_shrine_8017D940, taskKill },
};

/// Room task: runs the state handler `D_neo_ark_shrine_8017D5C4` names for
/// `Task::state`, through a copy of the table taken onto the stack.
void func_neo_ark_shrine_8017D948(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_neo_ark_shrine_8017D5C4;
    sp.funcs[task->state](task);
}

/// Idle state of the shrine's cap script: the hotspot the cursor sits on is
/// confirm-tested (`buttons.slots[0].state == ACTION_PROMPT_BUTTON_PRESSED`) and its `id` / `promptKind` are
/// latched into the work block's `selection` / `promptKind`. The confirm goes
/// to state 3, which opens the command prompt, for
/// `NEO_ARK_SHRINE_HOTSPOT_OFF_BOARD` and for any board cell while
/// `boardExamined` is clear; a board cell after that goes to state 6, which
/// slides its tile. The scan walks the hotspot table the
/// hit test `actionPromptHitTest` just marked, and
/// `buttons.slots[1].state == ACTION_PROMPT_BUTTON_PRESSED` leaves the scan by advancing the task to state 5.
///
/// Both oddities below are allocator levers, not logic. The `do { } while (0)`
/// around the last state store folds away, but flow counts the reference at
/// loop depth 2, which is what lifts the parameter above the hotspot pointer in
/// global-alloc's rank; without it the two swap `$s2`/`$s4`. Passing `task` to
/// the per-frame helper is the sched1 counterpart: the extra `$a0` set makes
/// the hotspot scan's argument setup *not* a "birthing insn" in sched1's
/// `adjust_priority`, so it is not launched at `LAUNCH_PRIORITY` and the scan
/// block keeps `hs` in the branch delay slot. At entry `$a0` still holds
/// `task`, so the copy itself is dropped by the allocator.
void func_neo_ark_shrine_8017D9A0(Task* task)
{
    ActionPromptHotspot*    hs     = D_neo_ark_shrine_80182430;
    ActionPrompt*           prompt = D_80114D28;
    NeoArkShrinePuzzleWork* work   = task->work;
    u16                     id;

    func_neo_ark_shrine_8017EAC0(task);
    gGameSession->hideHud = 1;
    if (capIsBusy() != 0) {
        prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
        prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
        return;
    }
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_AIM;
    if (actionPromptHitTest(hs, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
        prompt->mode = ACTION_PROMPT_MODE_HOTSPOT;
        if (prompt->buttons.slots[0].state == ACTION_PROMPT_BUTTON_PRESSED) {
            id = hs->id;
            if (hs->id != ACTION_PROMPT_HOTSPOT_END) {
                do {
                    if (hs->hit != 0) {
                        if ((s16)id == NEO_ARK_SHRINE_HOTSPOT_OFF_BOARD || work->boardExamined == 0) {
                            prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
                            prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
                            work->selection     = hs->id;
                            work->promptKind    = hs->promptKind;
                            task->state         = 3;
                            return;
                        }
                        work->selection  = id;
                        work->promptKind = hs->promptKind;
                        task->state      = 6;
                        return;
                    }
                    hs++;
                    id = hs->id;
                } while (hs->id != ACTION_PROMPT_HOTSPOT_END);
            }
        }
    } else {
        prompt->mode = ACTION_PROMPT_MODE_IDLE;
    }
    if (prompt->buttons.slots[1].state == ACTION_PROMPT_BUTTON_PRESSED) {
        do {
            task->state = 5;
        } while (0);
    }
}

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
    s16                     temp_v0;
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
    func_neo_ark_shrine_8017EAC0();
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
        temp_v0 = func_neo_ark_shrine_8017E254();
        switch (temp_v0) {
            case 1:
                if (gameFlagGetNibble(GAME_FLAG_NEO_ARK_SHRINE_PUZZLE_SOLVED) == 0) {
                    sndEvtRequestScriptStart(SOUND_NEO_ARK_SHRINE_PUZZLE_SOLVED, 0, 0);
                    gameFlagSetNibble(GAME_FLAG_NEO_ARK_SHRINE_PUZZLE_SOLVED, 1);
                    gameFlagSetNibble(GAME_FLAG_MAP_MARK_SHRINE, 0);
                    Gp_StartCapSlot(3, 0, 0);
                    return;
                }
                break;
            case 2:
                arg0->state = 9;
                break;
            case 3:
                sndEvtRequestScriptStart(SOUND_NEO_ARK_SHRINE_MECHANISM_ACTIVATE, 0, 0);
                arg0->state = 7;
                break;
            case 4:
                sndEvtRequestScriptStart(SOUND_NEO_ARK_SHRINE_MECHANISM_ACTIVATE, 0, 0);
                arg0->state = 0xE;
                break;
        }
    }
}

#include "../../shared/action_prompt_outline_rect.inc.c"

/// Animates and draws the shrine's sliding-tile puzzle. Each tile's target
/// position is taken from the board position it now occupies; its drawn
/// position eases halfway there every frame and snaps once both axes are
/// within four units. Every tile but tile 0, the gap, is then drawn as a 32x32
/// textured quad.
void func_neo_ark_shrine_8017DF7C(void)
{
    s32                     i;
    s32                     tile;
    NeoArkShrineTileOrigin* cur;
    NeoArkShrineTileOrigin* tgt;
    POLY_FT4*               prim;

    for (i = 0; i < 16; i++) {
        tile                              = D_neo_ark_shrine_8018686C[i];
        D_neo_ark_shrine_801868CC[tile].x = D_neo_ark_shrine_8018252C[i].x;
        D_neo_ark_shrine_801868CC[tile].y = D_neo_ark_shrine_8018252C[i].y;
    }

    for (i = 0; i < 16; i++) {
        tile    = D_neo_ark_shrine_8018686C[i];
        cur     = &D_neo_ark_shrine_8018688C[tile];
        tgt     = &D_neo_ark_shrine_801868CC[tile];
        cur->x += (tgt->x - cur->x) >> 1;
        cur->y += (tgt->y - cur->y) >> 1;
        if (ABS(cur->x - tgt->x) < 4 &&
            ABS(D_neo_ark_shrine_8018688C[tile].y - D_neo_ark_shrine_801868CC[tile].y) < 4) {
            D_neo_ark_shrine_8018688C[tile].x = D_neo_ark_shrine_801868CC[tile].x;
            D_neo_ark_shrine_8018688C[tile].y = D_neo_ark_shrine_801868CC[tile].y;
        }
        if (tile != 0) {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyFT4(prim);
            setUVWH(prim, D_neo_ark_shrine_801825AC[tile].x, D_neo_ark_shrine_801825AC[tile].y,
                    NEO_ARK_SHRINE_TILE_SIZE, NEO_ARK_SHRINE_TILE_SIZE);
            prim->tpage = 0x8D;
            prim->clut  = 0x3FC0;
            setShadeTex(prim, 1);
            setXYWH(prim, D_neo_ark_shrine_8018688C[tile].x, D_neo_ark_shrine_8018688C[tile].y,
                    NEO_ARK_SHRINE_TILE_SIZE, NEO_ARK_SHRINE_TILE_SIZE);
            addPrim(&gGpuCurrentOt[10], prim);
        }
    }
}

static s16 func_neo_ark_shrine_8017E254(void)
{
    s32 flag;

    if (D_neo_ark_shrine_8018686C[0] == 9 && D_neo_ark_shrine_8018686C[1] == 10 &&
        D_neo_ark_shrine_8018686C[2] == 11 && D_neo_ark_shrine_8018686C[3] == 12 &&
        D_neo_ark_shrine_8018686C[15] == 0) {
        return 3;
    }
    if (D_neo_ark_shrine_8018686C[0] == 12 && D_neo_ark_shrine_8018686C[1] == 11 &&
        D_neo_ark_shrine_8018686C[2] == 10 && D_neo_ark_shrine_8018686C[3] == 9 &&
        D_neo_ark_shrine_8018686C[15] == 0) {
        return 3;
    }
    if (D_neo_ark_shrine_80186868 == 1) {
        return 4;
    }
    if (D_neo_ark_shrine_8018686C[3] == 1 && D_neo_ark_shrine_8018686C[6] == 2 &&
        D_neo_ark_shrine_8018686C[9] == 3 && D_neo_ark_shrine_8018686C[12] == 4 &&
        D_neo_ark_shrine_8018686C[15] == 0) {
        return 1;
    }
    if (D_neo_ark_shrine_8018686C[3] == 4 && D_neo_ark_shrine_8018686C[6] == 3 &&
        D_neo_ark_shrine_8018686C[9] == 2 && D_neo_ark_shrine_8018686C[12] == 1 &&
        D_neo_ark_shrine_8018686C[15] == 0) {
        return 1;
    }
    flag = D_neo_ark_shrine_8018686A;
    if (flag == 1) {
        D_neo_ark_shrine_8018686A = 0;
        if (gameFlagGetNibble(GAME_FLAG_0E9) == 0) {
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = flag;
            gGameSession->location.loc.room                            = flag;
        } else {
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = 4;
            gGameSession->location.loc.room                            = 4;
        }
        gGameSession->roomObjsDirty = 1;
        sndEvtRequestScriptStart(SOUND_NEO_ARK_SHRINE_MECHANISM_REVERT, 0, 0);
        Gp_SpawnPadLerp(0x28, 0x30, 0x60);
    }
    if (D_neo_ark_shrine_8018686C[0] == 5 && D_neo_ark_shrine_8018686C[4] == 6 &&
        D_neo_ark_shrine_8018686C[8] == 7 && D_neo_ark_shrine_8018686C[12] == 8 &&
        D_neo_ark_shrine_8018686C[15] == 0) {
        return 2;
    }
    if (D_neo_ark_shrine_8018686C[0] == 8 && D_neo_ark_shrine_8018686C[4] == 7 &&
        D_neo_ark_shrine_8018686C[8] == 6 && D_neo_ark_shrine_8018686C[12] == 5 &&
        D_neo_ark_shrine_8018686C[15] == 0) {
        return 2;
    }
    return 0;
}
